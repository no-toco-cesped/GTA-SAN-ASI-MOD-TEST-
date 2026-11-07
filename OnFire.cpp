// TEST.asi v5 - GTA San Andreas 1.0 US  (banco de pruebas de trucos y mecanicas)
//
//  ONFIRE : CJ ardiendo con llama grande, inmune a fuego y explosiones, regeneracion leve,
//           lanzallamas con animacion de pistolas dobles (arma invisible), el fuego empuja
//           NPC/autos/objetos, las llamas del suelo se apagan rapido y los NPC se contagian.
//  ONWATER: extintor como canon de agua apuntable (clic derecho apunta, izquierdo dispara),
//           el agua empuja entidades, tecla X (mantener) agarra NPC/autos y al soltar los lanza
//           (estilo physics gun), tecla Z lanza una ola grande de agua que empuja y apaga fuegos.
//  CLAXON : al tocar la bocina de un vehiculo (conduciendo) este recibe impulso hacia adelante.
//
// Cada truco muestra "Truco activado" / "Truco desactivado" en pantalla.

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstring>

// ---------- Interruptores: pon false para apagar una parte si algo falla ----------
static const bool  SHOW_MESSAGES       = true;
static const bool  ENABLE_BIG_FLAME    = true;   // llama grande sobre CJ
static const bool  ENABLE_FLAMETHROWER = true;   // "pistolas" que lanzan llamas
static const bool  ENABLE_PISTOL_ANIM  = true;   // animacion de pistolas dobles
static const bool  HIDE_WEAPON_MODEL   = true;   // arma en mano invisible
static const bool  ENABLE_FIRE_PUSH    = true;   // el fuego empuja
static const bool  FAST_FLAME_DECAY    = true;   // apagar rapido las llamas del suelo
static const bool  ENABLE_WATER_GUN    = true;   // extintor como canon de agua
static const bool  ENABLE_WATER_FX     = true;   // efectos de agua (si el juego los tiene)
static const bool  ENABLE_GRAB         = true;   // agarrar y lanzar
static const bool  ENABLE_WAVE         = true;   // ola de agua
static const bool  ENABLE_HORN_BOOST   = true;   // impulso con la bocina

static const int   KEY_GRAB            = 'X';    // mantener = agarrar, soltar = lanzar
static const int   KEY_WAVE            = 'Z';    // ola de agua

static const float BIG_FLAME_STRENGTH  = 3.0f;
static const DWORD FLAME_LIFETIME_MS   = 600;    // vida maxima de las llamas del suelo
static const float FIRE_PUSH_RANGE     = 11.0f;
static const float WATER_PUSH_RANGE    = 14.0f;
static const float REGEN_PER_TICK      = 0.6f;   // vida cada 250 ms (~2.4 por segundo)
static const float GRAB_THROW_SPEED    = 1.3f;
static const float HORN_ACCEL          = 0.018f; // impulso por frame
static const float HORN_MAX_SPEED      = 1.5f;

// ---------- Direcciones GTA SA 1.0 ----------
static const DWORD ADDR_PLAYER_PED     = 0xB6F5F0;
static const DWORD ADDR_CAMERA         = 0xB6F028;
static const DWORD ADDR_FIRE_MANAGER   = 0xB71F80;
static const DWORD ADDR_START_FIRE_E   = 0x53A050;
static const DWORD ADDR_EXTINGUISH     = 0x5393F0;
static const DWORD ADDR_FX_DESTROY_ALL = 0x539D10;
static const DWORD ADDR_FX_CREATE_ALL  = 0x539D50;
static const DWORD ADDR_PED_POOL       = 0xB74490;
static const DWORD ADDR_VEHICLE_POOL   = 0xB74494;
static const DWORD ADDR_OBJECT_POOL    = 0xB7449C;
static const DWORD ADDR_GAME_PROCESS   = 0x53E981;
static const DWORD ADDR_GIVE_WEAPON    = 0x5E6080;
static const DWORD ADDR_SET_CUR_WEAPON = 0x5E6280;
static const DWORD ADDR_CLEAR_WEAPON   = 0x5E62B0;
static const DWORD ADDR_REQUEST_MODEL  = 0x4087E0;
static const DWORD ADDR_LOAD_MODELS    = 0x40EA10;
static const DWORD ADDR_CLUMP_ATOMICS  = 0x749B70;  // RpClumpForAllAtomics
static const DWORD ADDR_GET_PAD        = 0x53FB70;  // CPad::GetPad(int)
static const DWORD ADDR_GET_HORN       = 0x53FEE0;  // CPad::GetHorn()
static const DWORD ADDR_GET_WEAPONINFO = 0x743C60;  // CWeaponInfo::GetWeaponInfo(type, skill)
static const DWORD ADDR_GAME_TIME      = 0xB7CB84;  // CTimer::m_snTimeInMilliseconds
static const DWORD ADDR_ADD_MESSAGE    = 0x69F1E0;  // CMessages::AddMessageJumpQ
static const DWORD ADDR_FX_MANAGER     = 0xA9AE80;  // g_fxMan
static const DWORD ADDR_FX_FIND_BP     = 0x4A9360;  // FxManager_c::FindFxSystemBP(char*)
static const DWORD ADDR_FX_CREATE      = 0x4A9BE0;  // FxManager_c::CreateFxSystem(char*, RwV3d*, RwMatrix*, uchar)
static const DWORD ADDR_FX_PLAYKILL    = 0x4AA3D0;  // FxSystem_c::PlayAndKill()

