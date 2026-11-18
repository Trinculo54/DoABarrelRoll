// Do A Barrel Roll: the flight model, kept apart from the game so tests/flight_test.cpp can check it.
//
// The game's world is left-handed with Y up; a car's orientation rows are its right, up and forward axes in the
// world. A spin w moves a point r of the car at w x r (the algebraic cross product), so w = forward x up lifts the
// nose and w = up x right rolls the roof to the right.
#pragma once

#include <cmath>

namespace flight {

struct V3 {
    float x, y, z;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// The controller as it's read: the left stick (-1..1, up and right positive), the triggers (0..1) and LB (the
// clutch, which does nothing in the air). Every other button is left to the game: RB changes its camera, Y rewinds.
struct Pad {
    float stick_x, stick_y, rt, lt;
    bool lb;
};

// What that asks of the car: the stick, thrust and air brake (0..1), whether the stick rolls (simple controls with
// LB held) and the rudder (-1..1, right positive; flight sim controls only).
struct Controls {
    float stick_x, stick_y, throttle, brake;
    bool roll;
    float rudder;
};

// The car's state, as the game keeps it: its axes, velocity (m/s) and spin (rad/s), all in the world.
struct Body {
    V3 right, up, forward;
    V3 vel, spin;
};

constexpr float kGravity     = 9.81f;
constexpr float kFlySpeed    = 40.0f;  // m/s (144 km/h): from here lift carries the car and the stick has full say
constexpr float kStickSpeed  = 25.0f;  // m/s (90 km/h): below this the car drives as without the mod
constexpr float kPitchRate   = 2.2f;   // rad/s at full stick: a loop in about 3 s
constexpr float kRollRate    = 3.8f;   // rad/s at full stick: a barrel roll in under 2 s
constexpr float kTurnRate    = 0.9f;   // simple controls: rad/s of turn at full stick (a full circle in 7 s)
constexpr float kBank        = 0.9f;   // simple controls: how far it banks into a turn at full stick (rad, about 50 degrees)
constexpr float kLevel       = 4.0f;   // simple controls: how quickly it rolls to that bank (1/s)
constexpr float kResponse    = 6.0f;   // how quickly the spin reaches what the stick asks (1/s)
constexpr float kFollowNose  = 2.5f;   // how quickly the velocity turns to where the nose points (1/s)
constexpr float kThrust      = 14.0f;  // m/s^2 at full right trigger
constexpr float kAirBrake    = 0.6f;   // fraction of speed lost per second at full left trigger
constexpr float kYawRate     = 0.8f;   // flight sim controls: rad/s of yaw at full rudder
constexpr float kLeverRate   = 1.0f;   // flight sim controls: the throttle lever's travel per second at a full trigger
constexpr float kTopSpeed    = 125.0f; // flight sim controls: m/s (450 km/h), where drag matches full throttle

// The controller turned into what it asks of the car.
// Simple controls: RT is thrust and LT the air brake while they're held, and LB makes the stick roll.
// Flight sim controls: RT and LT move the throttle `lever` up and down, and it stays where it's left (LT is the
// air brake too). With LB held the triggers are the rudder pedals instead, and the lever stays put.
inline Controls controls(const Pad& p, bool simple, float& lever, float dt) {
    if (simple) return {p.stick_x, p.stick_y, p.rt, p.lt, p.lb, 0};
    if (p.lb) return {p.stick_x, p.stick_y, lever, 0, false, p.rt - p.lt};
    lever = clamp01(lever + (p.rt - p.lt) * kLeverRate * dt);
    return {p.stick_x, p.stick_y, lever, p.lt, false, 0};
}

// The compass heading of the nose, 0 to 359 degrees.
// shortcut: takes the world's +z as north and +x as east, which hasn't been checked in game; swap here if it's off.
inline int heading(V3 forward) {
    const int deg = static_cast<int>(std::lround(std::atan2(forward.x, forward.z) * 57.29578f));
    return (deg % 360 + 360) % 360;
}

// One update of `dt` seconds: the new velocity and spin (the game moves the car with them).
// Flight sim controls: the stick pitches and rolls, the rudder yaws, and lift is along the car's roof, so a turn is
// a roll and a pull. Drag grows with speed, so each throttle setting has a speed it settles at.
// Simple controls: the stick pitches and turns; the car banks into the turn and levels out by itself, lift always
// points at the sky (a banked or upside-down car doesn't sink). With LB held the stick rolls it instead, for a
// barrel roll, and nothing levels it.
inline void step(Body& b, const Controls& c, float dt, bool simple) {
    // 0 below kStickSpeed (the car drives as without the mod), 1 from kFlySpeed: how much it flies.
    const float air = clamp01((len(b.vel) - kStickSpeed) / (kFlySpeed - kStickSpeed));

    // Spin: back on the stick (down) lifts the nose, as in a plane; left and right roll, or turn.
    const V3 sky{0, 1, 0};
    V3 want    = cross(b.forward, b.up) * (-c.stick_y * kPitchRate);
    float roll = c.stick_x * kRollRate;
    if (simple && !c.roll) {
        roll = 0;
        // Over the top of a loop (roof down, stick held) the car is left to finish it rather than rolled upright.
        const bool looping = b.up.y < 0 && std::fabs(c.stick_y) > 0.5f;
        if (!looping) {
            const float pi = 3.14159265f;
            float off      = c.stick_x * kBank - std::atan2(-b.right.y, b.up.y); // the bank wanted less the bank now
            off            = off > pi ? off - 2 * pi : off < -pi ? off + 2 * pi : off;
            const float r  = off * kLevel;
            // Nose straight up or down, there is no bank to speak of: fade the levelling out.
            roll = (r > kRollRate ? kRollRate : r < -kRollRate ? -kRollRate : r) * (1 - b.forward.y * b.forward.y);
        }
        want = want + sky * (c.stick_x * kTurnRate);
    }
    want   = want + cross(b.up, b.right) * roll;
    if (!simple) want = want + b.up * (c.rudder * kYawRate); // a spin about the roof swings the nose to the right
    b.spin = b.spin + (want - b.spin) * (clamp01(kResponse * dt) * air);

    // The velocity swings round to the nose, keeping its speed, so the car goes where it points.
    const float v = len(b.vel);
    if (v > 1.0f && dot(b.vel, b.forward) > 0) {
        const V3 turned = b.vel + (b.forward * v - b.vel) * clamp01(kFollowNose * air * dt);
        const float t   = len(turned);
        if (t > 1e-3f) b.vel = turned * (v / t);
    }

    // Lift, along the car's roof or at the sky: from kFlySpeed it cancels the game's gravity in level flight.
    b.vel = b.vel + (simple ? sky : b.up) * (kGravity * air * dt);

    b.vel = b.vel + b.forward * (kThrust * c.throttle * dt);
    if (!simple) b.vel = b.vel * (1.0f - clamp01(kThrust * len(b.vel) / (kTopSpeed * kTopSpeed) * air * dt));
    b.vel = b.vel * (1.0f - clamp01(kAirBrake * c.brake * air * dt)); // scaled by air: LT still reverses when slow
}

} // namespace flight
