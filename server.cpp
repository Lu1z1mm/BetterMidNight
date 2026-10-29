#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <process.h>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

struct ClientSession {
    SOCKET socket;
    uint32_t player_id;
    char gamertag[32];
    PlayerState state;
    int active;
};

static ClientSession g_clients[MAX_PLAYERS];
static CRITICAL_SECTION g_lock;
static volatile int g_running = 1;
static uint32_t g_next_id = 1;
static MissionData g_mission;

// Checkpoints reais mapeados nas ruas do jogo
static Vec3 g_checkpoints[] = {
    {1390.0f, -2.0f, -125.0f},
    {1430.0f, -2.0f, -180.0f},
    {1195.0f, -2.0f, -350.0f},
    { 720.0f,  3.5f, -260.0f},
    {1475.0f, -1.5f,  140.0f}
};
static const int TOTAL_CPS = sizeof(g_checkpoints) / sizeof(g_checkpoints[0]);

void init_mission() {
    g_mission.mission_id = 1;
    strcpy_s(g_mission.title, "Midnight Sprint - Etapa 1");
    g_mission.checkpoint = g_checkpoints[0];
    g_mission.radius = 15.0f; // 15 metros
    g_mission.checkpoint_index = 1;
    g_mission.total_checkpoints = TOTAL_CPS;
    g_mission.active = 1;
}

void broadcast_packet(NetPacket *pkt, uint32_t exclude_player_id) {
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_clients[i].active && g_clients[i].player_id != exclude_player_id) {
            send(g_clients[i].socket, (char*)pkt, sizeof(NetPacket), 0);
        }
    }
    LeaveCriticalSection(&g_lock);
}

void send_packet(SOCKET s, NetPacket *pkt) {
    send(s, (char*)pkt, sizeof(NetPacket), 0);
}

static unsigned __stdcall client_thread(void *arg) {
    int slot = (int)(intptr_t)arg;
    SOCKET s = g_clients[slot].socket;
    uint32_t my_id = g_clients[slot].player_id;
    NetPacket pkt;

    printf("[+] Jogador conectado no Slot %d (ID %u)\n", slot, my_id);
    fflush(stdout);

    // Envia confirmacao de entrada (JOIN_ACK)
    NetPacket ack = {0};
    ack.magic = MC3_NET_MAGIC;
    ack.msg_type = MSG_JOIN_ACK;
    ack.player_id = my_id;
    send_packet(s, &ack);

    // Sincroniza a missao atual
    NetPacket miss_pkt = {0};
    miss_pkt.magic = MC3_NET_MAGIC;
    miss_pkt.msg_type = MSG_MISSION_SYNC;
    miss_pkt.player_id = 0;
    miss_pkt.data_len = sizeof(MissionData);
    memcpy(miss_pkt.data, &g_mission, sizeof(MissionData));
    send_packet(s, &miss_pkt);

    // Loop de recebimento
    while (g_running) {
        int received = 0;
        char *ptr = (char*)&pkt;
        while (received < (int)sizeof(NetPacket)) {
            int n = recv(s, ptr + received, sizeof(NetPacket) - received, 0);
            if (n <= 0) goto disconnect;
            received += n;
        }

        if (pkt.magic != MC3_NET_MAGIC) continue;

        switch (pkt.msg_type) {
            case MSG_JOIN_REQ: {
                EnterCriticalSection(&g_lock);
                strncpy_s(g_clients[slot].gamertag, pkt.data, 31);
                g_clients[slot].gamertag[31] = 0;
                printf("[LOBBY] '%s' entrou no jogo! (Total online: ", g_clients[slot].gamertag);
                int online = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) if (g_clients[i].active) online++;
                printf("%d)\n", online);
                fflush(stdout);
                LeaveCriticalSection(&g_lock);
                break;
            }

            case MSG_POS_UPDATE: {
                if (pkt.data_len == sizeof(PlayerState)) {
                    PlayerState *st = (PlayerState*)pkt.data;
                    EnterCriticalSection(&g_lock);
                    g_clients[slot].state = *st;
                    LeaveCriticalSection(&g_lock);

                    // Retransmite a posicao para todos os outros jogadores
                    broadcast_packet(&pkt, my_id);
                }
                break;
            }

            case MSG_CHECKPOINT_HIT: {
                EnterCriticalSection(&g_lock);
                printf("[MISSAO] '%s' alcancou o Checkpoint %u/%u!\n", 
                       g_clients[slot].gamertag, g_mission.checkpoint_index, g_mission.total_checkpoints);
                
                if (g_mission.checkpoint_index < (uint32_t)g_mission.total_checkpoints) {
                    g_mission.checkpoint = g_checkpoints[g_mission.checkpoint_index];
                    g_mission.checkpoint_index++;
                } else {
                    printf("[VITORIA] '%s' COMPLETOU A MISSAO EM PRIMEIRO LUGAR!\n", g_clients[slot].gamertag);
                    g_mission.checkpoint_index = 1;
                    g_mission.checkpoint = g_checkpoints[0];
                }

                // Avisa todos da nova posicao do checkpoint
                NetPacket sync = {0};
                sync.magic = MC3_NET_MAGIC;
                sync.msg_type = MSG_MISSION_SYNC;
                sync.data_len = sizeof(MissionData);
                memcpy(sync.data, &g_mission, sizeof(MissionData));
                LeaveCriticalSection(&g_lock);

                broadcast_packet(&sync, 0);
                break;
            }

            default:
                break;
        }
    }