// ---------- Offsets ----------
static const DWORD OFF_MATRIX       = 0x14;
static const DWORD OFF_PLACE_POS    = 0x04;
static const DWORD OFF_FLAGS2       = 0x42;    // 0x08 = a prueba de fuego, 0x80 = a prueba de explosiones
static const DWORD OFF_MOVESPEED    = 0x44;
static const DWORD OFF_HEALTH       = 0x540;
static const DWORD OFF_MAXHEALTH    = 0x544;
static const DWORD OFF_WEAPONS      = 0x5A0;   // CWeapon[13], 0x1C cada uno
static const DWORD OFF_VEHICLE      = 0x58C;
static const DWORD OFF_WEAPON_OBJ   = 0x4F4;
static const DWORD OFF_CUR_SLOT     = 0x718;
static const DWORD OFF_FIRE         = 0x730;
static const DWORD OFF_VEH_DRIVER   = 0x460;
static const BYTE  PROOF_BITS       = 0x08 | 0x80;
static const BYTE  FLAG_FIREPROOF   = 0x08;
static const int   PED_STRIDE       = 0x7C4;
static const int   VEHICLE_STRIDE   = 0xA18;
static const int   OBJECT_STRIDE    = 0x19C;
static const int   WEAPON_FLAMETHROWER = 37;
static const int   WEAPON_EXTINGUISHER = 42;

typedef void *(__thiscall *StartFireE_t)(void *mgr, void *target, void *creator, float size,
                                         unsigned char u, unsigned int time, signed char gens);
typedef void(__thiscall *Extinguish_t)(void *fire);
typedef void(__cdecl *VoidFn)();

// ---------- Estado ----------
static volatile LONG g_reqFire = 0, g_reqWater = 0, g_reqHorn = 0;
static bool g_fire = false, g_water = false, g_horn = false;

static bool g_proofApplied = false;
static BYTE g_oldProofBits = 0;
static bool g_hadFlame = false, g_hadExt = false;
static bool g_wasDead = false;
static DWORD g_nextFireTry = 0, g_nextScan = 0, g_nextRegen = 0, g_nextAmmo = 0, g_nextDecay = 0;
static DWORD g_nextStreamFx = 0, g_nextWaveFx = 0;
static bool g_animSaved = false;
static DWORD g_savedAnim[9];
static DWORD g_contact[1024];

static const char *g_fxStream = nullptr;
static const char *g_fxSplash = nullptr;

// agarrar / lanzar
static DWORD g_grabbed = 0, g_grabPool = 0;
static int g_grabStride = 0;
static float g_grabDist = 6.0f;
// ola
static bool g_waveActive = false;
static DWORD g_waveStart = 0;
static float g_waveOrigin[3] = {0, 0, 0};
static float g_waveDir[2] = {0, 1};
static bool g_waveKeyWas = false;

static int g_logCount = 0;
static bool g_useThreadFrame = false;
static VoidFn g_origProcess = nullptr;

