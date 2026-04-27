#pragma once
// ============================================================
//  LevelData.h  –  Serialisable level representation
//  Works independently of Collision.h / Ladder.h so the
//  editor can include it without pulling in game headers.
// ============================================================
#include "raylib.h"
#include <vector>
#include <string>

// ── Per-entity data structs ───────────────────────────────────────────────────

struct PlatformData {
    float x    = 0.f;
    float y    = 0.f;
    float w    = 128.f;
    float h    = 0.f;      // 0 → default thin floor (Platform::Make convention)
    float tilt = 0.f;
};

struct LadderData {
    float x = 0.f;
    float y = 0.f;
    float w = 40.f;
    float h = 100.f;
};

struct PathNodeData {
    float x             = 0.f;
    float y             = 0.f;
    int   next[2]       = { -1, -1 };
    int   rollThreshold = 5;
    bool  isSplitNode   = false;
};

// ── Full level ────────────────────────────────────────────────────────────────

struct LevelData {
    int  id    = 0;
    bool valid = false;    // false → not loaded yet

    // ── Singleton entities ──────────────────────────────────────────────────
    bool    hasPlayerSpawn = false;
    Vector2 playerSpawn    = { 269.f, 817.f };

    bool    hasRegulus  = false;
    Vector2 regulusPos  = { 22.f,  225.f };    // x = REGULUS_X, y = platform top

    bool    hasCave  = false;
    Vector2 cavePos  = { 35.f,  768.f };       // houseX, houseY

    // ── Multi-instance entities ─────────────────────────────────────────────
    std::vector<PlatformData> platforms;
    std::vector<LadderData>   ladders;
    std::vector<Vector2>      beams;
    std::vector<PathNodeData> pathNodes;
    std::vector<Vector2>      nukeSpawns;
    std::vector<Vector2>      beatriceSpawns;
    std::vector<Vector2>      enemySpawns;
};

// ── Persistence ───────────────────────────────────────────────────────────────

/// Save  →  <folder>/level_<id>.lvl   (creates folder if needed)
bool SaveLevel(const LevelData& lv, const char* folder = "Levels");

/// Load  ←  <folder>/level_<id>.lvl
/// Returns false if the file does not exist.
bool LoadLevel(LevelData& out, int id, const char* folder = "Levels");

/// Returns the hardcoded level 1 (exact copy of main_patch.cpp data).
LevelData GetDefaultLevel1();

/// Writes a self-contained C++ snippet you can paste into main_patch.cpp.
void ExportLevelAsCpp(const LevelData& lv, const char* outFile = "LevelExport.cpp");