disconnect:
    EnterCriticalSection(&g_lock);
    printf("[-] '%s' (ID %u) desconectou.\n", g_clients[slot].gamertag, my_id);
    g_clients[slot].active = 0;
    closesocket(s);
    LeaveCriticalSection(&g_lock);

    // Notifica outros jogadores que este jogador saiu
    NetPacket leave_pkt = {0};
    leave_pkt.magic = MC3_NET_MAGIC;
    leave_pkt.msg_type = MSG_LEAVE;
    leave_pkt.player_id = my_id;
    broadcast_packet(&leave_pkt, my_id);

    return 0;
}

int main(int argc, char **argv) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    InitializeCriticalSection(&g_lock);
    init_mission();

    int port = MC3_NET_PORT;
    if (argc > 1) port = atoi(argv[1]);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) {
        printf("[-] Erro ao criar socket: %d\n", WSAGetLastError());
        return 1;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[-] Falha ao associar porta %d: %d\n", port, WSAGetLastError());
        return 1;
    }

    if (listen(srv, SOMAXCONN) < 0) {
        printf("[-] Falha no listen: %d\n", WSAGetLastError());
        return 1;
    }

    printf("===================================================\n");
    printf("   BETTER-MIDNIGHT DEDICATED SERVER v1.0\n");
    printf("   Porta: %d | Max Jogadores: %d\n", port, MAX_PLAYERS);
    printf("   Missao Inicial: '%s'\n", g_mission.title);
    printf("===================================================\n");
    printf("Aguardando conexoes dos jogadores...\n\n");
    fflush(stdout);

    while (g_running) {
        sockaddr_in client_addr;
        int clen = sizeof(client_addr);
        SOCKET c = accept(srv, (sockaddr*)&client_addr, &clen);
        if (c == INVALID_SOCKET) break;

        EnterCriticalSection(&g_lock);
        int free_slot = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!g_clients[i].active) {
                free_slot = i;
                break;
            }
        }

        if (free_slot >= 0) {
            g_clients[free_slot].socket = c;
            g_clients[free_slot].player_id = g_next_id++;
            g_clients[free_slot].active = 1;
            strcpy_s(g_clients[free_slot].gamertag, "Piloto");
            _beginthreadex(NULL, 0, client_thread, (void*)(intptr_t)free_slot, 0, NULL);
        } else {
            printf("[!] Servidor cheio! Recusando conexao.\n");
            closesocket(c);
        }
        LeaveCriticalSection(&g_lock);
    }

    DeleteCriticalSection(&g_lock);
    closesocket(srv);
    WSACleanup();
    return 0;
}
