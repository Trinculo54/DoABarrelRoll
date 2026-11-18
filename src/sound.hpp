// Do A Barrel Roll: the cockpit warning sounds, the parts with no Windows in them, so tests/flight_test.cpp can check
// them: which sounds there are, how a WAV file is read, and when each one plays. src/doabarrelroll.cpp plays them
// (XAudio2).
//
// The mod ships no sounds. Each one is a WAV file of the player's own in fh2-mods\DoABarrelRoll\sounds (a fighter's
// warning voice and tones, say), and a warning with no file is simply not sounded.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "flight.hpp"

namespace sound {

enum { kPullUp, kLowSpeed, kSounds };

struct Slot {
    const char* file;   // its name in the sounds folder
    bool loops;
    const char* what;   // for the folder's README
};
constexpr Slot kSlots[kSounds] = {
    {"pull_up.wav", false, "the warning voice: said while the car sinks faster than 30 m/s, again every 3 s"},
    {"low_speed.wav", true, "the low speed warning tone: looped while the car is too slow to fly (under 144 km/h) and sinking"},
};

constexpr float kPullUpSink  = -30;   // m/s: sinking faster than this says "pull up"
constexpr float kPullUpEvery = 3;     // seconds between two of them
constexpr float kStallSink   = -5;    // m/s: slow and sinking faster than this is a stall, not a drive down a hill
constexpr float kFade        = 6;     // the tone's volume changes by at most this a second, so it doesn't click

// Which warnings are due, for a car being flown (`active`: flight on and its update running) at `speed` m/s,
// climbing at `climb` m/s. The mod doesn't know how far the ground is, so both go by how fast the car is sinking.
struct Warnings {
    bool pull_up, low_speed;
};
inline Warnings warnings(bool active, float speed, float climb) {
    return {active && climb < kPullUpSink, active && speed < flight::kFlySpeed && climb < kStallSink};
}

// One step of `dt` seconds from `now` towards `target`, at most `rate` a second.
inline float towards(float now, float target, float rate, float dt) {
    const float step = rate * dt;
    return target > now + step ? now + step : target < now - step ? now - step : target;
}

// A WAV file's format and samples, pointing into the file's bytes.
struct Wav {
    const uint8_t* fmt  = nullptr;   // the fmt chunk: a WAVEFORMATEX without its last member, or all of one
    uint32_t fmt_size   = 0;
    const uint8_t* data = nullptr;
    uint32_t data_size  = 0;         // whole frames only
    uint16_t tag = 0, channels = 0, bits = 0, align = 0;
    uint32_t rate = 0;
};

// Reads a WAV file's chunks. False, with why, for anything that isn't plain PCM or float samples.
inline bool parse_wav(const uint8_t* p, std::size_t n, Wav& w, const char*& why) {
    const auto u16 = [&](std::size_t at) { return static_cast<uint16_t>(p[at] | p[at + 1] << 8); };
    const auto u32 = [&](std::size_t at) { return static_cast<uint32_t>(p[at] | p[at + 1] << 8 | p[at + 2] << 16) | static_cast<uint32_t>(p[at + 3]) << 24; };
    w = Wav{};
    if (n < 12 || std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) return why = "it isn't a WAV file", false;
    for (std::size_t at = 12; at + 8 <= n;) {
        const uint32_t size   = u32(at + 4);
        const std::size_t has = n - at - 8 < size ? n - at - 8 : size;   // a file cut short keeps what's there
        if (std::memcmp(p + at, "fmt ", 4) == 0 && has >= 16) {
            w.fmt = p + at + 8, w.fmt_size = static_cast<uint32_t>(has > 40 ? 40 : has);
            w.tag = u16(at + 8), w.channels = u16(at + 10), w.rate = u32(at + 12), w.align = u16(at + 20), w.bits = u16(at + 22);
        } else if (std::memcmp(p + at, "data", 4) == 0 && !w.data) {
            w.data = p + at + 8, w.data_size = static_cast<uint32_t>(has);
        }
        at += 8 + static_cast<std::size_t>(size) + (size & 1);
    }
    if (!w.fmt) return why = "it has no format chunk", false;
    if (w.tag != 1 && w.tag != 3 && w.tag != 0xFFFE) return why = "its samples are compressed: save it as plain PCM", false;
    if (w.channels < 1 || w.channels > 2) return why = "it has to be mono or stereo", false;
    if (w.rate < 8000 || w.rate > 192000) return why = "its sample rate isn't between 8 and 192 kHz", false;
    if ((w.bits != 8 && w.bits != 16 && w.bits != 24 && w.bits != 32) || w.align != w.channels * w.bits / 8) return why = "its sample size isn't 8, 16, 24 or 32 bits", false;
    w.data_size -= w.data ? w.data_size % w.align : 0;
    if (!w.data || !w.data_size) return why = "it has no samples", false;
    return true;
}

} // namespace sound
