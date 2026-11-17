// The flight model's self-check: each case prints its name, and the run fails on the first one that doesn't hold.
#include <cstdio>
#include <cstdlib>

#include "flight.hpp"

using namespace flight;

namespace {

int g_failed;
void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_failed = 1;
}

// A level car pointing along +z (right +x, up +y), at `speed` m/s.
Body level(float speed) { return Body{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, speed}, {0, 0, 0}}; }

} // namespace

int main() {
    const float dt = 1.0f / 60;

    Body slow = level(20);
    step(slow, Controls{1, -1, 0, 1}, dt);
    check(len(slow.spin) == 0 && slow.vel.y == 0 && slow.vel.z == 20, "below 90 km/h the stick and LT change nothing");

    Body pull = level(50);
    for (int i = 0; i < 30; ++i) step(pull, Controls{0, -1, 0, 0}, dt);
    check(dot(cross(pull.spin, pull.forward), pull.up) > 1.5f, "stick back (down) lifts the nose");

    Body push = level(50);
    for (int i = 0; i < 30; ++i) step(push, Controls{0, 1, 0, 0}, dt);
    check(dot(cross(push.spin, push.forward), push.up) < -1.5f, "stick forward (up) drops the nose");

    Body roll = level(50);
    for (int i = 0; i < 30; ++i) step(roll, Controls{1, 0, 0, 0}, dt);
    check(dot(cross(roll.spin, roll.up), roll.right) > 3.0f, "stick right rolls the roof to the right");

    Body flat = level(50);
    flat.spin = {0.5f, 0.2f, -0.3f};
    for (int i = 0; i < 120; ++i) step(flat, Controls{0, 0, 0, 0}, dt);
    check(len(flat.spin) < 0.01f, "a centred stick steadies the car");

    Body lift = level(50);
    step(lift, Controls{0, 0, 0, 0}, dt);
    check(std::fabs(lift.vel.y - kGravity * dt) < 1e-4f, "from 144 km/h lift balances gravity in level flight");

    Body upside = level(50);
    upside.right = {-1, 0, 0};
    upside.up    = {0, -1, 0};
    for (int i = 0; i < 30; ++i) step(upside, Controls{0, -1, 0, 0}, dt);
    check(dot(cross(upside.spin, upside.forward), upside.up) > 1.5f, "upside down, stick back still pulls towards the roof");

    Body nose = level(50);
    nose.forward = {0, 0.5f, 0.8660254f}; // pointing 30 degrees up, still moving level
    nose.up      = {0, 0.8660254f, -0.5f};
    const float before = dot(nose.vel, nose.forward) / len(nose.vel);
    step(nose, Controls{0, 0, 0, 0}, dt);
    check(dot(nose.vel, nose.forward) / len(nose.vel) > before, "the velocity swings round to where the nose points");

    Body thrust = level(50);
    step(thrust, Controls{0, 0, 1, 0}, dt);
    check(thrust.vel.z > 50.2f, "RT pushes the car forward");

    Body brake = level(50);
    step(brake, Controls{0, 0, 0, 1}, dt);
    check(brake.vel.z < 49.6f, "LT slows the car in the air");

    std::printf(g_failed ? "FAILED\n" : "all passed\n");
    return g_failed;
}