static void Log(const char *fmt, ...) {
    FILE *f = fopen("TEST.log", "a");
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
        Log("TEST: EXCEPCION 0x%08X", code);
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

static void ShowMessage(const char *text) {
    if (SHOW_MESSAGES)
        ((void(__cdecl *)(const char *, unsigned int, unsigned short, int))ADDR_ADD_MESSAGE)(text, 2500, 0, 0);
}

// Direccion de la camara (donde mira el jugador / la mira)
static bool CamForward(float *fwd) {
    DWORD m = *(DWORD *)(ADDR_CAMERA + OFF_MATRIX);
    if (!m)
        return false;
    float *u = (float *)(m + 0x10);
    float len = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (len < 0.001f)
        return false;
    fwd[0] = u[0] / len; fwd[1] = u[1] / len; fwd[2] = u[2] / len;
    return true;
}

// ---------- Fuego ----------
static void IgniteEntity(DWORD target) {
    ((StartFireE_t)ADDR_START_FIRE_E)((void *)ADDR_FIRE_MANAGER, (void *)target, nullptr,
                                      0.8f, 1, 7000, 5);
}

static void IgniteBig(DWORD ped) {
    IgniteEntity(ped);
    if (ENABLE_BIG_FLAME) {
        DWORD f = *(DWORD *)(ped + OFF_FIRE);
        if (f) {
            *(float *)(f + 0x1C) = BIG_FLAME_STRENGTH;     // CFire::m_fStrength
            ((void(__thiscall *)(void *))ADDR_FX_DESTROY_ALL)((void *)ADDR_FIRE_MANAGER);
            ((void(__thiscall *)(void *))ADDR_FX_CREATE_ALL)((void *)ADDR_FIRE_MANAGER);
        }
    }
}

// ---------- Armas ----------
static DWORD *WInfo(int type) {
    return (DWORD *)((void *(__cdecl *)(int, unsigned char))ADDR_GET_WEAPONINFO)(type, 1);
}

static int WSlot(int type) {
    DWORD *wi = WInfo(type);
    int slot = wi ? (int)wi[0x14 / 4] : 0;
    return (slot < 0 || slot > 12) ? 0 : slot;
}

static DWORD WeaponAddr(DWORD ped, int type) {
    return ped + OFF_WEAPONS + (DWORD)WSlot(type) * 0x1C;
}

static bool HasWeapon(DWORD ped, int type) {
    return *(DWORD *)WeaponAddr(ped, type) == (DWORD)type;
}

// true si el arma esta seleccionada y disparando
static bool IsFiring(DWORD ped, int type) {
    if (!HasWeapon(ped, type))
        return false;
    if (*(BYTE *)(ped + OFF_CUR_SLOT) != (BYTE)WSlot(type))
        return false;
    return *(DWORD *)(WeaponAddr(ped, type) + 4) == 1;   // CWeapon::m_nState == FIRING
}

static void GiveWeaponFull(DWORD ped, int type) {
    DWORD *wi = WInfo(type);
    int model = wi ? (int)wi[0xC / 4] : -1;
    Log("TEST: [paso] dando arma %d (modelo %d)", type, model);
    if (model > 0) {
        ((void(__cdecl *)(int, int))ADDR_REQUEST_MODEL)(model, 6);
        ((void(__cdecl *)(int))ADDR_LOAD_MODELS)(0);
    }
    ((void(__thiscall *)(void *, int, unsigned int, int))ADDR_GIVE_WEAPON)((void *)ped, type, 99999, 1);
    ((void(__thiscall *)(void *, int))ADDR_SET_CUR_WEAPON)((void *)ped, type);
    Log("TEST: [ok] arma %d lista", type);
}

static void KeepAmmo(DWORD ped, int type) {
    if (!HasWeapon(ped, type))
        return;
    DWORD *ammo = (DWORD *)(WeaponAddr(ped, type) + 0xC);
    if (*ammo < 50000)
        *ammo = 99999;
}

static void *__cdecl HideAtomicCb(void *atomic, void *) {
    ((BYTE *)atomic)[2] &= (BYTE)~0x04;          // quita rpATOMICRENDER
    return atomic;
}

// Oculta el arma que CJ lleva en la mano
static void HideWeaponModel(DWORD ped) {
    DWORD obj = *(DWORD *)(ped + OFF_WEAPON_OBJ);
    if (!obj)
        return;
    BYTE type = *(BYTE *)obj;                    // RwObject::type: 1 = atomic, 2 = clump
    if (type == 2)
        ((void *(__cdecl *)(void *, void *(__cdecl *)(void *, void *), void *))ADDR_CLUMP_ATOMICS)(
            (void *)obj, HideAtomicCb, nullptr);
    else if (type == 1)
        HideAtomicCb((void *)obj, nullptr);
}

// ---------- Animacion de pistolas dobles para el lanzallamas ----------
static void ApplyPistolAnim() {
    DWORD *fi = WInfo(WEAPON_FLAMETHROWER);
    DWORD *pi = (DWORD *)((void *(__cdecl *)(int, unsigned char))ADDR_GET_WEAPONINFO)(22, 2);  // pistola PRO
    if (!fi || !pi || fi == pi)
        return;
    if (!g_animSaved) {
        g_savedAnim[0] = fi[0x18 / 4];
        g_savedAnim[1] = fi[0x1C / 4];
        for (int i = 0; i < 6; i++)
            g_savedAnim[2 + i] = fi[(0x40 + i * 4) / 4];
        g_savedAnim[8] = fi[0x58 / 4];
        g_animSaved = true;
    }
    Log("TEST: [paso] copiando animacion de pistolas dobles");
    fi[0x1C / 4] = pi[0x1C / 4];
    for (int i = 0; i < 6; i++)
        fi[(0x40 + i * 4) / 4] = pi[(0x40 + i * 4) / 4];
    fi[0x58 / 4] = pi[0x58 / 4];
    fi[0x18 / 4] |= 0x800;                       // bTwinPistol
    Log("TEST: [ok] animacion aplicada");
}

static void RestorePistolAnim() {
    if (!g_animSaved)
        return;
    DWORD *fi = WInfo(WEAPON_FLAMETHROWER);
    if (!fi)
        return;
    fi[0x18 / 4] = g_savedAnim[0];
    fi[0x1C / 4] = g_savedAnim[1];
    for (int i = 0; i < 6; i++)
        fi[(0x40 + i * 4) / 4] = g_savedAnim[2 + i];
    fi[0x58 / 4] = g_savedAnim[8];
    g_animSaved = false;
}

// ---------- Efectos (FX) de agua ----------
static const char *ResolveFx(const char *const *names) {
    for (int i = 0; names[i]; i++) {
        void *bp = ((void *(__thiscall *)(void *, char *))ADDR_FX_FIND_BP)((void *)ADDR_FX_MANAGER, (char *)names[i]);
        if (bp)
            return names[i];
    }
    return nullptr;
}

static void PlayFx(const char *name, float x, float y, float z) {
    if (!name || !ENABLE_WATER_FX)
        return;
    float p[3] = {x, y, z};
    void *fx = ((void *(__thiscall *)(void *, char *, float *, void *, unsigned char))ADDR_FX_CREATE)(
        (void *)ADDR_FX_MANAGER, (char *)name, p, nullptr, 1);
    if (fx)
        ((void(__thiscall *)(void *))ADDR_FX_PLAYKILL)(fx);
}

static void ResolveWaterFx() {
    static const char *const streamNames[] = {"water_hydrant", "water_fountain", "water_ripples", nullptr};
    static const char *const splashNames[] = {"water_splash_big", "water_splash", "water_ripples", nullptr};
    g_fxStream = ResolveFx(streamNames);
    g_fxSplash = ResolveFx(splashNames);
    Log("TEST: fx de chorro = %s, fx de salpicadura = %s",
        g_fxStream ? g_fxStream : "(ninguno)", g_fxSplash ? g_fxSplash : "(ninguno)");
}

// ---------- Pools ----------
static bool PoolInfo(DWORD poolAddr, DWORD &objs, BYTE *&map, int &size) {
    DWORD pool = *(DWORD *)poolAddr;
    if (!pool)
        return false;
    objs = *(DWORD *)pool;
    map = *(BYTE **)(pool + 4);
    size = *(int *)(pool + 8);
    return objs && map && size > 0 && size <= 2048;
}

static bool IsLive(DWORD poolAddr, int stride, DWORD e) {
    DWORD objs; BYTE *map; int size;
    if (!PoolInfo(poolAddr, objs, map, size))
        return false;
    if (e < objs)
        return false;
    DWORD off = e - objs;
    if (off % (DWORD)stride)
        return false;
    DWORD idx = off / (DWORD)stride;
    if ((int)idx >= size)
        return false;
    return (map[idx] & 0x80) == 0;
}

// ---------- Empujon en cono (fuego y agua) ----------
static void PushPool(DWORD poolAddr, int stride, DWORD player, const float *pp, float dx, float dy,
                     float strength, float range, bool isPed) {
    DWORD objs; BYTE *map; int size;
    if (!PoolInfo(poolAddr, objs, map, size))
        return;
    DWORD playerVeh = *(DWORD *)(player + OFF_VEHICLE);

    for (int i = 0; i < size; i++) {
        if (map[i] & 0x80)
            continue;
        DWORD e = objs + (DWORD)i * stride;
        if (e == player || e == playerVeh)
            continue;
        if (isPed && *(DWORD *)(e + OFF_VEHICLE))
            continue;
        float q[3];
        GetPos(e, q);
        float vx = q[0] - pp[0], vy = q[1] - pp[1], vz = q[2] - pp[2];
        if (vz < -4.0f || vz > 4.0f)
            continue;
        float dist = sqrtf(vx * vx + vy * vy);
        if (dist < 0.8f || dist > range)
            continue;
        if ((vx * dx + vy * dy) / dist < 0.82f)
            continue;
        float *ms = (float *)(e + OFF_MOVESPEED);
        if (ms[0] * dx + ms[1] * dy < 0.45f) {
            ms[0] += dx * strength;
            ms[1] += dy * strength;
            ms[2] += strength * 0.12f;
        }
    }
}

static void PushCone(DWORD ped, const float *pp, bool useCamera, float range, float sPed, float sVeh, float sObj) {
    float dx, dy;
    if (useCamera) {
        float f[3];
        if (!CamForward(f))
            return;
        dx = f[0]; dy = f[1];
    } else {
        DWORD m = *(DWORD *)(ped + OFF_MATRIX);
        if (!m)
            return;
        float *fwd = (float *)(m + 0x10);
        dx = fwd[0]; dy = fwd[1];
    }
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f)
        return;
    dx /= len; dy /= len;
    PushPool(ADDR_PED_POOL,     PED_STRIDE,     ped, pp, dx, dy, sPed, range, true);
    PushPool(ADDR_VEHICLE_POOL, VEHICLE_STRIDE, ped, pp, dx, dy, sVeh, range, false);
    PushPool(ADDR_OBJECT_POOL,  OBJECT_STRIDE,  ped, pp, dx, dy, sObj, range, false);
}

