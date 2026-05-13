# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A 2D arcade platformer inspired by the classic Donkey Kong (1981), built in **C++ with Raylib**. The story follows Subaru climbing to reach Regulus across 5 levels. The project includes a full in-game level editor, GPU-based 2D lighting, and a keyframe cinematic system.

## Build & Run

**Prerequisites**: Visual Studio 2022 (Windows) or MinGW with `mingw32-make`.

**Windows (VS Code tasks)**:
- `Ctrl+Shift+B` → select `build debug` or `build release`
- Or run directly: `mingw32-make config=debug_x64` from the `build/` directory

**Regenerate build files** (after editing `premake5.lua`):
- VS Code task: `UpdateMake`
- Or run `premake5 gmake2` / `premake5 vs2022`

**Output**: `bin/Debug/dONKEY-kONG-GAME.exe` or `bin/Release/`

The executable must run from the repo root so it can find `Levels/`, `Assets/`, and `Cinematics/` via relative paths.

There are no automated tests.

## Architecture

### Screen State Machine (`src/main.cpp`)

The entire game runs through a single `GameScreen` enum state machine:
```
SPLASH_SCREEN → SPLASH_SCREEN2 → MENU → GAMEPLAY → HOW_HIGH (win) or GAME_OVER
                                      ↘ CONTROLS
                                      ↘ LEVEL_EDITOR
```
`main.cpp` is ~2900 lines and handles all per-frame update + render logic for every state. Physics constants (gravity `0.4`, jump force `-7.52`, player speed `1.8`) are defined here.

### Major Systems

| File | Responsibility |
|------|---------------|
| `src/main.cpp` | Game loop, state machine, entity update/render |
| `src/LevelEditor.cpp/h` | In-game level editor (~185 KB) with gizmo tools, outliner, cinematic sequencer |
| `src/LevelData.cpp/h` | Binary-serialized level format (platforms, ladders, path nodes, lights, props, cinematics) |
| `src/Collision.cpp/h` | Parallelogram-based collision supporting tilted platforms and one-way passthrough |
| `src/Lighting.cpp/h` | GPU raytraced 2D lighting — point, spot, and sky lights with bounce/ambient |
| `src/CinematicPlayer.cpp/h` | Runtime keyframe animation playback |
| `src/CinematicData.cpp/h` | Cinematic sequence data structures and serialization |
| `src/Ladder.h` | Ladder climbing mechanics (progress 0.0–1.0, trim vs. interactive zone) |

### Entities

- **Player (Subaru)**: 63×63 px; states: walk, jump, ladder climb, carry. 3 lives with 1.5 s invincibility after hit.
- **Enemy (Regulus)**: AI state machine `IDLE → JUMP_TOWARD → LAND_PAUSE → JUMP_BACK`; stun state triggered by nuke.
- **Barrels**: Pool of 100; follow a 28-node path with probability-based branching at split nodes. Blue (rare/bonus) vs. orange (standard).
- **Items**: Nuke (6-frame explosion, stuns Regulus), Beatrice (7 s duration, shoots toward mouse).

### Level Data

- Levels 1–5: real data in `Levels/level_*.lvl` (binary `LevelData` structs).
- Levels 6–10: empty stubs.
- Cinematics: JSON sequences referenced by `Cinematics/manifest.txt`.

### Lighting System

Multi-pass GPU pipeline using Raylib render targets: scene → occluders → direct lit → bounce → final composite. Lights support per-light flicker/pulse animation via `uTime` shader uniform. Configured per-level in `LevelData`.

### Collision System

Platforms are internally represented as parallelograms to support shear/tilt. Collision resolution is velocity-based. One-way platforms allow jumping through from below. Ladders have a shorter interactive zone than their visual height.

## Key Conventions

- Window size is fixed at **875×950 px**.
- Asset paths are relative to the working directory: `Assets/Textures/`, `Assets/Nuevo audio/mp3/`, `Levels/`, `Cinematics/`.
- The project uses **C++17** (`/std:c++17` in MSVC, `-std=c++17` in GCC).
- Raylib source lives in `build/external/raylib-master/`; it is compiled as part of the project, not linked as a pre-built library.
- The Level Editor is toggled from the main menu and shares the same window/render context as gameplay — no separate process.
