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
    float x = 0.f;
    float y = 0.f;
    float w = 128.f;
    float h = 0.f;      // 0 → default thin floor (Platform::Make convention)
    float tilt = 0.f;
};

struct LadderData {
    float x = 0.f;
    float y = 0.f;
    float w = 40.f;
    float h = 100.f;
};

struct PathNodeData {
    float x = 0.f;
    float y = 0.f;
    int   next[2] = { -1, -1 };
    int   rollThreshold = 5;
    bool  isSplitNode = false;
};

// ── Win zone ──────────────────────────────────────────────────────────────────

struct WinZoneData {
    float x = 0.f;
    float y = 0.f;
    float w = 128.f;
    float h = 128.f;
};

// ── Kill zone ─────────────────────────────────────────────────────────────────

enum class KillZoneTexture : int {
    NONE = 0,
    DK_GOLDEN_PISTON = 1,
};

struct KillZoneData {
    float          x = 0.f;
    float          y = 0.f;
    float          w = 64.f;
    float          h = 64.f;
    float          rotation = 0.f;       // true rotation in degrees (0/90/180/270)
    KillZoneTexture texId = KillZoneTexture::NONE;
};

// ── Elevator ─────────────────────────────────────────────────────────────────

struct ElevatorData {
    float x = 0.f;
    float y = 0.f;   // top of shaft
    float w = 48.f;
    float h = 200.f;
    float speed = 60.f;  // px/sec children move
    int   direction = 1;   // 1 = children go up, -1 = down
};

// ── Generic entity reference (type = EditorTool int, index = vector index) ───

struct EntityRef {
    int type = -1;
    int index = -1;
    bool valid() const { return type >= 0 && index >= 0; }
    void clear() { type = -1; index = -1; }
    bool operator==(const EntityRef& o) const { return type == o.type && index == o.index; }
    bool operator!=(const EntityRef& o) const { return !(*this == o); }
};

// ── Parent-child relationship ─────────────────────────────────────────────────
// For elevator children, offsetY is the initial phase (0..elev.h) at attachment.

struct ParentChildRelation {
    EntityRef parent;
    EntityRef child;
    float     offsetX = 0.f;
    float     offsetY = 0.f;
};

// ── Full level ────────────────────────────────────────────────────────────────

struct LevelData {
    int  id = 0;
    bool valid = false;    // false → not loaded yet

    // ── Singleton entities ──────────────────────────────────────────────────
    bool    hasPlayerSpawn = false;
    Vector2 playerSpawn = { 269.f, 817.f };

    bool    hasRegulus = false;
    Vector2 regulusPos = { 22.f,  225.f };    // x = REGULUS_X, y = platform top

    bool    hasCave = false;
    Vector2 cavePos = { 35.f,  768.f };       // houseX, houseY

    // ── Multi-instance entities ─────────────────────────────────────────────
    std::vector<PlatformData> platforms;
    std::vector<LadderData>   ladders;
    std::vector<Vector2>      beams;
    std::vector<PathNodeData> pathNodes;
    std::vector<Vector2>            nukeSpawns;
    std::vector<Vector2>            beatriceSpawns;
    std::vector<Vector2>            enemySpawns;
    std::vector<ElevatorData>        elevators;
    std::vector<ParentChildRelation> relations;

    // ── Zone entities ───────────────────────────────────────────────────────
    bool         hasWinZone = false;
    WinZoneData  winZone;
    std::vector<KillZoneData> killZones;
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