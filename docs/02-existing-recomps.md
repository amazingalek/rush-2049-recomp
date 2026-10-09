# Existing N64 recomps and native ports

Surveyed 2026-10-09. About 110 recomp projects across roughly 95 games, plus about 20
decomp-based ports. About 60% of the repos were created after June 2026, many of them
AI-assisted solo projects. The N64Recomp Discord and the awesome-recompilations list don't
accept AI-generated projects, so those projects don't show up there.

**Coverage.** Five catalog sites (listed at the end), plus a GitHub code search for repos that
use N64ModernRuntime, N64Recomp or RecompFrontend (about 160 hits), plus readme searches for
"N64Recomp" and "RT64" (about 800 repos), plus GitLab. Projects announced only on Discord
can't be searched and are the biggest blind spot.

## Our two games

| Game | Recomp status | Notes |
|---|---|---|
| **San Francisco Rush 2049** | **None found** in any source | — |
| **Jet Force Gemini** | **Already exists.** [TK22-26/JetForceGemini-Recomp](https://github.com/TK22-26/JetForceGemini-Recomp) Rel v1.1.0 (2026-10); also [Dubstroy/JFG-Recomp](https://github.com/Dubstroy/JFG-Recomp) (WIP) | There's a YouTube "JFG Recompiled BETA" video, and a JFG decomp is in progress |
| Rush 2: Extreme Racing USA | [bryantmh/rush2recompiled](https://github.com/bryantmh/rush2recompiled) Rel v1.0.9 (2026-10) | Same series as Rush 2049; the closest reference we have |
| San Francisco Rush | [sp00nznet/sfrush](https://github.com/sp00nznet/sfrush) WIP, dormant (2026-02) | |

## A. N64 static recompilations

Legend: **Rel** = has a downloadable release. **WIP** = public repo without a release.
**Dormant** = no commits for 6+ months. **Gone** = deleted or private. **AI** = says it is AI-assisted.
Repos are on github.com unless noted.

| Game | Repo(s) | Status |
|---|---|---|
| Majora's Mask | Zelda64Recomp/Zelda64Recomp | Rel v1.2.2 |
| Ocarina of Time | Zelda64Recomp (announced); JonB-Dev/oot-recompiled, leggettc18/oot-recomp | WIP |
| Banjo-Kazooie | BanjoRecomp/BanjoRecomp | Rel v1.0.2 |
| Banjo-Tooie | some-scurvy-dog/banjo-tooie-scurvy-dog-port (AI), Vidanox/BanjoTooieRecompiled, … | Rel alpha |
| Donkey Kong 64 | Rainchus/Donkey-Kong-64-Recompiled | Rel v1.0.3 |
| Diddy Kong Racing | ThatGuyMcd/DKR-R, cyscott/dkr-recomp, … | Rel beta |
| Conker's Bad Fur Day | sciaschi/CBFD-Recompiled, DahSidiAbdallah/ConkerBFDReloaded, … | Rel |
| GoldenEye 007 | kholdfuzion/GoldenRecomp (WIP), cblock85/GoldenEye64Recomp | Rel v1.0.0 |
| The World Is Not Enough | riseio/TWIPE_00Sheven | Rel 0.1.82 |
| Jet Force Gemini | TK22-26/JetForceGemini-Recomp, Dubstroy/JFG-Recomp | Rel v1.1.0 |
| Perfect Dark | WINDROID-EMU/PERFECT-DARK-RECOMP | Gone |
| Blast Corps | SlaveOfIDO/blastcorps-recomp | Gone |
| Star Fox 64 | sonicdcer/Starfox64Recomp | Rel 1.0.3 |
| Mario Kart 64 | sonicdcer/MarioKart64Recomp | Rel v0.9.2 |
| Super Smash Bros. | zestydevy/smash64r (dormant), sonicdcer/SuperSmashBrosRecomp | WIP |
| Dr. Mario 64 | theboy181/drmario64_recomp_plus | Rel 1.0.0 |
| Pokémon Puzzle League | AngheloAlf/puzzleleague64_recomp | Archived |
| Kirby 64 | Kirby64Ret/Kirby64Recomp | WIP |
| Paper Mario | SMCGames/Paper-Mario-ReCut (AI) | Rel v0.1.2 pre |
| Mario Party 1 / 2 / 3 | fragments; Rainchus/MarioParty3-Recomp (dormant) | WIP |
| Mario Golf | Rainchus/MarioGolf64, jebsmartt/MarioGolf64Recomp | WIP |
| Pokémon Snap | JackandBeans/Snap64Recomp, … | Rel v1.1.2 |
| Pokémon Stadium / Stadium 2 | mstan/PokemonStadiumRecomp (archived) / michiiik/pokemonStadiumGSRecomp | Rel beta / WIP |
| Pilotwings 64 | NoahVl/Birdman64, gcsmith/Pilotwings64Recomp, … | Rel v1.0.1 |
| Wave Race 64 | DomazinUS/RaceWave46 (AI), … | Rel v1.0.0 |
| Mystical Ninja Starring Goemon | klorfmorf/Goemon64Recomp | Rel v0.2.0-dev |
| Goemon's Great Adventure | dantheman11294/GGA-Recomp | Gone |
| Quest 64 | Rainchus/Quest64-Recomp | Rel v0.1 |
| Harvest Moon 64 | HarvestMoon64Recomp/HarvestMoon64Recomp | Rel v1.2.2 |
| Hamster Monogatari 64 | SrBananaMan/Hamster64Recomp | WIP |
| Animal Forest | luispolis124/AnimalForest64-Recomp | Rel v1.0.0 |
| Mega Man 64 / Rockman Dash | MegaMan64Recomp/MegaMan64Recompiled / Rainchus/RockmanDash-Recompiled | Rel v0.9.1 / WIP |
| Bomberman 64 / Second Attack / Hero | RevoSucks/BM64Recomp, BM64TSARecomp, BMHeroRecomp | Rel / WIP / Rel pre |
| Castlevania / Legacy of Darkness | RevoSucks/CV64Recomp / fliperama86/cvlod_recomp | Rel |
| Chameleon Twist 1 / 2 | Rainchus/ChameleonTwist1-JP-Recomp / …2-JP-Recomp | Rel v0.1 / WIP, dormant |
| Dinosaur Planet (proto) | DinosaurPlanetRecomp/dino-recomp | Rel v0.3.0 |
| Duke Nukem: Zero Hour | sonicdcer/DNZHRecomp (GitLab) | Rel 0.0.3 |
| Extreme-G / Extreme-G 2 | sonicdcer/ExtremeGRecomp (GitLab) / Darkluiside305/… | Rel v1.0.0 / Gone |
| Snowboard Kids / 2 | cdlewis/snowboardkids-recomp / snowboardkids2-recomp | Rel |
| Space Station Silicon Valley | Cellenseres/SSSV_Recomp | Rel v0.2.0 |
| Mischief Makers | ThiagoLira/trouble-makers-pc-recomp | Rel v0.8.4 |
| Ogre Battle 64 | lfarroco/ogre-battle-64-recomp | Rel v0.7.0 |
| Glover | ThatGuyMcd/Glover-R | Rel v1.0.1 |
| Rocket: Robot on Wheels | ThatGuyMcd/Rocket-R | Rel v1.0.1 |
| Rayman 2 | danielgomesvieira2000/rayman-2-the-great-escape-recomp, … | Rel alpha |
| Body Harvest | danielgomesvieira2000/body-harvest-recomp, mcsoderh, … | Rel |
| Hybrid Heaven | ogdanimal/HybridHeaven-Recomp, … | Rel v1.0.1 |
| Beetle Adventure Racing | danielgomesvieira2000/beetle-adventure-racing-recomp, bryankruman/BeetleRecomp, … | Rel alpha |
| AeroGauge | alondero/aerogauge-recomp | Rel v0.6.2 |
| Automobili Lamborghini | alondero/automobililamborghini-recomp | Rel v0.8.0 |
| Road Rash 64 | linkssy2/RoadRash64Recompiled | Rel v1.4.5 |
| **San Francisco Rush** | sp00nznet/sfrush | WIP, dormant |
| **Rush 2** | bryantmh/rush2recompiled | Rel v1.0.9 |
| Cruis'n USA / World / Exotica | navjack/cruisn-*-recomp | Gone |
| Star Wars Episode I: Racer | sp00nznet/racer | WIP |
| Shadows of the Empire | GTTeancum/SotE-Recomp | Rel 0.9.2 |
| Rogue Squadron | MikeSemicolonD/RogueSquadron64Recomp (AI), … | WIP |
| Turok 2 / Rage Wars | SirBraga/turok2-recomp / ArtfulRascal8/rage-wars-recompiled | Rel beta |
| Rampage World Tour / Rampage 2 | sp00nznet/Rampage | WIP |
| BattleTanx / Global Assault | bdragoncore/battle-tanx-recomp / wootbeer/btgarecomp | WIP / Rel 0.1.1 |
| Sin and Punishment | maximilianoide/sin-punishment-recompiled | WIP |
| Super Robot Wars 64 | dyzz/srw64-recomp | Rel v0.4.4 |
| Doraemon (Mittsu no Seireiseki) | citrongames/Dora64 | Rel v1.0.4 |
| Wonder Project J2 | Vmarcelo49/wpj2-recomp, … | WIP |
| Tetrisphere / The New Tetris | djultra64/Blocksphere / Tiptup300/TheNewT64risRecomp | Rel |
| AKI wrestling games (×6) | jessetbh/*Recomp + AkiLauncher | Rel v0.1.x pre |
| 40 Winks | Faderz48/40-Winks-PC-Port | Rel alpha |
| Army Men: Sarge's Heroes | RangerXT/Army-Men-Sarges-Heroes-64-Recompiled | Rel alpha |
| Wayne Gretzky's 3D Hockey | geno55/wg3d-recomp | Rel v0.2.0 pre |
| Knife Edge | haydenmc/knife-edge-recomp (AI) | Rel v0.0.3 |
| Aero Fighters Assault | DohmBoy64Bit/Aero64Final | WIP |
| Buck Bumble | riseio/project-bumble | Gone |
| Gex 3 | jessicamaybe/gex3recomp | WIP, dormant |
| LEGO Racers / Hexen 64 | marijnvdwerf/lego-racers-recomp / benXhawk/hexen64recomp | Empty stubs |
| Wetrix | wetrix-enjoyers/wetrix-n64-recompilation | Tooling only |

## B. Rumored or unverified

- **Ocarina of Time in Zelda64Recomp** has been promised for about two years, with no release.
- **Kirby 64** has an active repo but no downloads.
- **Several games appear only in catalogs** and their repos are now deleted or private:
  Pokémon Stadium JP, Goemon's Great Adventure, Extreme-G 2, the three Cruis'n games, Blast
  Corps, Perfect Dark and Buck Bumble.
- **Discord-only work exists but is unnamed.** 2025 press mentions "8 projects in development".
- **retrolunch.com is unreliable**: it lists repo URLs that don't exist.

## C. Decomp-based native ports (not recomps)

| Game | Port |
|---|---|
| Ocarina of Time / Majora's Mask | Ship of Harkinian / 2 Ship 2 Harkinian |
| Super Mario 64 | sm64ex, Render96ex, sm64coopdx, Ghostship, … |
| Mario Kart 64 / Star Fox 64 | SpaghettiKart / Starship |
| Perfect Dark | fgsfdsfgs/perfect-dark-pc-port |
| Paper Mario / Banjo-Kazooie | PaperBoat / Lighthouse |
| Super Smash Bros. | JRickey/BattleShip (beta) |
| F-Zero X | Zorkats/G-Diffuser |
| Diddy Kong Racing | akratch/goldenballoon |
| GoldenEye 007 | jkdansereau/goldeneye-pc-port, … |
| Kirby 64 | JR3DFUL/HALBERD (WIP) |
| Doom 64 | DOOM64 EX, plus the official remaster |

## D. Notable games with nothing found

Yoshi's Story, Excitebike 64, 1080° Snowboarding, Mario Tennis, Mickey's Speedway USA,
Turok 1 & 3, Shadow Man, Battle for Naboo, Hey You Pikachu, Custom Robo, Mortal Kombat 4 /
Trilogy, Killer Instinct Gold, Mace, Gauntlet Legends, NFL Blitz, Hydro Thunder, Top Gear
Rally / Overdrive, Ridge Racer 64, F-1 World Grand Prix, World Driver Championship, Resident
Evil 2, WinBack, Tony Hawk 1–3, South Park, Forsaken 64, Gex 64, Aidyn Chronicles,
Shadowgate 64, Evangelion 64, Bangai-O, StarCraft 64.

## Sources

[recomp.fyi](https://recomp.fyi) (205 N64 rows),
[SubmitGame/awesome-decomp-games](https://github.com/SubmitGame/awesome-decomp-games),
[Nio03/unricopie](https://github.com/Nio03/unricopie),
[BlueInterlude/awesome-recompilations](https://github.com/BlueInterlude/awesome-recompilations),
[Sebastrion/awesome-unofficial-pc-ports](https://github.com/Sebastrion/awesome-unofficial-pc-ports),
[readonlymemo list](https://readonlymemo.com/decompilation-projects-and-n64-recompiled-list/),
[djcube directory](https://djcube.co.uk/n64-pc-port-directory/), Time Extension and
DSOGaming coverage. Couldn't read: heldgames.com, the gamicus wiki, ResetEra.
