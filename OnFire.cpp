#include <windows.h>
#include <string>

static const DWORD ADDR_PLAYER_PED   = 0xB6F5F0;
static const DWORD ADDR_FIRE_MANAGER = 0xB71F80;
static const DWORD ADDR_START_FIRE   = 0x53A050;
static const DWORD ADDR_EXTINGUISH   = 0x5393F0;

static const DWORD OFF_PHYS_FLAGS_B2 = 0x42;
static const BYTE  FLAG_FIREPROOF    = 0x08;
static const DWORD OFF_HEALTH        = 0x540;
static const DWORD OFF_FIRE          = 0x730;

typedef void *(__thiscall *StartFire_t)(void *mgr, void *target, void *creator, float size,
                                        unsigned char unused, unsigned int time, signed char gens);
typedef void(__thiscall *Extinguish_t)(void *fire);

static bool GameVersionOk() {
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

// Esta funcion no tiene objetos de C++, por eso puede usar __try
static void ApplyToPlayer(bool toggle, bool &active, bool &oldFireProof) {
    __try {
        DWORD ped = *(DWORD *)ADDR_PLAYER_PED;
        if (!ped)
            return;
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
            *flags |= FLAG_FIREPROOF;
            if (!*fire)
                ((StartFire_t)ADDR_START_FIRE)((void *)ADDR_FIRE_MANAGER, (void *)ped, nullptr,
                                               0.8f, 1, 7000, 100);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
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

        ApplyToPlayer(toggle, active, oldFireProof);
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
