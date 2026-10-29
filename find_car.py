import ctypes
from ctypes import wintypes
import struct
import math
import time
import sys

PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
TH32CS_SNAPPROCESS = 0x00000002

class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ProcessID', wintypes.DWORD),
        ('th32DefaultHeapID', ctypes.c_size_t),
        ('th32ModuleID', wintypes.DWORD),
        ('cntThreads', wintypes.DWORD),
        ('th32ParentProcessID', wintypes.DWORD),
        ('pcPriClassBase', wintypes.LONG),
        ('dwFlags', wintypes.DWORD),
        ('szExeFile', ctypes.c_char * 260)
    ]

def get_process_id():
    kernel32 = ctypes.windll.kernel32
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    pe = PROCESSENTRY32()
    pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pid = None
    if kernel32.Process32First(snapshot, ctypes.byref(pe)):
        while True:
            name = pe.szExeFile.decode('latin1', errors='ignore')
            if 'midnight club 3' in name.lower() and name.endswith('.exe'):
                pid = pe.th32ProcessID
                break
            if not kernel32.Process32Next(snapshot, ctypes.byref(pe)):
                break
    kernel32.CloseHandle(snapshot)
    return pid

def check_matrix(m):
    r = (m[0], m[1], m[2])
    u = (m[3], m[4], m[5])
    f = (m[6], m[7], m[8])
    pos = (m[9], m[10], m[11])
    
    lr = math.sqrt(r[0]**2 + r[1]**2 + r[2]**2)
    lu = math.sqrt(u[0]**2 + u[1]**2 + u[2]**2)
    lf = math.sqrt(f[0]**2 + f[1]**2 + f[2]**2)
    
    if abs(lr - 1.0) > 0.1 or abs(lu - 1.0) > 0.1 or abs(lf - 1.0) > 0.1:
        return False
        
    dot_ru = r[0]*u[0] + r[1]*u[1] + r[2]*u[2]
    dot_rf = r[0]*f[0] + r[1]*f[1] + r[2]*f[2]
    dot_uf = u[0]*f[0] + u[1]*f[1] + u[2]*f[2]
    
    if abs(dot_ru) > 0.15 or abs(dot_rf) > 0.15 or abs(dot_uf) > 0.15:
        return False
        
    for p in pos:
        if math.isnan(p) or math.isinf(p) or abs(p) > 20000.0:
            return False
            
    if abs(pos[0]) < 0.01 and abs(pos[1]) < 0.01 and abs(pos[2]) < 0.01:
        return False
    return True

def read_mem(handle, addr, size):
    buf = ctypes.create_string_buffer(size)
    bytesRead = ctypes.c_size_t()
    if ctypes.windll.kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, size, ctypes.byref(bytesRead)):
        return buf.raw[:bytesRead.value]
    return None

def main():
    print("=== MC3 Auto-Car-Finder ===", flush=True)
    pid = get_process_id()
    if not pid:
        print("[-] Midnight Club 3 nao esta aberto!", flush=True)
        return

    print(f"[+] Midnight Club 3 conectado! PID: {pid}", flush=True)
    handle = ctypes.windll.kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not handle:
        print("[-] Falha ao abrir processo.", flush=True)
        return

    class MEMORY_BASIC_INFORMATION64(ctypes.Structure):
        _fields_ = [
            ('BaseAddress', ctypes.c_uint64),
            ('AllocationBase', ctypes.c_uint64),
            ('AllocationProtect', wintypes.DWORD),
            ('alignment1', wintypes.DWORD),
            ('RegionSize', ctypes.c_uint64),
            ('State', wintypes.DWORD),
            ('Protect', wintypes.DWORD),
            ('Type', wintypes.DWORD),
            ('alignment2', wintypes.DWORD),
        ]

    mbi = MEMORY_BASIC_INFORMATION64()
    addr = 0
    MEM_COMMIT = 0x1000
    target_regions = []

    # Encontra apenas as regioes do guest RAM / heap do Xbox
    while ctypes.windll.kernel32.VirtualQueryEx(handle, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        if mbi.State == MEM_COMMIT and (mbi.Protect & 0x04 or mbi.Protect & 0x02):
            # As regioes principais do guest Xbox ficam em 60MB-75MB
            if 60 * 1024 * 1024 <= mbi.RegionSize <= 80 * 1024 * 1024:
                target_regions.append((mbi.BaseAddress, mbi.RegionSize))
        addr = mbi.BaseAddress + mbi.RegionSize

    print(f"[*] Mapeando matrizes 3D em {len(target_regions)} regioes principais...", flush=True)
    candidates = []
    for base, size in target_regions:
        raw = read_mem(handle, base, size)
        if raw:
            for off in range(0, len(raw) - 48, 16):
                chunk = raw[off:off+48]
                floats = struct.unpack('<12f', chunk)
                if check_matrix(floats):
                    candidates.append(base + off)

    print(f"[+] Matrizes 3D candidatas: {len(candidates)}", flush=True)
    if not candidates:
        print("[-] Nenhuma matriz encontrada.", flush=True)
        return

    # Captura posicoes iniciais
    initial_pos = {}
    for cand in candidates:
        data = read_mem(handle, cand, 48)
        if data and len(data) == 48:
            floats = struct.unpack('<12f', data)
            initial_pos[cand] = (floats[9], floats[10], floats[11])

    print("\n" + "="*50, flush=True)
    print(">> DIRIJA O CARRO AGORA! ACELERE POR 4 SEGUNDOS! <<", flush=True)
    print("="*50, flush=True)

    moved = []
    # Verifica a cada 0.5s por 4 segundos
    for _ in range(8):
        time.sleep(0.5)
        for cand, p0 in initial_pos.items():
            data = read_mem(handle, cand, 48)
            if data and len(data) == 48:
                floats = struct.unpack('<12f', data)
                p1 = (floats[9], floats[10], floats[11])
                dist = math.sqrt((p1[0]-p0[0])**2 + (p1[1]-p0[1])**2 + (p1[2]-p0[2])**2)
                if dist > 3.0 and cand not in moved:
                    moved.append((cand, dist, p1))
                    print(f"\n[CARRO ACHADO!] Endereco: 0x{cand:X} | Deslocamento: {dist:.1f}m", flush=True)

    if not moved:
        print("\n[-] Nenhum veiculo se moveu mais que 3 metros.", flush=True)
        print("    Dica: Acelere o carro assim que o script iniciar!", flush=True)
        return

    # Escolhe o que mais se moveu
    moved.sort(key=lambda x: x[1], reverse=True)
    target = moved[0][0]

    print("\n" + "="*50, flush=True)
    print(f"[SUCESSO] Rastreando carro em: 0x{target:X}", flush=True)
    print("="*50, flush=True)

    last_p = None
    last_t = time.time()
    for _ in range(15):
        time.sleep(0.3)
        data = read_mem(handle, target, 48)
        if data and len(data) == 48:
            floats = struct.unpack('<12f', data)
            px, py, pz = floats[9], floats[10], floats[11]
            now = time.time()
            dt = now - last_t
            speed = 0.0
            if last_p and dt > 0:
                dist = math.sqrt((px-last_p[0])**2 + (py-last_p[1])**2 + (pz-last_p[2])**2)
                speed = (dist / dt) * 3.6
            last_p = (px, py, pz)
            last_t = now
            print(f"X: {px:7.1f} | Y: {py:5.1f} | Z: {pz:7.1f} | Vel: {speed:5.1f} km/h", flush=True)

    ctypes.windll.kernel32.CloseHandle(handle)

if __name__ == '__main__':
    main()
