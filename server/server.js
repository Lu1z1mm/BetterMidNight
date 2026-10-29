const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const career = require('./career');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 17500;
const MAX_PLAYERS = 32;

// Servir arquivos estaticos do Dashboard Web
app.use(express.static(path.join(__dirname, 'public')));

// Estado dos Pilotos Conectados
let players = new Map(); // id -> { id, ws, gamertag, pos, rot, speed, ping, lastSeen }
let nextPlayerId = 1;

// Rota de Status / API REST
app.get('/api/status', (req, res) => {
    const playerList = Array.from(players.values()).map(p => ({
        id: p.id,
        gamertag: p.gamertag,
        speed: p.speed || 0,
        pos: p.pos || { x: 0, y: 0, z: 0 },
        ping: p.ping || 0
    }));

    res.json({
        status: "online",
        onlinePlayers: playerList.length,
        maxPlayers: MAX_PLAYERS,
        career: career.getStateSummary(),
        players: playerList,
        uptime: process.uptime()
    });
});

// Broadcast para todos os clientes conectados
function broadcast(data, excludeWs = null) {
    const payload = JSON.stringify(data);
    for (const client of players.values()) {
        if (client.ws.readyState === WebSocket.OPEN && client.ws !== excludeWs) {
            client.ws.send(payload);
        }
    }
}

// Conexoes WebSocket
wss.on('connection', (ws, req) => {
    if (players.size >= MAX_PLAYERS) {
        ws.close(1008, "Servidor cheio!");
        return;
    }

    const playerId = nextPlayerId++;
    const player = {
        id: playerId,
        ws: ws,
        gamertag: `Piloto_${playerId}`,
        pos: { x: 0, y: 0, z: 0 },
        rot: { right: [1,0,0], up: [0,1,0], forward: [0,0,1] },
        speed: 0,
        ping: 10,
        lastSeen: Date.now()
    };
    players.set(playerId, player);

    console.log(`[+] Jogador conectado: ID ${playerId} (${req.socket.remoteAddress})`);

    // Envia confirmacao de entrada e estado da carreira
    ws.send(JSON.stringify({
        type: "WELCOME",
        playerId: playerId,
        career: career.getStateSummary(),
        totalPlayers: players.size
    }));

    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            player.lastSeen = Date.now();

            switch (data.type) {
                case "JOIN":
                    if (data.gamertag) {
                        player.gamertag = String(data.gamertag).substring(0, 24);
                    }
                    console.log(`[LOBBY] '${player.gamertag}' (ID ${player.id}) pronto na pista!`);
                    broadcast({
                        type: "PLAYER_JOINED",
                        id: player.id,
                        gamertag: player.gamertag
                    });
                    break;

                case "POS_UPDATE":
                    player.pos = data.pos || player.pos;
                    player.rot = data.rot || player.rot;
                    player.speed = data.speed || 0;
                    player.ping = data.ping || 10;

                    // Retransmite a posicao para os outros jogadores
                    broadcast({
                        type: "POS_UPDATE",
                        id: player.id,
                        gamertag: player.gamertag,
                        pos: player.pos,
                        rot: player.rot,
                        speed: player.speed
                    }, ws);
                    break;

                case "CHECKPOINT_HIT":
                    const event = career.onCheckpointHit(player.gamertag);
                    if (event.type === "NEXT_CHECKPOINT") {
                        console.log(`[CHECKPOINT] '${player.gamertag}' passou no CP ${event.mission.checkpointIndex}/${event.mission.totalCheckpoints}`);
                        broadcast({
                            type: "MISSION_UPDATE",
                            event: "NEXT_CHECKPOINT",
                            career: career.getStateSummary(),
                            player: player.gamertag
                        });
                    } else if (event.type === "NEXT_LAP") {
                        console.log(`[VOLTA] '${player.gamertag}' completou uma volta! (Volta ${event.mission.currentLap}/${event.mission.totalLaps})`);
                        broadcast({
                            type: "MISSION_UPDATE",
                            event: "NEXT_LAP",
                            career: career.getStateSummary(),
                            player: player.gamertag
                        });
                    } else if (event.type === "TEAM_VICTORY") {
                        console.log(`=================================================`);
                        console.log(`🏆 [VITORIA DA EQUIPE] '${player.gamertag}' VENCEU A CORRIDA!`);
                        console.log(`   Missao Concluida: ${event.completedMission.title}`);
                        console.log(`   Recompensa: +$${event.completedMission.rewardCash} para toda a equipe!`);
                        console.log(`   Desbloqueio: ${event.completedMission.unlocks}`);
                        console.log(`   Saldo da Equipe: $${event.teamCash}`);
                        console.log(`=================================================`);

                        broadcast({
                            type: "TEAM_VICTORY",
                            winner: player.gamertag,
                            rewardCash: event.completedMission.rewardCash,
                            unlocks: event.completedMission.unlocks,
                            career: career.getStateSummary()
                        });
                    }
                    break;
            }
        } catch (err) {
            // Ignora pacote invalido
        }
    });

    ws.on('close', () => {
        console.log(`[-] Jogador saiu: '${player.gamertag}' (ID ${playerId})`);
        players.delete(playerId);
        broadcast({
            type: "PLAYER_LEFT",
            id: playerId,
            gamertag: player.gamertag
        });
    });

    ws.on('error', (err) => {
        console.error(`[!] Erro no socket ID ${playerId}:`, err.message);
    });
});

// Limpeza periodica de conexoes mortas
setInterval(() => {
    const now = Date.now();
    for (const [id, p] of players.entries()) {
        if (now - p.lastSeen > 15000) {
            p.ws.terminate();
            players.delete(id);
        }
    }
}, 5000);

// Iniciar servidor
server.listen(PORT, () => {
    const summary = career.getStateSummary();
    console.log("=================================================");
    console.log(`  BETTER-MIDNIGHT CLOUD SERVER ONLINE!`);
    console.log(`  Porta: ${PORT} | Ambiente: Render Cloud`);
    console.log(`  Cidade Ativa: ${summary.city} (Missao ${summary.missionIndex}/${summary.totalMissionsInCity})`);
    console.log(`  Desafio Atual: ${summary.currentMission.title}`);
    console.log(`  Rival: ${summary.currentMission.rival}`);
    console.log(`  Banco da Equipe: $${summary.teamCash}`);
    console.log(`  Dashboard Web: http://localhost:${PORT}`);
    console.log("=================================================");
});
