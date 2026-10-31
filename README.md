# Crowbot

A Metamod plugin that adds computer-controlled players (bots) to Half-Life Deathmatch.

Bots roam the map, pick up weapons, ammo, health and armor, choose weapons by range, strafe and aim at visible enemies,
and respawn on their own. They navigate with local obstacle probing (hull traces, ledge/lava avoidance, jumping),
so no waypoint files are needed. Teamplay is supported (bots join the smallest team).

## Build

Requires MSVC Build Tools (x86). Half-Life is 32-bit.

    scripts\fetch-deps.ps1   -> downloads HLSDK/Metamod headers into include\ (first time only)
    build.bat                -> build\crowbot.dll

## Install

1. Install [Metamod-r](https://github.com/theAsmodai/metamod-r) so `liblist.gam` has
   `gamedll "addons\metamod\dlls\metamod.dll"`.
2. Copy `build\crowbot.dll` to `valve\addons\crowbot\crowbot.dll`.
3. Add `win32 addons/crowbot/crowbot.dll` to `valve\addons\metamod\plugins.ini`.

## Commands and cvars

Server console (or the in-game console of a listen-server host):

    crowbot add [skill]    add one bot
    crowbot kick           remove one bot
    crowbot kickall        remove all bots and set quota 0
    crowbot quota <n>      keep n bots in the game
    crowbot skill <1-5>    default skill for new bots

| cvar | default | meaning |
|------|---------|---------|
| crowbot_quota | 0 | desired number of bots |
| crowbot_skill | 3 | 1 (easy) to 5 (hard): aim error, reaction and turn speed |
| crowbot_debug | 0 | print per-bot movement stats every 5s |

## Testing locally

`scripts\test-server.ps1` rebuilds, restarts a test HLDS under `deps\hlds` (installed with SteamCMD,
`app_update 90 -beta steam_legacy`) and prints bot activity.

## Known limitations

- No waypoints/navmesh, so bots can get lost on complex maps.
- Ammo is not tracked; empty weapons are detected when selection fails.
- Grenades, satchels, tripmines and snarks are not used.

## License

MIT, see [LICENSE](LICENSE). The HLSDK and Metamod-r headers fetched into `include\` are not part of this repo and
are covered by their own licenses.
