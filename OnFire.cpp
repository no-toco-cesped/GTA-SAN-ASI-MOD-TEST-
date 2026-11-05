// OnFire.asi v3 - GTA San Andreas 1.0 US
// Teclea ONFIRE para activar / desactivar TODO el modo:
//   - CJ cubierto por una llama grande, inmune al fuego y a las explosiones
//   - Regeneracion leve de vida
//   - Jetpack REAL del juego (sin la mochila visible) con llamas de impulso en los pies
//   - Lanzallamas con mira (boton derecho apunta, izquierdo dispara) y municion infinita
//   - Los NPC que pisen llamas o toquen a CJ unos segundos se prenden fuego

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>

// ---------- Interruptores: pon false para apagar una parte si algo falla ----------
static const bool  ENABLE_JETPACK      = true;   // jetpack real al activar ONFIRE
static const bool  HIDE_BACKPACK       = true;   // ocultar la mochila del jetpack
static const bool  ENABLE_FEET_FLAMES  = true;   // llamas extra en los pies al volar
static const bool  ENABLE_FLAMETHROWER = true;   // lanzallamas real con mira
static const bool  ENABLE_BIG_FLAME    = true;   // llama grande sobre CJ
static const float BIG_FLAME_STRENGTH  = 3.0f;
static const float REGEN_PER_TICK      = 0.6f;   // vida cada 250 ms (~2.4 por segundo)

// ---------- Direcciones GTA SA 1.0 ----------
static const DWORD ADDR_PLAYER_PED     = 0xB6F5F0;
static const DWORD ADDR_FIRE_MANAGER   = 0xB71F80;
static const DWORD ADDR_START_FIRE_E   = 0x53A050;
static const DWORD ADDR_START_FIRE_P   = 0x539F00;
static const DWORD ADDR_EXTINGUISH     = 0x5393F0;
static const DWORD ADDR_FX_DESTROY_ALL = 0x539D10;
static const DWORD ADDR_FX_CREATE_ALL  = 0x539D50;
static const DWORD ADDR_PED_POOL       = 0xB74490;
static const DWORD ADDR_GAME_PROCESS   = 0x53E981;
static const DWORD ADDR_JETPACK_CHEAT  = 0x439600;
static const DWORD ADDR_GET_TASK_JETPK = 0x601110;
static const DWORD ADDR_GIVE_WEAPON    = 0x5E6080;
static const DWORD ADDR_SET_CUR_WEAPON = 0x5E6280;
static const DWORD ADDR_CLEAR_WEAPON   = 0x5E62B0;
static const DWORD ADDR_REQUEST_MODEL  = 0x4087E0;
static const DWORD ADDR_LOAD_MODELS    = 0x40EA10;
static const DWORD ADDR_CLUMP_ATOMICS  = 0x749B70;  // RpClumpForAllAtomics

// ---------- Offsets ----------
static const DWORD OFF_MATRIX       = 0x14;
static const DWORD OFF_PLACE_POS    = 0x04;
static const DWORD OFF_FLAGS2       = 0x42;    // 0x08 = a prueba de fuego, 0x80 = a prueba de explosiones
static const DWORD OFF_MOVESPEED    = 0x44;
static const DWORD OFF_INTELLIGENCE = 0x47C;
static const DWORD OFF_HEALTH       = 0x540;
static const DWORD OFF_MAXHEALTH    = 0x544;
static const DWORD OFF_WEAPONS      = 0x5A0;   // CWeapon[13], 0x1C cada uno
static const DWORD OFF_VEHICLE      = 0x58C;
static const DWORD OFF_FIRE         = 0x730;
static const BYTE  PROOF_BITS       = 0x08 | 0x80;
static const BYTE  FLAG_FIREPROOF   = 0x08;
static const int   PED_STRIDE       = 0x7C4;
static const int   WEAPON_FLAMETHROWER = 37;
static const int   MODEL_FLAMETHROWER  = 361;
static const int   SLOT_FLAMETHROWER   = 7;

