Do A Barrel Roll
================

Your car takes off and flies like a plane in Forza Horizon 2 (Azimuth):
climb, dive, turn, loop the loop and do a barrel roll. Offline only. The
game's files are never touched.

Project page and source: https://github.com/Trinculo54/DoABarrelRoll


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth (from Azimuth 1.0.3, the
    platform is the DWrite.dll in this zip). Extra Options isn't needed.
  - A controller, to fly.


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       DWrite.dll                    next to azimuth.exe
       fh2-mods\DoABarrelRoll.dll
3. Start the game.

d3dcompiler_47.dll and DWrite.dll are the same file: the FH2 Mod
Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/fh2-mod-platform). Azimuth 1.0.3 loads
DWrite.dll, older versions d3dcompiler_47.dll. It loads every mod in
fh2-mods. Each mod's zip ships the platform it was built with, so extract
the zip with the newest platform last.

This zip has platform 2.9.0, and Do A Barrel Roll runs on 2.8.0 or newer
(its pop-ups and its flight HUD need 2.9.0). If platform.log says it "needs FH2 Mod Platform
2.8.0 or newer", another mod's zip put an older platform back: extract
this zip again.


Flying
~~~~~~

1. Turn flight on: F9 on the keyboard, or LB + d-pad up on the
   controller. The Flight row on the mod's page (Pause > MODS > YOUR MODS
   > Do A Barrel Roll) does the same. A pop-up says when it's on or off.
2. Get going. Below 90 km/h the car drives as usual. From there it starts
   to get light, and from 144 km/h its "wings" hold it up.
3. Pull back on the left stick to take off.

  Left stick back (down)       nose up: climb, and hold it for a loop
  Left stick forward (up)      nose down: dive
  Left stick left or right     turn; the car banks into the turn and
                               levels out when you let go
  LB held, stick left/right    roll: keep it there for a barrel roll.
                               Let go of LB and the car levels out
  RT                           thrust
  LT                           air brake

Up is down and down is up, as in a plane. Let go of the stick and the car
flies straight and level, the right way up. Slow down below 144 km/h and
it starts to sink; below 90 km/h it falls. Turn flight off and it drops
like a car again.

Flight starts Off each time the game starts. It only ever moves your own
car, never traffic or Drivatars.


Flight sim controls
~~~~~~~~~~~~~~~~~~~

Turn "Simple controls" off on the mod's page and the car flies with a
plane's full set of controls. Nothing levels it for you, and its lift
points out of its roof, so you turn by rolling and pulling back.

  Left stick back / forward    elevator: nose up / nose down
  Left stick left or right     ailerons: roll
  RT                           throttle up; let go and it stays
  LT                           throttle down, and the air brake while
                               it's held
  LB held, RT / LT             rudder: nose right / nose left; the
                               throttle stays as it was

The throttle starts closed each time flight goes on. Each setting has a
speed the car settles at: about 450 km/h in level flight at full
throttle. With the throttle closed the car glides, slows and comes down.


The flight HUD
~~~~~~~~~~~~~~

While flight is on, the screen has flight instruments, drawn in the
game's own HUD in the green of a real head-up display:

  - an artificial horizon round the middle of the screen. The long line
    is the horizon, the short lines are every 5 degrees of pitch, and
    the two bars and the dot in the middle are your car. It turns
    against your bank and slides with your pitch, as a plane's does.
    The marks along its top are the bank scale, with a pointer under it;
  - a speed tape on its left, in km/h, with the throttle as a bar
    beside it;
  - a height tape on its right, in the game world's own metres, with
    the climb in metres a second under it;
  - a compass tape above it, with your heading in degrees.

The "Flight HUD" row on the mod's page picks what shows: Instruments,
Labels (the same readings as plain numbers along the top of the screen)
or Off.


Warning sounds
~~~~~~~~~~~~~~

The mod can sound a cockpit's warnings while you fly, from sound files of
your own: it comes with none. Put WAV files with these names in
fh2-mods\DoABarrelRoll\sounds (the folder is made the first time the game
starts with the mod, with a README.txt that lists them):

  pull_up.wav      a warning voice: while the car sinks faster than
                   30 m/s, again every 3 seconds
  low_speed.wav    a warning tone, looped: while the car is too slow to
                   fly (under 144 km/h) and sinking

Any WAV with plain (PCM or float) samples works, mono or stereo. They're
read when the game starts, and play only while flight is on and the game
isn't paused. The mod doesn't know how far below the ground is, so both
warnings go by how fast the car is sinking. The "Warning sounds" row on
the mod's page sets how loud they are.


