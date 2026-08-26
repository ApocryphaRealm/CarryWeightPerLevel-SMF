# Carry Weight Per Level - SMF Settings

**Version 1.0.0** - the build's own version, starting fresh per this project's standing rule
(`CLAUDE.md` rule 6: generated content starts at 1.0.0). This is an original mod, not a fork,
so there is no upstream version to track.

A fresh, native SKSE plugin implementation of "carry weight scales with level", with a real
SKSE Menu Framework settings page. Every value the design was inspired by was fixed and
non-configurable; here, all of it is a setting you can change in game.

## Where this came from

This mod was **inspired by, not ported from**, *Increase Carry-Weight on Level-Up*
(Nexus Skyrim Special Edition mod 2654, by the original author of that mod - no public
repository, distributed as a `.7z` with its Papyrus source included). That mod's own
description: *"Mages want to carry loot, too! Increase the player's carry-weight, no matter
which stat you choose when levelling up."* In vanilla Skyrim, only choosing Stamina on
level-up increases carry weight - a Health/Magicka-focused build (a mage, say) never gets
any carry weight increase at all just from leveling. The original mod fixed that by adding a
flat carry weight bonus on every level-up, regardless of which attribute was chosen, via a
Story Manager event and a `GlobalVariable` (`CarryIncrease`) whose value was fixed by the
mod author and never exposed to the player. Its version 0.4 added a second mechanic: a
one-time potion that retroactively topped up an existing character's carry weight to what it
would have been if the mod had applied since level 1 (for players installing the mod on a
save that already had levels).

**This mod re-implements that general idea from scratch in C++** - no Papyrus, no `.esp`, no
code or assets from the original mod at all. Game mechanics ("carry weight scales with
level") are not copyrightable; only the original mod's own specific implementation is, and
none of it is reused here. See `D:\Claude output\Mod Analysis` conventions - this repo
started life via `D:\Claude output\analyze mods`, but as a from-scratch native build rather
than an SMF port of the original's own code, since the original had nothing but three fixed
Papyrus scripts to port in the first place.

## What it does

**Per-level bonus** (the core mechanic): every time the player levels up, add a configurable
amount of carry weight - by default 5.0, the same rate vanilla itself grants for a Stamina
level-up. Detected natively via CommonLibSSE-NG's `RE::LevelIncrease::Event` (see
`include/RE/L/LevelIncrease.h` in the vendored headers), a real engine event fired once per
level actually gained - simpler and more reliable than the Story Manager's
`OnStoryIncreaseLevel` event the original mod had to work around in Papyrus (which only fires
once even across a multi-level jump, and for the *first* of those levels rather than the
last).

**Retroactive catch-up** (native equivalent of the original's one-time potion): for a
character who already had levels before this feature was ever turned on, tops up their carry
weight to match what the per-level bonus would have granted since level 1, using its own
separately configurable per-level rate. Rather than a literal in-game potion item (which would
need its own `.esp`/Alchemy record - out of scope for a script-free native plugin), this runs
automatically on every save load and is also available on demand via a "Recalculate now"
button on the settings page. It is idempotent: it always compares what *should* have been
granted by now against a running total tracked in the save's own co-save data (SKSE's
`SerializationInterface`), and only ever grants the (non-negative) difference - so it can never
grant the same bonus twice, and (like the original) never removes carry weight.

Both features can be turned off independently, and both bonus rates are independently
configurable, all from the settings page under SKSE Menu Framework's Mod Control Panel.

## Settings persistence

`Restore defaults` reads only the DLL's own compiled-in values, never a file; `Save`/`Reload
from INI` both target this mod's own shipped `CarryWeightPerLevel.ini`. A saved change survives
to the next game load (`CLAUDE.md` rule 16) - the INI is rewritten with plain file I/O, not
`WritePrivateProfileString`, since Mod Organizer 2's usvfs does not reliably redirect the
latter. The retroactive catch-up's own running total is separate from the INI entirely - it is
per-character save data (see above), not a global setting, so it is unaffected by `Restore
defaults`, `Save`, or `Reload from INI`.

## Debugging

Send the debug log for any bug report:
`Documents\My Games\Skyrim Special Edition\SKSE\CarryWeightPerLevel.log`

Ships at `uLogLevel=0` (trace), the most comprehensive level, both as the compiled default and
in the shipped INI (`CLAUDE.md` rules 14/31) - a submitted log already carries decision-point
detail (level detected, bonus computed and applied, catch-up math) without asking the reporter
to change anything first. A DevBench tool, `carryweightperlevel.status`, is also registered if
DevBench is installed, exposing the same state live.

## Build

CMake + vcpkg, same scaffold as this project's other SMF mods (`AutoDraw-SMF`,
`DragonsEyeMinimap-SMF`): `configure.bat` then `build.bat`, preset SE/AE only
(`-UENABLE_SKYRIM_VR`, so both SE 1.5.97 and AE 1.6.x stay enabled - VR needs a different
build target this project doesn't ship). Requires `VCPKG_ROOT` pointing at a vcpkg checkout;
Visual Studio, and the CMake/Ninja that ship with it, are located automatically by
`find-msvc.bat`.

## Licence

MIT - see `LICENSE`. This is original code with no upstream mod's code or assets to inherit a
licence from, so MIT was chosen as a permissive default consistent with this project's other
originally-authored material.

## Status

**First attempt, not yet tested in game.** Built and reviewed for correctness against the
vendored CommonLibSSE-NG headers, but the level-up hook and the catch-up math have not been
exercised on a running save yet. Not packaged into `current test builds`, not finalized - per
the task this was built under, that is the author's call once he has reviewed it.
