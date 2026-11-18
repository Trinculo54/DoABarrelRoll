// Do A Barrel Roll: the player's car flies like a plane while flight is on (F9, or LB with d-pad up; both can be
// changed on the mod's page). src/flight.hpp has the flight model, README.md the controls.
//
// The controller while flying. The mod reads the left stick, the triggers and LB (the game's steering, pedals and
// clutch, none of which matter in the air), and nothing else: RB stays the game's camera button, Y its rewind, the
// right stick its look-around and the d-pad its radio. D-pad up with LB held is kept from the game by a pad layer.
//
// One hook, on ForzaCar's per-frame update (fh2game.h). After the game's own update of the player's car, the flight
// model reads the car's axes, velocity and spin from its CCarDynamics and hands the new velocity and spin back
// through the game's own setters, so the game's physics carries on from them. Every game address it uses is
// byte-checked first; on a mismatch the mod says so in its log and stays off.

#include <windows.h>
#include <xaudio2.h>
#include <xinput.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "fh2_version.h"
#include "fh2game.h"
#include "fh2mod.h"
#include "fh2mod_hud.h"
#include "fh2mod_pad.h"
#include "flight.hpp"
#include "gui.hpp"
#include "sound.hpp"

namespace {

// ---- game addresses (forza_durango_release_final.exe of 24 Nov 2015), as offsets from the game's base -------------

constexpr uintptr_t RVA_CAR_GET_DYN  = 0xF605D0;  // game+0x140F605D0: the car's CCarDynamics, car+0x4C08
constexpr uintptr_t RVA_DYN_VT_A     = 0x2EB4BB8; // game+0x142EB4BB8: CCarDynamics' vtable
constexpr uintptr_t RVA_DYN_VT_B     = 0x2EB5E18; // game+0x142EB5E18: its reference-counted form, same slots below
constexpr uintptr_t RVA_DYN_GET_POS  = 0x12CD150; // game+0x1412CD150, slot 0x270: position at dyn-0x390
constexpr uintptr_t RVA_DYN_GET_VEL  = 0x12CDCF0; // game+0x1412CDCF0, slot 0x288: velocity at dyn-0x350
constexpr uintptr_t RVA_DYN_GET_SPIN = 0x12C5C90; // game+0x1412C5C90, slot 0x2B8: spin (world) at dyn-0x340
constexpr uintptr_t RVA_DYN_GET_ROWS = 0x12CBEB0; // game+0x1412CBEB0, slot 0x2E0: right, up, forward rows at dyn-0x380
constexpr uintptr_t RVA_DYN_SET_VEL  = 0x12E3CF0; // game+0x1412E3CF0, slot 0x290: SetVelocity(dyn, const float4*)
constexpr uintptr_t RVA_DYN_SET_SPIN = 0x12DD910; // game+0x1412DD910, slot 0x2C0: SetAngularVelocity(dyn, const float4*)
constexpr uintptr_t RVA_CAM_ROLLED   = 0x3986A0;  // game+0x1403986A0: ForzaMultiCam's check for a rolled-over car (its table's slot 80)
constexpr uintptr_t RVA_CAM_VT       = 0x2C6C840; // game+0x142C6C840: ForzaMultiCam's vtable
constexpr ptrdiff_t CAM_SAVED        = 0x2C8;     // the camera that check took the view from, -1 while it hasn't
constexpr uintptr_t RVA_SET_CAMERA   = 0xFE5780;  // game+0x140FE5780: CMultiCam::SetActiveCamera(int), for the camera probe
constexpr ptrdiff_t CAR_DYN = 0x4C08, CAR_KIND = 0x4D08;
constexpr ptrdiff_t DYN_POS = -0x390, DYN_ROWS = -0x380, DYN_VEL = -0x350, DYN_SPIN = -0x340;

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
    CODE(RVA_DYN_GET_POS, "\xC5\xF8\x28\x81\x70\xFC\xFF\xFF", "the car's position"),
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
const Slot kSlots[] = {{0x270, RVA_DYN_GET_POS},  {0x288, RVA_DYN_GET_VEL},  {0x290, RVA_DYN_SET_VEL},
                       {0x2B8, RVA_DYN_GET_SPIN}, {0x2C0, RVA_DYN_SET_SPIN}, {0x2E0, RVA_DYN_GET_ROWS}};

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
std::atomic<bool> g_simple{true};  // simple controls (the stick turns) rather than flight sim controls
std::atomic<float> g_stick_x{0}, g_stick_y{0}, g_rt{0}, g_lt{0}; // the controller, from the worker
std::atomic<bool> g_lb{false};
std::atomic<float> g_lever{0};     // flight sim controls: the throttle lever, 0 to 1
// For the flight HUD, from the game's thread: speed (m/s), height (m), heading (degrees), climb (m/s), thrust (0..1).
std::atomic<float> g_speed{0}, g_height{0}, g_climb{0}, g_thrust{0};
std::atomic<int> g_heading{0};
const FH2HudInterface* g_hud;      // null: no HUD layer on this platform, and no flight HUD
const FH2PadInterface* g_padlayer; // null: no pad layers, so the game sees d-pad up with LB too
bool g_canvas;                     // the platform took the flight instruments' canvas (HUD layer interface 3)
std::atomic<int> g_hud_mode{2};    // the Flight HUD row: 0 off, 1 labels, 2 instruments
int g_camera_probe;                 // [Debug] CameraProbe=1: log every change of the game's camera, and who asked
std::atomic<float> g_up_y{1};       // the car's roof against the sky, 1 upright and -1 on its roof (for the probe)
std::atomic<uint32_t> g_buttons{0}; // the controller's buttons, as XInput has them (for the probe)
std::atomic<DWORD> g_flown{0};      // GetTickCount at the car's last update with flight on: none for a while = paused
std::atomic<float> g_volume{0.6f};  // the Warning sounds row, 0 to 1
wchar_t g_sounds[MAX_PATH];         // fh2-mods\DoABarrelRoll\sounds\, beside this DLL
wchar_t g_ini[MAX_PATH];
uint32_t g_vk, g_mods, g_pad; // the flight hotkey, as saved
int g_check_updates = 1;      // CheckUpdates, as saved

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
    if (on) g_lever.store(0); // the throttle lever starts closed
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
    const flight::Pad pad{g_stick_x.load(std::memory_order_relaxed), g_stick_y.load(std::memory_order_relaxed),
                          g_rt.load(std::memory_order_relaxed), g_lt.load(std::memory_order_relaxed),
                          g_lb.load(std::memory_order_relaxed)};
    const bool simple = g_simple.load(std::memory_order_relaxed);
    float lever       = g_lever.load(std::memory_order_relaxed);
    const flight::Controls in = flight::controls(pad, simple, lever, dt);
    g_lever.store(lever, std::memory_order_relaxed);
    flight::step(b, in, dt, simple);
    if (!std::isfinite(b.vel.x + b.vel.y + b.vel.z + b.spin.x + b.spin.y + b.spin.z)) return;
    g_speed.store(flight::len(b.vel), std::memory_order_relaxed);
    const float height = reinterpret_cast<const float*>(dyn + DYN_POS)[1];
    g_height.store(height, std::memory_order_relaxed);
    if (g_canvas && g_hud_mode.load(std::memory_order_relaxed) == 2) {   // the instruments' numbers, fresh every update
        float n[gui::kNumbers];
        gui::numbers(gui::reading(b, height, in.throttle), n);
        g_hud->set_hud_canvas_numbers(g_mod, n, gui::kNumbers);
    }
    g_climb.store(v[1], std::memory_order_relaxed); // the game's own, before this update: gravity is already in it
    g_heading.store(flight::heading(b.forward), std::memory_order_relaxed);
    g_thrust.store(in.throttle, std::memory_order_relaxed);
    g_flown.store(GetTickCount(), std::memory_order_relaxed);
    g_up_y.store(b.up.y, std::memory_order_relaxed);

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

// ---- the hood and bumper cameras in a roll -----------------------------------------------------------------------

// The game's camera manager checks the car every frame (ForzaMultiCam, table slot 80). In the hood or bumper camera,
// once the car's roof points less than 0.1 towards the sky (it's on its side or further over, or nose straight up)
// the view is taken to the chase camera, and given back a second after the roof is above 0.5 again. That suits a
// car that has crashed onto its roof, and spoils a barrel roll or a loop. While flight is on the check is skipped,
// so the camera stays where the player put it; but if the check has already taken the view away it still runs,
// because all it does then is give the view back. The cockpit camera never had the rule.
using RolledFn = uint64_t (*)(void* multicam, float dt);
RolledFn o_rolled;

__attribute__((noinline)) bool view_taken(void* multicam) {
    return *reinterpret_cast<const int32_t*>(static_cast<const char*>(multicam) + CAM_SAVED) != -1;
}

uint64_t d_rolled(void* multicam, float dt) {
    if (g_flying.load(std::memory_order_relaxed)) {
        bool taken = true;   // if it can't be read, the game's own check runs
        __try {
            taken = view_taken(multicam);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        if (!taken) return 0;
    }
    return o_rolled(multicam, dt);
}

// The worker thread, after the flight hook. Without it flight is the same, and the game's rule stands.
void keep_camera_start() {
    const bool code = std::memcmp(g_base + RVA_CAM_ROLLED, "\x48\x8B\xC4\x41\x56\x48\x83\xEC\x70\x48\xC7\x40\xA8\xFE\xFF\xFF\xFF", 17) == 0;
    const bool slot = *reinterpret_cast<char* const*>(g_base + RVA_CAM_VT + 80 * 8) == g_base + RVA_CAM_ROLLED;
    FH2Hook* h = code && slot ? g_api->hook_function(g_mod, "ForzaMultiCam: rolled-over check", g_base + RVA_CAM_ROLLED,
                                                     reinterpret_cast<void*>(&d_rolled), reinterpret_cast<void**>(&o_rolled))
                              : nullptr;
    if (h && g_api->enable_hooks(g_mod, h)) return;
    logf("The game's check for a rolled-over car (game+0x%llX) %s, so in the hood and bumper cameras the game still switches to "
         "the chase camera in a roll; flight is the same.",
         0x140000000ull + RVA_CAM_ROLLED, code && slot ? "couldn't be hooked" : "isn't as expected");
}

// ---- the camera probe (only with CameraProbe=1 under [Debug] in the ini) -------------------------------------------

// Logs each change of the game's camera (CMultiCam::SetActiveCamera, hooked only while the probe is on) with what the car
// was doing and the game code the call came from: the return addresses on the stack that point into the game.
using SetCameraFn = uint64_t (*)(void* multicam, int index);
SetCameraFn o_set_camera;

uint64_t d_set_camera(void* multicam, int index) {
    static std::atomic<int> last{-1}, lines{0};
    const int was = last.exchange(index);
    if (was != index && lines.fetch_add(1) < 300) {
        static const char* const kNames[] = {"FollowCam", "FollowCam2", "DriverCam", "Hood", "BumperHigh"};
        const uintptr_t lo = reinterpret_cast<uintptr_t>(g_base) + 0x1000, hi = reinterpret_cast<uintptr_t>(g_base) + 0x1DB4000;
        const uintptr_t* sp = static_cast<const uintptr_t*>(__builtin_frame_address(0));
        char callers[8 * 20] = "";
        for (int i = 1, n = 0; i < 120 && n < 8; ++i)
            if (sp[i] >= lo && sp[i] < hi) {
                std::snprintf(callers + std::strlen(callers), 20, " game+0x%llX", 0x140000000ull + (sp[i] - reinterpret_cast<uintptr_t>(g_base)));
                ++n;
            }
        logf("Camera probe: camera %d -> %d (%s); flight %s, roof %+.2f, %d km/h, RB %s, game time %s; from%s", was, index,
             index >= 0 && index < 5 ? kNames[index] : index == 16 ? "PhotoMode" : index == 17 ? "RearView" : "another",
             g_flying.load() ? "on" : "off", static_cast<double>(g_up_y.load()), static_cast<int>(std::lround(g_speed.load() * 3.6f)),
             (g_buttons.load() & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? "held" : "up", GetTickCount() - g_flown.load() < 300 ? "running" : "stopped",
             callers);
    }
    return o_set_camera(multicam, index);
}

void camera_probe_start() {
    if (!g_camera_probe) return;
    if (std::memcmp(g_base + RVA_SET_CAMERA, "\x40\x53\x57\x41\x56\x48\x83\xEC\x20\x48\x8B\x81\x90\x01\x00\x00", 16) != 0) {
        logf("Camera probe: the game's code at game+0x%llX doesn't match, so the probe is off.", 0x140000000ull + RVA_SET_CAMERA);
        return;
    }
    FH2Hook* h = g_api->hook_function(g_mod, "CMultiCam::SetActiveCamera (camera probe)", g_base + RVA_SET_CAMERA,
                                      reinterpret_cast<void*>(&d_set_camera), reinterpret_cast<void**>(&o_set_camera));
    if (h && g_api->enable_hooks(g_mod, h)) logf("Camera probe: on (CameraProbe=1 in DoABarrelRoll.ini). Each change of the game's camera is logged.");
    else logf("Camera probe: its hook couldn't go in, so the probe is off.");
}

bool code_ok() {
    bool ok = true;
    for (const Code& k : kCode)
        if (std::memcmp(g_base + k.rva, k.bytes, k.n) != 0) {
            logf("The game's code for %s (game+0x%llX) doesn't match this game version; Do A Barrel Roll is off.", k.what,
                 0x140000000ull + k.rva);
            ok = false;
        }
    g_vt_a = g_base + RVA_DYN_VT_A;
    g_vt_b = g_base + RVA_DYN_VT_B;
    for (const void* vt : {g_vt_a, g_vt_b})
        for (const Slot& s : kSlots)
            if (*reinterpret_cast<char* const*>(static_cast<const char*>(vt) + s.offset) != g_base + s.rva) {
                logf("The car physics' table (game+0x%llX, slot 0x%zX) isn't as expected; Do A Barrel Roll is off.",
                     0x140000000ull + (static_cast<const char*>(vt) - g_base), s.offset);
                ok = false;
            }
    return ok;
}

// ---- the flight HUD ----------------------------------------------------------------------------------------------------

// The flight HUD while flight is on, as the Flight HUD row says. Instruments: the canvas of src/gui.hpp, whose
// numbers come straight from the game's thread (fly) and whose texts are set here. Labels: chips along the top of
// the screen, speed, height, heading and climb at the left and the throttle as five dots at the right. From the
// worker, five times a second (which also keeps the layer's lease).
void hud_tick() {
    static uint32_t shown;   // the parts last asked for
    if (!g_hud) return;
    const int mode      = g_flying.load() ? g_hud_mode.load() : 0;
    const uint32_t want = mode == 2 && g_canvas ? FH2_HUD_PART_CANVAS : mode ? FH2_HUD_PART_CHIPS : 0;
    const int hdg = g_heading.load(), kmh = static_cast<int>(std::lround(g_speed.load() * 3.6f));
    const int metres = static_cast<int>(std::lround(g_height.load())), climbing = static_cast<int>(std::lround(g_climb.load()));
    char spd[24], alt[24], dir[24], climb[24];
    if (want == FH2_HUD_PART_CANVAS) {
        std::snprintf(spd, sizeof spd, "%d", kmh);
        std::snprintf(alt, sizeof alt, "%d", metres);
        std::snprintf(climb, sizeof climb, "%+d M/S", climbing);
        std::snprintf(dir, sizeof dir, "%03d", hdg);
        g_hud->set_hud_canvas_text(g_mod, gui::kSpeedText, spd);
        g_hud->set_hud_canvas_text(g_mod, gui::kHeightText, alt);
        g_hud->set_hud_canvas_text(g_mod, gui::kClimbText, climb);
        g_hud->set_hud_canvas_text(g_mod, gui::kHeadingText, dir);
    } else if (want) {
        static const char* const kPoints[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        std::snprintf(spd, sizeof spd, "Spd %d km/h", kmh);
        std::snprintf(alt, sizeof alt, "Alt %d m", metres);
        std::snprintf(dir, sizeof dir, "Hdg %03d %s", hdg, kPoints[(hdg + 22) % 360 / 45]);
        std::snprintf(climb, sizeof climb, "Climb %+d m/s", climbing);
        FH2HudState s{};
        s.size               = sizeof s;
        s.chips[0].text      = spd;
        s.chips[1].text      = alt;
        s.chips[2].text      = dir;
        s.chips[3].text      = climb;
        s.corner.text        = "Throttle";
        s.corner.dots        = FH2_HUD_MAX_CHIP_DOTS;
        s.corner.dots_filled = static_cast<uint8_t>(std::lround(flight::clamp01(g_thrust.load()) * FH2_HUD_MAX_CHIP_DOTS));
        g_hud->set_hud_state(g_mod, &s);
    }
    // Asked for once each time it changes (flight on or off, or the row): if another mod's layer takes the screen
    // after that, it keeps it.
    if (want != shown && (g_hud->show_hud_layer(g_mod, want) || !want)) shown = want;
}

// ---- the warning sounds -----------------------------------------------------------------------------------------------

// The cockpit warnings of src/sound.hpp, played through XAudio2 on the worker thread: nothing of the game's own
// sound is touched. They are WAV files of the player's own, read once as the game starts. With none there, no
// sound engine is made at all.
struct Voice {
    std::vector<uint8_t> file;
    sound::Wav wav;
    IXAudio2SourceVoice* voice = nullptr;
    float volume  = 0;
    bool playing  = false, said = false;
};
Voice g_voice[sound::kSounds];
IXAudio2* g_xaudio;
IXAudio2MasteringVoice* g_master;

bool submit(Voice& v, bool loop) {
    XAUDIO2_BUFFER b{};
    b.Flags      = XAUDIO2_END_OF_STREAM;
    b.AudioBytes = v.wav.data_size;
    b.pAudioData = v.wav.data;
    b.LoopCount  = loop ? XAUDIO2_LOOP_INFINITE : 0;
    return SUCCEEDED(v.voice->SubmitSourceBuffer(&b));
}

// The folder (made, with a README that lists the files, the first time), the files, and the sound engine if any
// file is there. The worker thread, once.
void sounds_start() {
    CreateDirectoryW(std::wstring(g_sounds, wcslen(g_sounds) - 8).c_str(), nullptr);   // ...\DoABarrelRoll
    CreateDirectoryW(g_sounds, nullptr);
    const std::wstring readme = std::wstring(g_sounds) + L"README.txt";
    if (GetFileAttributesW(readme.c_str()) == INVALID_FILE_ATTRIBUTES)
        if (FILE* f = _wfopen(readme.c_str(), L"w")) {
            std::fprintf(f, "Do A Barrel Roll: warning sounds\n\nThe mod comes with no sounds. Put WAV files of your own in this folder (a fighter's warning voice\n"
                            "and tones, say), with these names, and they sound while flight is on. A warning with no file here is\n"
                            "simply not sounded. They're read when the game starts.\n\n");
            for (const sound::Slot& s : sound::kSlots) std::fprintf(f, "  %-15s %s\n", s.file, s.what);
            std::fprintf(f, "\nAny WAV with plain (PCM or float) samples works, mono or stereo. The tone is looped, so it should start\n"
                            "and end at the same point of the sound, or it clicks each time round. The mod doesn't know how far the\n"
                            "ground is, so both warnings go by how fast the car is sinking. How loud they are is the Warning sounds\n"
                            "row on the mod's page (Pause > MODS > YOUR MODS > Do A Barrel Roll).\n");
            std::fclose(f);
        }
    int loaded = 0;
    for (int i = 0; i < sound::kSounds; ++i) {
        Voice& v = g_voice[i];
        wchar_t name[32];
        MultiByteToWideChar(CP_UTF8, 0, sound::kSlots[i].file, -1, name, 32);
        FILE* f = _wfopen((std::wstring(g_sounds) + name).c_str(), L"rb");
        if (!f) continue;
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        const char* why = "it's empty, or over 64 MB";
        if (size > 0 && size <= 64 * 1024 * 1024) {
            v.file.resize(static_cast<size_t>(size));
            if (std::fread(v.file.data(), 1, v.file.size(), f) != v.file.size()) why = "it couldn't be read";
            else if (sound::parse_wav(v.file.data(), v.file.size(), v.wav, why)) why = nullptr;
        }
        std::fclose(f);
        if (why) {
            logf("Warning sounds: %s isn't used: %s.", sound::kSlots[i].file, why);
            v.file.clear();
        } else {
            ++loaded;
        }
    }
    if (!loaded) {
        logf("Warning sounds: no WAV files in fh2-mods\\DoABarrelRoll\\sounds, so there are none. README.txt there lists the files it plays.");
        return;
    }
    using CreateFn = HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
    HMODULE x = LoadLibraryExW(L"xaudio2_9.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!x) x = LoadLibraryExW(L"xaudio2_8.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    const auto create = x ? reinterpret_cast<CreateFn>(GetProcAddress(x, "XAudio2Create")) : nullptr;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (!create || FAILED(create(&g_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR)) || FAILED(g_xaudio->CreateMasteringVoice(&g_master))) {
        logf("Warning sounds: Windows' sound engine (XAudio2) couldn't be started, so there are none.");
        g_xaudio = nullptr;
        return;
    }
    std::string names;
    for (int i = 0; i < sound::kSounds; ++i) {
        Voice& v = g_voice[i];
        if (v.file.empty()) continue;
        alignas(8) uint8_t fmt[40] = {};   // a WAVEFORMATEXTENSIBLE's size: a plain PCM chunk leaves cbSize 0
        std::memcpy(fmt, v.wav.fmt, v.wav.fmt_size);
        if (FAILED(g_xaudio->CreateSourceVoice(&v.voice, reinterpret_cast<const WAVEFORMATEX*>(fmt))) || (sound::kSlots[i].loops && !submit(v, true))) {
            logf("Warning sounds: %s isn't used: the sound engine didn't take its format.", sound::kSlots[i].file);
            v.voice = nullptr;
            continue;
        }
        v.voice->SetVolume(0);
        names += (names.empty() ? "" : ", ") + std::string(sound::kSlots[i].file);
    }
    logf("Warning sounds: %s.", names.empty() ? "none of the files could be used" : ("ready: " + names).c_str());
}

void play_once(int which) {
    Voice& v = g_voice[which];
    if (!v.voice) return;
    v.voice->Stop();
    v.voice->FlushSourceBuffers();
    if (!submit(v, false)) return;
    v.voice->SetVolume(1);
    v.voice->Start();
    if (!v.said) logf("Warning sounds: %s sounded (said once a session).", sound::kSlots[which].file);
    v.said = true;
}

// The looped tone at `volume`, stopped while it's silent.
void play_loop(int which, float volume) {
    Voice& v = g_voice[which];
    if (!v.voice) return;
    v.voice->SetVolume(volume);
    if (volume > 0 && !v.playing) {
        v.voice->Start();
        if (!v.said) logf("Warning sounds: %s sounded (said once a session).", sound::kSlots[which].file);
        v.said = true;
    }
    if (volume <= 0 && v.playing) v.voice->Stop();
    v.playing = volume > 0;
}

// About 30 times a second on the worker thread. A warning only sounds while the car is being flown: flight on and
// its update running, so not while the game is paused or in a menu.
void sounds_tick(float dt) {
    if (!g_xaudio) return;
    static float tone, since_pull_up = sound::kPullUpEvery;
    const bool active       = g_flying.load() && GetTickCount() - g_flown.load() < 300;
    const sound::Warnings w = sound::warnings(active, g_speed.load(), g_climb.load());
    g_master->SetVolume(g_volume.load());
    tone = sound::towards(tone, w.low_speed ? 1.0f : 0.0f, sound::kFade, dt);
    play_loop(sound::kLowSpeed, tone);
    since_pull_up += dt;
    if (w.pull_up && since_pull_up >= sound::kPullUpEvery) {
        since_pull_up = 0;
        play_once(sound::kPullUp);
    }
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
        logf("The game didn't get going within 10 minutes; Do A Barrel Roll is off.");
        return 0;
    }
    if (!code_ok()) return 0;
    FH2Hook* h = g_api->hook_function(g_mod, "ForzaCar::Update", g_base + FH2_GAME_FORZACAR_UPDATE,
                                      reinterpret_cast<void*>(&d_update), reinterpret_cast<void**>(&o_update));
    if (!h || !g_api->enable_hooks(g_mod, h)) {
        logf("The hook on the car's update couldn't go in (the platform's log says why); Do A Barrel Roll is off.");
        return 0;
    }
    g_ready.store(true);
    keep_camera_start();
    camera_probe_start();
    logf("Ready: the car's physics checked and hooked. Turn flight on with the Flight row or its hotkey.");

    HMODULE x = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!x) x = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto get = x ? reinterpret_cast<XInputGetStateFn>(GetProcAddress(x, "XInputGetState")) : nullptr;
    if (!get) logf("XInput isn't there, so the controller can't be read: flight would have no controls.");

    sounds_start();
    DWORD pad = 0, next_scan = 0, tick = 0, sounds_at = GetTickCount();
    bool have = false, said_fault = false;
    for (;;) {
        Sleep(8);
        if (++tick % 25 == 0) hud_tick();
        if (const DWORD now = GetTickCount(); now - sounds_at >= 30) {
            sounds_tick(static_cast<float>(now - sounds_at) / 1000.0f);
            sounds_at = now;
        }
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
        g_buttons.store(st.Gamepad.wButtons, std::memory_order_relaxed);
        g_stick_x.store(axis(st.Gamepad.sThumbLX), std::memory_order_relaxed);
        g_stick_y.store(axis(st.Gamepad.sThumbLY), std::memory_order_relaxed);
        g_rt.store(st.Gamepad.bRightTrigger / 255.0f, std::memory_order_relaxed);
        g_lt.store(st.Gamepad.bLeftTrigger / 255.0f, std::memory_order_relaxed);
        g_lb.store((st.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0, std::memory_order_relaxed);
    }
}

// ---- settings ---------------------------------------------------------------------------------------------------------

void on_flight_row(void*, const char*, int value) { set_flying(value != 0, "the Flight row"); }
// The hotkey only counts in the driving view: in a menu LB and d-pad up are the menu's own (tab left, move up), and
// held together there they would turn flight on and off behind it. Where the platform can't tell (no pop-ups this
// session, or before 2.9.0) every press counts, as its pad layer docs advise.
void on_hotkey(void*, const char*) {
    if (g_api->api_version >= 10) {
        const uint32_t ui = g_api->ui_status(g_mod);
        if ((ui & FH2_UI_TOASTS) && !(ui & FH2_UI_ON_SCREEN)) return;
    }
    set_flying(!g_flying.load(), "the hotkey");
}

// Writes fh2-mods\DoABarrelRoll.ini whole, from what the rows say now. On the UI thread (a row or a binding changed).
void save_ini() {
    if (FILE* f = _wfopen(g_ini, L"w")) {
        std::fprintf(f, "[Flight]\n; 1: simple controls (the stick turns the car; with LB held it rolls). 0: flight sim controls\n"
                        "; (the stick rolls, the triggers move the throttle lever, and with LB held they're the rudder)\n"
                        "Simple=%d\n; The flight HUD while flying. 0: none. 1: labels along the top of the screen. 2: flight instruments\n"
                        "; (an artificial horizon, speed and height tapes, a compass; they need FH2 Mod Platform 2.9.1)\n"
                        "Hud=%d\n\n"
                        "[Sounds]\n; How loud the warning sounds are, 0 to 100. They're WAV files of your own in fh2-mods\\DoABarrelRoll\\sounds\n"
                        "Volume=%d\n\n"
                        "[Keys]\n; The key that turns flight on and off, as a Windows key code (120 = F9; 0 = none)\n"
                        "FlightKey=%u\n; Held with it: 1 Ctrl, 2 Shift, 4 Alt, added together\nFlightModifiers=%u\n"
                        "; The controller buttons, held together, as XInput bits (257 = LB+d-pad up; 0 = none)\n"
                        "FlightPad=%u\n\n"
                        "[Updates]\n; 0: don't check GitHub for a newer version when the game starts\nCheckUpdates=%d\n\n"
                        "[Debug]\n; 1: log each change of the game's camera and the game code that asked for it (for finding a fault)\n"
                        "CameraProbe=%d\n",
                     g_simple.load() ? 1 : 0, g_hud_mode.load(), static_cast<int>(std::lround(g_volume.load() * 100)), g_vk, g_mods, g_pad, g_check_updates, g_camera_probe);
        std::fclose(f);
    }
}

void on_simple_row(void*, const char*, int value) {
    g_simple.store(value != 0);
    logf("Simple controls are %s.", value ? "on: the stick turns the car, and rolls it while LB is held"
                                          : "off: flight sim controls, with a throttle lever and a rudder");
    save_ini();
}

void on_volume(void*, const char*, float value) {
    g_volume.store(flight::clamp01(value / 100));
    save_ini();
}

const char* const kHudChoices[] = {"Off", "Labels", "Instruments"};

void on_hud_row(void*, const char*, int value) {
    if (value < 0 || value > (g_canvas ? 2 : 1)) return;
    g_hud_mode.store(value);
    logf("The flight HUD is %s.", value == 2 ? "the instruments" : value ? "the labels" : "off");
    save_ini();
}

// The default buttons are LB with d-pad up. A pad layer keeps that d-pad up from the game while LB is held (the
// hotkey itself still fires: the platform reads the controller for it); with other buttons the layer is off.
constexpr uint32_t kDefaultPad = FH2_PAD_LB | FH2_PAD_DPAD_UP;
void on_layer_press(void*, const char*, uint32_t) {}

void on_rebind(void*, const char*, uint32_t vk, uint32_t mods, uint32_t pad) {
    g_vk = vk, g_mods = mods, g_pad = pad;
    if (g_padlayer) g_padlayer->set_modifier(g_mod, "Flight", pad == kDefaultPad ? FH2_PAD_LB : 0);
    save_ini();
}

// fh2-mods\DoABarrelRoll.ini, beside this DLL.
void find_ini() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&find_ini), &self);
    DWORD n = GetModuleFileNameW(self, g_ini, MAX_PATH);
    while (n && g_ini[n - 1] != L'\\') --n;
    g_ini[n] = 0;
    wcsncpy(g_sounds, g_ini, MAX_PATH);
    wcsncat(g_ini, L"DoABarrelRoll.ini", MAX_PATH - n - 1);
    wcsncat(g_sounds, L"DoABarrelRoll\\sounds\\", MAX_PATH - n - 1);
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api) return 0;
    g_api = api;
    find_ini();
    g_check_updates = GetPrivateProfileIntW(L"Updates", L"CheckUpdates", 1, g_ini) != 0;
    g_camera_probe  = GetPrivateProfileIntW(L"Debug", L"CameraProbe", 0, g_ini) != 0;
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Do A Barrel Roll";
    info.version          = MOD_VERSION_STR;
    info.github_repo      = "Trinculo54/DoABarrelRoll";
    info.check_updates    = g_check_updates;
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.description      = "Your car flies like a plane. Turn flight on with F9 or LB+d-pad up, get above 144 km/h "
                            "and pull back on the left stick. Back climbs, forward dives, left and right turn. RT is "
                            "thrust, LT an air brake; hold LB and the stick rolls. Turn Simple controls off for "
                            "flight sim controls. Offline only.";
    info.offline_only     = 1;
    g_mod = api->register_mod(&info);
    if (!g_mod) return 0;
    if (api->api_version < 9) {
        api->log(g_mod, "Do A Barrel Roll needs FH2 Mod Platform 2.8.0 or newer, so it's off.");
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

    g_simple.store(GetPrivateProfileIntW(L"Flight", L"Simple", 1, g_ini) != 0);
    FH2Setting simple{};
    simple.size        = sizeof simple;
    simple.key         = "Simple";
    simple.label       = "Simple controls";
    simple.description = "On: left and right on the stick turn the car. It banks and levels out by itself; hold LB and "
                         "the stick rolls it, for a barrel roll. Off: flight sim controls. The stick pitches and rolls, "
                         "RT and LT move a throttle that stays where you leave it (LT is the air brake too), and with "
                         "LB held RT and LT are the rudder.";
    simple.value       = g_simple.load() ? 1 : 0;
    simple.on_change   = &on_simple_row;
    api->add_setting(g_mod, &simple);

    g_volume.store(flight::clamp01(static_cast<float>(GetPrivateProfileIntW(L"Sounds", L"Volume", 60, g_ini)) / 100));
    FH2Setting vol{};
    vol.size         = sizeof vol;
    vol.key          = "Volume";
    vol.label        = "Warning sounds";
    vol.description  = "How loud the cockpit warnings are: a voice when you're sinking fast, a tone when you're too slow to "
                       "fly. They're WAV files of your own in fh2-mods\\DoABarrelRoll\\sounds (README.txt there lists them); "
                       "none come with the mod.";
    vol.kind         = FH2_SETTING_SLIDER;
    vol.min          = 0;
    vol.max          = 100;
    vol.step         = 5;
    vol.slider_value = g_volume.load() * 100;
    vol.on_slide     = &on_volume;
    api->add_setting(g_mod, &vol);

    g_vk   = GetPrivateProfileIntW(L"Keys", L"FlightKey", VK_F9, g_ini);
    g_mods = GetPrivateProfileIntW(L"Keys", L"FlightModifiers", 0, g_ini);
    g_pad  = GetPrivateProfileIntW(L"Keys", L"FlightPad", kDefaultPad, g_ini);
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

    // Both are preview interfaces of platform 2.9.0; without them the mod flies the same, with no HUD of its own
    // and with d-pad up reaching the game.
    g_padlayer = static_cast<const FH2PadInterface*>(api->get_interface(FH2_PAD_INTERFACE, FH2_PAD_INTERFACE_VERSION));
    if (g_padlayer) {
        FH2PadLayer layer{sizeof layer};
        layer.key      = "Flight";
        layer.modifier = g_pad == kDefaultPad ? FH2_PAD_LB : 0;
        layer.buttons  = FH2_PAD_DPAD_UP;
        layer.on_press = &on_layer_press;
        if (!g_padlayer->add_layer(g_mod, &layer)) g_padlayer = nullptr;
    }
    // The labels need the layer of platform 2.9.0 (interface 2); the instruments its canvas, from 2.9.1 (interface 3).
    g_hud = static_cast<const FH2HudInterface*>(api->get_interface(FH2_HUD_INTERFACE, 2));
    if (g_hud && g_hud->size >= offsetof(FH2HudInterface, add_hud_canvas)) {
        const bool canvas = g_hud->version >= 3 && g_hud->size >= sizeof(FH2HudInterface);
        FH2HudLayer layer{};
        layer.size     = sizeof layer;
        layer.parts    = FH2_HUD_PART_CHIPS | (canvas ? FH2_HUD_PART_CANVAS : 0);
        layer.lease_ms = 2000;
        if (!g_hud->add_hud_layer(g_mod, &layer)) {
            g_hud = nullptr;
        } else if (canvas) {
            const std::string xaml = gui::xaml();
            FH2HudCanvas c{};
            c.size    = sizeof c;
            c.xaml    = xaml.c_str();
            c.numbers = gui::kNumbers;
            c.texts   = gui::kTexts;
            g_canvas  = g_hud->add_hud_canvas(g_mod, &c) != 0;
        } else {
            api->log(g_mod, "The flight instruments need FH2 Mod Platform 2.9.1; on this one the flight HUD is its labels.");
        }
    } else {
        g_hud = nullptr;
    }
    if (!g_hud) api->log(g_mod, "This platform has no HUD layer for the mod, so there's no flight HUD; flying is the same.");
    const int hud_saved = GetPrivateProfileIntW(L"Flight", L"Hud", 2, g_ini);
    g_hud_mode.store(hud_saved <= 0 ? 0 : hud_saved == 1 || !g_canvas ? 1 : 2);
    if (g_hud) {
        FH2Setting hud{};
        hud.size         = sizeof hud;
        hud.key          = "Hud";
        hud.label        = "Flight HUD";
        hud.description  = g_canvas ? "What shows while flight is on. Instruments: an artificial horizon, speed and height tapes, "
                                      "a compass, the climb and the throttle. Labels: the same readings as plain numbers along "
                                      "the top of the screen."
                                    : "Labels: while flight is on, the top of the screen shows your speed, height, compass heading "
                                      "and climb, and the throttle as five dots.";
        hud.kind         = FH2_SETTING_CHOICE;
        hud.choices      = kHudChoices;
        hud.choice_count = g_canvas ? 3 : 2;
        hud.value        = g_hud_mode.load();
        hud.on_change    = &on_hud_row;
        api->add_setting(g_mod, &hud);
    }

    if (HANDLE t = CreateThread(nullptr, 0, &worker, nullptr, 0, nullptr)) CloseHandle(t);
    else api->log(g_mod, "Do A Barrel Roll couldn't start its thread, so it's off.");
    return 0;
}
