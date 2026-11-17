// Flying Car: the flight model, kept apart from the game so tests/flight_test.cpp can check it.
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

// What the player is doing: the left stick (-1..1, up and right positive) and the triggers (0..1).
struct Controls {
    float stick_x, stick_y, throttle, brake;
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
constexpr float kResponse    = 6.0f;   // how quickly the spin reaches what the stick asks (1/s)
constexpr float kFollowNose  = 2.5f;   // how quickly the velocity turns to where the nose points (1/s)
constexpr float kThrust      = 14.0f;  // m/s^2 at full right trigger
constexpr float kAirBrake    = 0.6f;   // fraction of speed lost per second at full left trigger

// One update of `dt` seconds: the new velocity and spin (the game moves the car with them).
inline void step(Body& b, const Controls& c, float dt) {
    // 0 below kStickSpeed (the car drives as without the mod), 1 from kFlySpeed: how much it flies.
    const float air = clamp01((len(b.vel) - kStickSpeed) / (kFlySpeed - kStickSpeed));

    // Spin: back on the stick (down) lifts the nose, as in a plane; left and right roll.
    const V3 want = cross(b.forward, b.up) * (-c.stick_y * kPitchRate) + cross(b.up, b.right) * (c.stick_x * kRollRate);
    b.spin        = b.spin + (want - b.spin) * (clamp01(kResponse * dt) * air);

    // The velocity swings round to the nose, keeping its speed, so the car goes where it points.
    const float v = len(b.vel);
    if (v > 1.0f && dot(b.vel, b.forward) > 0) {
        const V3 turned = b.vel + (b.forward * v - b.vel) * clamp01(kFollowNose * air * dt);
        const float t   = len(turned);
        if (t > 1e-3f) b.vel = turned * (v / t);
    }

    // Lift along the car's roof: from kFlySpeed it cancels the game's gravity in level flight.
    b.vel = b.vel + b.up * (kGravity * air * dt);

    b.vel = b.vel + b.forward * (kThrust * c.throttle * dt);
    b.vel = b.vel * (1.0f - clamp01(kAirBrake * c.brake * air * dt)); // scaled by air: LT still reverses when slow
}

} // namespace flight
