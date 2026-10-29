@echo off
title BetterMidNight - Cliente Sync
color 0B
cd /d "%~dp0"
echo =======================================================
echo         CONECTAR AO MULTIPLAYER DO MC3
echo =======================================================
set /p NICK="Digite seu Nickname [Enter para 'Luiz']: "
if "%NICK%"=="" set NICK=Luiz

set /p IP="Digite o IP do Servidor [Enter para '127.0.0.1']: "
if "%IP%"=="" set IP=127.0.0.1

echo.
echo [*] Conectando como '%NICK%' em %IP%...
mc3_client.exe %IP% "%NICK%"
pause

