@echo off
chcp 65001 >nul
setlocal
echo [*] Configurando ambiente MSVC x64...

set VCV=14.52.36815
set SDKINC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0
set SDKLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0

set PATH=C:\BuildTools\VC\Tools\MSVC\%VCV%\bin\Hostx64\x64;C:\BuildTools\MSBuild\Current\Bin;%PATH%
set INCLUDE=C:\BuildTools\VC\Tools\MSVC\%VCV%\include;%SDKINC%\ucrt;%SDKINC%\um;%SDKINC%\winrt;%SDKINC%\shared
set LIB=C:\BuildTools\VC\Tools\MSVC\%VCV%\lib\x64;%SDKLIB%\um\x64;%SDKLIB%\ucrt\x64

cd /d "%~dp0"

echo [*] Compilando server.cpp -> mc3_server.exe ...
cl /nologo /EHsc /O2 /W3 server.cpp /Fe:mc3_server.exe /link ws2_32.lib /SUBSYSTEM:CONSOLE
if %ERRORLEVEL% neq 0 (
    echo [-] Erro ao compilar server.cpp
    exit /b %ERRORLEVEL%
)

echo [*] Compilando client.cpp -> mc3_client.exe ...
cl /nologo /EHsc /O2 /W3 client.cpp /Fe:mc3_client.exe /link ws2_32.lib /SUBSYSTEM:CONSOLE
if %ERRORLEVEL% neq 0 (
    echo [-] Erro ao compilar client.cpp
    exit /b %ERRORLEVEL%
)

echo ========================================================
echo [+] BUILD CONCLUIDO COM SUCESSO!
echo     - mc3_server.exe (Servidor Dedicado de Salas e Missoes)
echo     - mc3_client.exe (Cliente Sincronizador do Jogo)
echo ========================================================
