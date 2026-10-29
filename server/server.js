const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 17500;
const MAX_PLAYERS = 32;

// Servir arquivos estaticos do Dashboard Web
app.use(express.static(path.join(__dirname, 'public')));

// Estado Global do Servidor
let players = new Map(); // id -> { id, ws, gamertag, pos, rot, speed, ping, lastSeen }
let nextPlayerId = 1;

// Checkpoints oficiais mapeados nas ruas de Midnight Club 3
const checkpoints = [
    { id: 1, name: "Largada Downtown", x: 1390.0, y: -2.0, z: -125.0, radius: 20.0 },
    { id: 2, name: "Avenida Principal", x: 1430.0, y: -2.0, z: -180.0, radius: 20.0 },
    { id: 3, name: "Curva do Porto", x: 1195.0, y: -2.0, z: -350.0, radius: 25.0 },
    { id: 4, name: "Subida do Viaduto", x: 720.0, y: 3.5, z: -260.0, radius: 20.0 },
    { id: 5, name: "Reta Final San Diego", x: 1475.0, y: -1.5, z: 140.0, radius: 25.0 }
];

let activeMission = {
    id: 1,
    title: "Midnight Sprint - San Diego Open",
    totalCheckpoints: checkpoints.length,
    currentCheckpointIndex: 0,
    currentCheckpoint: checkpoints[0],
    active: true,
    leader: "Nenhum",
    lapsCompleted: 0
};

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
        mission: activeMission,
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

    // Envia confirmacao de entrada e estado da missao
    ws.send(JSON.stringify({
        type: "WELCOME",
        playerId: playerId,
        mission: activeMission,
        totalPlayers: players.size
    }));

    // Trata mensagens recebidas
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
                    console.log(`[MISSAO] '${player.gamertag}' alcancou o Checkpoint ${activeMission.currentCheckpointIndex + 1}!`);
                    activeMission.leader = player.gamertag;

                    if (activeMission.currentCheckpointIndex + 1 < activeMission.totalCheckpoints) {
                        activeMission.currentCheckpointIndex++;
                        activeMission.currentCheckpoint = checkpoints[activeMission.currentCheckpointIndex];
                    } else {
                        console.log(`[VITORIA] '${player.gamertag}' VENCEU A MISSAO!`);
                        activeMission.lapsCompleted++;
                        activeMission.currentCheckpointIndex = 0;
                        activeMission.currentCheckpoint = checkpoints[0];
                    }

                    // Sincroniza novo checkpoint com todos
                    broadcast({
                        type: "MISSION_SYNC",
                        mission: activeMission,
                        leader: player.gamertag
                    });
                    break;
            }
        } catch (err) {
            // Ignora pacote corrompido
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
        if (now - p.lastSeen > 15000) { // 15s sem pacote
            p.ws.terminate();
            players.delete(id);
        }
    }
}, 5000);

// Iniciar servidor HTTP + WebSockets
server.listen(PORT, () => {
    console.log("=================================================");
    console.log(`  BETTER-MIDNIGHT CLOUD SERVER ONLINE!`);
    console.log(`  Porta: ${PORT} | Ambiente: Render Cloud`);
    console.log(`  Missao Ativa: ${activeMission.title}`);
    console.log(`  Dashboard Web: http://localhost:${PORT}`);
    console.log("=================================================");
});