What each button does while flying
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The mod only reads the controls the game doesn't need in the air:

  Left stick        flies the car (the game: steering)
  RT / LT           thrust or throttle / air brake (the game: pedals)
  LB                held: the stick rolls (simple controls), or RT and
                    LT are the rudder (flight sim controls). The game:
                    the clutch, only with manual with clutch
  LB + d-pad up     flight on or off; the game doesn't see this d-pad up

Everything else is left to the game: RB still changes the camera, the
right stick still looks around, Y still rewinds, and A, B, X, the d-pad,
the stick clicks, View and Menu do what they always do.


Settings
~~~~~~~~

On the mod's page (Pause > MODS > YOUR MODS > Do A Barrel Roll):

  Flight              turns flight on and off, like the hotkey
  Simple controls     On (the default): the stick turns the car and it
                      levels itself. Off: flight sim controls
  Flight HUD          what shows while flight is on: Instruments (the
                      default), Labels or Off
  Warning sounds      how loud the warning sounds are, 0 (off) to 100
  Keys and buttons    the key and the controller buttons that turn
                      flight on and off. With other buttons than
                      LB + d-pad up the game sees them too: avoid RB
                      (the camera) and Y (rewind)

They're saved in fh2-mods\DoABarrelRoll.ini, which is written the first
time you change one, with a comment next to each setting:

  Simple=1            0 = flight sim controls
  Hud=2               the flight HUD: 0 none, 1 labels, 2 instruments
  Volume=60           the warning sounds, 0 to 100
  FlightKey=120       the key, as a Windows key code (120 = F9)
  FlightModifiers=0   held with it: 1 Ctrl, 2 Shift, 4 Alt, added up
  FlightPad=257       the controller buttons (257 = LB+d-pad up)
  CheckUpdates=1      0 = don't check GitHub for a newer version

When a newer version is out, the title screen says so after
"PRESS (A) TO START".


Good to know
~~~~~~~~~~~~

  - Offline only. When the game is started from FH2MP's launcher, the
    platform doesn't load the mod.
  - The keyboard can turn flight on and off, but flying needs a
    controller.
  - The key and the buttons for flight only work while you're driving,
    not in a menu, the map or photo mode. The Flight row works anywhere.
  - The hood and bumper cameras stay through a roll. The game takes
    those two views to the chase camera whenever a car goes onto its
    side or roof; with flight on the mod skips that, so a barrel roll
    or a loop keeps the view you chose.
  - LB is the game's clutch. That does nothing in the air, but if you
    drive manual with clutch, holding LB on the ground with flight on
    still works the clutch.
  - The triggers are still the car's pedals. In the air the wheels just
    spin or stop; on the ground with flight on, RT also drives the car
    and LT also brakes it.
  - If another mod puts its own bar or labels on the screen, the flight
    HUD makes way until flight is next turned on.
  - Rewind isn't handled: flight carries on through the game's rewind.
    Turn flight off first if it gets odd.
  - Pulling back hard near the ground at speed can tip the car over
    before it leaves the ground: lift off with a gentle pull.
  - It's made for the 24 November 2015 game exe that Azimuth runs. It
    checks the game's code first, and on anything else it stays off.


Troubleshooting
~~~~~~~~~~~~~~~

The mod logs to fh2-mods\platform.log, as lines starting
"[Do A Barrel Roll]". A working launch has "Ready: the car's physics
checked and hooked.", and each switch adds "Flight is on" or "Flight is
off". If the game crashes, the platform saves a report in
fh2-mods\crashes.

  - No "[Do A Barrel Roll]" line at all: the platform isn't loading the
    mod. Check that d3dcompiler_47.dll and DWrite.dll are both next to
    azimuth.exe (Azimuth 1.0.3 only loads DWrite.dll), and
    DoABarrelRoll.dll is in fh2-mods.
  - "doesn't match this game version" or "couldn't go in": the game exe
    isn't the one the mod was made for, or another mod changed that code
    first. The platform's lines just above say which.
  - No pop-up when flight goes on: pop-ups need platform 2.9.0, and
    follow the game's notification option and the "Pop-ups" row on the
    mod's page. Flight works without them.
  - No flight HUD: check the "Flight HUD" row. If it has no Instruments,
    or there's no such row, another mod's zip put an older platform
    back (the instruments need platform 2.9.1, the labels 2.9.0):
    extract this zip again.


Uninstall
~~~~~~~~~

Delete fh2-mods\DoABarrelRoll.dll and fh2-mods\DoABarrelRoll.ini. If you
don't use any other FH2 mods, delete d3dcompiler_47.dll, DWrite.dll and
the fh2-mods folder too. Nothing else was changed.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
