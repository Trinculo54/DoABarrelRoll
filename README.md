# Flying Car

A mod for Forza Horizon 2 on PC (Azimuth) that lets your car take off and fly like a plane. It needs the
[FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform) 2.8.0 or newer (2.9.0 or newer for its pop-ups).

## Flying

1. Turn flight on: **F9** on the keyboard, or **LB + RB + d-pad up** on the controller. The **Flight** row on the
   mod's page (Pause > MODS > YOUR MODS > Flying Car) does the same. A pop-up says when it's on or off.
2. Get going. Below 90 km/h the car drives as usual. From there it starts to get light, and from 144 km/h its
   "wings" hold it up.
3. Pull back on the left stick to take off.

| Control | In the air |
|---|---|
| Left stick back (down) | Nose up: climb, and hold it for a loop |
| Left stick forward (up) | Nose down: dive |
| Left stick left or right | Turn. The car banks into the turn and levels out when you let go |
| LB held, left stick left or right | Roll: keep it there for a barrel roll. Let go of LB and the car levels out |
| RT | Thrust |
| LT | Air brake |

Up is down and down is up, as in a plane. Let go of the stick and the car flies straight and level, the right
way up. Slow down below 144 km/h and the car starts to sink; below 90 km/h it falls. Turn flight off and it
drops like a car again.

**Simple controls** (a row on the mod's page, On by default) are the ones above. Turn the row Off for a plane's
controls: left and right on the stick roll the car instead, nothing levels it for you, and you turn by rolling
and pulling back.

Flight starts Off each time the game starts. It only ever moves your own car, and it's offline only: the
platform turns it off when you go online.

## Settings

On the mod's page: the **Flight** row, the **Simple controls** row, and under "Keys and buttons" the flight key
and controller buttons. Simple controls and the key and buttons are saved in `fh2-mods\FlyingCar.ini`.

## Known limits

- It reads the controller with XInput. The keyboard can turn flight on and off, but can't fly the car.
- LB is the game's clutch. That does nothing in the air, but if you drive manual with clutch, holding LB on the
  ground with flight on still works the clutch.
- Pulling back near the ground at speed can tip the car over before it leaves the ground: lift off with a
  gentle pull.

## How it works

One hook, on the game's per-frame update of each car. After the game has updated your car, the mod reads the
car's axes, velocity and spin, works out the new velocity and spin (`src/flight.hpp`) and hands them back through
the game's own setters, so the game's physics carries on from them. Every game address it uses is checked first;
if this isn't the game version it expects, it says so in the platform's log and stays off.

## Building

With llvm-mingw (clang) and Ninja:

```
git clone --recurse-submodules <this repo>
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DFLYINGCAR_TESTS=ON
cmake --build build
build\flight_test.exe
```

`build\FlyingCar.dll` goes in the game's `fh2-mods` folder.
