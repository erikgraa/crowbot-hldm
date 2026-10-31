# 🗺️ Crowbot TODO

Roughly ordered by impact. Contributions and ideas welcome. 🔧

## 🧭 Navigation
- [ ] Auto-generate a waypoint graph from spawns, items and visited positions
- [ ] A* pathfinding so bots route to out-of-sight items and enemies
- [ ] Ladders, buttons, doors, platforms and teleporters
- [ ] Better water and long-jump handling
- [ ] Per-map waypoint cache (save and load)

## ⚔️ Combat
- [ ] Ammo tracking, and switching away from empty weapons
- [ ] Use grenades, satchels, tripmines and snarks
- [ ] Gauss charge shots and RPG laser guidance
- [ ] Hear footsteps and gunfire, and investigate sounds
- [ ] Retreat and hunt for health when low
- [ ] Camp or ambush at power weapon spawns
- [ ] Skill levels that also scale detection range and weapon choice

## 💬 Chat and personality
- [ ] Event-triggered lines (join, kill, death, map change) with cooldowns
- [ ] Bot-to-bot conversations with typing delay and occasional typos
- [ ] React to what real players type
- [ ] Optional LLM-backed chat through an external helper process
- [ ] `crowbot_chat` cvar and a chattiness setting

## 🛠️ Server and admin
- [ ] Config file for bot names, models and per-weapon preferences
- [ ] Admin access to `crowbot` commands from in-game clients
- [ ] Fill the server with bots when empty and kick them as players join (`crowbot_fill`)
- [ ] Per-bot skill variation and named personalities
- [ ] Stats output (JSON or log lines) for HLstatsX-style tools

## 📦 Project
- [ ] Linux build (`crowbot_i386.so`) and Makefile
- [ ] GitHub Actions: build on every push, and attach releases
- [ ] Test on more maps (`stalkyard`, `datacore`, `boot_camp`, `snark_pit`)
- [ ] Test teamplay and mixed human/bot games
- [ ] Rewrite the Metamod boilerplate in `src\` to settle licensing before going public
- [ ] Screenshots or a short gameplay clip for the README

## ✅ Done
- [x] Metamod plugin scaffold
- [x] Fake client creation, respawn and quota management
- [x] Obstacle, ledge and lava avoidance with jumping
- [x] Item pickup, weapon selection by range, projectile leading
- [x] Teamplay support (bots join the smallest team)
- [x] Local test server script
