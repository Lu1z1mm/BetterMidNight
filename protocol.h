#pragma once
#include <stdint.h>

#define MC3_NET_PORT 17500
#define MC3_NET_MAGIC 0x4D43334E // "MC3N"
#define MAX_PLAYERS 32
#define MAX_REMOTE_SLOTS 8

enum NetMsgType {
    MSG_JOIN_REQ       = 1,
    MSG_JOIN_ACK       = 2,
    MSG_POS_UPDATE     = 3,
    MSG_LEAVE          = 4,
    MSG_MISSION_SYNC   = 10,
    MSG_CHECKPOINT_HIT = 11,
    MSG_CHAT           = 20
};

#pragma pack(push, 1)
struct Vec3 {
    float x, y, z;
};

struct Mat33 {
    float right[3];
    float up[3];
    float forward[3];
};

struct PlayerState {
    uint32_t player_id;
    char gamertag[32];
    Vec3 pos;
    Mat33 rot;
    Vec3 vel;
    uint32_t car_id;
    uint32_t ping_ms;
};

struct MissionData {
    uint32_t mission_id;
    char title[64];
    Vec3 checkpoint;
    float radius;
    uint32_t checkpoint_index;
    uint32_t total_checkpoints;
    uint32_t active;
};

struct NetPacket {
    uint32_t magic;      // MC3_NET_MAGIC
    uint16_t msg_type;   // NetMsgType
    uint16_t data_len;   // Tamanho dos dados em bytes
    uint32_t player_id;  // ID do remetente
    char data[512];      // Payload
};
#pragma pack(pop)

