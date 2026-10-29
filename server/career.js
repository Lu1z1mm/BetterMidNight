const fs = require('fs');
const path = require('path');

const SAVE_FILE = path.join(__dirname, 'career_save.json');

// Mapeamento das Missoes da Campanha de San Diego (Midnight Club 3)
const SAN_DIEGO_CAMPAIGN = [
    {
        id: "sd_01",
        title: "Primeiro Desafio: Vanessa",
        rival: "Vanessa (Mitsubishi Eclipse GSX)",
        desc: "Apresente-se às ruas de San Diego e mostre para a Vanessa que a sua equipe não está de brincadeira.",
        type: "Circuit",
        laps: 2,
        rewardCash: 2500,
        unlocks: "Peças de Performance Nível 1",
        checkpoints: [
            { id: 1, name: "Largada Downtown", x: 1390.0, y: -2.0, z: -125.0, radius: 20.0 },
            { id: 2, name: "Avenida Principal", x: 1430.0, y: -2.0, z: -180.0, radius: 20.0 },
            { id: 3, name: "Curva do Porto", x: 1195.0, y: -2.0, z: -350.0, radius: 25.0 },
            { id: 4, name: "Subida do Viaduto", x: 720.0, y: 3.5, z: -260.0, radius: 20.0 },
            { id: 5, name: "Reta de Chegada", x: 1475.0, y: -1.5, z: 140.0, radius: 25.0 }
        ]
    },
    {
        id: "sd_02",
        title: "Disputa de Músculo: Bishop",
        rival: "Bishop (Chevrolet Bel Air '57)",
        desc: "Os clássicos dominaram a orla. Vença o Bishop para abrir caminho para os torneios da cidade.",
        type: "Sprint",
        laps: 1,
        rewardCash: 4500,
        unlocks: "Nitro Nível 1 & Rodas DUB",
        checkpoints: [
            { id: 1, name: "Largada da Marina", x: 1450.0, y: -2.0, z: 50.0, radius: 20.0 },
            { id: 2, name: "Avenida Central", x: 1380.0, y: -2.0, z: 200.0, radius: 20.0 },
            { id: 3, name: "Cruzamento do Farol", x: 1100.0, y: 1.0, z: -100.0, radius: 25.0 },
            { id: 4, name: "Descida da Autoestrada", x: 850.0, y: 2.0, z: -220.0, radius: 25.0 },
            { id: 5, name: "Chegada no Pier", x: 1390.0, y: -2.0, z: -125.0, radius: 25.0 }
        ]
    },
    {
        id: "sd_03",
        title: "Torneio de San Diego: Etapa 1",
        rival: "Roy & Pilotos de Clube",
        desc: "O primeiro grande torneio oficial da cidade. O primeiro membro da equipe a cruzar a linha garante a vaga.",
        type: "Circuit",
        laps: 3,
        rewardCash: 8000,
        unlocks: "Kits de Carroceria Nível 2",
        checkpoints: [
            { id: 1, name: "Grid de Largada", x: 1390.0, y: -2.0, z: -125.0, radius: 20.0 },
            { id: 2, name: "Setor 1 - Balboa Park", x: 1475.0, y: -1.5, z: 140.0, radius: 20.0 },
            { id: 3, name: "Setor 2 - Túnel Expresso", x: 1250.0, y: -2.0, z: -150.0, radius: 25.0 },
            { id: 4, name: "Setor 3 - Reta dos Galpões", x: 720.0, y: 3.5, z: -260.0, radius: 25.0 },
            { id: 5, name: "Linha de Chegada", x: 1390.0, y: -2.0, z: -125.0, radius: 25.0 }
        ]
    },
    {
        id: "sd_04",
        title: "Grande Final de San Diego: Rumo a Atlanta!",
        rival: "Vanessa (Retorno com Skyline) & Roy",
        desc: "A corrida definitiva de San Diego. Vença para conquistar o troféu da cidade e abrir o passaporte para Atlanta!",
        type: "Sprint",
        laps: 1,
        rewardCash: 15000,
        unlocks: "Passaporte para Atlanta & Carro Novo Desbloqueado",
        checkpoints: [
            { id: 1, name: "Largada Imperial", x: 1475.0, y: -1.5, z: 140.0, radius: 25.0 },
            { id: 2, name: "Curva do Centro", x: 1390.0, y: -2.0, z: -125.0, radius: 20.0 },
            { id: 3, name: "Passagem Subterrânea", x: 1195.0, y: -2.0, z: -350.0, radius: 25.0 },
            { id: 4, name: "Viaduto Panorâmico", x: 720.0, y: 3.5, z: -260.0, radius: 25.0 },
            { id: 5, name: "Pórtico da Vitória", x: 1430.0, y: -2.0, z: -180.0, radius: 25.0 }
        ]
    }
];

