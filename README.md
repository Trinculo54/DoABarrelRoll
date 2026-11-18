# Do A Barrel Roll

A mod for Forza Horizon 2 on PC (Azimuth) that lets your car take off and fly like a plane: climb, dive, turn,
loop the loop and, of course, do a barrel roll. Offline only. The game's files are never touched.

It's built on the [FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform), which comes in the zip.

## Install

1. Close Azimuth.
2. Download the zip from the [latest release](https://github.com/Trinculo54/DoABarrelRoll/releases/latest) and
   extract it into your Azimuth folder (the one with `azimuth.exe`). You should end up with:

   ```
   d3dcompiler_47.dll            next to azimuth.exe
   DWrite.dll                    next to azimuth.exe
   fh2-mods\DoABarrelRoll.dll
   ```
3. Start the game.

`d3dcompiler_47.dll` and `DWrite.dll` are the same file: the FH2 Mod Platform 2.9.0. Azimuth 1.0.3 and newer load
`DWrite.dll`, older versions `d3dcompiler_47.dll`. Every mod's zip ships the platform it was built with, so if
you use other FH2 mods, extract the zip with the newest platform last.

## Flying

1. Turn flight on: **F9** on the keyboard, or **LB + RB + d-pad up** on the controller. The **Flight** row on the
   mod's page (Pause > MODS > YOUR MODS > Do A Barrel Roll) does the same. A pop-up says when it's on or off.
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

Flight starts Off each time the game starts. It only ever moves your own car, never traffic or Drivatars.

## Settings

On the mod's page (Pause > MODS > YOUR MODS > Do A Barrel Roll):

| Row | What it does |
|---|---|
| Flight | Turns flight on and off, like the hotkey. Starts Off each time the game starts |
| Simple controls | On (the default): the controls in the table above. Off: a plane's controls. Left and right on the stick roll the car, nothing levels it for you, and you turn by rolling and pulling back |
| Keys and buttons | The key and the controller buttons that turn flight on and off |

They're saved in `fh2-mods\DoABarrelRoll.ini`, which is written the first time you change one. Each key has a
comment. `CheckUpdates=0` there stops the check for a newer version on GitHub when the game starts.

## Good to know

- **Offline only.** When the game is started from FH2MP's launcher, the platform doesn't load the mod.
- **Controller only in the air.** The keyboard can turn flight on and off, but flying needs a controller.
- **LB is the game's clutch.** That does nothing in the air, but if you drive manual with clutch, holding LB on
  the ground with flight on still works the clutch.
- **Rewind isn't handled.** Flight carries on through the game's rewind; turn flight off first if it gets odd.
- Pulling back hard near the ground at speed can tip the car over before it leaves the ground: lift off with a
  gentle pull.
- It's made for the 24 November 2015 game exe that Azimuth runs. It checks the game's code first, and on
  anything else it stays off.

## Troubleshooting

The mod logs to `fh2-mods\platform.log`, as lines starting `[Do A Barrel Roll]`. A working launch has
"Ready: the car's physics checked and hooked.", and each switch adds "Flight is on" or "Flight is off".

- **No `[Do A Barrel Roll]` line at all:** the platform isn't loading the mod. Check that `d3dcompiler_47.dll`
  and `DWrite.dll` are both next to `azimuth.exe` and `DoABarrelRoll.dll` is in `fh2-mods`.
- **"needs FH2 Mod Platform 2.8.0 or newer":** another mod's zip put an older platform back. Extract this zip
  again.
- **"doesn't match this game version" or "couldn't go in":** the game exe isn't the one the mod was made for,
  or another mod changed that code first. The platform's lines just above say which.
- **No pop-up when flight goes on:** pop-ups need platform 2.9.0, and follow the game's notification option and
  the "Pop-ups" row on the mod's page. Flight works without them.

If the game crashes, the platform saves a report in `fh2-mods\crashes`. Please attach it and `platform.log` to
an [issue](https://github.com/Trinculo54/DoABarrelRoll/issues).

## Uninstall

Delete `fh2-mods\DoABarrelRoll.dll` and `fh2-mods\DoABarrelRoll.ini`. If you don't use any other FH2 mods, delete
`d3dcompiler_47.dll`, `DWrite.dll` and the `fh2-mods` folder too. Nothing else was changed.

## How it works

One hook, on the game's per-frame update of each car. After the game has updated your car, the mod reads the
car's axes, velocity and spin, works out the new velocity and spin (`src/flight.hpp`) and hands them back through
the game's own setters, so the game's physics carries on from them. Every game address it uses is checked first.
A fault while reading the car is caught and turns flight off for the session.

## Building

With [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang) and Ninja; MSVC isn't supported.

```
git clone --recurse-submodules https://github.com/Trinculo54/DoABarrelRoll.git
cd DoABarrelRoll
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DDOABARRELROLL_TESTS=ON
cmake --build build
build\flight_test.exe
```

`build\DoABarrelRoll.dll` is the mod, and `build\platform\` has the platform DLLs. On Linux, `scripts/build.sh`
builds everything and stages the release zip's contents in `dist/`, as the GitHub build does on every push.
Pushing a tag like `v1.0.0` (it has to match `VERSION` in `CMakeLists.txt`) also makes a draft release with the
zip attached.

## License

Released under the [GNU General Public License v3.0](LICENSE). The FH2 Mod Platform includes
[MinHook](https://github.com/TsudaKageyu/minhook), whose licence ships in the zip as `LICENSE-MinHook.txt`.

Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
