@echo off
chcp 65001 >nul
title BetterMidNight - 1-Click Multiplayer
color 0B
cd /d "%~dp0"

echo =======================================================
echo          BETTER-MIDNIGHT MULTIPLAYER (1-CLIQUE)
echo =======================================================
echo.

REM Verifica se o servidor ja esta rodando na porta 17500
netstat -ano | findstr 17500 >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [*] Servidor dedicado ja esta online na porta 17500!
) else (
    echo [*] Servidor nao detectado. Iniciando servidor dedicado em segundo plano...
    start /min "BetterMidNight Server" mc3_server.exe
    ping 127.0.0.1 -n 2 >nul
    echo [+] Servidor iniciado com sucesso!
)

echo.
set /p NICK="Digite seu Nickname [Pressione Enter para 'Luiz']: "
if "%NICK%"=="" set NICK=Luiz

echo.
echo [*] Conectando ao Midnight Club 3 como '%NICK%'...
echo.
mc3_client.exe 127.0.0.1 "%NICK%"

echo.
echo =======================================================
echo Sessao encerrada.
pause
