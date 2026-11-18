// The flight model's self-check: each case prints its name, and the run fails on the first one that doesn't hold.
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

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
    step(slow, Controls{1, -1, 0, 1, false}, dt, false);
    check(len(slow.spin) == 0 && slow.vel.y == 0 && slow.vel.z == 20, "below 90 km/h the stick and LT change nothing");

    Body pull = level(50);
    for (int i = 0; i < 30; ++i) step(pull, Controls{0, -1, 0, 0, false}, dt, false);
    check(dot(cross(pull.spin, pull.forward), pull.up) > 1.5f, "stick back (down) lifts the nose");

    Body push = level(50);
    for (int i = 0; i < 30; ++i) step(push, Controls{0, 1, 0, 0, false}, dt, false);
    check(dot(cross(push.spin, push.forward), push.up) < -1.5f, "stick forward (up) drops the nose");

    Body roll = level(50);
    for (int i = 0; i < 30; ++i) step(roll, Controls{1, 0, 0, 0, false}, dt, false);
    check(dot(cross(roll.spin, roll.up), roll.right) > 3.0f, "stick right rolls the roof to the right");

    Body flat = level(50);
    flat.spin = {0.5f, 0.2f, -0.3f};
    for (int i = 0; i < 120; ++i) step(flat, Controls{0, 0, 0, 0, false}, dt, false);
    check(len(flat.spin) < 0.01f, "a centred stick steadies the car");

    Body lift = level(50);
    step(lift, Controls{0, 0, 0, 0, false}, dt, false);
    check(std::fabs(lift.vel.y - kGravity * dt) < 1e-3f, "from 144 km/h lift balances gravity in level flight");

    Body upside = level(50);
    upside.right = {-1, 0, 0};
    upside.up    = {0, -1, 0};
    for (int i = 0; i < 30; ++i) step(upside, Controls{0, -1, 0, 0, false}, dt, false);
    check(dot(cross(upside.spin, upside.forward), upside.up) > 1.5f, "upside down, stick back still pulls towards the roof");

    Body nose = level(50);
    nose.forward = {0, 0.5f, 0.8660254f}; // pointing 30 degrees up, still moving level
    nose.up      = {0, 0.8660254f, -0.5f};
    const float before = dot(nose.vel, nose.forward) / len(nose.vel);
    step(nose, Controls{0, 0, 0, 0, false}, dt, false);
    check(dot(nose.vel, nose.forward) / len(nose.vel) > before, "the velocity swings round to where the nose points");

    Body thrust = level(50);
    step(thrust, Controls{0, 0, 1, 0, false}, dt, false);
    check(thrust.vel.z > 50.15f, "full throttle pushes the car forward");

    Body brake = level(50);
    step(brake, Controls{0, 0, 0, 1, false}, dt, false);
    check(brake.vel.z < 49.6f, "LT slows the car in the air");

    // ---- simple controls ----
    Body turn = level(50);
    for (int i = 0; i < 30; ++i) step(turn, Controls{1, 0, 0, 0, false}, dt, true);
    check(dot(cross(turn.spin, turn.forward), turn.right) > 0.7f && dot(cross(turn.spin, turn.up), turn.right) > 1.0f,
          "simple: stick right turns the nose right and banks into the turn");

    Body banked = level(50); // rolled 30 degrees to the right, stick centred
    banked.right = {0.8660254f, -0.5f, 0};
    banked.up    = {0.5f, 0.8660254f, 0};
    Body held    = banked;
    for (int i = 0; i < 30; ++i) step(banked, Controls{0, 0, 0, 0, false}, dt, true);
    check(dot(cross(banked.spin, banked.up), banked.right) < -1.0f, "simple: a centred stick rolls the car level");
    step(held, Controls{0, 0, 0, 0, false}, dt, true);
    check(std::fabs(held.vel.y - kGravity * dt) < 1e-4f && std::fabs(held.vel.x) < 1e-4f, "simple: a banked car doesn't sink");

    Body over = level(50); // upside down
    over.right = {-1, 0, 0};
    over.up    = {0, -1, 0};
    Body loop  = over;
    for (int i = 0; i < 30; ++i) step(over, Controls{0, 0, 0, 0, false}, dt, true);
    check(std::fabs(dot(cross(over.spin, over.up), over.right)) > 3.0f,
          "simple: upside down with a centred stick, it rolls upright");
    for (int i = 0; i < 30; ++i) step(loop, Controls{0, -1, 0, 0, false}, dt, true);
    check(std::fabs(dot(cross(loop.spin, loop.up), loop.right)) < 0.01f && dot(cross(loop.spin, loop.forward), loop.up) > 1.5f,
          "simple: over the top of a loop with the stick held back, it keeps looping");

    Body barrel = level(50);
    for (int i = 0; i < 30; ++i) step(barrel, Controls{1, 0, 0, 0, true}, dt, true);
    check(dot(cross(barrel.spin, barrel.up), barrel.right) > 3.0f && std::fabs(dot(cross(barrel.spin, barrel.forward), barrel.right)) < 0.01f,
          "simple: with LB held, stick right rolls right for a barrel roll and doesn't turn");

    Body knife = level(50); // on its side, LB held, stick centred
    knife.right = {0, -1, 0};
    knife.up    = {1, 0, 0};
    for (int i = 0; i < 30; ++i) step(knife, Controls{0, 0, 0, 0, true}, dt, true);
    check(len(knife.spin) < 0.01f, "simple: with LB held and the stick centred, nothing levels the car");

    Body up = level(50); // nose straight up
    up.forward = {0, 1, 0};
    up.up      = {0, 0, -1};
    up.vel     = {0, 50, 0};
    step(up, Controls{0, 0, 0, 0, false}, dt, true);
    check(std::isfinite(up.spin.x + up.spin.y + up.spin.z) && len(up.spin) < 0.01f,
          "simple: nose straight up, no levelling twitch");

    // ---- flight sim controls ----
    float lever  = 0;
    Controls up1 = controls(Pad{0, 0, 1, 0, false}, false, lever, 0.5f);
    check(std::fabs(lever - 0.5f) < 1e-4f && up1.throttle == lever && up1.rudder == 0, "sim: RT pushes the throttle lever up");
    Controls stay = controls(Pad{0, 0, 0, 0, false}, false, lever, 5.0f);
    check(std::fabs(stay.throttle - 0.5f) < 1e-4f && stay.brake == 0, "sim: with the triggers let go, the lever stays where it was");
    Controls down = controls(Pad{0, 0, 0, 1, false}, false, lever, 0.25f);
    check(std::fabs(lever - 0.25f) < 1e-4f && down.brake == 1, "sim: LT pulls the lever down and opens the air brake");
    Controls ped = controls(Pad{0.3f, 0, 1, 0, true}, false, lever, 1.0f);
    check(ped.rudder == 1 && std::fabs(lever - 0.25f) < 1e-4f && ped.brake == 0 && !ped.roll && ped.stick_x == 0.3f,
          "sim: with LB held the triggers are the rudder, and the lever and the stick's roll are as they were");
    check(controls(Pad{0, 0, 0, 1, true}, false, lever, 1.0f).rudder == -1, "sim: LB with LT is left rudder");
    float unused  = 0.7f;
    Controls easy = controls(Pad{0, 0, 0.4f, 0.2f, true}, true, unused, 1.0f);
    check(easy.throttle == 0.4f && easy.brake == 0.2f && easy.roll && easy.rudder == 0 && unused == 0.7f,
          "simple: the triggers are thrust and air brake, LB is the roll button, and there's no lever");

    Body yaw = level(50);
    for (int i = 0; i < 30; ++i) step(yaw, Controls{0, 0, 0, 0, false, 1}, dt, false);
    check(dot(cross(yaw.spin, yaw.forward), yaw.right) > 0.6f && std::fabs(dot(cross(yaw.spin, yaw.up), yaw.right)) < 0.01f,
          "sim: right rudder swings the nose right and doesn't roll");
    Body noyaw = level(50);
    for (int i = 0; i < 30; ++i) step(noyaw, Controls{0, 0, 0, 0, false, 1}, dt, true);
    check(len(noyaw.spin) < 0.01f, "simple: there's no rudder");

    // Two minutes at one throttle setting, with the game's gravity taken off each step, as the game does.
    for (float throttle : {1.0f, 0.5f}) {
        Body cruise = level(50);
        for (int i = 0; i < 120 * 60; ++i) {
            step(cruise, Controls{0, 0, throttle, 0, false, 0}, dt, false);
            cruise.vel.y -= kGravity * dt;
        }
        check(std::fabs(len(cruise.vel) - kTopSpeed * std::sqrt(throttle)) < 1.0f && std::fabs(cruise.vel.y) < 0.01f,
              throttle == 1 ? "sim: full throttle settles at 450 km/h in level flight" : "sim: half throttle settles at about 318 km/h");
    }
    Body coast = level(20);
    step(coast, Controls{0, 0, 0, 0, false, 0}, dt, false);
    check(coast.vel.z == 20, "sim: below 90 km/h there's no drag, so the car drives as without the mod");

    check(heading({0, 0, 1}) == 0 && heading({1, 0, 0}) == 90 && heading({0, 0, -1}) == 180 && heading({-1, 0, 0}) == 270 &&
              heading({-0.01f, 0.9f, 1}) == 359,
          "the heading is 0 along +z, 90 along +x, and never 360");

    std::printf(g_failed ? "FAILED\n" : "all passed\n");
    return g_failed;
}