class CareerManager {
    constructor() {
        this.state = this.loadSave();
    }

    loadSave() {
        try {
            if (fs.existsSync(SAVE_FILE)) {
                return JSON.parse(fs.readFileSync(SAVE_FILE, 'utf8'));
            }
        } catch (e) {
            console.error("[-] Erro ao carregar save da carreira:", e.message);
        }

        // Save padrao inicial
        return {
            city: "San Diego",
            missionIndex: 0,
            teamCash: 5000,
            completedMissions: [],
            totalWins: 0,
            activeRace: {
                inProgress: false,
                currentCheckpointIndex: 0,
                currentLap: 1,
                leader: "Aguardando largada"
            }
        };
    }

    persist() {
        try {
            fs.writeFileSync(SAVE_FILE, JSON.stringify(this.state, null, 2), 'utf8');
        } catch (e) {
            console.error("[-] Falha ao salvar progresso:", e.message);
        }
    }

    getCurrentMission() {
        const m = SAN_DIEGO_CAMPAIGN[this.state.missionIndex] || SAN_DIEGO_CAMPAIGN[0];
        const curCp = m.checkpoints[this.state.activeRace.currentCheckpointIndex] || m.checkpoints[0];
        return {
            ...m,
            activeCheckpoint: curCp,
            checkpointIndex: this.state.activeRace.currentCheckpointIndex + 1,
            totalCheckpoints: m.checkpoints.length,
            currentLap: this.state.activeRace.currentLap,
            totalLaps: m.laps,
            leader: this.state.activeRace.leader
        };
    }

    startRace() {
        this.state.activeRace.inProgress = true;
        this.state.activeRace.currentCheckpointIndex = 0;
        this.state.activeRace.currentLap = 1;
        this.state.activeRace.leader = "Largada autorizada!";
        this.persist();
        return this.getCurrentMission();
    }

    // Processa quando qualquer jogador da equipe bate no checkpoint
    onCheckpointHit(playerName) {
        const mission = SAN_DIEGO_CAMPAIGN[this.state.missionIndex];
        this.state.activeRace.leader = playerName;

        if (this.state.activeRace.currentCheckpointIndex + 1 < mission.checkpoints.length) {
            this.state.activeRace.currentCheckpointIndex++;
            this.persist();
            return {
                type: "NEXT_CHECKPOINT",
                mission: this.getCurrentMission(),
                player: playerName
            };
        } else {
            // Fim da volta ou fim da corrida
            if (this.state.activeRace.currentLap < mission.laps) {
                this.state.activeRace.currentLap++;
                this.state.activeRace.currentCheckpointIndex = 0;
                this.persist();
                return {
                    type: "NEXT_LAP",
                    mission: this.getCurrentMission(),
                    player: playerName
                };
            } else {
                // VITORIA DA EQUIPE!
                this.state.teamCash += mission.rewardCash;
                this.state.totalWins++;
                this.state.completedMissions.push(mission.id);

                const finishedMission = mission;

                // Avanca para a proxima missao
                if (this.state.missionIndex + 1 < SAN_DIEGO_CAMPAIGN.length) {
                    this.state.missionIndex++;
                } else {
                    this.state.missionIndex = 0; // Loop ou vitoria total de San Diego
                }

                this.state.activeRace.currentCheckpointIndex = 0;
                this.state.activeRace.currentLap = 1;
                this.persist();

                return {
                    type: "TEAM_VICTORY",
                    completedMission: finishedMission,
                    nextMission: this.getCurrentMission(),
                    winner: playerName,
                    teamCash: this.state.teamCash
                };
            }
        }
    }

    getStateSummary() {
        return {
            city: this.state.city,
            teamCash: this.state.teamCash,
            totalWins: this.state.totalWins,
            missionIndex: this.state.missionIndex + 1,
            totalMissionsInCity: SAN_DIEGO_CAMPAIGN.length,
            currentMission: this.getCurrentMission()
        };
    }
}

module.exports = new CareerManager();
