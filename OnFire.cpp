// OnFire.asi v2 - GTA San Andreas 1.0 US
//   ONFIRE  : CJ en llamas sin daño (los NPC que lo toquen ~1.5 s se prenden)
//   FLAMER  : activa/desactiva el lanzallamas (mantener la tecla B)
//   FLYFIRE : activa/desactiva el vuelo con llamas de impulso
//             W/A/S/D = mover segun la camara, SHIFT o ESPACIO = subir, C = bajar
// Con cualquier modo activo, los NPC que pisen llamas se prenden fuego.

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>

// ---------- Direcciones GTA SA 1.0 ----------
static const DWORD ADDR_PLAYER_PED    = 0xB6F5F0;
static const DWORD ADDR_FIRE_MANAGER  = 0xB71F80;
static const DWORD ADDR_START_FIRE_E  = 0x53A050; // StartFire(target ped, ...)
static const DWORD ADDR_START_FIRE_P  = 0x539F00; // StartFire(punto x,y,z, ...)
static const DWORD ADDR_EXTINGUISH    = 0x5393F0;
static const DWORD ADDR_PED_POOL      = 0xB74490;
static const DWORD ADDR_CAMERA        = 0xB6F028;
static const DWORD ADDR_GAME_PROCESS  = 0x53E981; // call que se ejecuta cada frame

// ---------- Offsets ----------
static const DWORD OFF_MATRIX     = 0x14;   // CMatrix* (pos +0x30, forward +0x10, right +0x00)
static const DWORD OFF_PLACE_POS  = 0x04;   // posicion si no hay matriz
static const DWORD OFF_FLAGS0     = 0x40;   // bit 0x02 = bApplyGravity
static const DWORD OFF_FLAGS2     = 0x42;   // bit 0x08 = bFireProof
static const DWORD OFF_MOVESPEED  = 0x44;   // CVector
static const DWORD OFF_HEALTH     = 0x540;
static const DWORD OFF_FIRE       = 0x730;  // CFire*
static const BYTE  FLAG_GRAVITY   = 0x02;
static const BYTE  FLAG_FIREPROOF = 0x08;
static const int   PED_STRIDE     = 0x7C4;  // sizeof(CCopPed), tamaño de cada slot del pool

// ---------- Ajustes ----------
static const float FLY_SPEED = 0.45f;   // horizontal
static const float FLY_UP    = 0.25f;
static const float FLY_DOWN  = 0.15f;
static const int   KEY_FLAME = 'B';     // tecla del lanzallamas (mantener)

typedef void *(__thiscall *StartFireE_t)(void *mgr, void *target, void *creator, float size,
                                         unsigned char u, unsigned int time, signed char gens);
typedef void *(__thiscall *StartFireP_t)(void *mgr, float x, float y, float z, float size,
                                         unsigned char u, void *creator, unsigned int time,
                                         signed char gens, unsigned char u2);
typedef void(__thiscall *Extinguish_t)(void *fire);
typedef void(__cdecl *VoidFn)();

// ---------- Estado ----------
static volatile LONG g_reqFire = 0, g_reqFlame = 0, g_reqFly = 0;
static bool g_modeFire = false, g_modeFlame = false, g_modeFly = false;
static bool g_proofApplied = false, g_oldProof = false;
static bool g_gravSaved = false, g_oldGrav = true;
static DWORD g_nextFireTry = 0, g_nextFlame = 0, g_nextThrust = 0, g_nextScan = 0;
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