typedef void *(__thiscall *StartFireE_t)(void *mgr, void *target, void *creator, float size,
                                         unsigned char u, unsigned int time, signed char gens);
typedef void *(__thiscall *StartFireP_t)(void *mgr, float x, float y, float z, float size,
                                         unsigned char u, void *creator, unsigned int time,
                                         signed char gens, unsigned char u2);
typedef void(__thiscall *Extinguish_t)(void *fire);
typedef void(__cdecl *VoidFn)();

// ---------- Estado ----------
static volatile LONG g_reqToggle = 0;
static bool g_active = false;
static bool g_proofApplied = false;
static BYTE g_oldProofBits = 0;
static bool g_hadFlamethrower = false;
static bool g_wasDead = false;
static DWORD g_nextFireTry = 0, g_nextThrust = 0, g_nextScan = 0, g_nextRegen = 0, g_nextAmmo = 0;
static DWORD g_contact[1024];
static int g_logCount = 0;
static bool g_useThreadFrame = false;
static VoidFn g_origProcess = nullptr;

static void Log(const char *fmt, ...) {
    FILE *f = fopen("OnFire.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

static int ExFilter(unsigned int code) {
    if (g_logCount < 10) {
        g_logCount++;
        Log("OnFire: EXCEPCION 0x%08X", code);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static bool ReadDword(DWORD addr, DWORD &out) {
    __try {
        out = *(DWORD *)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool Focused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static bool Key(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void GetPos(DWORD ent, float *p) {
    DWORD m = *(DWORD *)(ent + OFF_MATRIX);
    float *src = m ? (float *)(m + 0x30) : (float *)(ent + OFF_PLACE_POS);
    p[0] = src[0]; p[1] = src[1]; p[2] = src[2];
}

static void IgniteEntity(DWORD target) {
    ((StartFireE_t)ADDR_START_FIRE_E)((void *)ADDR_FIRE_MANAGER, (void *)target, nullptr,
                                      0.8f, 1, 7000, 5);
}

static void FirePoint(float x, float y, float z, unsigned int lifetimeMs) {
    ((StartFireP_t)ADDR_START_FIRE_P)((void *)ADDR_FIRE_MANAGER, x, y, z, 0.8f, 1, nullptr,
                                      lifetimeMs, 0, 0);
}

// CJ ardiendo con la llama grande
static void IgniteBig(DWORD ped) {
    IgniteEntity(ped);
    if (ENABLE_BIG_FLAME) {
        DWORD f = *(DWORD *)(ped + OFF_FIRE);
        if (f) {
            *(float *)(f + 0x1C) = BIG_FLAME_STRENGTH;     // CFire::m_fStrength
            // El juego recrea los efectos de todas las llamas segun su fuerza
            ((void(__thiscall *)(void *))ADDR_FX_DESTROY_ALL)((void *)ADDR_FIRE_MANAGER);
            ((void(__thiscall *)(void *))ADDR_FX_CREATE_ALL)((void *)ADDR_FIRE_MANAGER);
        }
    }
}

// ---------- Lanzallamas real ----------
static bool HasFlamethrower(DWORD ped) {
    return *(DWORD *)(ped + OFF_WEAPONS + SLOT_FLAMETHROWER * 0x1C) == (DWORD)WEAPON_FLAMETHROWER;
}

static void GiveFlamethrower(DWORD ped) {
    Log("OnFire: [paso] cargando modelo del lanzallamas");
    ((void(__cdecl *)(int, int))ADDR_REQUEST_MODEL)(MODEL_FLAMETHROWER, 6);
    ((void(__cdecl *)(int))ADDR_LOAD_MODELS)(0);
    Log("OnFire: [paso] dando el lanzallamas");
    ((void(__thiscall *)(void *, int, unsigned int, int))ADDR_GIVE_WEAPON)(
        (void *)ped, WEAPON_FLAMETHROWER, 99999, 1);
    Log("OnFire: [paso] seleccionando el lanzallamas");
    ((void(__thiscall *)(void *, int))ADDR_SET_CUR_WEAPON)((void *)ped, WEAPON_FLAMETHROWER);
    Log("OnFire: [ok] lanzallamas listo");
}

// ---------- Jetpack real ----------
static DWORD GetJetpackTask(DWORD ped) {
    DWORD intel = *(DWORD *)(ped + OFF_INTELLIGENCE);
    if (!intel)
        return 0;
    return (DWORD)((void *(__thiscall *)(void *))ADDR_GET_TASK_JETPK)((void *)intel);
}

static void GiveJetpack(DWORD ped) {
    if (*(DWORD *)(ped + OFF_VEHICLE))
        return;                                  // dentro de un vehiculo
    if (GetJetpackTask(ped))
        return;                                  // ya tiene jetpack
    Log("OnFire: [paso] activando el jetpack");
    ((void(__cdecl *)())ADDR_JETPACK_CHEAT)();
    Log("OnFire: [ok] jetpack listo");
}

static void *__cdecl HideAtomicCb(void *atomic, void *) {
    ((BYTE *)atomic)[2] &= (BYTE)~0x04;          // quita rpATOMICRENDER
    return atomic;
}

static void HideBackpack(DWORD task) {
    DWORD clump = *(DWORD *)(task + 0x40);       // CTaskSimpleJetPack::m_pJetPackClump
    if (clump)
        ((void *(__cdecl *)(void *, void *(__cdecl *)(void *, void *), void *))ADDR_CLUMP_ATOMICS)(
            (void *)clump, HideAtomicCb, nullptr);
}

// NPC que pisan llamas del suelo, o que tocan a CJ, se prenden fuego
static void Contagion(DWORD player, DWORD now) {
    DWORD pool = *(DWORD *)ADDR_PED_POOL;
    if (!pool)
        return;
    DWORD objs = *(DWORD *)pool;
    BYTE *map = *(BYTE **)(pool + 4);
    int size = *(int *)(pool + 8);
    if (!objs || !map || size <= 0 || size > 1024)
        return;

    float pp[3];
    GetPos(player, pp);

    float fx[60], fy[60], fz[60];
    int nf = 0;
    for (int i = 0; i < 60; i++) {
        BYTE *f = (BYTE *)(ADDR_FIRE_MANAGER + i * 0x28);
        if ((f[0] & 1) && *(DWORD *)(f + 0x10) == 0) {
            float *pos = (float *)(f + 4);
            fx[nf] = pos[0]; fy[nf] = pos[1]; fz[nf] = pos[2];
            nf++;
        }
    }

    for (int i = 0; i < size; i++) {
        if (map[i] & 0x80)
            continue;
        DWORD p = objs + (DWORD)i * PED_STRIDE;
        if (p == player)
            continue;
        if (*(float *)(p + OFF_HEALTH) <= 0.0f)
            continue;
        if (*(DWORD *)(p + OFF_FIRE))
            { g_contact[i] = 0; continue; }

        float q[3];
        GetPos(p, q);
        bool ignite = false;

        for (int k = 0; k < nf && !ignite; k++) {
            float dx = q[0] - fx[k], dy = q[1] - fy[k], dz = q[2] - fz[k];
            if (dx * dx + dy * dy < 1.4f * 1.4f && dz > -3.0f && dz < 3.0f)
                ignite = true;
        }

        if (!ignite) {
            float dx = q[0] - pp[0], dy = q[1] - pp[1], dz = q[2] - pp[2];
            if (dx * dx + dy * dy < 1.1f * 1.1f && dz > -2.0f && dz < 2.0f) {
                if (g_contact[i] == 0)
                    g_contact[i] = now;
                else if (now - g_contact[i] >= 1500)
                    ignite = true;
            } else {
                g_contact[i] = 0;
            }
        }

        if (ignite) {
            g_contact[i] = 0;
            IgniteEntity(p);
        }
    }
}

static void Frame() {
    DWORD ped = *(DWORD *)ADDR_PLAYER_PED;
    if (!ped)
        return;
    float health = *(float *)(ped + OFF_HEALTH);
    BYTE *fl2 = (BYTE *)(ped + OFF_FLAGS2);
    DWORD *fire = (DWORD *)(ped + OFF_FIRE);
    DWORD now = GetTickCount();

    // ----- Interruptor ONFIRE -----
    if (InterlockedExchange(&g_reqToggle, 0)) {
        g_active = !g_active;
        Log("OnFire: ONFIRE = %d", (int)g_active);
        g_nextFireTry = 0;
        if (g_active) {
            g_wasDead = (health <= 0.0f);
            if (health > 0.0f) {
                if (ENABLE_FLAMETHROWER) {
                    g_hadFlamethrower = HasFlamethrower(ped);
                    GiveFlamethrower(ped);
                }
                if (ENABLE_JETPACK)
                    GiveJetpack(ped);
            }
        } else {
            if (*fire)
                ((Extinguish_t)ADDR_EXTINGUISH)((void *)*fire);
            if (ENABLE_FLAMETHROWER && !g_hadFlamethrower && HasFlamethrower(ped))
                ((void(__thiscall *)(void *, int))ADDR_CLEAR_WEAPON)((void *)ped, WEAPON_FLAMETHROWER);
            if (ENABLE_JETPACK) {
                DWORD task = GetJetpackTask(ped);
                if (task)
                    *(BYTE *)(task + 0x8) = 1;       // m_bIsFinished: CJ suelta el jetpack
            }
        }
    }

    // ----- Inmunidad a fuego y explosiones -----
    if (g_active && !g_proofApplied) {
        g_oldProofBits = (BYTE)(*fl2 & PROOF_BITS);
        g_proofApplied = true;
    }
    if (!g_active && g_proofApplied) {
        *fl2 = (BYTE)((*fl2 & ~PROOF_BITS) | g_oldProofBits);
        g_proofApplied = false;
    }
    if (g_active)
        *fl2 |= PROOF_BITS;

    if (!g_active)
        return;

    // ----- Si CJ murio y reaparecio, volver a dar las habilidades -----
    bool dead = (health <= 0.0f);
    if (g_wasDead && !dead) {
        if (ENABLE_FLAMETHROWER) {
            g_hadFlamethrower = false;
            GiveFlamethrower(ped);
        }
        if (ENABLE_JETPACK)
            GiveJetpack(ped);
        g_nextFireTry = 0;
    }
    g_wasDead = dead;
    if (dead)
        return;

    // ----- CJ ardiendo con llama grande -----
    if (!*fire && (int)(now - g_nextFireTry) >= 0) {
        g_nextFireTry = now + 500;
        *fl2 &= (BYTE)~FLAG_FIREPROOF;       // si es a prueba de fuego el juego no lo enciende
        IgniteBig(ped);
        *fl2 |= FLAG_FIREPROOF;
    }

    // ----- Regeneracion leve -----
    if ((int)(now - g_nextRegen) >= 0) {
        g_nextRegen = now + 250;
        float maxHealth = *(float *)(ped + OFF_MAXHEALTH);
        if (health < maxHealth) {
            health += REGEN_PER_TICK;
            if (health > maxHealth)
                health = maxHealth;
            *(float *)(ped + OFF_HEALTH) = health;
        }
    }

    // ----- Municion infinita del lanzallamas -----
    if (ENABLE_FLAMETHROWER && (int)(now - g_nextAmmo) >= 0) {
        g_nextAmmo = now + 1000;
        if (HasFlamethrower(ped)) {
            DWORD *ammo = (DWORD *)(ped + OFF_WEAPONS + SLOT_FLAMETHROWER * 0x1C + 0xC);
            if (*ammo < 50000)
                *ammo = 99999;
        }
    }

    float pos[3] = {0, 0, 0};
    GetPos(ped, pos);

    // ----- Jetpack: ocultar mochila y llamas de impulso en los pies -----
    if (ENABLE_JETPACK) {
        DWORD task = GetJetpackTask(ped);
        if (task) {
            if (HIDE_BACKPACK)
                HideBackpack(task);
            if (ENABLE_FEET_FLAMES) {
                float *ms = (float *)(ped + OFF_MOVESPEED);
                float speed = sqrtf(ms[0] * ms[0] + ms[1] * ms[1] + ms[2] * ms[2]);
                if (speed > 0.03f && (int)(now - g_nextThrust) >= 0) {
                    g_nextThrust = now + 110;
                    float hs = sqrtf(ms[0] * ms[0] + ms[1] * ms[1]);
                    float bx = 0, by = 0;
                    if (hs > 0.001f) { bx = -ms[0] / hs * 0.5f; by = -ms[1] / hs * 0.5f; }
                    FirePoint(pos[0] + bx, pos[1] + by, pos[2] - 1.0f, 450);
                }
            }
        }
    }

    // ----- Contagio a los NPC (10 veces por segundo) -----
    if ((int)(now - g_nextScan) >= 0) {
        g_nextScan = now + 100;
        Contagion(ped, now);
    }
}

static void OnFrame() {
    __try {
        Frame();
    } __except (ExFilter(GetExceptionCode())) {
    }
}

static void __cdecl HookedProcess() {
    g_origProcess();
    OnFrame();
}

static bool InstallHook() {
    BYTE *p = (BYTE *)ADDR_GAME_PROCESS;
    if (p[0] != 0xE8) {
        Log("OnFire: el byte en 0x%08X no es un CALL (0x%02X)", ADDR_GAME_PROCESS, p[0]);
        return false;
    }
    DWORD rel = *(DWORD *)(p + 1);
    g_origProcess = (VoidFn)(ADDR_GAME_PROCESS + 5 + rel);
    DWORD oldProt = 0;
    if (!VirtualProtect(p + 1, 4, PAGE_EXECUTE_READWRITE, &oldProt))
        return false;
    *(DWORD *)(p + 1) = (DWORD)HookedProcess - (ADDR_GAME_PROCESS + 5);
    VirtualProtect(p + 1, 4, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return true;
}

static DWORD WINAPI Worker(LPVOID) {
    std::string typed;
    bool wasDown[26] = {};

    for (;;) {
        Sleep(10);

        if (Focused()) {
            for (int i = 0; i < 26; i++) {
                bool down = Key('A' + i);
                if (down && !wasDown[i]) {
                    typed += (char)('A' + i);
                    if (typed.size() > 16) typed.erase(0, typed.size() - 16);
                }
                wasDown[i] = down;
            }
        }

        if (typed.size() >= 6 && typed.compare(typed.size() - 6, 6, "ONFIRE") == 0) {
            typed.clear();
            InterlockedExchange(&g_reqToggle, 1);
        }

        if (g_useThreadFrame)
            OnFrame();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Log("OnFire v3: DLL cargada");

        DWORD sigUS = 0, sigEU = 0;
        bool okUS = ReadDword(0x82457C, sigUS);
        bool okEU = ReadDword(0x8245BC, sigEU);
        if (!((okUS && sigUS == 0x94BF) || (okEU && sigEU == 0x94BF))) {
            Log("OnFire: version del juego NO reconocida, el mod se desactiva");
            return TRUE;
        }

        if (InstallHook()) {
            Log("OnFire: enganchado al bucle del juego");
        } else {
            g_useThreadFrame = true;
            Log("OnFire: no se pudo enganchar, se usa el hilo (modo de respaldo)");
        }
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
