# BetterMidNight — Servidor Multiplayer em Nuvem (Render)

Servidor dedicado 24/7 com suporte a salas abertas, sincronização de veículos e missões de corrida para **Midnight Club 3: DUB Edition Remix (PC / Xrecomp)**.

---

## 🚀 Como Colocar no Ar (Para seu amigo no GitHub e Render)

### Passo 1: Subir para o GitHub
No terminal da pasta do projeto, execute:
```bash
git init
git add .
git commit -m "BetterMidNight: Cloud Server e Dashboard v1.0"
git branch -M main
git remote add origin https://github.com/SEU_USUARIO/SEU_REPOSITORIO.git
git push -u origin main
```

---

### Passo 2: Conectar no Render (100% Gratuito)
1. Acesse **[dashboard.render.com](https://dashboard.render.com)** e faça login com o GitHub.
2. Clique no botão **"New +"** no canto superior direito e escolha **"Web Service"**.
3. Selecione o repositório do GitHub criado no Passo 1.
4. O Render vai ler o arquivo `render.yaml` automaticamente! Caso peça confirmação manual:
   * **Root Directory:** `server`
   * **Build Command:** `npm install`
   * **Start Command:** `npm start`
   * **Instance Type:** `Free`
5. Clique em **"Deploy Web Service"**.

Em cerca de 1 minuto, seu servidor estará **online 24 horas por dia** com um link público:
`https://seu-projeto.onrender.com`

---

## 🌐 Dashboard Web em Tempo Real
Ao abrir `https://seu-projeto.onrender.com` em qualquer navegador (PC ou celular):
* Veja a lista de todos os pilotos online.
* Velocímetro ao vivo de cada carro.
* Acompanhamento dos checkpoints e do líder da missão.

---

## 🎮 Como os Pilotos Conectam ao Jogo
Basta abrir o cliente passando o link do Render (sem o `https://`):
```bash
mc3_client.exe seu-projeto.onrender.com "MeuNick"
```
Ou dar dois cliques no `jogar_multiplayer.bat` e digitar o link do Render quando pedir o IP!