// ---------- Llamas del suelo: vida corta ----------
static void DecayFires() {
    DWORD limit = *(DWORD *)ADDR_GAME_TIME + FLAME_LIFETIME_MS;
    for (int i = 0; i < 60; i++) {
        BYTE *f = (BYTE *)(ADDR_FIRE_MANAGER + i * 0x28);
        if (!(f[0] & 1))
            continue;
        if (f[0] & 2)                            // script de mision
            continue;
        if (*(DWORD *)(f + 0x10) != 0)           // pegada a una entidad
            continue;
        DWORD *expiry = (DWORD *)(f + 0x18);
        if (*expiry > limit)
            *expiry = limit;
    }
}

// ---------- NPC que pisan llamas o tocan a CJ se prenden ----------
static void Contagion(DWORD player, DWORD now) {
    DWORD objs; BYTE *map; int size;
    if (!PoolInfo(ADDR_PED_POOL, objs, map, size) || size > 1024)
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

// ---------- Agarrar y lanzar (estilo physics gun) ----------
static void PickBest(DWORD poolAddr, int stride, bool isPed, DWORD player, const float *pp,
                     const float *fwd, DWORD &best, DWORD &bestPool, int &bestStride, float &bestDist) {
    DWORD objs; BYTE *map; int size;
    if (!PoolInfo(poolAddr, objs, map, size))
        return;
    DWORD playerVeh = *(DWORD *)(player + OFF_VEHICLE);
    for (int i = 0; i < size; i++) {
        if (map[i] & 0x80)
            continue;
        DWORD e = objs + (DWORD)i * stride;
        if (e == player || e == playerVeh)
            continue;
        if (isPed && *(DWORD *)(e + OFF_VEHICLE))
            continue;
        float q[3];
        GetPos(e, q);
        float vx = q[0] - pp[0], vy = q[1] - pp[1], vz = q[2] - pp[2];
        float dist = sqrtf(vx * vx + vy * vy + vz * vz);
        if (dist < 1.5f || dist > 25.0f)
            continue;
        float cosA = (vx * fwd[0] + vy * fwd[1] + vz * fwd[2]) / dist;
        if (cosA < 0.92f)
            continue;
        if (dist < bestDist) {
            bestDist = dist; best = e; bestPool = poolAddr; bestStride = stride;
        }
    }
}

static void ReleaseGrab(bool doThrow) {
    if (g_grabbed && doThrow && IsLive(g_grabPool, g_grabStride, g_grabbed)) {
        float f[3];
        if (CamForward(f)) {
            float *ms = (float *)(g_grabbed + OFF_MOVESPEED);
            ms[0] = f[0] * GRAB_THROW_SPEED;
            ms[1] = f[1] * GRAB_THROW_SPEED;
            ms[2] = f[2] * GRAB_THROW_SPEED + 0.08f;
        }
    }
    g_grabbed = 0;
}

static void UpdateGrab(DWORD ped, const float *pp, bool held) {
    if (!held) {
        if (g_grabbed)
            ReleaseGrab(true);                    // soltar = lanzar
        return;
    }
    float f[3];
    if (!CamForward(f)) {
        ReleaseGrab(false);
        return;
    }
    if (g_grabbed && !IsLive(g_grabPool, g_grabStride, g_grabbed))
        g_grabbed = 0;

    if (!g_grabbed) {
        DWORD best = 0, bestPool = 0;
        int bestStride = 0;
        float bestDist = 1e9f;
        PickBest(ADDR_PED_POOL,     PED_STRIDE,     true,  ped, pp, f, best, bestPool, bestStride, bestDist);
        PickBest(ADDR_VEHICLE_POOL, VEHICLE_STRIDE, false, ped, pp, f, best, bestPool, bestStride, bestDist);
        if (best) {
            g_grabbed = best; g_grabPool = bestPool; g_grabStride = bestStride;
            g_grabDist = bestDist < 3.0f ? 3.0f : (bestDist > 12.0f ? 12.0f : bestDist);
        }
        return;
    }

    // Mantener flotando delante de CJ
    float q[3];
    GetPos(g_grabbed, q);
    float tx = pp[0] + f[0] * g_grabDist;
    float ty = pp[1] + f[1] * g_grabDist;
    float tz = pp[2] + f[2] * g_grabDist + 1.0f;
    float vx = (tx - q[0]) * 0.2f, vy = (ty - q[1]) * 0.2f, vz = (tz - q[2]) * 0.2f;
    float len = sqrtf(vx * vx + vy * vy + vz * vz);
    if (len > 0.8f) {
        float k = 0.8f / len;
        vx *= k; vy *= k; vz *= k;
    }
    float *ms = (float *)(g_grabbed + OFF_MOVESPEED);
    ms[0] = vx; ms[1] = vy; ms[2] = vz;
}

// ---------- Ola de agua ----------
static void WavePoolPush(DWORD poolAddr, int stride, DWORD player, bool isPed, float r) {
    DWORD objs; BYTE *map; int size;
    if (!PoolInfo(poolAddr, objs, map, size))
        return;
    DWORD playerVeh = *(DWORD *)(player + OFF_VEHICLE);
    for (int i = 0; i < size; i++) {
        if (map[i] & 0x80)
            continue;
        DWORD e = objs + (DWORD)i * stride;
        if (e == player || e == playerVeh || e == g_grabbed)
            continue;
        if (isPed && *(DWORD *)(e + OFF_VEHICLE))
            continue;
        float q[3];
        GetPos(e, q);
        float vx = q[0] - g_waveOrigin[0], vy = q[1] - g_waveOrigin[1], vz = q[2] - g_waveOrigin[2];
        if (vz < -5.0f || vz > 5.0f)
            continue;
        float dist = sqrtf(vx * vx + vy * vy);
        if (dist < 0.5f || fabsf(dist - r) > 3.5f)
            continue;
        if ((vx * g_waveDir[0] + vy * g_waveDir[1]) / dist < 0.45f)
            continue;
        float *ms = (float *)(e + OFF_MOVESPEED);
        if (ms[0] * g_waveDir[0] + ms[1] * g_waveDir[1] < 1.0f) {
            ms[0] += g_waveDir[0] * 0.10f;
            ms[1] += g_waveDir[1] * 0.10f;
            ms[2] += 0.03f;
        }
    }
}

static void StartWave(const float *pp) {
    float f[3];
    if (!CamForward(f))
        return;
    float len = sqrtf(f[0] * f[0] + f[1] * f[1]);
    if (len < 0.001f)
        return;
    g_waveDir[0] = f[0] / len; g_waveDir[1] = f[1] / len;
    g_waveOrigin[0] = pp[0]; g_waveOrigin[1] = pp[1]; g_waveOrigin[2] = pp[2];
    g_waveStart = GetTickCount();
    g_waveActive = true;
    g_nextWaveFx = 0;
}

static void UpdateWave(DWORD ped, DWORD now) {
    if (!g_waveActive)
        return;
    DWORD el = now - g_waveStart;
    if (el > 1800) {
        g_waveActive = false;
        return;
    }
    float r = 2.0f + (float)el * 0.016f;

    if ((int)(now - g_nextWaveFx) >= 0) {
        g_nextWaveFx = now + 120;
        for (int k = -1; k <= 1; k++) {
            float a = (float)k * 0.5f, c = cosf(a), s = sinf(a);
            float dx = g_waveDir[0] * c - g_waveDir[1] * s;
            float dy = g_waveDir[0] * s + g_waveDir[1] * c;
            PlayFx(g_fxSplash, g_waveOrigin[0] + dx * r, g_waveOrigin[1] + dy * r, g_waveOrigin[2] - 0.6f);
        }
    }

    WavePoolPush(ADDR_PED_POOL,     PED_STRIDE,     ped, true,  r);
    WavePoolPush(ADDR_VEHICLE_POOL, VEHICLE_STRIDE, ped, false, r);
    WavePoolPush(ADDR_OBJECT_POOL,  OBJECT_STRIDE,  ped, false, r);

    // La ola apaga las llamas del suelo que alcanza
    for (int i = 0; i < 60; i++) {
        BYTE *f = (BYTE *)(ADDR_FIRE_MANAGER + i * 0x28);
        if (!(f[0] & 1) || *(DWORD *)(f + 0x10) != 0)
            continue;
        float *pos = (float *)(f + 4);
        float vx = pos[0] - g_waveOrigin[0], vy = pos[1] - g_waveOrigin[1];
        float dist = sqrtf(vx * vx + vy * vy);
        if (fabsf(dist - r) < 3.5f && (vx * g_waveDir[0] + vy * g_waveDir[1]) / (dist + 0.001f) > 0.45f)
            ((Extinguish_t)ADDR_EXTINGUISH)((void *)f);
    }
}

// ---------- Bocina con impulso ----------
static void HornBoost(DWORD ped) {
    DWORD veh = *(DWORD *)(ped + OFF_VEHICLE);
    if (!veh || *(DWORD *)(veh + OFF_VEH_DRIVER) != ped)
        return;
    void *pad = ((void *(__cdecl *)(int))ADDR_GET_PAD)(0);
    if (!pad)
        return;
    unsigned char horn = ((unsigned char(__thiscall *)(void *))ADDR_GET_HORN)(pad);
    if (!horn)
        return;
    DWORD m = *(DWORD *)(veh + OFF_MATRIX);
    if (!m)
        return;
    float *fwd = (float *)(m + 0x10);
    float *ms = (float *)(veh + OFF_MOVESPEED);
    float along = ms[0] * fwd[0] + ms[1] * fwd[1] + ms[2] * fwd[2];
    if (along < HORN_MAX_SPEED) {
        ms[0] += fwd[0] * HORN_ACCEL;
        ms[1] += fwd[1] * HORN_ACCEL;
        ms[2] += fwd[2] * HORN_ACCEL;
    }
}

// ---------- Activar / desactivar trucos ----------
static void SetFire(DWORD ped, bool on, float health) {
    g_fire = on;
    Log("TEST: ONFIRE = %d", (int)on);
    g_nextFireTry = 0;
    DWORD *fire = (DWORD *)(ped + OFF_FIRE);
    if (on) {
        if (health > 0.0f && ENABLE_FLAMETHROWER) {
            g_hadFlame = HasWeapon(ped, WEAPON_FLAMETHROWER);
            if (ENABLE_PISTOL_ANIM)
                ApplyPistolAnim();
            GiveWeaponFull(ped, WEAPON_FLAMETHROWER);
        }
        ShowMessage("Truco activado: ONFIRE");
    } else {
        RestorePistolAnim();
        if (*fire)
            ((Extinguish_t)ADDR_EXTINGUISH)((void *)*fire);
        if (ENABLE_FLAMETHROWER && !g_hadFlame && HasWeapon(ped, WEAPON_FLAMETHROWER))
            ((void(__thiscall *)(void *, int))ADDR_CLEAR_WEAPON)((void *)ped, WEAPON_FLAMETHROWER);
        ShowMessage("Truco desactivado: ONFIRE");
    }
}

static void SetWater(DWORD ped, bool on, float health) {
    g_water = on;
    Log("TEST: ONWATER = %d", (int)on);
    if (on) {
        ResolveWaterFx();
        if (health > 0.0f && ENABLE_WATER_GUN) {
            g_hadExt = HasWeapon(ped, WEAPON_EXTINGUISHER);
            GiveWeaponFull(ped, WEAPON_EXTINGUISHER);
        }
        ShowMessage("Truco activado: ONWATER");
    } else {
        ReleaseGrab(false);
        g_waveActive = false;
        if (ENABLE_WATER_GUN && !g_hadExt && HasWeapon(ped, WEAPON_EXTINGUISHER))
            ((void(__thiscall *)(void *, int))ADDR_CLEAR_WEAPON)((void *)ped, WEAPON_EXTINGUISHER);
        ShowMessage("Truco desactivado: ONWATER");
    }
}

static void SetHorn(bool on) {
    g_horn = on;
    Log("TEST: CLAXON = %d", (int)on);
    ShowMessage(on ? "Truco activado: CLAXON" : "Truco desactivado: CLAXON");
}

static void Frame() {
    DWORD ped = *(DWORD *)ADDR_PLAYER_PED;
    if (!ped)
        return;
    float health = *(float *)(ped + OFF_HEALTH);
    BYTE *fl2 = (BYTE *)(ped + OFF_FLAGS2);
    DWORD *fire = (DWORD *)(ped + OFF_FIRE);
    DWORD now = GetTickCount();

    // ----- Interruptores -----
    if (InterlockedExchange(&g_reqFire, 0))
        SetFire(ped, !g_fire, health);
    if (InterlockedExchange(&g_reqWater, 0))
        SetWater(ped, !g_water, health);
    if (InterlockedExchange(&g_reqHorn, 0))
        SetHorn(!g_horn);

    // ----- Inmunidad (solo con ONFIRE) -----
    if (g_fire && !g_proofApplied) {
        g_oldProofBits = (BYTE)(*fl2 & PROOF_BITS);
        g_proofApplied = true;
    }
    if (!g_fire && g_proofApplied) {
        *fl2 = (BYTE)((*fl2 & ~PROOF_BITS) | g_oldProofBits);
        g_proofApplied = false;
    }
    if (g_fire)
        *fl2 |= PROOF_BITS;

    // ----- Bocina (puede usarse conduciendo, sin salir del vehiculo) -----
    if (g_horn && ENABLE_HORN_BOOST && health > 0.0f)
        HornBoost(ped);

    if (!g_fire && !g_water)
        return;

    // ----- Reaparecer: volver a dar las armas -----
    bool dead = (health <= 0.0f);
    if (g_wasDead && !dead) {
        if (g_fire && ENABLE_FLAMETHROWER) {
            g_hadFlame = false;
            GiveWeaponFull(ped, WEAPON_FLAMETHROWER);
        }
        if (g_water && ENABLE_WATER_GUN) {
            g_hadExt = false;
            GiveWeaponFull(ped, WEAPON_EXTINGUISHER);
        }
        g_nextFireTry = 0;
    }
    g_wasDead = dead;
    if (dead)
        return;

    float pos[3] = {0, 0, 0};
    GetPos(ped, pos);

    // =================== ONFIRE ===================
    if (g_fire) {
        if (!*fire && (int)(now - g_nextFireTry) >= 0) {
            g_nextFireTry = now + 500;
            *fl2 &= (BYTE)~FLAG_FIREPROOF;       // si es a prueba de fuego el juego no lo enciende
            IgniteBig(ped);
            *fl2 |= FLAG_FIREPROOF;
        }

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

        if (ENABLE_FLAMETHROWER) {
            if (HIDE_WEAPON_MODEL && HasWeapon(ped, WEAPON_FLAMETHROWER) &&
                *(BYTE *)(ped + OFF_CUR_SLOT) == (BYTE)WSlot(WEAPON_FLAMETHROWER))
                HideWeaponModel(ped);
            if (ENABLE_FIRE_PUSH && IsFiring(ped, WEAPON_FLAMETHROWER))
                PushCone(ped, pos, false, FIRE_PUSH_RANGE, 0.035f, 0.030f, 0.050f);
        }

        if (FAST_FLAME_DECAY && (int)(now - g_nextDecay) >= 0) {
            g_nextDecay = now + 100;
            DecayFires();
        }
    }

    // =================== ONWATER ===================
    if (g_water) {
        bool foc = Focused();

        if (ENABLE_WATER_GUN && IsFiring(ped, WEAPON_EXTINGUISHER)) {
            PushCone(ped, pos, true, WATER_PUSH_RANGE, 0.060f, 0.050f, 0.080f);
            float f[3];
            if (g_fxStream && (int)(now - g_nextStreamFx) >= 0 && CamForward(f)) {
                g_nextStreamFx = now + 100;
                for (int d = 3; d <= 9; d += 3)
                    PlayFx(g_fxStream, pos[0] + f[0] * d, pos[1] + f[1] * d, pos[2] + f[2] * d);
            }
        }

        if (ENABLE_GRAB)
            UpdateGrab(ped, pos, foc && Key(KEY_GRAB));

        if (ENABLE_WAVE) {
            bool k = foc && Key(KEY_WAVE);
            if (k && !g_waveKeyWas)
                StartWave(pos);
            g_waveKeyWas = k;
            UpdateWave(ped, now);
        }
    }

    // ----- Mantener municion infinita -----
    if ((int)(now - g_nextAmmo) >= 0) {
        g_nextAmmo = now + 1000;
        if (g_fire && ENABLE_FLAMETHROWER)
            KeepAmmo(ped, WEAPON_FLAMETHROWER);
        if (g_water && ENABLE_WATER_GUN)
            KeepAmmo(ped, WEAPON_EXTINGUISHER);
    }

    // ----- Contagio de fuego a los NPC (10 veces por segundo) -----
    if (g_fire && (int)(now - g_nextScan) >= 0) {
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
        Log("TEST: el byte en 0x%08X no es un CALL (0x%02X)", ADDR_GAME_PROCESS, p[0]);
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

static bool EndsWith(const std::string &s, const char *word) {
    size_t n = strlen(word);
    return s.size() >= n && s.compare(s.size() - n, n, word) == 0;
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

        if (EndsWith(typed, "ONFIRE")) {
            typed.clear();
            InterlockedExchange(&g_reqFire, 1);
        } else if (EndsWith(typed, "ONWATER")) {
            typed.clear();
            InterlockedExchange(&g_reqWater, 1);
        } else if (EndsWith(typed, "CLAXON")) {
            typed.clear();
            InterlockedExchange(&g_reqHorn, 1);
        }

        if (g_useThreadFrame)
            OnFrame();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Log("TEST v5: DLL cargada");

        DWORD sigUS = 0, sigEU = 0;
        bool okUS = ReadDword(0x82457C, sigUS);
        bool okEU = ReadDword(0x8245BC, sigEU);
        if (!((okUS && sigUS == 0x94BF) || (okEU && sigEU == 0x94BF))) {
            Log("TEST: version del juego NO reconocida, el mod se desactiva");
            return TRUE;
        }

        if (InstallHook()) {
            Log("TEST: enganchado al bucle del juego");
        } else {
            g_useThreadFrame = true;
            Log("TEST: no se pudo enganchar, se usa el hilo (modo de respaldo)");
        }
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
