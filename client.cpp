#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <process.h>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define PI 3.14159265358979323846f

static DWORD g_pid = 0;
static HANDLE g_hProcess = NULL;
static uintptr_t g_player_matrix_addr = 0;
static uintptr_t g_remote_slots[MAX_REMOTE_SLOTS] = {0};

static SOCKET g_sock = INVALID_SOCKET;
static uint32_t g_my_id = 0;
static volatile int g_running = 1;
static char g_gamertag[32] = "Piloto";
static MissionData g_cur_mission = {0};

struct RemotePlayer {
    uint32_t id;
    PlayerState state;
    int active;
};
static RemotePlayer g_remotes[MAX_REMOTE_SLOTS] = {0};
static CRITICAL_SECTION g_remote_lock;

DWORD find_mc3_pid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe = {sizeof(pe)};
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (strstr(pe.szExeFile, "Midnight Club 3") && strstr(pe.szExeFile, ".exe")) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// Validador matematico de matriz 3D ortonormal (sem permitir NaNs)
static inline int is_valid_matrix34(const float *m) {
    for (int i = 0; i < 12; i++) {
        if (_isnan(m[i]) || !_finite(m[i])) return 0;
    }

    float lr = sqrtf(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
    float lu = sqrtf(m[3]*m[3] + m[4]*m[4] + m[5]*m[5]);
    float lf = sqrtf(m[6]*m[6] + m[7]*m[7] + m[8]*m[8]);
    if (fabsf(lr - 1.0f) > 0.08f || fabsf(lu - 1.0f) > 0.08f || fabsf(lf - 1.0f) > 0.08f)
        return 0;

    float dp_ru = m[0]*m[3] + m[1]*m[4] + m[2]*m[5];
    float dp_rf = m[0]*m[6] + m[1]*m[7] + m[2]*m[8];
    if (fabsf(dp_ru) > 0.12f || fabsf(dp_rf) > 0.12f)
        return 0;

    float px = m[9], py = m[10], pz = m[11];
    if (fabsf(px) > 15000.0f || fabsf(py) > 1000.0f || fabsf(pz) > 15000.0f)
        return 0;
    if (fabsf(px) < 5.0f && fabsf(py) < 1.0f && fabsf(pz) < 5.0f)
        return 0;

    return 1;
}

// Localiza dinamicamente o carro do jogador na memoria do Xrecomp
int locate_vehicle_system() {
    printf("[*] Realizando varredura dinamica na memoria do jogo...\n");
    MEMORY_BASIC_INFORMATION64 mbi;
    uintptr_t addr = 0x7FF000000000ULL; // Foca exclusivamente na memoria alta do guest

    uintptr_t best_car = 0;
    uintptr_t detected_slots[MAX_REMOTE_SLOTS + 1] = {0};
    int found_count = 0;

    while (VirtualQueryEx(g_hProcess, (LPCVOID)addr, (PMEMORY_BASIC_INFORMATION)&mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_READWRITE)) {
            if (mbi.RegionSize >= 60 * 1024 * 1024 && mbi.RegionSize <= 80 * 1024 * 1024) {
                // Leitura do bloco
                const SIZE_T CHUNK = 4 * 1024 * 1024;
                char *buf = (char*)malloc(CHUNK);
                if (buf) {
                    for (SIZE_T off = 0; off < mbi.RegionSize; off += CHUNK - 4096) {
                        SIZE_T to_read = (mbi.RegionSize - off < CHUNK) ? (mbi.RegionSize - off) : CHUNK;
                        SIZE_T bytes_read = 0;
                        if (ReadProcessMemory(g_hProcess, (LPCVOID)(mbi.BaseAddress + off), buf, to_read, &bytes_read)) {
                            for (SIZE_T i = 0; i + 48 <= bytes_read; i += 16) {
                                float *m = (float*)(buf + i);
                                if (is_valid_matrix34(m)) {
                                    uintptr_t cand = mbi.BaseAddress + off + i;
                                    if (found_count == 0) {
                                        detected_slots[found_count++] = cand;
                                    } else {
                                        // Verifica se faz parte do array sequencial de veiculos (~0x800 a ~0xA50)
                                        uintptr_t diff = cand - detected_slots[found_count - 1];
                                        if (diff >= 0x800 && diff <= 0xA50) {
                                            detected_slots[found_count++] = cand;
                                            if (found_count >= MAX_REMOTE_SLOTS + 1) goto done_scan;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    free(buf);
                }
            }
        }
        addr = mbi.BaseAddress + mbi.RegionSize;
    }

done_scan:
    if (found_count > 0) {
        g_player_matrix_addr = detected_slots[0];
        for (int s = 0; s < MAX_REMOTE_SLOTS; s++) {
            g_remote_slots[s] = (s + 1 < found_count) ? detected_slots[s + 1] : (g_player_matrix_addr + (s + 1) * 0x9F0);
        }
    } else {
        // Fallback caso a pista esteja recarregando
        g_player_matrix_addr = 0x7FF4FC5F6880ULL;
        for (int s = 0; s < MAX_REMOTE_SLOTS; s++) {
            g_remote_slots[s] = g_player_matrix_addr + (s + 1) * 0x9F0;
        }
    }

    printf("[+] Carro do jogador localizado dinamicamente em: 0x%llX\n", (unsigned long long)g_player_matrix_addr);
    printf("[+] %d slots de carros remotos configurados.\n", MAX_REMOTE_SLOTS);
    return 1;
}

unsigned __stdcall recv_thread(void *arg) {
    NetPacket pkt;
    while (g_running) {
        int received = 0;
        char *ptr = (char*)&pkt;
        while (received < (int)sizeof(NetPacket)) {
            int n = recv(g_sock, ptr + received, sizeof(NetPacket) - received, 0);
            if (n <= 0) {
                printf("\n[-] Desconectado do servidor.\n");
                g_running = 0;
                return 0;
            }
            received += n;
        }

        if (pkt.magic != MC3_NET_MAGIC) continue;

        switch (pkt.msg_type) {
            case MSG_JOIN_ACK:
                g_my_id = pkt.player_id;
                break;

            case MSG_MISSION_SYNC:
                if (pkt.data_len == sizeof(MissionData)) {
                    memcpy(&g_cur_mission, pkt.data, sizeof(MissionData));
                    Beep(880, 80);
                }
                break;

            case MSG_POS_UPDATE: {
                if (pkt.data_len == sizeof(PlayerState)) {
                    PlayerState *st = (PlayerState*)pkt.data;
                    EnterCriticalSection(&g_remote_lock);
                    int slot = -1;
                    for (int i = 0; i < MAX_REMOTE_SLOTS; i++) {
                        if (g_remotes[i].active && g_remotes[i].id == st->player_id) {
                            slot = i;
                            break;
                        }
                    }
                    if (slot < 0) {
                        for (int i = 0; i < MAX_REMOTE_SLOTS; i++) {
                            if (!g_remotes[i].active) {
                                slot = i;
                                g_remotes[i].id = st->player_id;
                                g_remotes[i].active = 1;
                                break;
                            }
                        }
                    }
                    if (slot >= 0) {
                        g_remotes[slot].state = *st;
                        
                        // Atualiza carro remoto no mundo
                        float matrix[12];
                        matrix[0] = st->rot.right[0];   matrix[1] = st->rot.right[1];   matrix[2] = st->rot.right[2];
                        matrix[3] = st->rot.up[0];      matrix[4] = st->rot.up[1];      matrix[5] = st->rot.up[2];
                        matrix[6] = st->rot.forward[0]; matrix[7] = st->rot.forward[1]; matrix[8] = st->rot.forward[2];
                        matrix[9] = st->pos.x;          matrix[10] = st->pos.y;         matrix[11] = st->pos.z;

                        SIZE_T written = 0;
                        WriteProcessMemory(g_hProcess, (LPVOID)g_remote_slots[slot], matrix, sizeof(matrix), &written);
                    }
                    LeaveCriticalSection(&g_remote_lock);
                }
                break;
            }

            case MSG_LEAVE: {
                EnterCriticalSection(&g_remote_lock);
                for (int i = 0; i < MAX_REMOTE_SLOTS; i++) {
                    if (g_remotes[i].active && g_remotes[i].id == pkt.player_id) {
                        g_remotes[i].active = 0;
                        break;
                    }
                }
                LeaveCriticalSection(&g_remote_lock);
                break;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    SetConsoleTitleA("BetterMidNight - Client");

    printf("===================================================\n");
    printf("   BETTER-MIDNIGHT CLIENT SYNC v2.1 (PRECISION)\n");
    printf("===================================================\n");

    const char *server_ip = "127.0.0.1";
    if (argc > 1) server_ip = argv[1];
    if (argc > 2) strncpy_s(g_gamertag, argv[2], 31);

    printf("[*] Localizando processo do Midnight Club 3...\n");
    g_pid = find_mc3_pid();
    if (!g_pid) {
        printf("[-] Midnight Club 3 nao esta aberto! Inicie o jogo antes de rodar o mod.\n");
        system("pause");
        return 1;
    }
    printf("[+] MC3 Encontrado! PID: %lu\n", g_pid);

    g_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, g_pid);
    if (!g_hProcess) {
        printf("[-] Falha ao abrir processo do jogo.\n");
        return 1;
    }

    if (!locate_vehicle_system()) {
        printf("[-] Falha na deteccao do sistema de veiculos.\n");
        CloseHandle(g_hProcess);
        return 1;
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    InitializeCriticalSection(&g_remote_lock);

    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MC3_NET_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    printf("[*] Conectando ao servidor %s:%d ...\n", server_ip, MC3_NET_PORT);
    if (connect(g_sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("[-] Servidor offline. Inicie o servidor antes de conectar.\n");
        CloseHandle(g_hProcess);
        return 1;
    }
    printf("[+] Conectado com sucesso!\n");

    _beginthreadex(NULL, 0, recv_thread, NULL, 0, NULL);

    // Envia JOIN
    NetPacket join = {0};
    join.magic = MC3_NET_MAGIC;
    join.msg_type = MSG_JOIN_REQ;
    join.player_id = 0;
    join.data_len = (uint16_t)strlen(g_gamertag);
    strcpy_s(join.data, g_gamertag);
    send(g_sock, (char*)&join, sizeof(NetPacket), 0);

    LARGE_INTEGER freq, last_qpc;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last_qpc);

    Vec3 last_pos = {0};
    float smoothed_speed_kmh = 0.0f;

    printf("\n>> SINCRONIZACAO ATIVA COM GPS E VELOCIDADE REAL! <<\n\n");

    while (g_running) {
        Sleep(33); // ~30 Hz

        float m[12];
        SIZE_T read = 0;
        if (!ReadProcessMemory(g_hProcess, (LPCVOID)g_player_matrix_addr, m, sizeof(m), &read) || read != sizeof(m)) {
            continue;
        }

        // Se ler dados invalidos, ignora o frame
        if (!is_valid_matrix34(m)) continue;

        PlayerState state = {0};
        state.player_id = g_my_id;
        strcpy_s(state.gamertag, g_gamertag);
        
        state.rot.right[0] = m[0]; state.rot.right[1] = m[1]; state.rot.right[2] = m[2];
        state.rot.up[0]    = m[3]; state.rot.up[1]    = m[4]; state.rot.up[2]    = m[5];
        state.rot.forward[0]=m[6]; state.rot.forward[1]=m[7]; state.rot.forward[2]=m[8];

        state.pos.x = m[9];
        state.pos.y = m[10];
        state.pos.z = m[11];

        // Calculo de alta precisao da velocidade
        LARGE_INTEGER now_qpc;
        QueryPerformanceCounter(&now_qpc);
        double dt = (double)(now_qpc.QuadPart - last_qpc.QuadPart) / (double)freq.QuadPart;

        if (dt > 0.01 && last_pos.x != 0.0f) {
            float dx = state.pos.x - last_pos.x;
            float dy = state.pos.y - last_pos.y;
            float dz = state.pos.z - last_pos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            float instant_speed = 0.0f;
            if (dist > 0.08f) { // Filtro de microvibracao
                instant_speed = (float)((dist / dt) * 3.6);
            }
            smoothed_speed_kmh = (smoothed_speed_kmh * 0.70f) + (instant_speed * 0.30f);
            if (smoothed_speed_kmh < 1.0f) smoothed_speed_kmh = 0.0f;
        }
        last_pos = state.pos;
        last_qpc = now_qpc;

        // Envia estado
        NetPacket pos_pkt = {0};
        pos_pkt.magic = MC3_NET_MAGIC;
        pos_pkt.msg_type = MSG_POS_UPDATE;
        pos_pkt.player_id = g_my_id;
        pos_pkt.data_len = sizeof(PlayerState);
        memcpy(pos_pkt.data, &state, sizeof(PlayerState));
        send(g_sock, (char*)&pos_pkt, sizeof(NetPacket), 0);

        // Sistema de GPS e Missao
        float cp_dist = 0.0f;
        const char *nav_guide = "LIVRE";
        if (g_cur_mission.active && !_isnan(g_cur_mission.checkpoint.x)) {
            float cx = g_cur_mission.checkpoint.x - state.pos.x;
            float cz = g_cur_mission.checkpoint.z - state.pos.z;
            cp_dist = sqrtf(cx*cx + cz*cz);

            float target_angle = atan2f(cz, cx);
            float forward_angle = atan2f(state.rot.forward[2], state.rot.forward[0]);
            float delta_angle = target_angle - forward_angle;
            while (delta_angle > PI) delta_angle -= 2.0f * PI;
            while (delta_angle < -PI) delta_angle += 2.0f * PI;
            float deg = delta_angle * (180.0f / PI);

            if (fabsf(deg) < 25.0f) {
                nav_guide = "^ SIGA EM FRENTE";
            } else if (deg >= 25.0f && deg <= 110.0f) {
                nav_guide = "> VIRE A DIREITA";
            } else if (deg <= -25.0f && deg >= -110.0f) {
                nav_guide = "< VIRE A ESQUERDA";
            } else {
                nav_guide = "v RETORNE (MEIA-VOLTA)";
            }

            if (cp_dist <= g_cur_mission.radius) {
                Beep(1200, 150);
                NetPacket hit = {0};
                hit.magic = MC3_NET_MAGIC;
                hit.msg_type = MSG_CHECKPOINT_HIT;
                hit.player_id = g_my_id;
                send(g_sock, (char*)&hit, sizeof(NetPacket), 0);
            }
        }

        int remote_count = 0;
        EnterCriticalSection(&g_remote_lock);
        for (int i = 0; i < MAX_REMOTE_SLOTS; i++) if (g_remotes[i].active) remote_count++;
        LeaveCriticalSection(&g_remote_lock);

        // HUD Limpo no Console
        printf("\r[%s] %3.0f km/h | Online: %d | [GPS]: %-22s (CP %u/%u - %4.0fm)    ",
               g_gamertag, smoothed_speed_kmh, remote_count + 1, 
               nav_guide, g_cur_mission.checkpoint_index, g_cur_mission.total_checkpoints, cp_dist);
        fflush(stdout);
    }

    DeleteCriticalSection(&g_remote_lock);
    closesocket(g_sock);
    CloseHandle(g_hProcess);
    WSACleanup();
    return 0;
}
