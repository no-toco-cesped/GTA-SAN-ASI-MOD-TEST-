// OnFire.asi - GTA San Andreas 1.0 (US/EU)
// Teclea ONFIRE: CJ se prende en fuego sin recibir daño. Teclea ONFIRE otra vez: se apaga.
// Version SIN plugin-sdk: usa direcciones directas (verificadas con las cabeceras de plugin-sdk)
// para que compile con un solo comando de MSVC x86.

#include <windows.h>
#include <string>

// --- Direcciones GTA SA 1.0 ---
static const DWORD ADDR_PLAYER_PED   = 0xB6F5F0; // CPlayerPed* del jugador 0
static const DWORD ADDR_FIRE_MANAGER = 0xB71F80; // gFireManager
static const DWORD ADDR_START_FIRE   = 0x53A050; // CFireManager::StartFire(target, creator, size, u8, time, gens)
static const DWORD ADDR_EXTINGUISH   = 0x5393F0; // CFire::Extinguish()

// --- Offsets dentro de CPed ---
static const DWORD OFF_PHYS_FLAGS_B2 = 0x42;     // 3er byte de flags fisicos
static const BYTE  FLAG_FIREPROOF    = 0x08;     // bFireProof
static const DWORD OFF_HEALTH        = 0x540;    // float
static const DWORD OFF_FIRE          = 0x730;    // CFire* m_pFire

typedef void *(__thiscall *StartFire_t)(void *mgr, void *target, void *creator, float size,
                                        unsigned char unused, unsigned int time, signed char gens);
typedef void(__thiscall *Extinguish_t)(void *fire);

static bool GameVersionOk() {
    // Firma conocida: 1.0 US en 0x82457C, 1.0 EU en 0x8245BC (valor 0x94BF)
    __try {
        return *(DWORD *)0x82457C == 0x94BF || *(DWORD *)0x8245BC == 0x94BF;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool Focused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static DWORD WINAPI Worker(LPVOID) {
    if (!GameVersionOk())
        return 0;

    std::string typed;
    bool wasDown[26] = {};
    bool active = false;
    bool oldFireProof = false;

    for (;;) {
        Sleep(10);

        // 1) Leer letras tecleadas (independiente del buffer de cheats del juego)
        if (Focused()) {
            for (int i = 0; i < 26; i++) {
                bool down = (GetAsyncKeyState('A' + i) & 0x8000) != 0;
                if (down && !wasDown[i]) {
                    typed += (char)('A' + i);
                    if (typed.size() > 16) typed.erase(0, typed.size() - 16);
                }
                wasDown[i] = down;
            }
        }

        bool toggle = false;
        if (typed.size() >= 6 && typed.compare(typed.size() - 6, 6, "ONFIRE") == 0) {
            typed.clear();
            toggle = true;
        }

        // 2) Aplicar al jugador
        __try {
            DWORD ped = *(DWORD *)ADDR_PLAYER_PED;
            if (!ped)
                continue;
            BYTE *flags = (BYTE *)(ped + OFF_PHYS_FLAGS_B2);
            DWORD *fire = (DWORD *)(ped + OFF_FIRE);

            if (toggle) {
                active = !active;
                if (active) {
                    oldFireProof = (*flags & FLAG_FIREPROOF) != 0;
                } else {
                    if (*fire)
                        ((Extinguish_t)ADDR_EXTINGUISH)((void *)*fire);
                    if (!oldFireProof)
                        *flags &= (BYTE)~FLAG_FIREPROOF;
                }
            }

            if (active && *(float *)(ped + OFF_HEALTH) > 0.0f) {
                *flags |= FLAG_FIREPROOF;   // sin daño por fuego
                if (!*fire)                 // (re)prender si se apago
                    ((StartFire_t)ADDR_START_FIRE)((void *)ADDR_FIRE_MANAGER, (void *)ped, nullptr,
                                                   0.8f, 1, 7000, 100);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Si el juego esta cargando/cambiando de estado, ignorar este ciclo
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