// NPC que pisan llamas del suelo, o que tocan a CJ en llamas, se prenden fuego
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

    // Llamas activas que no estan pegadas a una entidad (llamas del suelo / lanzallamas)
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
            continue;                                   // slot vacio
        DWORD p = objs + (DWORD)i * PED_STRIDE;
        if (p == player)
            continue;
        if (*(float *)(p + OFF_HEALTH) <= 0.0f)
            continue;                                   // muerto
        if (*(DWORD *)(p + OFF_FIRE))
            { g_contact[i] = 0; continue; }             // ya esta ardiendo

        float q[3];
        GetPos(p, q);
        bool ignite = false;

        for (int k = 0; k < nf && !ignite; k++) {
            float dx = q[0] - fx[k], dy = q[1] - fy[k], dz = q[2] - fz[k];
            if (dx * dx + dy * dy < 1.4f * 1.4f && dz > -3.0f && dz < 3.0f)
                ignite = true;
        }

        if (!ignite && g_modeFire) {
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
    BYTE *fl0 = (BYTE *)(ped + OFF_FLAGS0);
    BYTE *fl2 = (BYTE *)(ped + OFF_FLAGS2);
    DWORD *fire = (DWORD *)(ped + OFF_FIRE);
    DWORD now = GetTickCount();

    // ----- Interruptores (palabras tecleadas) -----
    if (InterlockedExchange(&g_reqFire, 0)) {
        g_modeFire = !g_modeFire;
        Log("OnFire: ONFIRE = %d", (int)g_modeFire);
        if (!g_modeFire && *fire)
            ((Extinguish_t)ADDR_EXTINGUISH)((void *)*fire);
        g_nextFireTry = 0;
    }
    if (InterlockedExchange(&g_reqFlame, 0)) {
        g_modeFlame = !g_modeFlame;
        Log("OnFire: FLAMER = %d", (int)g_modeFlame);
    }
    if (InterlockedExchange(&g_reqFly, 0)) {
        g_modeFly = !g_modeFly;
        Log("OnFire: FLYFIRE = %d", (int)g_modeFly);
    }

    // ----- Inmunidad al fuego mientras haya algun modo activo -----
    bool needProof = g_modeFire || g_modeFlame || g_modeFly;
    if (needProof && !g_proofApplied) {
        g_oldProof = (*fl2 & FLAG_FIREPROOF) != 0;
        g_proofApplied = true;
    }
    if (!needProof && g_proofApplied) {
        if (!g_oldProof)
            *fl2 &= (BYTE)~FLAG_FIREPROOF;
        g_proofApplied = false;
    }
    if (needProof)
        *fl2 |= FLAG_FIREPROOF;

    // ----- ONFIRE: CJ ardiendo -----
    if (g_modeFire && health > 0.0f && !*fire && (int)(now - g_nextFireTry) >= 0) {
        g_nextFireTry = now + 500;
        *fl2 &= (BYTE)~FLAG_FIREPROOF;      // si es a prueba de fuego el juego no lo enciende
        IgniteEntity(ped);
        *fl2 |= FLAG_FIREPROOF;
    }

    float pos[3] = {0, 0, 0};
    GetPos(ped, pos);

    // ----- Lanzallamas (mantener tecla B) -----
    if (g_modeFlame && health > 0.0f && Focused() && Key(KEY_FLAME) &&
        (int)(now - g_nextFlame) >= 0) {
        g_nextFlame = now + 150;
        DWORD m = *(DWORD *)(ped + OFF_MATRIX);
        if (m) {
            float *fwd = (float *)(m + 0x10);
            float dx = fwd[0], dy = fwd[1];
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0.001f) {
                dx /= len; dy /= len;
                static const float dist[3] = {2.0f, 3.5f, 5.0f};
                for (int i = 0; i < 3; i++)
                    FirePoint(pos[0] + dx * dist[i], pos[1] + dy * dist[i], pos[2] - 0.7f, 800);
            }
        }
    }

    // ----- Vuelo con llamas de impulso -----
    if (g_modeFly) {
        if (!g_gravSaved) {
            g_oldGrav = (*fl0 & FLAG_GRAVITY) != 0;
            g_gravSaved = true;
        }
        *fl0 &= (BYTE)~FLAG_GRAVITY;

        if (health > 0.0f) {
            bool foc = Focused();
            bool kW = foc && Key('W'), kS = foc && Key('S');
            bool kA = foc && Key('A'), kD = foc && Key('D');
            bool kUp = foc && (Key(VK_SHIFT) || Key(VK_SPACE));
            bool kDn = foc && Key('C');

            float fx = 0, fy = 1, rx = 1, ry = 0;
            DWORD cm = *(DWORD *)(ADDR_CAMERA + OFF_MATRIX);
            if (cm) {
                float *r = (float *)cm;
                float *u = (float *)(cm + 0x10);
                float lf = sqrtf(u[0] * u[0] + u[1] * u[1]);
                float lr = sqrtf(r[0] * r[0] + r[1] * r[1]);
                if (lf > 0.001f) { fx = u[0] / lf; fy = u[1] / lf; }
                if (lr > 0.001f) { rx = r[0] / lr; ry = r[1] / lr; }
            }

            float dx = 0, dy = 0;
            if (kW) { dx += fx; dy += fy; }
            if (kS) { dx -= fx; dy -= fy; }
            if (kD) { dx += rx; dy += ry; }
            if (kA) { dx -= rx; dy -= ry; }
            float len = sqrtf(dx * dx + dy * dy);
            bool moving = len > 0.001f;
            if (moving) { dx = dx / len; dy = dy / len; }

            float *ms = (float *)(ped + OFF_MOVESPEED);
            ms[0] = dx * FLY_SPEED;
            ms[1] = dy * FLY_SPEED;
            ms[2] = kUp ? FLY_UP : (kDn ? -FLY_DOWN : 0.0f);

            // Llamas del propulsor bajo los pies (y hacia atras si se mueve)
            if ((moving || kUp || kDn) && (int)(now - g_nextThrust) >= 0) {
                g_nextThrust = now + 110;
                FirePoint(pos[0] - dx * 0.6f, pos[1] - dy * 0.6f, pos[2] - 1.0f, 500);
            }
        }
    } else if (g_gravSaved) {
        if (g_oldGrav)
            *fl0 |= FLAG_GRAVITY;
        g_gravSaved = false;
    }

    // ----- Contagio a los NPC (10 veces por segundo) -----
    if (needProof && (int)(now - g_nextScan) >= 0) {
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

// Hilo: solo lee las teclas tecleadas (y ejecuta el frame si no se pudo enganchar el juego)
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
            InterlockedExchange(&g_reqFire, 1);
        } else if (typed.size() >= 6 && typed.compare(typed.size() - 6, 6, "FLAMER") == 0) {
            typed.clear();
            InterlockedExchange(&g_reqFlame, 1);
        } else if (typed.size() >= 7 && typed.compare(typed.size() - 7, 7, "FLYFIRE") == 0) {
            typed.clear();
            InterlockedExchange(&g_reqFly, 1);
        }

        if (g_useThreadFrame)
            OnFrame();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Log("OnFire v2: DLL cargada");

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
