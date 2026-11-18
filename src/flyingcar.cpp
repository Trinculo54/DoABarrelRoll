// Flying Car: the player's car flies like a plane while flight is on (F9, or LB+RB with d-pad up; both can be
// changed on the mod's page). src/flight.hpp has the flight model, README.md the controls.
//
// One hook, on ForzaCar's per-frame update (fh2game.h). After the game's own update of the player's car, the flight
// model reads the car's axes, velocity and spin from its CCarDynamics and hands the new velocity and spin back
// through the game's own setters, so the game's physics carries on from them. Every game address it uses is
// byte-checked first; on a mismatch the mod says so in its log and stays off.

#include <windows.h>
#include <xinput.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "fh2_version.h"
#include "fh2game.h"
#include "fh2mod.h"
#include "flight.hpp"

namespace {

// ---- game addresses (forza_durango_release_final.exe of 24 Nov 2015), as offsets from the game's base -------------

constexpr uintptr_t RVA_CAR_GET_DYN  = 0xF605D0;  // game+0x140F605D0: the car's CCarDynamics, car+0x4C08
constexpr uintptr_t RVA_DYN_VT_A     = 0x2EB4BB8; // game+0x142EB4BB8: CCarDynamics' vtable
constexpr uintptr_t RVA_DYN_VT_B     = 0x2EB5E18; // game+0x142EB5E18: its reference-counted form, same slots below
constexpr uintptr_t RVA_DYN_GET_VEL  = 0x12CDCF0; // game+0x1412CDCF0, slot 0x288: velocity at dyn-0x350
constexpr uintptr_t RVA_DYN_GET_SPIN = 0x12C5C90; // game+0x1412C5C90, slot 0x2B8: spin (world) at dyn-0x340
constexpr uintptr_t RVA_DYN_GET_ROWS = 0x12CBEB0; // game+0x1412CBEB0, slot 0x2E0: right, up, forward rows at dyn-0x380
constexpr uintptr_t RVA_DYN_SET_VEL  = 0x12E3CF0; // game+0x1412E3CF0, slot 0x290: SetVelocity(dyn, const float4*)
constexpr uintptr_t RVA_DYN_SET_SPIN = 0x12DD910; // game+0x1412DD910, slot 0x2C0: SetAngularVelocity(dyn, const float4*)
constexpr ptrdiff_t CAR_DYN = 0x4C08, CAR_KIND = 0x4D08;
constexpr ptrdiff_t DYN_ROWS = -0x380, DYN_VEL = -0x350, DYN_SPIN = -0x340;

#define CODE(rva, bytes, what) {rva, bytes, sizeof(bytes) - 1, what}
struct Code {
    uintptr_t rva;
    const char* bytes;
    size_t n;
    const char* what;
};
const Code kCode[] = {
    CODE(FH2_GAME_FORZACAR_KIND, FH2_GAME_FORZACAR_KIND_BYTES, "the car's kind"),
    CODE(RVA_CAR_GET_DYN, "\x48\x8B\x81\x08\x4C\x00\x00\xC3", "the car's dynamics"),
    CODE(RVA_DYN_GET_VEL, "\xC5\xF8\x28\x81\xB0\xFC\xFF\xFF", "the car's velocity"),
    CODE(RVA_DYN_GET_SPIN, "\xC5\xF8\x28\x81\xC0\xFC\xFF\xFF", "the car's spin"),
    CODE(RVA_DYN_GET_ROWS, "\x48\x8B\xC2\x48\x8D\x91\x80\xFC\xFF\xFF", "the car's orientation"),
    CODE(RVA_DYN_SET_VEL, "\xC5\xF8\x28\x02\xC5\xF8\x29\x81\xB0\xFC\xFF\xFF", "SetVelocity"),
    CODE(RVA_DYN_SET_SPIN, "\x48\x83\xEC\x38\xC5\xF8\x28\x02\x48\x8D\x91\xE0\xFC\xFF\xFF", "SetAngularVelocity"),
};
struct Slot {
    size_t offset;
    uintptr_t rva;
};
const Slot kSlots[] = {{0x288, RVA_DYN_GET_VEL}, {0x290, RVA_DYN_SET_VEL},   {0x2B8, RVA_DYN_GET_SPIN},
                       {0x2C0, RVA_DYN_SET_SPIN}, {0x2E0, RVA_DYN_GET_ROWS}};

using UpdateFn = uint64_t (*)(void* car, float dt, float b, uint64_t d); // rcx, xmm1, xmm2, r9
using SetFn    = void (*)(void* dyn, const float* v4);                    // reads 16 aligned bytes

// ---- state ----------------------------------------------------------------------------------------------------------

const FH2ModApi* g_api;
FH2Mod* g_mod;
char* g_base;
const void* g_vt_a;
const void* g_vt_b;
UpdateFn o_update;
std::atomic<bool> g_ready{false};  // the hook is in
std::atomic<bool> g_flying{false}; // flight is on
std::atomic<bool> g_fault{false};  // the flight model faulted once (caught): off for the session
std::atomic<bool> g_simple{true};  // simple controls (the stick turns) rather than a plane's (the stick rolls)
std::atomic<float> g_stick_x{0}, g_stick_y{0}, g_throttle{0}, g_brake{0};
std::atomic<bool> g_roll{false};   // LB is held: the stick rolls
wchar_t g_ini[MAX_PATH];
uint32_t g_vk, g_mods, g_pad; // the flight hotkey, as saved

void logf(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_api->log(g_mod, line);
}

void toast(const char* title, const char* subtitle) {
    if (g_api->api_version < 10) return;
    FH2Toast t{};
    t.size     = sizeof t;
    t.key      = "Flight";
    t.title    = title;
    t.subtitle = subtitle;
    t.expire_s = 10;
    g_api->show_toast(g_mod, &t);
}

void set_flying(bool on, const char* by) {
    if (on && !g_ready.load()) {
        logf("Flight can't start: %s", g_fault.load() ? "it was switched off after a fault this session."
                                                      : "the game's code isn't checked and hooked yet (see above).");
        g_api->set_setting(g_mod, "Flight", 0);
        return;
    }
    if (g_flying.exchange(on) == on) return;
    g_api->set_setting(g_mod, "Flight", on ? 1 : 0);
    logf("Flight is %s (%s).", on ? "on" : "off", by);
    if (on) toast("Flight on", "Over 144 km/h, pull back on the left stick");
    else toast("Flight off", "Back to driving");
}

// ---- the game side --------------------------------------------------------------------------------------------------

// The flight model on the player's car, after the game's own update. Kept out of line so the __try below covers every
// read here (clang only guards calls inside __try).
__attribute__((noinline)) void fly(void* car, float dt) {
    char* c = static_cast<char*>(car);
    if (*reinterpret_cast<const int32_t*>(c + CAR_KIND) != 0) return; // 0 = the local player's car
    dt = std::fabs(dt);
    if (!(dt > 0 && dt < 0.25f)) return; // paused, or a hitch: leave this one alone
    char* dyn = *reinterpret_cast<char**>(c + CAR_DYN);
    if (!dyn) return;
    const void* vt = *reinterpret_cast<const void* const*>(dyn);
    if (vt != g_vt_a && vt != g_vt_b) return;

    const float* r = reinterpret_cast<const float*>(dyn + DYN_ROWS);
    const float* v = reinterpret_cast<const float*>(dyn + DYN_VEL);
    const float* w = reinterpret_cast<const float*>(dyn + DYN_SPIN);
    flight::Body b{{r[0], r[1], r[2]}, {r[4], r[5], r[6]}, {r[8], r[9], r[10]}, {v[0], v[1], v[2]}, {w[0], w[1], w[2]}};
    const flight::Controls in{g_stick_x.load(std::memory_order_relaxed), g_stick_y.load(std::memory_order_relaxed),
                              g_throttle.load(std::memory_order_relaxed), g_brake.load(std::memory_order_relaxed),
                              g_roll.load(std::memory_order_relaxed)};
    flight::step(b, in, dt, g_simple.load(std::memory_order_relaxed));
    if (!std::isfinite(b.vel.x + b.vel.y + b.vel.z + b.spin.x + b.spin.y + b.spin.z)) return;

    alignas(16) const float nv[4] = {b.vel.x, b.vel.y, b.vel.z, v[3]};
    alignas(16) const float nw[4] = {b.spin.x, b.spin.y, b.spin.z, w[3]};
    reinterpret_cast<SetFn>(g_base + RVA_DYN_SET_VEL)(dyn, nv);
    reinterpret_cast<SetFn>(g_base + RVA_DYN_SET_SPIN)(dyn, nw);
}

void fly_guarded(void* car, float dt) {
    __try {
        fly(car, dt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_ready.store(false);
        g_flying.store(false);
        g_fault.store(true); // the worker logs it
    }
}

uint64_t d_update(void* car, float dt, float b, uint64_t d) {
    const uint64_t r = o_update(car, dt, b, d);
    if (g_flying.load(std::memory_order_relaxed)) fly_guarded(car, dt);
    return r;
}

bool code_ok() {
    bool ok = true;
    for (const Code& k : kCode)
        if (std::memcmp(g_base + k.rva, k.bytes, k.n) != 0) {
            logf("The game's code for %s (game+0x%llX) doesn't match this game version; Flying Car is off.", k.what,
                 0x140000000ull + k.rva);
            ok = false;
        }
    g_vt_a = g_base + RVA_DYN_VT_A;
    g_vt_b = g_base + RVA_DYN_VT_B;
    for (const void* vt : {g_vt_a, g_vt_b})
        for (const Slot& s : kSlots)
            if (*reinterpret_cast<char* const*>(static_cast<const char*>(vt) + s.offset) != g_base + s.rva) {
                logf("The car physics' table (game+0x%llX, slot 0x%zX) isn't as expected; Flying Car is off.",
                     0x140000000ull + (static_cast<const char*>(vt) - g_base), s.offset);
                ok = false;
            }
    return ok;
}

// ---- the controller ---------------------------------------------------------------------------------------------------

float axis(SHORT v) {
    const float dead = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    const float a    = std::fabs(static_cast<float>(v));
    if (a <= dead) return 0;
    const float s = (a - dead) / (32767.0f - dead);
    return std::copysign(s > 1 ? 1.0f : s, static_cast<float>(v));
}

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

// The game's code checks and the hook, then the controller read, ~120 times a second, for the rest of the session.
DWORD WINAPI worker(void*) {
    g_base = static_cast<char*>(g_api->wait_for_code(10 * 60 * 1000));
    if (!g_base) {
        logf("The game didn't get going within 10 minutes; Flying Car is off.");
        return 0;
    }
    if (!code_ok()) return 0;
    FH2Hook* h = g_api->hook_function(g_mod, "ForzaCar::Update", g_base + FH2_GAME_FORZACAR_UPDATE,
                                      reinterpret_cast<void*>(&d_update), reinterpret_cast<void**>(&o_update));
    if (!h || !g_api->enable_hooks(g_mod, h)) {
        logf("The hook on the car's update couldn't go in (the platform's log says why); Flying Car is off.");
        return 0;
    }
    g_ready.store(true);
    logf("Ready: the car's physics checked and hooked. Turn flight on with the Flight row or its hotkey.");

    HMODULE x = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!x) x = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto get = x ? reinterpret_cast<XInputGetStateFn>(GetProcAddress(x, "XInputGetState")) : nullptr;
    if (!get) logf("XInput isn't there, so the controller can't be read: flight would have no controls.");

    DWORD pad = 0, next_scan = 0;
    bool have = false, said_fault = false;
    for (;;) {
        Sleep(8);
        if (g_fault.load() && !said_fault) {
            said_fault = true;
            g_api->set_setting(g_mod, "Flight", 0);
            logf("The flight model faulted reading the car (caught); flight is off for the rest of the session.");
        }
        if (!get) continue;
        XINPUT_STATE st{};
        if (have && get(pad, &st) != ERROR_SUCCESS) have = false;
        if (!have && GetTickCount() - next_scan < 0x80000000u) { // a disconnected pad is slow to ask: once a second
            next_scan = GetTickCount() + 1000;
            for (DWORD i = 0; i < 4 && !have; ++i)
                if (get(i, &st) == ERROR_SUCCESS) pad = i, have = true;
        }
        if (!have) st = XINPUT_STATE{};
        g_stick_x.store(axis(st.Gamepad.sThumbLX), std::memory_order_relaxed);
        g_stick_y.store(axis(st.Gamepad.sThumbLY), std::memory_order_relaxed);
        g_throttle.store(st.Gamepad.bRightTrigger / 255.0f, std::memory_order_relaxed);
        g_brake.store(st.Gamepad.bLeftTrigger / 255.0f, std::memory_order_relaxed);
        g_roll.store((st.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0, std::memory_order_relaxed);
    }
}

// ---- settings ---------------------------------------------------------------------------------------------------------

void on_flight_row(void*, const char*, int value) { set_flying(value != 0, "the Flight row"); }
void on_hotkey(void*, const char*) { set_flying(!g_flying.load(), "the hotkey"); }

// Writes fh2-mods\FlyingCar.ini whole, from what the rows say now. On the UI thread (a row or a binding changed).
void save_ini() {
    if (FILE* f = _wfopen(g_ini, L"w")) {
        std::fprintf(f, "[Flight]\n; 1: simple controls (the stick turns the car; with LB held it rolls). 0: a plane's (the stick rolls)\n"
                        "Simple=%d\n\n"
                        "[Keys]\n; The key that turns flight on and off, as a Windows key code (120 = F9; 0 = none)\n"
                        "FlightKey=%u\n; Held with it: 1 Ctrl, 2 Shift, 4 Alt, added together\nFlightModifiers=%u\n"
                        "; The controller buttons, held together, as XInput bits (769 = LB+RB+d-pad up; 0 = none)\n"
                        "FlightPad=%u\n",
                     g_simple.load() ? 1 : 0, g_vk, g_mods, g_pad);
        std::fclose(f);
    }
}

void on_simple_row(void*, const char*, int value) {
    g_simple.store(value != 0);
    logf("Simple controls are %s.", value ? "on: the stick turns the car, and rolls it while LB is held" : "off: the stick rolls the car, as a plane's does");
    save_ini();
}

void on_rebind(void*, const char*, uint32_t vk, uint32_t mods, uint32_t pad) {
    g_vk = vk, g_mods = mods, g_pad = pad;
    save_ini();
}

// fh2-mods\FlyingCar.ini, beside this DLL.
void find_ini() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&find_ini), &self);
    DWORD n = GetModuleFileNameW(self, g_ini, MAX_PATH);
    while (n && g_ini[n - 1] != L'\\') --n;
    g_ini[n] = 0;
    wcsncat(g_ini, L"FlyingCar.ini", MAX_PATH - n - 1);
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api) return 0;
    g_api = api;
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Flying Car";
    info.version          = MOD_VERSION_STR;
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.description      = "Your car flies like a plane. Turn flight on with F9 or LB+RB+d-pad up, get above 144 km/h "
                            "and pull back on the left stick. Back climbs, forward dives, left and right turn. RT is "
                            "thrust, LT an air brake; hold LB and the stick rolls. Offline only.";
    info.offline_only     = 1;
    g_mod = api->register_mod(&info);
    if (!g_mod) return 0;
    if (api->api_version < 9) {
        api->log(g_mod, "Flying Car needs FH2 Mod Platform 2.8.0 or newer, so it's off.");
        return 0;
    }

    FH2Setting row{};
    row.size        = sizeof row;
    row.key         = "Flight";
    row.label       = "Flight";
    row.description = "On: the car flies once it's over 144 km/h. Left stick back climbs, forward dives; RT is thrust, "
                      "LT an air brake. Starts Off each time the game starts.";
    row.on_change   = &on_flight_row;
    api->add_setting(g_mod, &row);

    find_ini();
    g_simple.store(GetPrivateProfileIntW(L"Flight", L"Simple", 1, g_ini) != 0);
    FH2Setting simple{};
    simple.size        = sizeof simple;
    simple.key         = "Simple";
    simple.label       = "Simple controls";
    simple.description = "On: left and right on the stick turn the car. It banks and levels out by itself; hold LB and "
                         "the stick rolls it, for a barrel roll. Off: left and right roll the car, as a plane's stick "
                         "does, and you turn by rolling and pulling back.";
    simple.value       = g_simple.load() ? 1 : 0;
    simple.on_change   = &on_simple_row;
    api->add_setting(g_mod, &simple);

    g_vk   = GetPrivateProfileIntW(L"Keys", L"FlightKey", VK_F9, g_ini);
    g_mods = GetPrivateProfileIntW(L"Keys", L"FlightModifiers", 0, g_ini);
    g_pad  = GetPrivateProfileIntW(L"Keys", L"FlightPad", FH2_PAD_LB | FH2_PAD_RB | FH2_PAD_DPAD_UP, g_ini);
    FH2Hotkey hk{};
    hk.size      = sizeof hk;
    hk.key       = "Flight";
    hk.label     = "Flight on or off";
    hk.vk        = g_vk;
    hk.modifiers = g_mods;
    hk.pad_chord = g_pad;
    hk.on_press  = &on_hotkey;
    hk.on_rebind = &on_rebind;
    api->add_hotkey(g_mod, &hk);
    if (api->api_version >= 10) api->ui_uses(g_mod, FH2_UI_USES_TOASTS);

    if (HANDLE t = CreateThread(nullptr, 0, &worker, nullptr, 0, nullptr)) CloseHandle(t);
    else api->log(g_mod, "Flying Car couldn't start its thread, so it's off.");
    return 0;
}
