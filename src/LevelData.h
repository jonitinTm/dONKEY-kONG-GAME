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

// ── Beam ──────────────────────────────────────────────────────────────────────

struct BeamData {
    float x = 0.f;
    float y = 0.f;
    int   texVariant = 0;
    int   renderLayer = 0;
    bool  flipX = false;
};

// ── Kill zone ─────────────────────────────────────────────────────────────────

enum class KillZoneTexture : int {
    NONE = 0,
    DK_GOLDEN_PISTON = 1,
};

struct KillZoneData {
    float           x = 0.f;
    float           y = 0.f;
    float           w = 64.f;
    float           h = 64.f;
    float           rotation = 0.f;
    KillZoneTexture texId = KillZoneTexture::NONE;
    int             renderLayer = 1;
};

// ── Conveyor belt ─────────────────────────────────────────────────────────────

struct ConveyorData {
    float x = 0.f;
    float y = 0.f;
    float length = 192.f;
    float speed = 80.f;
    int   direction = 1;
    float rotation = 0.f;
    float endCapW = 32.f;
    float beltH = 24.f;
};

// ── Elevator ──────────────────────────────────────────────────────────────────

struct ElevatorData {
    float x = 0.f;
    float y = 0.f;
    float w = 48.f;
    float h = 200.f;
    float speed = 60.f;
    int   direction = 1;
};

// ── Prop ──────────────────────────────────────────────────────────────────────
// A static decorative / collision object placed in the level.
//
//  texVariant  : which prop texture to use (0 = Assets/Textures/Lighting/Light.png)
//  hasCollision: if true a thin Platform is added at draw time so entities land on it
//  lightAffect : 0 = always fully bright (drawn after lighting composite),
//                1 = normal (drawn inside scene, lit like everything else),
//               >1 = overbright / glow (normal draw + additive glow pass scaled
//                    by (lightAffect-1)/2 after composite)
//  renderLayer : 0 = behind all entities (before House draw),
//                1 = in front of entities (after Enemies draw),
//                2 = above the lighting composite (use with lightAffect 0 or for
//                    screen-space overlays)

struct PropData {
    float x = 0.f;
    float y = 0.f;
    float width = 64.f;
    float height = 64.f;
    float rotation = 0.f;
    bool  hasCollision = false;
    float lightAffect = 1.f;
    int   renderLayer = 0;
    int   texVariant = 0;
};

// ── Light ─────────────────────────────────────────────────────────────────────
// One struct covers all three light types. Set `type` to choose semantics:
//   POINT : omnidirectional, falloff by `radius`.
//   SPOT  : `direction` (degrees, 0 = right, 90 = down) and `angle` (full cone).
//   SKY   : directional ambient — `direction` is the angle the sky comes FROM,
//           e.g. 270 = light comes from directly above (since +Y is down).
//           `radius` doubles as the max ray length we trace toward the sky.

enum class LightType : int {
    POINT = 0,
    SPOT = 1,
    SKY = 2,
};

struct LightData {
    LightType type = LightType::POINT;

    float x = 0.f;
    float y = 0.f;

    // Color in linear 0..1
    float r = 1.f;
    float g = 0.92f;
    float b = 0.78f;

    float intensity = 1.0f;   // emissiveness multiplier
    float radius = 220.f;  // POINT/SPOT falloff radius; SKY: max trace dist
    float innerRadius = 0.f;    // inside this dist light is FULL intensity,
    // then fades to 0 at radius. 0 = pure quadratic.

    float angle = 60.f;     // SPOT: cone full-angle in degrees
    float direction = 270.f;    // SPOT: aim dir (deg); SKY: incoming dir (270=above)

    int   bounces = 0;       // 0 = direct only; 1 = enable GI bounce pass
    float fogStrength = 0.f;    // 0 = no volumetric fog; ~0.5 = nice god rays

    bool  enabled = true;

    // ── Per-light animation (GPU-driven via uTime, zero CPU cost) ─────────────
    float flickerFreq = 0.f;
    float flickerAmp = 0.f;
    float pulseFreq = 0.f;
    float pulseAmp = 0.f;
};

// ── Generic entity reference ──────────────────────────────────────────────────

struct EntityRef {
    int type = -1;
    int index = -1;
    bool valid() const { return type >= 0 && index >= 0; }
    void clear() { type = -1; index = -1; }
    bool operator==(const EntityRef& o) const { return type == o.type && index == o.index; }
    bool operator!=(const EntityRef& o) const { return !(*this == o); }
};

// ── Parent-child relationship ─────────────────────────────────────────────────

struct ParentChildRelation {
    EntityRef parent;
    EntityRef child;
    float     offsetX = 0.f;
    float     offsetY = 0.f;
};

// ── Full level ────────────────────────────────────────────────────────────────

struct LevelData {
    int  id = 0;
    bool valid = false;

    bool    hasPlayerSpawn = false;
    Vector2 playerSpawn = { 269.f, 817.f };

    bool    hasRegulus = false;
    Vector2 regulusPos = { 22.f, 225.f };

    bool    hasCave = false;
    Vector2 cavePos = { 35.f, 768.f };

    std::vector<PlatformData>        platforms;
    std::vector<LadderData>          ladders;
    std::vector<BeamData>            beams;
    std::vector<PathNodeData>        pathNodes;
    std::vector<Vector2>             nukeSpawns;
    std::vector<Vector2>             beatriceSpawns;
    std::vector<Vector2>             enemySpawns;
    std::vector<ElevatorData>        elevators;
    std::vector<ConveyorData>        conveyors;
    std::vector<PropData>            props;      // ← NEW
    std::vector<ParentChildRelation> relations;

    bool                      hasWinZone = false;
    WinZoneData               winZone;
    std::vector<KillZoneData> killZones;

    std::vector<LightData> lights;
};

// ── Persistence ───────────────────────────────────────────────────────────────

bool      SaveLevel(const LevelData& lv, const char* folder = "Levels");
bool      LoadLevel(LevelData& out, int id, const char* folder = "Levels");
LevelData GetDefaultLevel1();
void      ExportLevelAsCpp(const LevelData& lv, const char* outFile = "LevelExport.cpp");