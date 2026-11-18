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
(its pop-ups need 2.9.0). If platform.log says it "needs FH2 Mod Platform
2.8.0 or newer", another mod's zip put an older platform back: extract
this zip again.


Flying
~~~~~~

1. Turn flight on: F9 on the keyboard, or LB + RB + d-pad up on the
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


Settings
~~~~~~~~

On the mod's page (Pause > MODS > YOUR MODS > Do A Barrel Roll):

  Flight              turns flight on and off, like the hotkey
  Simple controls     On (the default): the controls above. Off: a
                      plane's controls. Left and right on the stick roll
                      the car, nothing levels it for you, and you turn by
                      rolling and pulling back
  Keys and buttons    the key and the controller buttons that turn
                      flight on and off

They're saved in fh2-mods\DoABarrelRoll.ini, which is written the first
time you change one, with a comment next to each setting:

  Simple=1            0 = a plane's controls
  FlightKey=120       the key, as a Windows key code (120 = F9)
  FlightModifiers=0   held with it: 1 Ctrl, 2 Shift, 4 Alt, added up
  FlightPad=769       the controller buttons (769 = LB+RB+d-pad up)
  CheckUpdates=1      0 = don't check GitHub for a newer version

When a newer version is out, the title screen says so after
"PRESS (A) TO START".


Good to know
~~~~~~~~~~~~

  - Offline only. When the game is started from FH2MP's launcher, the
    platform doesn't load the mod.
  - The keyboard can turn flight on and off, but flying needs a
    controller.
  - LB is the game's clutch. That does nothing in the air, but if you
    drive manual with clutch, holding LB on the ground with flight on
    still works the clutch.
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


Uninstall
~~~~~~~~~

Delete fh2-mods\DoABarrelRoll.dll and fh2-mods\DoABarrelRoll.ini. If you
don't use any other FH2 mods, delete d3dcompiler_47.dll, DWrite.dll and
the fh2-mods folder too. Nothing else was changed.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
