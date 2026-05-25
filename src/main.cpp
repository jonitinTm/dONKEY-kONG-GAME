#include "raylib.h"
#include "raymath.h"
#include "Collision.h"
#include "Ladder.h"
#include "LevelData.h"
#include "LevelEditor.h"
#include "CinematicPlayer.h"
#include "Lighting.h"
#include <ctime>
#include <algorithm>

enum GameScreen { SPLASH_SCREEN = 0, SPLASH_SCREEN2, MENU, CONTROLS, GAMEPLAY, GAME_OVER, HOW_HIGH, LEVEL_EDITOR, CARD_SELECT, SETTINGS };

static constexpr float DEATH_FLASH_DURATION = 0.40f;
static constexpr float DEATH_FADE_DURATION = 1.10f;
static constexpr float DEATH_TOTAL_FALL = DEATH_FLASH_DURATION + DEATH_FADE_DURATION;
static constexpr float DEATH_BLACK_HOLD = 0.50f;
static constexpr float DEATH_FALL_GRAVITY = 0.50f;
static constexpr float DEATH_FALL_INITIAL_VY = -5.0f;
static constexpr float DEATH_SHAKE_DURATION = 0.80f;
static constexpr float DEATH_SHAKE_AMOUNT   = 9.0f;
static constexpr float DASH_SHAKE_DURATION  = 0.22f;
static constexpr float DASH_SHAKE_AMOUNT    = 5.0f;
static constexpr float ACTIVE_SPAWN_INTERVAL = 1.2f;

struct PathNode
{
    Vector2 pos = { 0.f, 0.f };
    int     next[3] = { -1, -1, -1 };
    int     edgeType[3] = { 0, 0, 0 };  // 0=normal, 1=ladder
    int     rollThreshold = 5;
    bool    isSplitNode = false;
};

struct Barrel
{
    Rectangle hitbox = { 0, 0, 40, 40 };
    int       currentNode = 0;
    float     speed = 2.5f;
    bool      active = true;
    bool      isBlue = false;
    bool      isFalling = false;
    bool      movingLeft = false;
    float     animTimer = 0.0f;
    int       animFrame = 0;
    bool      jumpScored = false;
    bool      reachedEnd = false;
};

struct NukeItem { Vector2 pos; bool active = true; };
struct FlyingNuke { Rectangle rect = { 0,0,0,0 }; Vector2 vel = { 0,0 }; bool active = false; };
struct BeatriceItem { Vector2 pos; bool active = false; };

struct BeaBullet
{
    Vector2 pos = { 0, 0 };
    Vector2 vel = { 0, 0 };
    float   lifetime = 0.0f;
    bool    active = false;
};

enum EnemyType { GRUNT = 0, SPECTER };
enum EnemyState { ES_IDLE, ES_JUMP_TOWARD, ES_LAND_PAUSE, ES_JUMP_BACK };

struct Enemy
{
    Rectangle  hitbox = { 0, 0, 44, 44 };
    Vector2    velocity = { 0.0f, 0.0f };
    EnemyType  type = GRUNT;
    EnemyState state = ES_IDLE;
    float      stateTimer = 0.0f;
    bool       active = false;
    bool       grounded = false;
    bool       facingRight = true;
    int        animFrame = 0;
    float      animTimer = 0.0f;
    bool       onLadder = false;
    int        ladderIdx = -1;
    float      ladderProgress = 0.f;
    float      ladderCooldown = 0.f;
    bool       justJumped = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// ─── Powerup system ──────────────────────────────────────────────────────────

enum PowerupType {
    PU_NONE = -1,
    PU_RETURN_BY_DEATH = 0, PU_DASH, PU_EL_SHAMAK, PU_LARPER,
    PU_REINHARD, PU_WHIP, PU_EXTRA_LIFE, PU_NUKE_PU,
    PU_ONE_MORE_LARP, PU_SPEEDRUN, PU_SHIELD, PU_SHOP,
    PU_BEATRICE,
    PU_COUNT
};

struct PowerupInfo {
    const char* name; const char* desc; const char* cdInfo;
    int rarityIdx; int cost; bool passive; float maxCD; int maxCharges; int maxStack;
};
static const PowerupInfo PU_INFO[PU_COUNT] = {
    {"RETURN BY DEATH","On death rewinds 5s. No life lost.","1 use",      2,25, true,  0.f,1,1},
    {"DASH",           "Dash 4x forward. 0.5s invuln.",    "CD: 10s",     1,15, false,10.f,0,1},
    {"EL SHAMAK",      "Summon Beatrice for 7s.",          "1 use",       4,50, false, 0.f,1,1},
    {"LARPER",         "Place a ladder at feet. x4 stack.", "1 use (x4)", 2,10, false, 0.f,1,4},
    {"REINHARD",       "Instantly win the level.",         "1 use",       5,100,false, 0.f,1,1},
    {"WHIP",           "Kill enemies 5x forward.",         "CD: 25s",     0,10, false,25.f,0,1},
    {"EXTRA LIFE",     "+1 life. Raises cap to 5.",        "1 use",       4,70, false, 0.f,1,1},
    {"NUKE",           "Explosion: kills enemies.",        "1 use",       4,60, false, 0.f,1,1},
    {"ONE MORE LARP",  "Restore 1 life.",                  "1 use",       2,50, false, 0.f,1,1},
    {"SPEEDRUN",       "1.5x speed for 5s.",               "CD: 20s",     0,10, false,20.f,0,1},
    {"SHIELD",         "Block hits for 1.2s.",             "CD: 15s",     1,20, false,15.f,0,1},
    {"SHOP",           "Opens shop. Stops time.",          "1 use",       3,70, false, 0.f,1,1},
    {"BEATRICE",       "Companion shoots at enemies 7s.", "1 use",       2,30, false, 0.f,1,1},
};
// Powerup pool per rarity
static const PowerupType PU_BY_RARITY[6][4] = {
    {PU_WHIP,           PU_SPEEDRUN,      PU_NONE,        PU_NONE},   // Common
    {PU_DASH,           PU_SHIELD,        PU_NONE,        PU_NONE},   // Rare
    {PU_RETURN_BY_DEATH, PU_LARPER, PU_ONE_MORE_LARP, PU_BEATRICE},  // Stairs
    {PU_SHOP,           PU_NONE,          PU_NONE,        PU_NONE},   // Astolfo
    {PU_EL_SHAMAK,      PU_EXTRA_LIFE,    PU_NUKE_PU,     PU_NONE},   // Legendary
    {PU_REINHARD,       PU_NONE,          PU_NONE,        PU_NONE},   // Mythic
};
static const int PU_RARITY_COUNT[6] = {2,2,4,1,3,1};

struct HotbarSlot {
    PowerupType type    = PU_NONE;
    float       cd      = 0.f;   // remaining cooldown
    int         charges = 0;     // remaining uses (0=CD based)
};

// Card display / animation
struct CardDisplay {
    PowerupType type       = PU_NONE;
    int         rarity     = 0;
    float       scale      = 1.f;
    float       appearT    = 0.f;   // negative = stagger delay; 0→APPEAR_DUR scale grow
    float       heightFrac = 0.f;   // 0→1 uniform appear scale (both axes)
    bool        appeared   = false;
    float       hoverLerp  = 0.f;   // 0→1
    bool        hovered   = false;
    bool        selected  = false;
    bool        dismissed = false;
    float       exitT     = 0.f;   // 0→1 exit animation
    float       spinPhase = 0.f;   // 0→1 x-axis spin for selected card
    float       shakeTimer = 0.f; // > 0 while shake animation plays
};

// Return-by-death snapshot
struct RBDSnapshot {
    Rectangle   player; float vx,vy; bool facingR,onLadder; int curLadder; float ladderProg;
    int lives; unsigned score; int coins;
    struct BS{Rectangle h;int node;float spd;bool active,isBlue,isFalling;};
    struct ES{Rectangle h;Vector2 vel;EnemyType type;EnemyState st;float stT;bool active,grounded,facingR;};
    vector<BS> barrels; vector<ES> enemies;
    vector<NukeItem> nukes; vector<BeatriceItem> beatrices;
    bool regThrowing,regPending,regForceBlue,regActive,regStunned;
    float regThrowT; int regThrowFr; float regIdleT; int regIdleFr;
};

// Growing ladder (Larper)
struct LarperLadder { float x,y,curH,targH,growSpeed; };

// Hotbar drag item (during CARD_SELECT)
struct HBDragItem {
    bool        active   = false;
    PowerupType type     = PU_NONE;
    int         charges  = 0;
    float       cd       = 0.f;
    int         srcSlot  = -1;
    float       x        = 0.f, y = 0.f;
    float       velX     = 0.f, velY = 0.f;
    bool        falling  = false;
};

// ─────────────────────────────────────────────────────────────────────────────

static Barrel SpawnBarrel(const vector<PathNode>& path, int startNode = 0,
    float spd = 4.0f, float w = 26.25f, float h = 26.25f)
{
    Barrel b;
    if (path.empty()) { b.active = false; return b; }
    b.currentNode = startNode;
    b.speed = spd;
    b.isBlue = (GetRandomValue(0, 9) == 0);
    b.isFalling = false;
    b.movingLeft = false;
    b.animFrame = 0;
    b.animTimer = 0.0f;
    b.active = true;
    b.jumpScored = false;
    b.hitbox = { path[startNode].pos.x, path[startNode].pos.y, w, h };
    return b;
}

static bool SpawnBarrelFromPool(vector<Barrel>& barrels, const vector<PathNode>& path,
    float spd = 4.0f, float w = 26.25f, float h = 26.25f, bool forceBlue = false)
{
    if (path.empty()) return false;
    for (auto& b : barrels)
    {
        if (!b.active)
        {
            b = SpawnBarrel(path, 0, spd, w, h);
            if (forceBlue) b.isBlue = true;
            return true;
        }
    }
    return false;
}

static void UpdateBarrel(Barrel& b, const vector<PathNode>& path, float delta)
{
    if (!b.active || path.empty()) return;

    const PathNode& node = path[b.currentNode];
    Vector2         target = node.pos;
    Vector2         pos = { b.hitbox.x, b.hitbox.y };
    float           dist = Vector2Distance(pos, target);

    if (dist < b.speed)
    {
        b.hitbox.x = target.x;
        b.hitbox.y = target.y;
        if (!node.isSplitNode) b.isFalling = false;

        // Collect valid branches (equal probability)
        int validNext[3] = { -1, -1, -1 };
        int validCount = 0;
        for (int k = 0; k < 3; k++) {
            if (node.next[k] >= 0 && node.next[k] < (int)path.size())
                validNext[validCount++] = node.next[k];
        }

        if (validCount > 0)
        {
            int choice = GetRandomValue(0, validCount - 1);
            if (node.isSplitNode) { b.isFalling = (choice == 0 && validCount > 1); b.animFrame = 0; }
            b.currentNode = validNext[choice];
        }
        else { b.active = false; b.reachedEnd = true; }
    }
    else
    {
        Vector2 dir = Vector2Normalize(Vector2Subtract(target, pos));
        b.hitbox.x += dir.x * b.speed;
        b.hitbox.y += dir.y * b.speed;
        b.movingLeft = (dir.x < 0.0f);
    }

    float frameTime = b.isFalling ? 0.12f : 0.10f;
    int   frameCount = b.isFalling ? 2 : 4;
    b.animTimer += delta;
    if (b.animTimer >= frameTime) { b.animFrame = (b.animFrame + 1) % frameCount; b.animTimer = 0.0f; }
}

static void UpdateEnemy(Enemy& e, const Rectangle& playerRect,
    vector<Platform>& platforms, const vector<Ladder>& lads, float delta)
{
    if (!e.active) return;

    // ── Fall-off-map kill ─────────────────────────────────────────────────────
    if (e.hitbox.y > 1050.0f) { e.active = false; e.onLadder = false; return; }

    // ── Timers ────────────────────────────────────────────────────────────────
    if (e.ladderCooldown > 0.f) e.ladderCooldown -= delta;
    e.justJumped = false;

    // ── Ladder climbing ───────────────────────────────────────────────────────
    // Player ladder speed is 2.1*0.75 = 1.575; bunny climbs at 0.5x that = 0.7875
    static constexpr float BUNNY_CLIMB_SPEED = 2.1f * 0.75f * 0.5f;

    if (e.onLadder) {
        if (e.ladderIdx < 0 || e.ladderIdx >= (int)lads.size()) {
            e.onLadder = false;
        } else {
            const Ladder& lad = lads[e.ladderIdx];
            e.ladderProgress += BUNNY_CLIMB_SPEED / lad.ClimbHeight();
            e.hitbox.x = lad.x + lad.width * 0.5f - e.hitbox.width * 0.5f;
            e.hitbox.y = lad.PlayerYAtProgress(e.ladderProgress, e.hitbox.height);
            e.velocity = { 0.f, 0.f };
            e.grounded = false;
            if (e.ladderProgress >= 0.92f) {
                // Push the bunny clear above the ladder top, then let physics land it
                e.hitbox.y   = lad.y - e.hitbox.height - 6.f;
                e.velocity   = { 0.f, -1.5f };
                e.onLadder   = false;
                e.ladderIdx  = -1;
                e.ladderProgress = 0.f;
                e.grounded   = false;
                e.ladderCooldown = 0.6f; // prevent immediate re-entry
            }
        }
        // Animation while climbing
        static constexpr float climbAnimSpd = 0.18f;
        e.animTimer += delta;
        if (e.animTimer >= climbAnimSpd) { e.animTimer = 0.f; e.animFrame = (e.animFrame+1)%2; }
        return;
    }

    // ── Enter ladder if grounded and overlapping ──────────────────────────────
    // Only enter when truly grounded and not immediately after a jump, to prevent stuck loops
    if (e.grounded && e.ladderCooldown <= 0.f && !e.justJumped
        && e.state != ES_JUMP_TOWARD && e.state != ES_JUMP_BACK) {
        for (int i = 0; i < (int)lads.size(); i++) {
            Rectangle lhb = lads[i].GetHitbox();
            if (CheckCollisionRecs(e.hitbox, lhb)) {
                e.onLadder = true;
                e.ladderIdx = i;
                e.ladderProgress = Clamp(
                    lads[i].ProgressFromPlayerY(e.hitbox.y, e.hitbox.height), 0.f, 0.85f);
                e.velocity = { 0.f, 0.f };
                e.state = ES_IDLE;
                e.stateTimer = 0.f;
                e.ladderCooldown = 0.4f;
                return;
            }
        }
    }

    float jumpForce = (e.type == GRUNT) ? -5.5f : -5.0f;
    float speedToward = 4.0f;
    float speedBack = (e.type == GRUNT) ? 1.8f : 2.5f;
    float idleTime = (e.type == GRUNT) ? 2.0f : 1.65f;
    float pauseTime = (e.type == GRUNT) ? 1.45f : 0.28f;
    float animSpeed = (e.type == GRUNT) ? 0.22f : 0.14f;

    float playerCX = playerRect.x + playerRect.width * 0.5f;
    float enemyCX = e.hitbox.x + e.hitbox.width * 0.5f;
    bool  playerRight = (playerCX > enemyCX);

    e.stateTimer += delta;

    switch (e.state)
    {
    case ES_IDLE:
        e.velocity.x = 0.0f;
        if (e.stateTimer >= idleTime)
        {
            e.state = ES_JUMP_TOWARD;
            e.stateTimer = 0.0f;
            e.velocity.y = jumpForce;
            e.velocity.x = playerRight ? speedToward : -speedToward;
            e.facingRight = playerRight;
            e.grounded = false;
            e.justJumped = true;
        }
        break;

    case ES_JUMP_TOWARD:
        if (e.grounded && e.stateTimer > 0.12f)
        {
            e.state = ES_LAND_PAUSE;
            e.stateTimer = 0.0f;
            e.velocity.x = 0.0f;
        }
        break;

    case ES_LAND_PAUSE:
        e.velocity.x = 0.0f;
        if (e.stateTimer >= pauseTime)
        {
            e.state = ES_JUMP_BACK;
            e.stateTimer = 0.0f;
            e.velocity.y = jumpForce * 0.55f;
            e.velocity.x = playerRight ? -speedBack : speedBack;
            e.facingRight = !playerRight;
            e.grounded = false;
        }
        break;

    case ES_JUMP_BACK:
        if (e.grounded && e.stateTimer > 0.12f)
        {
            e.state = ES_IDLE;
            e.stateTimer = 0.0f;
            e.velocity.x = 0.0f;
        }
        break;
    }

    float prevX = e.hitbox.x, prevY = e.hitbox.y;
    if (!e.grounded) e.velocity.y += 0.4f;
    e.hitbox.x += e.velocity.x;
    e.hitbox.y += e.velocity.y;

    float vx = e.velocity.x, vy = e.velocity.y;
    CollisionResult col = CollisionManager::ResolveAll(
        e.hitbox, vx, vy, platforms, prevX, prevY);
    e.velocity.x = vx;
    e.velocity.y = vy;
    e.grounded = col.grounded;
    if (col.grounded) e.velocity.y = 0.0f;

    e.animTimer += delta;
    if (e.animTimer >= animSpeed)
    {
        e.animTimer = 0.0f;
        e.animFrame = (e.animFrame + 1) % 2;
    }
}

static void DrawEnemy(const Enemy& e,
    Texture2D& idleGrunt, Texture2D& jumpGrunt,
    Texture2D& idleSpecter, Texture2D& jumpSpecter)
{
    if (!e.active) return;

    // Show jump sprite only when actually in a jumping state (not briefly after ladder exit)
    Texture2D* tex = nullptr;
    bool airborne = !e.onLadder && (e.state == ES_JUMP_TOWARD || e.state == ES_JUMP_BACK);
    if (e.type == GRUNT)
        tex = airborne ? &jumpGrunt   : &idleGrunt;
    else
        tex = airborne ? &jumpSpecter : &idleSpecter;

    float scale = 2.5f;
    float drawW = tex->width * scale;
    float drawH = tex->height * scale;
    float drawX = e.hitbox.x + e.hitbox.width * 0.5f - drawW * 0.5f;
    float drawY = e.hitbox.y + e.hitbox.height - drawH;

    Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };
    if (!e.facingRight) src.width *= -1;

    DrawTexturePro(*tex, src, { drawX, drawY, drawW, drawH }, {}, 0.f, WHITE);
}

static void DrawBarrelPathDebug(const vector<PathNode>& path, const vector<Barrel>& barrels, int screenHeight)
{
    for (int i = 0; i < (int)path.size(); i++)
    {
        const PathNode& n = path[i];
        for (int b = 0; b < 2; b++)
        {
            if (n.next[b] == -1) continue;
            Color c = (b == 0) ? Color{ 255, 140, 0, 200 } : Color{ 0, 200, 255, 200 };
            DrawLineEx(n.pos, path[n.next[b]].pos, 2.5f, c);
        }
    }
    for (int i = 0; i < (int)path.size(); i++)
    {
        const PathNode& n = path[i];
        Color fill = BLACK;
        if (i == 0)                                         fill = WHITE;
        else if (n.next[0] == -1 && n.next[1] == -1)       fill = RED;
        else if (n.isSplitNode && n.rollThreshold == 10)    fill = ORANGE;
        else if (n.isSplitNode)                             fill = GREEN;
        else                                                fill = YELLOW;
        DrawCircleV(n.pos, 11.0f, BLACK);
        DrawCircleV(n.pos, 9.0f, fill);
        const char* lbl = TextFormat("%d", i);
        int tw = MeasureText(lbl, 9);
        DrawText(lbl, (int)n.pos.x - tw / 2, (int)n.pos.y - 4, 9, BLACK);
    }
    for (int i = 0; i < (int)barrels.size(); i++)
    {
        const Barrel& b = barrels[i];
        if (!b.active) continue;
        Vector2 c = { b.hitbox.x + b.hitbox.width * 0.5f, b.hitbox.y + b.hitbox.height * 0.5f };
        DrawCircleV(c, 7.0f, BLACK);
        DrawCircleV(c, 5.0f, b.isBlue ? BLUE : ORANGE);
        DrawText(TextFormat("B%d N%d", i, b.currentNode), (int)c.x + 8, (int)c.y - 5, 9, WHITE);
    }
    int lx = 10, ly = screenHeight - 125;
    DrawRectangle(6, ly, 190, 120, { 0,0,0,180 });
    DrawText("[F1] toggle debug", lx, ly + 4, 9, GRAY);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 20) }, 5.f, WHITE);  DrawText("Start", lx + 14, ly + 15, 9, WHITE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 34) }, 5.f, GREEN);  DrawText("Split 50/50", lx + 14, ly + 29, 9, GREEN);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 48) }, 5.f, ORANGE); DrawText("Edge (fall)", lx + 14, ly + 43, 9, ORANGE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 62) }, 5.f, YELLOW); DrawText("Obligatory", lx + 14, ly + 57, 9, YELLOW);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 76) }, 5.f, RED);    DrawText("End", lx + 14, ly + 71, 9, RED);
    DrawLineEx({ (float)lx,(float)(ly + 90) }, { (float)(lx + 18),(float)(ly + 90) }, 2.f, { 255,140,0,255 }); DrawText("Stair/Fall", lx + 22, ly + 85, 9, { 255,140,0,255 });
    DrawLineEx({ (float)lx,(float)(ly + 104) }, { (float)(lx + 18),(float)(ly + 104) }, 2.f, { 0,200,255,255 }); DrawText("Flat/Roll", lx + 22, ly + 99, 9, { 0,200,255,255 });
    DrawText("Dot=barrel", lx, ly + 113, 9, GRAY);
}

// ─────────────────────────────────────────────────────────────────────────────
int main(void)
{
    const int screenWidth = 875;
    const int screenHeight = 950;

    GameScreen  currentScreen = SPLASH_SCREEN;
    float       splashTimer = 0.0f;
    const float splashDuration = 5.0f;

    int       selectedOption = 0;
    int       settingsTab      = 0;   // 0=Controls, 1=Settings, 2=Debug
    float     settingsDbgScroll= 0.f; // scroll offset for DEBUG tab
    Rectangle btnPlay    = { 340, 450, 200, 40 };
    Rectangle btnExit    = { 340, 500, 200, 40 };
    Rectangle btnEditor  = { 340, 550, 200, 40 };
    Rectangle btnSettings= { 340, 600, 200, 40 };

    bool debugPath = false;

    // ── Debug menu ────────────────────────────────────────────────────────────
    bool  dbgMenuOpen      = false;
    float dbgMenuScroll    = 0.f;
    bool  dbgImmortal      = false;
    bool  dbgFlight        = false;
    bool  dbgFlightNoCol   = false;
    int   dbgGivePUIdx     = 0;
    bool  dbgBloomEnabled  = true;
    float dbgBloomThreshold= 0.7f;
    float dbgBloomIntensity= 0.8f;
    bool  isPaused         = false;

    // ── Audio volumes (music / sfx / abilities / ui) ─────────────────────────
    float volMusic   = 0.8f; // background music
    float volSFX     = 1.f;  // gameplay sounds (death, hit, barrel jump, RBD)
    float volAbility = 2.f;  // ability activation sounds (shield, dash, whip, etc.) — 200% default
    float volUI      = 1.f;  // UI/card sounds

    // ── Music fade-out state ──────────────────────────────────────────────────
    static constexpr float MUSIC_FADE_DUR = 0.5f;
    float musicFadeTimer    = 0.f;   // counts down from MUSIC_FADE_DUR to 0
    bool  musicPendingPause = false; // pause stream once fade reaches 0

    // ── Heavy item equip state ─────────────────────────────────────────────────
    float heavyEquipTimer  = 0.f;
    bool  isCarryingHeavy  = false;

    // ── Shield animation ──────────────────────────────────────────────────────
    float shieldAnimTimer  = 0.f;
    int   shieldAnimFrame  = 0;

    unsigned int score = 0;
    unsigned int runScore     = 0;  // points this run (coins*10 + level bonuses), never decreases
    int          levelsCleared = 0; // levels completed this run
    unsigned int highScore    = 0;  // all-time best runScore
    int          highLevels   = 0;  // all-time most levels cleared in one run
    int          currentLevelId = 1;

    // ── Level priority system ────────────────────────────────────────────────
    static constexpr int MAX_LEVEL_ID = 20;
    int levelPriority[MAX_LEVEL_ID + 1] = {};  // index = level id
    int levelRangeMin = 1, levelRangeMax = 10;

    LightingSystem gameLighting;
    LevelData      currentLevelData;
    LevelData      pendingLevelData;
    bool           hasPendingLevel = false;

    // ── Card select system ───────────────────────────────────────────────────
    static constexpr int MAX_DISP_CARDS = 5;
    CardDisplay    displayCards[MAX_DISP_CARDS] = {};
    int            numDispCards  = 0;
    bool           anyCardPicked = false;
    float          cardFadeOut   = 0.f;  // 0→1 fade to black after pick
    enum { CSM_LEVEL, CSM_SHOP } cardSelectMode = CSM_LEVEL;

    // ── Hotbar ───────────────────────────────────────────────────────────────
    HotbarSlot     hotbar[3];
    int            hotbarSlot    = 0;   // 0-2 active slot

    // ── Powerup effect state ─────────────────────────────────────────────────
    int            coins         = 0;
    int            maxLives      = 3;
    bool           speedrunActive= false;
    float          speedrunTimer = 0.f;
    bool           shieldActive  = false;
    float          shieldTimer   = 0.f;
    bool           dashActive    = false;
    float          dashInvulTimer= 0.f;

    // ── Return-by-death ──────────────────────────────────────────────────────
    static constexpr int RBD_BUF = 50;
    RBDSnapshot    rbdBuf[RBD_BUF] = {};
    int            rbdHead    = 0;
    int            rbdCount   = 0;
    float          rbdSaveTimer = 0.f;
    bool           rbdTriggered = false;
    float          rbdFadeTimer = 0.f;   // 2s fade-in after rewind
    bool           rbdFading    = false;

    // ── Larper ladders ───────────────────────────────────────────────────────
    vector<LarperLadder> larperLadders;

    // ── Hotbar drag (during CARD_SELECT) ─────────────────────────────────────
    HBDragItem hbDrag;

    Rectangle wincondition = { 400, 150, 40, 40 };

    float playerSpawnX = 35.0f + 64.0f * 3.5f + 10.0f;
    float playerSpawnY = 817.0f;

    LevelEditor editor;

    const float SUBARU_ANIM_FPS = 5.0f;
    const int   SUBARU_FRAME_COUNT = 5;
    int         subaruFrame = 0;
    float       subaruTimer = 0.0f;

    // ── Player ────────────────────────────────────────────────────────────────
    Rectangle player = { 35.0f + 64.0f * 3.5f + 10.0f, 817.0f, 63, 63 };
    float     playerSpeed = 2.0f * 0.9f;
    float     jumpForce = -8.0f * 0.94f;
    float     gravity = 0.4f;
    float     velocityX = 0.0f;
    float     velocityY = 0.0f;
    bool      isJumping = false;
    bool      facingRight = true;
    bool      isGrounded = false;
    bool      wasGrounded = false;
    float     airborneTime = 0.f;    // time spent ungrounded; gates landing SFX
    float     playerStepDist = 0.f;

    // ── Whip visual ───────────────────────────────────────────────────────────
    bool  whipActive      = false;
    float whipTimer       = 0.f;
    bool  whipFacingRight = true;

    int         lives = 3;
    bool        invincible = false;
    float       invincibleTimer = 0.0f;
    const float invincibleDuration = 1.5f;

    // ── Ladder ────────────────────────────────────────────────────────────────
    bool  onLadder = false;
    int   currentLadder = -1;
    float ladderProgress = 0.0f;
    float ladderClimbSpeed = 2.1f;
    int   ladderClimbFrame = 0;
    float ladderClimbTimer = 0.0f;
    float ladderClimbAnimSpeed = 0.15f;
    bool  ladderExitPlaying = false;
    int   ladderExitStep = 0;
    float ladderExitTimer = 0.0f;
    float ladderExitFrameDuration = 0.12f;
    float ladderCooldown = 0.0f;
    float ladderStepDist = 0.f;
    bool  ladderEntryClamp = false;
    float ladderEntryClampTimer = 0.0f;
    float ladderEntryClampStart = 0.0f;

    // ── Death sequence ────────────────────────────────────────────────────────
    bool    isDying = false;
    float   deathTimer = 0.0f;
    bool    hitPlayed = false;
    bool    deathPlayed = false;
    float   deathFallVelY = 0.0f;
    bool    deathReachedBlack = false;
    float   deathBlackTimer = 0.0f;
    float   deathShakeTimer = 0.0f;
    Vector2 deathShakeOffset = { 0, 0 };
    float   dashShakeTimer  = 0.0f;
    Vector2 dashShakeOffset = { 0, 0 };

    // ── House / explosion ─────────────────────────────────────────────────────
    const float HOUSE_ANIM_FPS = 10.0f;
    const int   HOUSE_SWAP_AT_FRAME = 4;
    const float HOUSE_DRAW_SCALE = 3.5f;
    const float CAVE_DRAW_SCALE = 4.335f;
    const float FLOOR_DRAW_SCALE = 4.0f;
    const float HOUSE_NATIVE_W = 64.0f;
    const float HOUSE_NATIVE_H = 32.0f;
    const float CAVE_NATIVE_W = 64.0f;
    const float CAVE_NATIVE_H = 32.0f;
    const float FLOOR_NATIVE_W = 80.0f;
    const float FLOOR_NATIVE_H = 16.0f;

    float     houseW = HOUSE_NATIVE_W * HOUSE_DRAW_SCALE;
    float     houseH = HOUSE_NATIVE_H * HOUSE_DRAW_SCALE;
    float     houseX = 35.0f;
    float     houseY = 880.0f - houseH;
    Rectangle houseHitbox = { houseX, houseY, houseW, houseH };
    bool      houseAnimPlaying = false;
    int       houseAnimFrame = 0;
    float     houseAnimTimer = 0.0f;
    bool      houseIsSnowed = false;
    float     caveSpawnTimer = 0.f;

    // ── Nuke ──────────────────────────────────────────────────────────────────
    const float NUKE_SCALE = 1.5f;
    const float NUKE_NATIVE_W = 74.0f;
    const float NUKE_NATIVE_H = 35.0f;
    const float NUKE_EXPL_FPS = 10.0f;
    const float NUKE_FLASH_IN = 1.0f;
    const float NUKE_FLASH_OUT = 4.0f;
    const float NUKE_SHAKE_AMOUNT = 12.0f;
    bool        playerHasNuke = false;
    bool        playerHasBeatrice = false;
    float       beatriceItemAnimTimer = 0.0f;
    int         beatriceItemAnimFrame = 0;
    float       nukeExtraDelay = 0.0f;
    bool        nukeExplosionPlaying = false;
    int         nukeExplosionFrame = 0;
    float       nukeExplosionTimer = 0.0f;
    Vector2     nukeExplosionPos = { 0, 0 };
    float       nukeFlashTimer = 5.0f;
    Vector2     nukeShakeOffset = { 0, 0 };

    // ── Regulus ───────────────────────────────────────────────────────────────
    const float REGULUS_SCALE = 3.5f * 0.7f * 1.2f;
    const float REGULUS_X = 22.0f;
    const float REGULUS_IDLE_FPS = 3.6f;
    const float REGULUS_THROW_FPS = 4.7f;

    int   regulusIdleFrame = 0;
    float regulusIdleTimer = 0.0f;
    int   regulusThrowFrame = 0;
    float regulusThrowTimer = 0.0f;
    bool  regulusThrowing = false;
    bool  regulusSpawnPending = false;
    bool  regulusForceBlue = false;

    const float REGULUS_STUN_FPS = 6.0f;
    const float REGULUS_STUN_END_FPS = 5.0f;
    const int   REGULUS_STUN_LOOPS = 2;
    bool  regulusIsStunned = false;
    bool  regulusStunEnding = false;
    int   regulusStunFrame = 0;
    float regulusStunTimer = 0.0f;
    int   regulusStunLoops = 0;
    int   regulusStunEndFrame = 0;
    float regulusStunEndTimer = 0.0f;

    bool  regulusIsActive = true;
    float regulusStateTickTimer = 0.0f;
    float regulusInactiveTime = 0.0f;
    int   regulusActiveFails = 0;
    int   regulusInactiveFails = 0;
    float regulusActiveSpawnTimer = 0.0f;

    // ── Platforms & ladders ───────────────────────────────────────────────────
    vector<ElevatorData>        liveElevators;
    vector<ParentChildRelation> liveRelations;
    vector<float>               elevChildPhases;
    vector<float>               liveElevTravel;    // per-elevator travel for backAndForth
    vector<int>                 liveElevBounceDir; // per-elevator bounce dir (+1/-1)
    vector<KillZoneData>        liveKillZones;
    vector<ConveyorData>        liveConveyors;
    int                         conveyorPlatStart = 0;

    vector<Platform> platforms = {
        Platform::Make(27,  880, 412, 0,  0.0f),
        Platform::Make(430, 870, 420, 0, -3.0f),
        Platform::Make(60,  750, 720, 0,  3.0f),
        Platform::Make(110, 620, 720, 0, -3.0f),
        Platform::Make(60,  490, 720, 0,  3.0f),
        Platform::Make(110, 360, 720, 0, -3.0f),
        Platform::Make(460, 246, 320, 0,  3.0f),
        Platform::Make(60,  240, 400, 0,  0.0f),
    };

    vector<Ladder> ladders = {
        Ladder::Make(675, 245, 40, 104),
        Ladder::Make(160, 375, 40, 102),
        Ladder::Make(300, 365, 40, 117),
        Ladder::Make(680, 495, 40, 110),
        Ladder::Make(430, 489, 40, 128),
        Ladder::Make(380, 621, 40, 124),
        Ladder::Make(160, 632, 40, 101),
        Ladder::Make(670, 760, 40, 105),
    };

    // ── Enemies ───────────────────────────────────────────────────────────────
    vector<Vector2> enemySpawnPositions = {
        { 180.0f, 706.0f }, { 620.0f, 706.0f },
        { 200.0f, 576.0f }, { 550.0f, 576.0f },
        { 160.0f, 446.0f }, { 520.0f, 446.0f },
        { 200.0f, 316.0f }, { 500.0f, 316.0f },
    };

    vector<Enemy> enemies(8);
    for (auto& en : enemies) en.active = false;

    // ── Barrel path ───────────────────────────────────────────────────────────
    vector<PathNode> barrelPath = {
        /*  0 */ { {125, 210},  {  1, -1 },  9, false },
        /*  1 */ { {438, 210},  {  2, -1 },  5, false },
        /*  2 */ { {680, 219},  {  3,  4 },  5, true  },
        /*  3 */ { {680, 319},  {  6, -1 },  5, false },
        /*  4 */ { {780, 224},  {  5, -1 }, 10, true  },
        /*  5 */ { {800, 313},  {  6, -1 },  5, false },
        /*  6 */ { {550, 326},  {  7, -1 },  5, false },
        /*  7 */ { {305, 339},  {  8,  9 },  5, true  },
        /*  8 */ { {305, 454},  { 11, -1 },  5, false },
        /*  9 */ { {110, 349},  { 10, -1 }, 10, true  },
        /* 10 */ { { 70, 442},  { 11, -1 },  5, false },
        /* 11 */ { {430, 461},  { 12, -1 },  5, false },
        /* 12 */ { {685, 474},  { 13, 14 },  5, true  },
        /* 13 */ { {685, 579},  { 16, -1 },  5, false },
        /* 14 */ { {780, 479},  { 15, -1 }, 10, true  },
        /* 15 */ { {800, 573},  { 16, -1 },  5, false },
        /* 16 */ { {550, 586},  { 17, -1 },  5, false },
        /* 17 */ { {165, 606},  { 18, 19 },  5, true  },
        /* 18 */ { {165, 707},  { 21, -1 },  5, false },
        /* 19 */ { {110, 609},  { 20, -1 }, 10, true  },
        /* 20 */ { { 70, 702},  { 21, -1 },  5, false },
        /* 21 */ { {430, 721},  { 22, -1 },  5, false },
        /* 22 */ { {675, 733},  { 23, 24 },  5, true  },
        /* 23 */ { {675, 838},  { 26, -1 },  5, false },
        /* 24 */ { {780, 739},  { 25, -1 }, 10, true  },
        /* 25 */ { {800, 832},  { 26, -1 },  5, false },
        /* 26 */ { {400, 850},  { 27, -1 },  5, false },
        /* 27 */ { {148, 850},  { -1, -1 },  5, false },
    };

    vector<Barrel>    barrels(100);
    for (auto& b : barrels) b.active = false;

    vector<FlyingNuke> flyingNukes;

    // ── Nuke spawn positions ──────────────────────────────────────────────────
    vector<Vector2> nukeSpawnNodes = {
        { 150.0f, 845.0f }, { 330.0f, 845.0f },
        { 180.0f, 693.0f }, { 480.0f, 693.0f }, { 650.0f, 693.0f },
        { 250.0f, 563.0f }, { 570.0f, 563.0f },
        { 150.0f, 433.0f }, { 500.0f, 433.0f },
        { 300.0f, 303.0f },
    };
    vector<Vector2> nukeRespawnNodes = {
        { 200.0f, 303.0f }, { 380.0f, 303.0f }, { 520.0f, 303.0f },
        { 150.0f, 433.0f }, { 350.0f, 433.0f }, { 520.0f, 433.0f },
        { 200.0f, 350.0f }, { 450.0f, 350.0f },
    };
    vector<NukeItem> nukes;
    if (!nukeSpawnNodes.empty()) {
        int idx = GetRandomValue(0, (int)nukeSpawnNodes.size() - 1);
        nukes.push_back({ nukeSpawnNodes[idx], true });
    }

    // ── Beatrice spawn positions ──────────────────────────────────────────────
    vector<Vector2> beatriceSpawnNodes = {
        { 250.0f, 750.0f }, { 500.0f, 750.0f },
        { 150.0f, 620.0f }, { 420.0f, 620.0f }, { 660.0f, 620.0f },
        { 200.0f, 490.0f }, { 480.0f, 490.0f },
        { 310.0f, 360.0f }, { 560.0f, 360.0f },
    };
    vector<BeatriceItem> beatrices;
    if (!beatriceSpawnNodes.empty()) {
        int idx = GetRandomValue(0, (int)beatriceSpawnNodes.size() - 1);
        beatrices.push_back({ beatriceSpawnNodes[idx], true });
    }

    // ── Beatrice ability ──────────────────────────────────────────────────────
    const float BEATRICE_DURATION = 7.0f;
    const float BEA_BULLET_SHOOT_INTERVAL = 0.6f;
    const float BEA_BULLET_SPEED = 500.0f;
    const float BEA_BULLET_LIFETIME = 5.0f;
    float       beatriceAbilityTimer = 0.0f;
    float       beaBulletShootTimer = 0.0f;
    vector<BeaBullet> beaBullets(20);
    for (auto& bb : beaBullets) bb.active = false;

    // ── Beam positions ────────────────────────────────────────────────────────
    vector<BeamData> beamPositions = {
        {  50, 225 }, { 114, 225 }, { 178, 225 }, { 212, 225 },
        { 276, 225 }, { 340, 225 }, { 372, 225 }, { 436, 230 },
        { 468, 230 }, { 532, 235 }, { 564, 235 }, { 628, 240 },
        { 660, 240 }, { 724, 245 },
        { 110, 365 }, { 142, 365 }, { 206, 360 }, { 238, 360 },
        { 302, 355 }, { 334, 355 }, { 398, 350 }, { 430, 350 },
        { 494, 345 }, { 526, 345 }, { 590, 340 }, { 622, 340 },
        { 686, 335 }, { 718, 335 }, { 782, 330 },
        {  54, 460 }, {  86, 460 }, { 150, 465 }, { 182, 465 },
        { 246, 470 }, { 278, 470 }, { 342, 475 }, { 374, 475 },
        { 438, 480 }, { 470, 480 }, { 534, 485 }, { 566, 485 },
        { 630, 490 }, { 662, 490 }, { 726, 495 },
        { 110, 625 }, { 142, 625 }, { 206, 620 }, { 238, 620 },
        { 302, 615 }, { 334, 615 }, { 398, 605 }, { 430, 605 },
        { 494, 600 }, { 526, 600 }, { 590, 595 }, { 622, 595 },
        { 686, 590 }, { 718, 590 }, { 782, 585 },
        {  54, 715 }, {  86, 715 }, { 150, 720 }, { 182, 720 },
        { 246, 725 }, { 278, 725 }, { 342, 730 }, { 374, 730 },
        { 438, 735 }, { 470, 735 }, { 534, 740 }, { 566, 740 },
        { 630, 745 }, { 662, 745 }, { 726, 750 },
        {  30, 865 }, {  94, 865 }, { 158, 865 }, { 192, 865 },
        { 256, 865 }, { 320, 865 }, { 352, 865 }, { 416, 860 },
        { 448, 860 }, { 512, 855 }, { 544, 855 }, { 608, 850 },
        { 640, 850 }, { 704, 845 }, { 768, 845 },
        { 360, 120 }, { 424, 120 }, { 456, 120 },
        { 296, 150 }, { 264, 150 },
    };

    // Initial Regulus throw on startup
    regulusThrowing = true;
    regulusSpawnPending = true;
    regulusForceBlue = true;
    regulusThrowFrame = 0;
    regulusThrowTimer = 0.0f;

    // ── Window / audio init ───────────────────────────────────────────────────
    InitWindow(screenWidth, screenHeight, "Donkey Kong");
    editor.Init(screenWidth, screenHeight);
    gameLighting.Init(screenWidth, screenHeight, LightingSystem::Quality::MEDIUM);
    gameLighting.SetGlobalAmbient(0.06f);
    gameLighting.SetGlobalDarkness(1.00f);
    gameLighting.SetAmbientColor({ 25, 30, 50, 255 });
    Cinematic::Global.LoadAll();
    SetRandomSeed((unsigned int)time(NULL));
    InitAudioDevice();
    SetExitKey(KEY_NULL);

    TraceLog(LOG_INFO, TextFormat("Working Directory: %s", GetWorkingDirectory()));

    Music music = LoadMusicStream("Assets/Nuevo audio/mp3/Danza.mp3");
    Sound deathSound = LoadSound("Assets/Nuevo audio/mp3/20. Dead.mp3");
    Sound HitSound = LoadSound("Assets/Nuevo audio/mp3/19. Bonus.mp3");
    Sound nukeSound = LoadSound("Assets/Nuevo audio/mp3/Flash.mp3");
    Sound jumpBrlSound = LoadSound("Assets/Nuevo audio/mp3/19. Bonus.mp3");
    Sound rbdSound     = LoadSound("Assets/Nuevo audio/mp3/re-zero-return-by-death.mp3");

    // Ability activation SFX — dedicated .wav files where available
    static const char* WAV = "Assets/Nuevo audio/.wav/";
    static const char* BALATRO = "Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/";
    Sound sndShield        = LoadSound(TextFormat("%s%s", WAV,     "Shield.wav"));
    Sound sndDash          = LoadSound(TextFormat("%s%s", WAV,     "Dash.wav"));
    Sound sndBeatrice      = LoadSound(TextFormat("%s%s", WAV,     "PickBeatrice.wav"));
    static const char* NEWSFX = "Assets/Nuevo audio/NewSFX/";
    Sound sndBeatriceAtk   = LoadSound(TextFormat("%s%s", NEWSFX,  "MagicShot.wav"));
    Sound sndLarper        = LoadSound(TextFormat("%s%s", WAV,     "Escalera.wav"));
    Sound sndSpeedrun      = LoadSound(TextFormat("%s%s", WAV,     "Increasing Speed.wav"));
    Sound sndWhip[3] = {
        LoadSound(TextFormat("%s%s", NEWSFX, "WhipStart1.wav")),
        LoadSound(TextFormat("%s%s", NEWSFX, "WhipStart2.wav")),
        LoadSound(TextFormat("%s%s", NEWSFX, "WhipStart3.wav")),
    };
    Sound sndEnemyKill     = LoadSound(TextFormat("%s%s", NEWSFX, "NPC_Killed_1.wav"));
    Sound sndExtraLife     = LoadSound(TextFormat("%s%s", WAV,     "ExtraLife.wav"));
    Sound sndOneLarp       = LoadSound(TextFormat("%s%s", BALATRO, "coin1.ogg"));
    Sound sndError         = LoadSound(TextFormat("%s%s", BALATRO, "cancel.ogg"));
    Sound sndReinhard      = LoadSound(TextFormat("%s%s", BALATRO, "gong.ogg"));
    Sound sndBunnyJump     = LoadSound(TextFormat("%s%s", WAV, "Arcade - Donkey Kong - Sound Effects_grunt.wav"));
    Sound sndMoney         = LoadSound(TextFormat("%s%s", WAV, "Money.wav"));

    // ── Footstep sounds ───────────────────────────────────────────────────────
    static const char* MINE = "Assets/Nuevo audio/Mine/";
    struct FootstepSounds { std::vector<Sound> walk, run; };
    // Indexed by BeamMaterial (0=NONE, 1=BLUNTWOOD … 16=SQUEAKYWOOD)
    static const struct { const char* name; int walkN, runN; } MAT_INFO[] = {
        { "",           0,  0 },  // NONE
        { "bluntwood", 11,  0 },
        { "concrete",  11, 11 },
        { "deckwood",   0, 11 },
        { "dirt",      11, 11 },
        { "grass",     10,  4 },
        { "gravel",    11, 11 },
        { "lino",       8,  6 },
        { "marble",    11, 11 },
        { "metalbar",  11, 11 },
        { "metalbox",   9, 10 },
        { "mud",        6,  0 },
        { "sand",      11, 11 },
        { "snow",      11, 11 },
        { "stone",     11, 11 },
        { "wood",      11,  0 },
        { "squeakywood",11, 0 },
    };
    static const int MAT_COUNT = 17;
    FootstepSounds footsteps[MAT_COUNT];
    for (int mi = 1; mi < MAT_COUNT; mi++) {
        const char* mat = MAT_INFO[mi].name;
        footsteps[mi].walk.resize(MAT_INFO[mi].walkN);
        for (int n = 0; n < MAT_INFO[mi].walkN; n++)
            footsteps[mi].walk[n] = LoadSound(TextFormat("%s%s/%s_walk%d.ogg", MINE, mat, mat, n + 1));
        footsteps[mi].run.resize(MAT_INFO[mi].runN);
        for (int n = 0; n < MAT_INFO[mi].runN; n++)
            footsteps[mi].run[n] = LoadSound(TextFormat("%s%s/%s_run%d.ogg", MINE, mat, mat, n + 1));
    }

    // Card / UI sounds (Balatro SFX)
    Sound cardFanSnd   = LoadSound("Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/cardFan2.ogg");
    Sound cardSlideSnd = LoadSound("Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/cardSlide1.ogg");
    Sound cardHoverSnd = LoadSound("Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/paper1.ogg");
    Sound cardPickSnd  = LoadSound("Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/card1.ogg");
    Sound cardThrowSnd = LoadSound("Assets/Nuevo audio/PC _ Computer - Balatro - Miscellaneous - Sound Effects/whoosh1.ogg");

    SetMasterVolume(1.0f);
    SetMusicVolume(music, volMusic);
    SetMusicPan(music, 0.0f);
    PlayMusicStream(music);

    // ── Textures ──────────────────────────────────────────────────────────────
    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Texture2D imgMarioFalling = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Falling.png");
    Texture2D background = LoadTexture("Wiki/SubaruStairs.png");
    Texture2D beam = LoadTexture("Assets/Textures/Architecture/Dk_FloorPart.png");

    Texture2D beamVariants[12];
    for (int i = 0; i < 10; i++) {
        char path[128];
        snprintf(path, sizeof(path), "Assets/Textures/Architecture/Dk_FloorPart%d.png", i + 1);
        beamVariants[i] = LoadTexture(path);
    }
    beamVariants[10] = LoadTexture("Assets/Textures/Architecture/TransFloor.png");
    beamVariants[11] = LoadTexture("Assets/Textures/Architecture/TransFloor2.png");

    Texture2D imgMarioClimb1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder1.png");
    Texture2D imgMarioClimb2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder2.png");
    Texture2D imgMarioClimbEnd1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd1.png");
    Texture2D imgMarioClimbEnd2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd2.png");
    Texture2D imgMarioClimbDown = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_IdleBack.png");

    static constexpr int   EMOTE_FRAME_COUNT = 5;
    static constexpr float EMOTE_FRAME_SPD = 0.12f;
    Texture2D imgSubaruDance[EMOTE_FRAME_COUNT] = {
        LoadTexture("Assets/Textures/Characters/Mario/SubaruDance1.png"),
        LoadTexture("Assets/Textures/Characters/Mario/SubaruDance2.png"),
        LoadTexture("Assets/Textures/Characters/Mario/SubaruDance3.png"),
        LoadTexture("Assets/Textures/Characters/Mario/SubaruDance4.png"),
        LoadTexture("Assets/Textures/Characters/Mario/SubaruDance5.png"),
    };
    bool  isEmoting = false;
    int   emoteFrame = 0;
    float emoteTimer = 0.f;

    Texture2D LadderPart = LoadTexture("Assets/Textures/Architecture/Dk_Ladder.png");
    Texture2D RopeTex = LoadTexture("Assets/Textures/Architecture/Rope.png");
    Texture2D GoldenPistonTex = LoadTexture("Assets/Textures/Items/GoldenPiston.png");
    Texture2D ConvSide[3] = {
        LoadTexture("Assets/Textures/Items/ConveyorSide_1.png"),
        LoadTexture("Assets/Textures/Items/ConveyorSide_2.png"),
        LoadTexture("Assets/Textures/Items/ConveyorSide_3.png")
    };
    Texture2D ConvM[3] = {
        LoadTexture("Assets/Textures/Items/ConveyorMid_1.png"),
        LoadTexture("Assets/Textures/Items/ConveyorMid_2.png"),
        LoadTexture("Assets/Textures/Items/ConveyorMid_3.png")
    };
    Texture2D SnowFloor = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Floor.png");

    Texture2D BarrelMov1 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Mov1.png");
    Texture2D BarrelMov2 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Mov2.png");
    Texture2D BarrelMov3 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Mov3.png");
    Texture2D BarrelMov4 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Mov4.png");
    Texture2D BarrelFall1 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Fall1.png");
    Texture2D BarrelFall2 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Fall2.png");
    Texture2D BlueBarrelMov1 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Mov1.png");
    Texture2D BlueBarrelMov2 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Mov2.png");
    Texture2D BlueBarrelMov3 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Mov3.png");
    Texture2D BlueBarrelMov4 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Mov4.png");
    Texture2D BlueBarrelFall1 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Fall1.png");
    Texture2D BlueBarrelFall2 = LoadTexture("Assets/Textures/Barrel/Dk_Barrel_Blue_Fall2.png");

    Texture2D House1 = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_House_1.png");
    Texture2D House2 = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_House_Snowed.png");
    Texture2D cave1 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion.png");
    Texture2D cave2 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_2.png");
    Texture2D cave3 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_3.png");
    Texture2D cave4 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_4.png");
    Texture2D cave5 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_5.png");
    Texture2D cave6 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_6.png");
    Texture2D cave7 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_7.png");
    Texture2D cave8 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_8.png");
    Texture2D cave9 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_9.png");
    Texture2D cave10 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_10.png");
    Texture2D cave11 = LoadTexture("Assets/Textures/Characters/FireSprites/Snow_Explosion_11.png");

    const int CAVE_FRAME_COUNT = 11;
    Texture2D* caveFrames[CAVE_FRAME_COUNT] = {
        &cave1,&cave2,&cave3,&cave4,&cave5,
        &cave6,&cave7,&cave8,&cave9,&cave10,&cave11
    };

    Texture2D Nuke = LoadTexture("Assets/Textures/Items/Nuke.png");
    Texture2D imgMarioIdleNuke = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_NukeHold_Idle.png");
    Texture2D imgMarioWalk1Nuke = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_NukeHold_Walk1.png");
    Texture2D imgMarioWalk2Nuke = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_NukeHold_Walk2.png");
    Texture2D imgMarioJumpNuke = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_NukeHold_Jump.png");

    Texture2D Explosion1 = LoadTexture("Assets/Textures/Items/Explosion1.png");
    Texture2D Explosion2 = LoadTexture("Assets/Textures/Items/Explosion2.png");
    Texture2D Explosion3 = LoadTexture("Assets/Textures/Items/Explosion3.png");
    Texture2D Explosion4 = LoadTexture("Assets/Textures/Items/Explosion4.png");
    Texture2D Explosion5 = LoadTexture("Assets/Textures/Items/Explosion5.png");
    Texture2D Explosion6 = LoadTexture("Assets/Textures/Items/Explosion6.png");

    Texture2D Rain = LoadTexture("Assets/Textures/Architecture/Rain.png");
    Texture2D Rain2 = LoadTexture("Assets/Textures/Architecture/Rain2.png");

    Texture2D RegulusGrab1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab1.png");
    Texture2D RegulusGrab2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab2.png");
    Texture2D RegulusGrab3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab3.png");
    Texture2D RegulusIdle1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle1.png");
    Texture2D RegulusIdle2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle2.png");
    Texture2D RegulusIdle3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle3.png");
    Texture2D Regulus_Stun1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun1.png");
    Texture2D Regulus_Stun2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun2.png");
    Texture2D Regulus_Stun3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun3.png");
    Texture2D Regulus_StunEnd1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd1.png");
    Texture2D Regulus_StunEnd2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd2.png");
    Texture2D Regulus_StunEnd3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd3.png");
    Texture2D Regulus_StunEnd4 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd4.png");
    Texture2D Regulus_StunEnd5 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd5.png");

    Texture2D Dk_Mario_Idle1_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1_Beatrice.png");
    Texture2D Dk_Mario_Idle2_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle2_Beatrice.png");
    Texture2D Dk_Mario_Jump_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump_Beatrice.png");
    Texture2D Dk_Mario_Walk1_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1_Beatrice.png");
    Texture2D Dk_Mario_Walk2_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2_Beatrice.png");
    Texture2D Beatrice_Idle1 = LoadTexture("Assets/Textures/Characters/Beatrice/Beatrice_Idle1.png");
    Texture2D Beatrice_Idle2 = LoadTexture("Assets/Textures/Characters/Beatrice/Beatrice_Idle2.png");
    Texture2D texBeaBullet = LoadTexture("Assets/Textures/Characters/Beatrice/BeaBullet.png");

    Texture2D Subaru1 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru1.png");
    Texture2D Subaru2 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru2.png");
    Texture2D Subaru3 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru3.png");
    Texture2D Subaru4 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru4.png");
    Texture2D Subaru5 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru5.png");
    Texture2D Subaru_Background = LoadTexture("Assets/Textures/Characters/Subaru/Subaru_Background.png");

    Texture2D rabbitIdleBlack = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Blue1.png");
    Texture2D rabbitJumpBlack = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Jump_Blue1.png");
    Texture2D rabbitIdleWhite = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite1.png");
    Texture2D rabbitJumpWhite = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Jump1.png");

    Texture2D FButton      = LoadTexture("Assets/Textures/UI/FButton.png");
    Texture2D texGoldHeart = LoadTexture("Assets/Textures/Cards/GoldHeart.png");
    Texture2D texHeart     = LoadTexture("Assets/Textures/Cards/heart.png");
    Texture2D texShield1   = LoadTexture("Assets/Textures/Cards/Shield1.png");
    Texture2D texShield2   = LoadTexture("Assets/Textures/Cards/Shield2.png");

    static constexpr int PROP_TEX_COUNT = 6;
    static constexpr int PROP_FIRE_VARIANT = 5;
    Texture2D propTextures[PROP_TEX_COUNT] = {
        LoadTexture("Assets/Textures/Lighting/Light.png"),
        LoadTexture("Assets/Textures/Architecture/WoodBox.png"),
        LoadTexture("Assets/Textures/Architecture/Dk_Barrel_Idle.png"),
        LoadTexture("Assets/Textures/Architecture/SupportBeam.png"),
        LoadTexture("Assets/Textures/Items/Dk_OilCanister.png"),
        LoadTexture("Assets/Textures/Items/Dk_Oil_Fire3.png"),
    };
    Texture2D propFireFrame2 = LoadTexture("Assets/Textures/Items/Dk_Oil_Fire4.png");
    float propFireTimer = 0.f;
    int   propFireFrame = 0;

    // ── Card textures (rarity order: Common, Rare, Stairs, Astolfo, Legendary, Mythic) ──
    static constexpr int CARD_TEX_COUNT = 6;
    Texture2D cardTextures[CARD_TEX_COUNT] = {
        LoadTexture("Assets/Textures/Cards/Common_Template.png"),
        LoadTexture("Assets/Textures/Cards/Rare_Template.png"),
        LoadTexture("Assets/Textures/Cards/Stairs_Template.png"),
        LoadTexture("Assets/Textures/Cards/Astolfo_Template.png"),
        LoadTexture("Assets/Textures/Cards/Legendary_Template.png"),
        LoadTexture("Assets/Textures/Cards/Mythic_Template.png"),
    };
    {
        Texture2D* ptrs[PROP_TEX_COUNT];
        for (int i = 0; i < PROP_TEX_COUNT; i++) ptrs[i] = &propTextures[i];
        editor.SetPropTextures(ptrs, PROP_TEX_COUNT);
    }

    Texture2D texCoin = LoadTexture("Assets/Textures/UI/coin.png");  // coin icon (fallback to drawn circle)

    // ── Per-powerup item textures ──────────────────────────────────────────────
    Texture2D itemTex_ReturnByDeath = LoadTexture("Assets/Textures/Cards/Card_ReturnByDeath.png");
    Texture2D itemTex_Dash          = LoadTexture("Assets/Textures/Cards/Card_Dash.png");
    Texture2D itemTex_Reinhard      = LoadTexture("Assets/Textures/Cards/Card_Reinhard.png");
    Texture2D itemTex_Whip          = LoadTexture("Assets/Textures/Cards/Card_Whip.png");
    Texture2D whipTex1              = LoadTexture("Assets/Textures/Items/Whip1.png");
    Texture2D whipTex2              = LoadTexture("Assets/Textures/Items/Whip2.png");
    Texture2D itemTex_Speedrun      = LoadTexture("Assets/Textures/Cards/Card_Speedrun.png");
    Texture2D itemTex_Shield        = LoadTexture("Assets/Textures/Cards/Card_Shield.png");
    Texture2D itemTex_Shop          = LoadTexture("Assets/Textures/Cards/Card_astolfoShop.png");

    // Returns the item texture for a given powerup type (animates Beatrice frames)
    auto GetItemTex = [&](PowerupType pu) -> Texture2D* {
        bool beaFrame2 = fmod(GetTime(), 0.6) > 0.3;
        switch (pu) {
            case PU_RETURN_BY_DEATH: return &itemTex_ReturnByDeath;
            case PU_DASH:            return &itemTex_Dash;
            case PU_EL_SHAMAK:       return beaFrame2 ? &Beatrice_Idle2 : &Beatrice_Idle1;
            case PU_LARPER:          return &LadderPart;
            case PU_REINHARD:        return &itemTex_Reinhard;
            case PU_WHIP:            return &itemTex_Whip;
            case PU_EXTRA_LIFE:      return &texGoldHeart;
            case PU_NUKE_PU:         return &Nuke;
            case PU_ONE_MORE_LARP:   return &texHeart;
            case PU_SPEEDRUN:        return &itemTex_Speedrun;
            case PU_SHIELD:          return &itemTex_Shield;
            case PU_SHOP:            return itemTex_Shop.id > 0 ? &itemTex_Shop : (texCoin.id > 0 ? &texCoin : nullptr);
            case PU_BEATRICE:        return beaFrame2 ? &Beatrice_Idle2 : &Beatrice_Idle1;
            default:                 return nullptr;
        }
    };

    // Rarity border colors: Common, Rare, Stairs, Astolfo, Legendary, Mythic
    static const Color rarityBorderCols[6] = {
        {180,185,200,255},  // Common: gray/white
        {60,160,255,255},   // Rare: blue
        {30,110,45,255},    // Stairs: dark green
        {255,140,190,255},  // Astolfo: pink
        {255,170,30,255},   // Legendary: orange-yellow
        {210,35,120,255},   // Mythic: red-purple
    };

    // Fit a texture inside dest preserving aspect ratio (letterbox/pillarbox)
    auto FitTexRect = [](Rectangle dest, float texW, float texH) -> Rectangle {
        float s = fminf(dest.width / texW, dest.height / texH);
        float fw = texW * s, fh = texH * s;
        return { dest.x + (dest.width - fw) * 0.5f, dest.y + (dest.height - fh) * 0.5f, fw, fh };
    };

    Texture2D* subaruFrames[SUBARU_FRAME_COUNT] = { &Subaru1, &Subaru2, &Subaru3, &Subaru4, &Subaru5 };

    Texture2D* regulusIdleFrames[3] = { &RegulusIdle1,    &RegulusIdle2,    &RegulusIdle3 };
    Texture2D* regulusThrowFrames[3] = { &RegulusGrab1,    &RegulusGrab2,    &RegulusGrab3 };
    Texture2D* regulusStunFrames[3] = { &Regulus_Stun1,   &Regulus_Stun2,   &Regulus_Stun3 };
    Texture2D* regulusStunEndFrames[5] = { &Regulus_StunEnd1,&Regulus_StunEnd2,&Regulus_StunEnd3,
                                           &Regulus_StunEnd4,&Regulus_StunEnd5 };

    const int NUKE_EXPL_FRAME_COUNT = 6;
    Texture2D* explosionFrames[NUKE_EXPL_FRAME_COUNT] = {
        &Explosion1,&Explosion2,&Explosion3,&Explosion4,&Explosion5,&Explosion6
    };

    Texture2D* barrelRoll[4] = { &BarrelMov1,     &BarrelMov2,     &BarrelMov3,     &BarrelMov4 };
    Texture2D* barrelFall[2] = { &BarrelFall1,    &BarrelFall2 };
    Texture2D* blueBarrelRoll[4] = { &BlueBarrelMov1, &BlueBarrelMov2, &BlueBarrelMov3, &BlueBarrelMov4 };
    Texture2D* blueBarrelFall[2] = { &BlueBarrelFall1,&BlueBarrelFall2 };

    // ── Bake static beam layer ────────────────────────────────────────────────
    RenderTexture2D staticLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(staticLayer);
    ClearBackground(BLANK);
    float beamScale = 4.0f;
    for (auto& b : beamPositions) {
        if (b.renderLayer > 0) continue;
        Texture2D* bTex = (b.texVariant >= 1 && b.texVariant <= 12 && beamVariants[b.texVariant - 1].id > 0)
            ? &beamVariants[b.texVariant - 1] : &beam;
        float bSrcX = b.flipX ? (float)bTex->width : 0.f;
        float bSrcW = b.flipX ? -(float)bTex->width : (float)bTex->width;
        DrawTexturePro(*bTex, { bSrcX, 0, bSrcW, (float)bTex->height },
            { b.x, b.y, (float)bTex->width * beamScale, (float)bTex->height * beamScale },
            { 0, 0 }, 0.f, WHITE);
    }
    EndTextureMode();

    // ── Bake ladder layer ─────────────────────────────────────────────────────
    float ladderScale = 4.0f;
    float ladderTileH = 16 * ladderScale;
    float ladderTileW = 16 * ladderScale;
    const float ladderTopOffset = ladderTileH * 0.3f;

    RenderTexture2D ladderLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(ladderLayer);
    ClearBackground(BLANK);
    for (int li = 0; li < (int)ladders.size(); li++)
    {
        const Ladder& lad = ladders[li];
        bool  isLast = (li == (int)ladders.size() - 1);
        bool  isLeftSide = (li == 1 || li == 6);
        float topOff = isLast ? 0.0f : (isLeftSide ? ladderTileH * 0.42f : ladderTopOffset);
        float drawX = lad.x + lad.width * 0.5f - ladderTileW * 0.5f;
        float y = lad.y + topOff;
        float bottom = lad.y + lad.height - (isLast ? 0.0f : (isLeftSide ? 8.0f : topOff));
        while (y < bottom)
        {
            DrawTexturePro(LadderPart, { 0, 0, 16.0f, 16.0f },
                { drawX, y, ladderTileW, ladderTileH }, { 0, 0 }, 0.f, WHITE);
            y += ladderTileH;
        }
    }
    EndTextureMode();

    editor.SetGameTextures(&background, &beam, &LadderPart,
        &imgMarioIdle, &RegulusIdle1, &House1, &RopeTex, &GoldenPistonTex);
    editor.SetConveyorTextures(&ConvSide[0], &ConvSide[1], &ConvSide[2], &ConvM[0], &ConvM[1], &ConvM[2]);
    {
        Texture2D* bvPtrs[12];
        for (int i = 0; i < 12; i++) bvPtrs[i] = &beamVariants[i];
        editor.SetBeamVariantTextures(bvPtrs);
    }

    // ── RebuildLayers ─────────────────────────────────────────────────────────
    auto RebuildLayers = [&]()
        {
            float bScale = 4.0f;
            BeginTextureMode(staticLayer);
            ClearBackground(BLANK);
            for (int bi = 0; bi < (int)beamPositions.size(); bi++)
            {
                const auto& b = beamPositions[bi];
                if (b.renderLayer > 0) continue;
                bool isElevChild = false;
                for (const auto& rel : liveRelations)
                    if (rel.parent.type == 11 && rel.child.type == 6 && rel.child.index == bi)
                    {
                        isElevChild = true; break;
                    }
                if (isElevChild) continue;
                Texture2D* bTex = (b.texVariant >= 1 && b.texVariant <= 12 && beamVariants[b.texVariant - 1].id > 0)
                    ? &beamVariants[b.texVariant - 1] : &beam;
                float rbSrcX = b.flipX ? (float)bTex->width : 0.f;
                float rbSrcW = b.flipX ? -(float)bTex->width : (float)bTex->width;
                DrawTexturePro(*bTex, { rbSrcX, 0, rbSrcW, (float)bTex->height },
                    { b.x, b.y, (float)bTex->width * bScale, (float)bTex->height * bScale },
                    { 0, 0 }, 0.f, WHITE);
            }
            EndTextureMode();

            float lScale = 4.0f;
            float tileW = 16.f * lScale;
            float tileH = 16.f * lScale;
            BeginTextureMode(ladderLayer);
            ClearBackground(BLANK);
            for (int li = 0; li < (int)ladders.size(); li++)
            {
                bool isElevChild = false;
                for (const auto& rel : liveRelations)
                    if (rel.parent.type == 11 && rel.child.type == 5 && rel.child.index == li)
                    {
                        isElevChild = true; break;
                    }
                if (isElevChild) continue;

                const Ladder& lad = ladders[li];
                float drawX = lad.x + lad.width * 0.5f - tileW * 0.5f;
                for (float y = lad.y; y < lad.y + lad.height; y += tileH)
                {
                    float dh = fminf(tileH, lad.y + lad.height - y);
                    float srh = dh / lScale;
                    DrawTexturePro(LadderPart, { 0, 0, 16.f, srh },
                        { drawX, y, tileW, dh }, { 0, 0 }, 0.f, WHITE);
                }
            }
            EndTextureMode();
        };

    // ── ApplyLevelData ────────────────────────────────────────────────────────
    auto ApplyLevelData = [&](const LevelData& lv)
        {
            currentLevelData = lv;

            platforms.clear();
            for (const auto& pd : lv.platforms)
                platforms.push_back(Platform::Make(pd.x, pd.y, pd.w, pd.h, pd.tilt));

            ladders.clear();
            for (const auto& ld : lv.ladders)
                ladders.push_back(Ladder::Make(ld.x, ld.y, ld.w, ld.h));

            barrelPath.clear();
            for (const auto& nd : lv.pathNodes) {
                PathNode n;
                n.pos = { nd.x, nd.y };
                n.next[0] = nd.next[0];
                n.next[1] = nd.next[1];
                n.next[2] = nd.next[2];
                n.edgeType[0] = nd.edgeType[0];
                n.edgeType[1] = nd.edgeType[1];
                n.edgeType[2] = nd.edgeType[2];
                n.rollThreshold = nd.rollThreshold;
                n.isSplitNode = nd.isSplitNode;
                barrelPath.push_back(n);
            }

            nukeSpawnNodes.clear();
            for (const auto& v : lv.nukeSpawns)    nukeSpawnNodes.push_back(v);
            beatriceSpawnNodes.clear();
            for (const auto& v : lv.beatriceSpawns) beatriceSpawnNodes.push_back(v);
            enemySpawnPositions.clear();
            for (const auto& v : lv.enemySpawns)   enemySpawnPositions.push_back(v);

            beamPositions.clear();
            for (const auto& b : lv.beams) beamPositions.push_back(b);

            if (lv.hasCave) {
                houseX = lv.cavePos.x;
                houseY = lv.cavePos.y;
                houseHitbox = { houseX, houseY, houseW, houseH };
            }

            if (lv.hasPlayerSpawn) {
                playerSpawnX = lv.playerSpawn.x;
                playerSpawnY = lv.playerSpawn.y;
            }

            liveElevators = lv.elevators;
            liveRelations = lv.relations;
            elevChildPhases.resize(liveRelations.size());
            for (int ri = 0; ri < (int)liveRelations.size(); ri++) {
                const auto& rel = liveRelations[ri];
                bool horiz = (rel.parent.type == 11 && rel.parent.index >= 0
                    && rel.parent.index < (int)liveElevators.size()
                    && liveElevators[rel.parent.index].horizontal);
                elevChildPhases[ri] = horiz ? rel.offsetX : rel.offsetY;
            }
            liveElevTravel.assign(liveElevators.size(), 0.f);
            liveElevBounceDir.assign(liveElevators.size(), 1);

            RebuildLayers();

            if (lv.hasWinZone)
                wincondition = { lv.winZone.x, lv.winZone.y, lv.winZone.w, lv.winZone.h };

            liveKillZones.clear();
            for (const auto& kz : lv.killZones) liveKillZones.push_back(kz);

            liveConveyors = lv.conveyors;

            conveyorPlatStart = (int)platforms.size();
            for (const auto& cv : lv.conveyors)
                platforms.push_back(Platform::Make(cv.x, cv.y, cv.length, 0.f, cv.rotation));
            for (const auto& pr : lv.props)
                if (pr.hasCollision)
                    platforms.push_back(Platform::Make(pr.x, pr.y, pr.width, 0.f, pr.rotation));
        };

    Texture2D* image = &imgMarioIdle;
    SetTargetFPS(60);
    float animationTimer = 0.0f;
    float animationSpeed = 0.15f;
    int   walkFrame = 0;

    float rainScrollY = 0.0f;
    float rain2ScrollY = 0.0f;
    float rainSpeed = -1230.0f;
    float rain2Speed = rainSpeed * 0.8f;
    Color rainTint = { 255, 255, 255,  80 };
    Color rain2Tint = { 255, 255, 255,  50 };

    // ── Lambdas ───────────────────────────────────────────────────────────────
    auto RespawnItems = [&]()
        {
            if (nukeRespawnNodes.empty()) return;
            if (GetRandomValue(1, 100) <= 30)
            {
                int  idx = GetRandomValue(0, (int)nukeRespawnNodes.size() - 1);
                bool blocked = false;
                for (const auto& nk : nukes)
                    if (nk.active && Vector2Distance(nk.pos, nukeRespawnNodes[idx]) < 50.0f)
                        blocked = true;
                if (!blocked)
                    nukes.push_back({ nukeRespawnNodes[idx], true });
            }
        };

    auto ResetRegulus = [&]()
        {
            regulusThrowing = false;
            regulusThrowFrame = 0;
            regulusThrowTimer = 0.0f;
            regulusSpawnPending = false;
            regulusForceBlue = false;
            regulusIdleFrame = 0;
            regulusIdleTimer = 0.0f;
            regulusActiveSpawnTimer = 0.0f;
            regulusIsActive = true;
            regulusStateTickTimer = 0.0f;
            regulusInactiveTime = 0.0f;
            regulusActiveFails = 0;
            regulusInactiveFails = 0;
            regulusIsStunned = false;
            regulusStunEnding = false;
            regulusStunFrame = 0;
            regulusStunTimer = 0.0f;
            regulusStunLoops = 0;
            regulusStunEndFrame = 0;
            regulusStunEndTimer = 0.0f;
        };

    // Roll a random powerup (rarity weighted): Common45% Rare20% Stairs15% Astolfo10% Legendary7% Mythic3%
    auto RollPowerup = [&]() -> PowerupType {
        int r = GetRandomValue(1, 100);
        int ri = (r<=45)?0:(r<=65)?1:(r<=80)?2:(r<=90)?3:(r<=97)?4:5;
        int cnt = PU_RARITY_COUNT[ri];
        int pick = GetRandomValue(0, cnt - 1);
        return PU_BY_RARITY[ri][pick];
    };

    // Initialise the card-select screen with n cards
    auto InitCardSelect = [&](int n, bool shopMode) {
        numDispCards = n;
        anyCardPicked = false;
        cardFadeOut = 0.f;
        cardSelectMode = shopMode ? CSM_SHOP : CSM_LEVEL;
        hbDrag = {};
        for (int i = 0; i < n; i++) {
            displayCards[i] = {};
            displayCards[i].type      = RollPowerup();
            displayCards[i].rarity    = PU_INFO[(int)displayCards[i].type].rarityIdx;
            displayCards[i].scale     = 1.f;
            displayCards[i].heightFrac = 0.f;
            displayCards[i].appearT   = -(float)i * 0.15f;  // stagger per card
            displayCards[i].appeared  = false;
        }
        SetSoundVolume(cardFanSnd, volUI * 1.2f); PlaySound(cardFanSnd);
    };

    // Add powerup to hotbar; returns true if added
    auto AddToHotbar = [&](PowerupType pu) -> bool {
        const PowerupInfo& info = PU_INFO[(int)pu];
        // For stackable: find existing slot of same type
        if (info.maxStack > 1) {
            for (auto& s : hotbar) {
                if (s.type == pu && s.charges < info.maxStack) {
                    s.charges++; return true;
                }
            }
        }
        // Find empty slot
        for (auto& s : hotbar) {
            if (s.type == PU_NONE) {
                s.type    = pu;
                s.cd      = 0.f;
                s.charges = info.maxCharges > 0 ? 1 : 0;
                return true;
            }
        }
        // Replace active slot
        auto& s = hotbar[hotbarSlot];
        s.type    = pu;
        s.cd      = 0.f;
        s.charges = info.maxCharges > 0 ? 1 : 0;
        return true;
    };

    auto ClearRoundEntities = [&]()
        {
            for (auto& b : barrels)     b.active = false;
            for (auto& bb : beaBullets)  bb.active = false;
            for (auto& en : enemies)     { en.active = false; en.onLadder = false; en.ladderIdx = -1; en.ladderCooldown = 0.f; en.justJumped = false; }
            for (auto& fn : flyingNukes) fn.active = false;
            flyingNukes.clear();
            playerHasNuke = false;
            playerHasBeatrice = false;
            beatriceAbilityTimer = 0.0f;
            beaBulletShootTimer = 0.0f;
            caveSpawnTimer = 0.f;
        };

    auto ResetPlayerPos = [&]()
        {
            player.x = playerSpawnX;
            player.y = playerSpawnY;
            velocityX = 0.0f;
            velocityY = 0.0f;
            isJumping = false;
            isGrounded = false;
            facingRight = true;
            onLadder = false;
            currentLadder = -1;
            ladderProgress = 0.0f;
            ladderCooldown = 0.0f;
            ladderExitPlaying = false;
            ladderExitStep = 0;
            ladderExitTimer = 0.0f;
            ladderEntryClamp = false;
            walkFrame = 0;
            animationTimer = 0.0f;
            isEmoting = false;
            emoteFrame = 0;
            emoteTimer = 0.f;
            image = &imgMarioIdle;
        };

    auto ClearDeathState = [&]()
        {
            isDying = false;
            deathTimer = 0.0f;
            hitPlayed = false;
            deathPlayed = false;
            deathFallVelY = 0.0f;
            deathReachedBlack = false;
            deathBlackTimer = 0.0f;
            deathShakeTimer = 0.0f;
            deathShakeOffset = { 0, 0 };
        };

    auto TriggerDeath = [&]()
        {
            if (isDying) return;
            if (dbgImmortal) return;
            // Return By Death intercept
            for (auto& s : hotbar) {
                if (s.type == PU_RETURN_BY_DEATH && s.charges > 0) {
                    s.charges--;
                    if (s.charges == 0) s.type = PU_NONE;
                    rbdTriggered = true;
                    return;
                }
            }
            lives--;
            isDying = true;
            deathTimer = 0.0f;
            hitPlayed = false;
            deathPlayed = false;
            deathFallVelY = DEATH_FALL_INITIAL_VY;
            deathReachedBlack = false;
            deathBlackTimer = 0.0f;
            deathShakeTimer = DEATH_SHAKE_DURATION;
            deathShakeOffset = { 0, 0 };
            playerHasNuke = false;
            playerHasBeatrice = false;
            beatriceAbilityTimer = 0.0f;
            beaBulletShootTimer = 0.0f;
            for (auto& bb : beaBullets) bb.active = false;
            invincible = true;
            invincibleTimer = invincibleDuration;
            musicFadeTimer    = MUSIC_FADE_DUR;  // start 0.5s fade-out
            musicPendingPause = true;
            ResetRegulus();
            RespawnItems();
        };

    auto ResetRound = [&]()
        {
            ClearDeathState();
            ClearRoundEntities();
            ResetPlayerPos();
            ResetRegulus();
            invincible = true;
            invincibleTimer = invincibleDuration;
            musicFadeTimer = 0.f; musicPendingPause = false;
            SetMusicVolume(music, volMusic);
            ResumeMusicStream(music);
        };

    auto FullReset = [&]()
        {
            currentLevelId = 1;
            LevelData lv1;
            if (LoadLevel(lv1, 1)) ApplyLevelData(lv1);
            else                   ApplyLevelData(GetDefaultLevel1());
            ClearDeathState();
            ClearRoundEntities();
            ResetPlayerPos();
            ResetRegulus();

            lives = 3;
            invincible = false;
            invincibleTimer = 0.0f;
            score = 0; coins = 0;
            runScore = 0; levelsCleared = 0;

            playerHasBeatrice = false;
            beatriceAbilityTimer = 0.0f;
            beaBulletShootTimer = 0.0f;
            for (auto& bb : beaBullets) bb.active = false;
            beatriceItemAnimTimer = 0.0f;
            beatriceItemAnimFrame = 0;

            houseAnimPlaying = false;
            houseAnimFrame = 0;
            houseAnimTimer = 0.0f;
            houseIsSnowed = false;

            nukeExtraDelay = 0.0f;
            nukeExplosionPlaying = false;
            nukeExplosionFrame = 0;
            nukeExplosionTimer = 0.0f;
            nukeFlashTimer = 5.0f;
            nukeShakeOffset = { 0, 0 };
            for (auto& nk : nukes) nk.active = false;
            nukes.clear();
            if (!nukeSpawnNodes.empty()) {
                int idx = GetRandomValue(0, (int)nukeSpawnNodes.size() - 1);
                nukes.push_back({ nukeSpawnNodes[idx], true });
            }

            beatrices.clear();
            if (!beatriceSpawnNodes.empty()) {
                int idx = GetRandomValue(0, (int)beatriceSpawnNodes.size() - 1);
                beatrices.push_back({ beatriceSpawnNodes[idx], true });
            }

            regulusThrowing = true;
            regulusThrowFrame = 0;
            regulusThrowTimer = 0.0f;
            regulusSpawnPending = true;
            regulusForceBlue = true;
            regulusIdleFrame = 0;
            regulusIdleTimer = 0.0f;

            subaruFrame = 0;
            subaruTimer = 0.0f;

            musicFadeTimer = 0.f; musicPendingPause = false;
            SetMusicVolume(music, volMusic);
            ResumeMusicStream(music);
        };

    auto SpawnEnemyAtEnd = [&](float spawnCX, float spawnFloorY = 880.0f)
        {
            for (auto& en : enemies)
            {
                if (en.active) continue;
                EnemyType t = (GetRandomValue(0, 9) == 0) ? GRUNT : SPECTER;  // 10% black, 90% white
                float     hw = (t == GRUNT) ? 30.8f : 26.6f;
                float     hh = (t == GRUNT) ? 30.8f : 35.0f;
                en.hitbox = { spawnCX - hw * 0.5f, spawnFloorY - hh, hw, hh };
                en.type = t;
                en.state = ES_IDLE;
                en.stateTimer = 0.0f;
                en.velocity = { 0.0f, 0.0f };
                en.grounded = true;
                en.animFrame = 0;
                en.animTimer = 0.0f;
                en.facingRight = (GetRandomValue(0, 1) == 1);
                en.onLadder = false; en.ladderIdx = -1; en.ladderProgress = 0.f;
                en.ladderCooldown = 0.f; en.justJumped = false;
                en.active = true;
                break;
            }
        };

    auto SpawnInitialEnemies = [&]()
        {
            for (const auto& pos : enemySpawnPositions)
            {
                for (auto& en : enemies)
                {
                    if (en.active) continue;
                    EnemyType t2 = (GetRandomValue(0, 9) == 0) ? GRUNT : SPECTER;  // 10% black, 90% white
                    float hw = (t2 == GRUNT) ? 30.8f : 26.6f;
                    float hh = (t2 == GRUNT) ? 30.8f : 35.0f;
                    en.hitbox = { pos.x - hw * 0.5f, pos.y - hh, hw, hh };
                    en.type = t2;
                    en.state = ES_IDLE;
                    en.stateTimer = 0.0f;
                    en.velocity = { 0.0f, 0.0f };
                    en.grounded = false;
                    en.animFrame = 0;
                    en.animTimer = 0.0f;
                    en.facingRight = (GetRandomValue(0, 1) == 1);
                    en.onLadder = false; en.ladderIdx = -1; en.ladderProgress = 0.f;
                    en.ladderCooldown = 0.f; en.justJumped = false;
                    en.active = true;
                    break;
                }
            }
        };

    Cinematic::Global.SetApplyCallback([&](const CinematicEntityState& st) {
        int i = st.entityIndex;
        switch (st.entityType) {
        case 4:
            if (i >= 0 && i < (int)platforms.size())
                platforms[i] = Platform::Make(st.x, st.y,
                    st.width > 0.f ? st.width : 128.f, 0, st.tilt);
            break;
        case 5:
            if (i >= 0 && i < (int)ladders.size())
                ladders[i] = Ladder::Make(st.x, st.y,
                    ladders[i].width, st.height > 0.f ? st.height : ladders[i].height);
            break;
        case 6:
            if (i >= 0 && i < (int)beamPositions.size())
            {
                beamPositions[i].x = st.x; beamPositions[i].y = st.y;
            }
            break;
        case 7:
            if (i >= 0 && i < (int)barrelPath.size())
            {
                barrelPath[i].pos.x = st.x; barrelPath[i].pos.y = st.y;
            }
            break;
        case 3:
            houseX = st.x; houseY = st.y;
            houseHitbox = { houseX, houseY, houseW, houseH };
            break;
        case 8:  if (i >= 0 && i < (int)nukeSpawnNodes.size())      nukeSpawnNodes[i] = { st.x, st.y }; break;
        case 9:  if (i >= 0 && i < (int)beatriceSpawnNodes.size())   beatriceSpawnNodes[i] = { st.x, st.y }; break;
        case 10: if (i >= 0 && i < (int)enemySpawnPositions.size())  enemySpawnPositions[i] = { st.x, st.y }; break;
        default: break;
        }
        });

    // ── Level priority init ───────────────────────────────────────────────────
    LoadGameSettings(levelRangeMin, levelRangeMax, volMusic, volSFX, volAbility, volUI, highScore, highLevels);
    SetMusicVolume(music, volMusic);
    for (int i = 1; i <= MAX_LEVEL_ID; i++) levelPriority[i] = 1;

    // Updates and saves highscore at end of run; call before any GAME_OVER transition
    auto TryUpdateHighscore = [&]() {
        bool changed = false;
        if (runScore    > highScore)  { highScore  = runScore;    changed = true; }
        if (levelsCleared > highLevels) { highLevels = levelsCleared; changed = true; }
        if (changed)
            SaveGameSettings(levelRangeMin, levelRangeMax, volMusic, volSFX, volAbility, volUI, highScore, highLevels);
    };

    // ─────────────────────────────────────────────────────────────────────────
    // MAIN LOOP
    // ─────────────────────────────────────────────────────────────────────────
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_N) && currentScreen != MENU) {
            editor.ClearFlags();
            currentScreen = MENU;
        }
        if (IsKeyPressed(KEY_F1)) debugPath = !debugPath;
        if (IsKeyPressed(KEY_M)) dbgMenuOpen = !dbgMenuOpen;

        if (currentScreen == SPLASH_SCREEN)
        {
            if (IsKeyPressed(KEY_ENTER)) currentScreen = SPLASH_SCREEN2;
            splashTimer += dt;
            if (splashTimer >= splashDuration) { splashTimer = 0.0f; currentScreen = SPLASH_SCREEN2; }
        }
        else if (currentScreen == SPLASH_SCREEN2)
        {
            if (IsKeyPressed(KEY_ENTER)) currentScreen = MENU;
            splashTimer += dt;
            if (splashTimer >= splashDuration) { splashTimer = 0.0f; currentScreen = MENU; }
        }
        else if (currentScreen == MENU)
        {
            Vector2 mouse = GetMousePosition();
            if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER))
            {
                selectedOption = 0; FullReset(); currentScreen = HOW_HIGH; splashTimer = 0.0f; subaruFrame = 0; subaruTimer = 0.0f;
            }
            if (CheckCollisionPointRec(mouse, btnPlay))
            {
                selectedOption = 0; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { FullReset(); currentScreen = HOW_HIGH; splashTimer = 0.0f; subaruFrame = 0; subaruTimer = 0.0f; }
            }
            if (CheckCollisionPointRec(mouse, btnEditor))
            {
                selectedOption = 2; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { editor.ClearFlags(); currentScreen = LEVEL_EDITOR; }
            }
            if (CheckCollisionPointRec(mouse, btnExit))
            {
                selectedOption = 1; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) break;
            }
            if (CheckCollisionPointRec(mouse, btnSettings))
            {
                selectedOption = 3; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { settingsTab = 0; currentScreen = SETTINGS; }
            }
        }
        else if (currentScreen == SETTINGS)
        {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N)) {
                SaveGameSettings(levelRangeMin, levelRangeMax, volMusic, volSFX, volAbility, volUI, highScore, highLevels);
                currentScreen = MENU;
            }
        }
        else if (currentScreen == CONTROLS)
        {
            if (IsKeyPressed(KEY_ESCAPE)) currentScreen = MENU;
            Rectangle btnsalida = { 750, 900, 200, 40 };
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnsalida))
            {
                DrawText(">", 725, 900, 40, ORANGE);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentScreen = MENU;
            }
        }
        else if (currentScreen == LEVEL_EDITOR)
        {
            if (IsKeyPressed(KEY_N)) {
                editor.ClearFlags();
                Cinematic::Global.LoadAll();
                currentScreen = MENU;
            }
            else {
                editor.Update(dt);
                if (editor.WantsMenu()) { editor.ClearFlags(); currentScreen = MENU; }
                if (editor.WantsPlay()) {
                    editor.ClearFlags();
                    ApplyLevelData(editor.GetLevel());
                    currentLevelId = editor.GetCurrentLevelId();
                    Cinematic::Global.LoadAll();
                    ClearDeathState();
                    ClearRoundEntities();
                    ResetPlayerPos();
                    ResetRegulus();
                    lives = 3; score = 0; coins = 0;
                    invincible = true; invincibleTimer = invincibleDuration;
                    playerHasBeatrice = false;
                    beatriceAbilityTimer = 0.0f;
                    beaBulletShootTimer = 0.0f;
                    for (auto& bb : beaBullets) bb.active = false;
                    beatriceItemAnimTimer = 0.0f; beatriceItemAnimFrame = 0;
                    houseAnimPlaying = false; houseAnimFrame = 0;
                    houseAnimTimer = 0.0f;  houseIsSnowed = false;
                    nukeExtraDelay = 0.0f; nukeExplosionPlaying = false;
                    nukeExplosionFrame = 0;    nukeExplosionTimer = 0.0f;
                    nukeFlashTimer = 5.0f; nukeShakeOffset = { 0, 0 };
                    for (auto& nk : nukes) nk.active = false;
                    nukes.clear();
                    if (!nukeSpawnNodes.empty()) {
                        int idx = GetRandomValue(0, (int)nukeSpawnNodes.size() - 1);
                        nukes.push_back({ nukeSpawnNodes[idx], true });
                    }
                    beatrices.clear();
                    if (!beatriceSpawnNodes.empty()) {
                        int idx = GetRandomValue(0, (int)beatriceSpawnNodes.size() - 1);
                        beatrices.push_back({ beatriceSpawnNodes[idx], true });
                    }
                    regulusThrowing = true; regulusThrowFrame = 0;
                    regulusThrowTimer = 0.0f; regulusSpawnPending = true;
                    regulusForceBlue = true; regulusIdleFrame = 0;
                    regulusIdleTimer = 0.0f;
                    subaruFrame = 0; subaruTimer = 0.0f;
                    SpawnInitialEnemies();
                    musicFadeTimer = 0.f; musicPendingPause = false;
            SetMusicVolume(music, volMusic);
            ResumeMusicStream(music);
                    currentScreen = GAMEPLAY;
                }
            }
        }
        else if (currentScreen == HOW_HIGH)
        {
            splashTimer += dt;
            subaruTimer += dt;
            if (subaruTimer >= 1.0f / SUBARU_ANIM_FPS)
            {
                subaruTimer -= 1.0f / SUBARU_ANIM_FPS; subaruFrame = (subaruFrame + 1) % SUBARU_FRAME_COUNT;
            }
            if (splashTimer >= 2.0f)
            {
                splashTimer = 0.0f;
                ClearDeathState();
                ClearRoundEntities();
                ResetPlayerPos();
                ResetRegulus();
                invincible = true; invincibleTimer = invincibleDuration;
                playerHasBeatrice = false;
                beatriceAbilityTimer = 0.0f;
                beaBulletShootTimer = 0.0f;
                for (auto& bb : beaBullets) bb.active = false;
                beatriceItemAnimTimer = 0.0f; beatriceItemAnimFrame = 0;
                houseAnimPlaying = false; houseAnimFrame = 0;
                houseAnimTimer = 0.0f; houseIsSnowed = false;
                nukeExtraDelay = 0.0f; nukeExplosionPlaying = false;
                nukeExplosionFrame = 0; nukeExplosionTimer = 0.0f;
                nukeFlashTimer = 5.0f; nukeShakeOffset = { 0, 0 };
                for (auto& nk : nukes) nk.active = false;
                nukes.clear();
                if (!nukeSpawnNodes.empty()) {
                    int idx = GetRandomValue(0, (int)nukeSpawnNodes.size() - 1);
                    nukes.push_back({ nukeSpawnNodes[idx], true });
                }
                beatrices.clear();
                if (!beatriceSpawnNodes.empty()) {
                    int idx = GetRandomValue(0, (int)beatriceSpawnNodes.size() - 1);
                    beatrices.push_back({ beatriceSpawnNodes[idx], true });
                }
                regulusThrowing = true; regulusThrowFrame = 0;
                regulusThrowTimer = 0.0f; regulusSpawnPending = true;
                regulusForceBlue = true; regulusIdleFrame = 0;
                regulusIdleTimer = 0.0f;
                subaruFrame = 0; subaruTimer = 0.0f;
                SpawnInitialEnemies();
                musicFadeTimer = 0.f; musicPendingPause = false;
            SetMusicVolume(music, volMusic);
            ResumeMusicStream(music);
                currentScreen = GAMEPLAY;
            }
        }
        else if (currentScreen == GAMEPLAY)
        {
            UpdateMusicStream(music);

            // ── Music fade-out (triggered on death) ───────────────────────────
            if (musicFadeTimer > 0.f) {
                musicFadeTimer -= dt;
                float t = fmaxf(musicFadeTimer / MUSIC_FADE_DUR, 0.f);
                SetMusicVolume(music, volMusic * t);
                if (musicFadeTimer <= 0.f && musicPendingPause) {
                    PauseMusicStream(music);
                    musicPendingPause = false;
                    SetMusicVolume(music, volMusic);
                }
            }

            {
                static LevelData _cinematicDummy;
                Cinematic::Global.Update(dt, _cinematicDummy);
            }

            if (IsKeyPressed(KEY_ESCAPE) && !isDying) { isPaused = !isPaused; settingsTab = 0; }

            if (!isPaused) {

            // ── RBD: save snapshot every 0.1s ────────────────────────────────
            {
                bool rbdEquipped = false;
                for (auto& s : hotbar) if (s.type == PU_RETURN_BY_DEATH && s.charges > 0) rbdEquipped = true;
                if (rbdEquipped && !isDying) {
                    rbdSaveTimer += dt;
                    if (rbdSaveTimer >= 0.1f) {
                        rbdSaveTimer = 0.f;
                        RBDSnapshot& snap = rbdBuf[rbdHead % RBD_BUF];
                        snap.player = player; snap.vx = velocityX; snap.vy = velocityY;
                        snap.facingR = facingRight; snap.onLadder = onLadder;
                        snap.curLadder = currentLadder; snap.ladderProg = ladderProgress;
                        snap.lives = lives; snap.score = score; snap.coins = coins;
                        snap.barrels.clear();
                        for (auto& b : barrels) snap.barrels.push_back({b.hitbox,b.currentNode,b.speed,b.active,b.isBlue,b.isFalling});
                        snap.enemies.clear();
                        for (auto& e : enemies) snap.enemies.push_back({e.hitbox,e.velocity,e.type,e.state,e.stateTimer,e.active,e.grounded,e.facingRight});
                        snap.nukes = nukes; snap.beatrices = beatrices;
                        snap.regThrowing=regulusThrowing; snap.regPending=regulusSpawnPending;
                        snap.regForceBlue=regulusForceBlue; snap.regActive=regulusIsActive;
                        snap.regStunned=regulusIsStunned;
                        snap.regThrowT=regulusThrowTimer; snap.regThrowFr=regulusThrowFrame;
                        snap.regIdleT=regulusIdleTimer; snap.regIdleFr=regulusIdleFrame;
                        rbdHead = (rbdHead + 1) % RBD_BUF;
                        if (rbdCount < RBD_BUF) rbdCount++;
                    }
                }
            }

            // ── RBD rewind triggered ──────────────────────────────────────────
            if (rbdTriggered) {
                rbdTriggered = false;
                int snapIdx = (rbdCount > 0) ? ((rbdHead - rbdCount + RBD_BUF) % RBD_BUF) : -1;
                if (snapIdx >= 0) {
                    const RBDSnapshot& s = rbdBuf[snapIdx];
                    player = s.player; velocityX = s.vx; velocityY = s.vy;
                    facingRight = s.facingR; onLadder = s.onLadder;
                    currentLadder = s.curLadder; ladderProgress = s.ladderProg;
                    lives = s.lives; score = s.score; coins = s.coins;
                    for (int i = 0; i < (int)s.barrels.size() && i < (int)barrels.size(); i++) {
                        barrels[i].hitbox=s.barrels[i].h; barrels[i].currentNode=s.barrels[i].node;
                        barrels[i].speed=s.barrels[i].spd; barrels[i].active=s.barrels[i].active;
                        barrels[i].isBlue=s.barrels[i].isBlue; barrels[i].isFalling=s.barrels[i].isFalling;
                    }
                    for (int i = 0; i < (int)s.enemies.size() && i < (int)enemies.size(); i++) {
                        enemies[i].hitbox=s.enemies[i].h; enemies[i].velocity=s.enemies[i].vel;
                        enemies[i].type=s.enemies[i].type; enemies[i].state=s.enemies[i].st;
                        enemies[i].stateTimer=s.enemies[i].stT; enemies[i].active=s.enemies[i].active;
                        enemies[i].grounded=s.enemies[i].grounded; enemies[i].facingRight=s.enemies[i].facingR;
                    }
                    nukes = s.nukes; beatrices = s.beatrices;
                    regulusThrowing=s.regThrowing; regulusSpawnPending=s.regPending;
                    regulusForceBlue=s.regForceBlue; regulusIsActive=s.regActive;
                    regulusIsStunned=s.regStunned;
                    regulusThrowTimer=s.regThrowT; regulusThrowFrame=s.regThrowFr;
                    regulusIdleTimer=s.regIdleT; regulusIdleFrame=s.regIdleFr;
                    rbdCount = 0; rbdHead = 0;
                }
                SetSoundVolume(rbdSound, volSFX); PlaySound(rbdSound);
                rbdFading = true; rbdFadeTimer = 0.f;
                ClearDeathState();
                invincible = true; invincibleTimer = 2.0f;
                musicFadeTimer = 0.f; musicPendingPause = false;
            SetMusicVolume(music, volMusic);
            ResumeMusicStream(music);
            }

            // ── RBD fade-in ───────────────────────────────────────────────────
            if (rbdFading) {
                rbdFadeTimer += dt;
                if (rbdFadeTimer >= 2.f) rbdFading = false;
            }

            // ── Hotbar cooldown tick ──────────────────────────────────────────
            for (auto& s : hotbar) {
                if (s.type != PU_NONE && PU_INFO[(int)s.type].maxCD > 0.f && s.cd > 0.f)
                    s.cd -= dt;
            }

            // ── Hotbar slot switching ─────────────────────────────────────────
            {
                int prevSlot = hotbarSlot;
                if (IsKeyPressed(KEY_ONE))   hotbarSlot = 0;
                if (IsKeyPressed(KEY_TWO))   hotbarSlot = 1;
                if (IsKeyPressed(KEY_THREE)) hotbarSlot = 2;
                {
                    float wheel = GetMouseWheelMove();
                    if (wheel > 0.f) hotbarSlot = (hotbarSlot + 2) % 3;
                    if (wheel < 0.f) hotbarSlot = (hotbarSlot + 1) % 3;
                }
                auto isHeavyItem = [](PowerupType t) -> bool {
                    return t==PU_NUKE_PU || t==PU_LARPER || t==PU_EXTRA_LIFE || t==PU_ONE_MORE_LARP;
                };
                if (prevSlot != hotbarSlot && isHeavyItem(hotbar[hotbarSlot].type))
                    heavyEquipTimer = 0.f;
            }
            auto isHeavyItem = [](PowerupType t) -> bool {
                return t==PU_NUKE_PU || t==PU_LARPER || t==PU_EXTRA_LIFE || t==PU_ONE_MORE_LARP;
            };
            isCarryingHeavy = isHeavyItem(hotbar[hotbarSlot].type);
            if (isCarryingHeavy) heavyEquipTimer = fminf(heavyEquipTimer + dt, 1.f);
            else                 heavyEquipTimer = 0.f;

            // ── Active powerup effects ────────────────────────────────────────
            if (speedrunActive) { speedrunTimer -= dt; if (speedrunTimer <= 0.f) speedrunActive = false; }
            if (dashActive) { dashInvulTimer -= dt; if (dashInvulTimer <= 0.f) { dashActive = false; } }
            if (shieldActive) { shieldTimer -= dt; if (shieldTimer <= 0.f) shieldActive = false; }

            // ── Larper: grow ladders ──────────────────────────────────────────
            for (auto& ll : larperLadders) {
                if (ll.curH < ll.targH) {
                    ll.curH = fminf(ll.curH + ll.growSpeed * dt, ll.targH);
                    if (ll.curH >= ll.targH) {
                        ladders.push_back(Ladder::Make(ll.x, ll.y - ll.targH, 40.f, ll.targH));
                        RebuildLayers();
                    }
                }
            }
            larperLadders.erase(
                std::remove_if(larperLadders.begin(), larperLadders.end(),
                    [](const LarperLadder& ll){ return ll.curH >= ll.targH; }),
                larperLadders.end());

            // ── Hotbar E key / Left-click use ────────────────────────────────
            bool ePressHandled = false;
            if (!isDying && (IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
                HotbarSlot& slot = hotbar[hotbarSlot];
                if (slot.type != PU_NONE) {
                    const PowerupInfo& info = PU_INFO[(int)slot.type];
                    bool ready = info.maxCD > 0.f ? (slot.cd <= 0.f) : (slot.charges > 0);
                    bool heavyOK = !isHeavyItem(slot.type) || (heavyEquipTimer >= 1.f);
                    if (ready && !info.passive && heavyOK) {
                        ePressHandled = true;
                        // ── Apply powerup ──────────────────────────────────────
                        switch(slot.type) {
                        case PU_DASH: {
                            float dist = player.width * 4.f;
                            player.x += facingRight ? dist : -dist;
                            dashActive = true; dashInvulTimer = 0.5f;
                            invincible = true; invincibleTimer = 0.5f;
                            dashShakeTimer = DASH_SHAKE_DURATION;
                            SetSoundVolume(sndDash, volAbility); PlaySound(sndDash);
                            slot.cd = info.maxCD; break; }
                        case PU_EL_SHAMAK:
                            playerHasBeatrice = true;
                            beatriceAbilityTimer = BEATRICE_DURATION;
                            beaBulletShootTimer = 0.f;
                            SetSoundVolume(sndBeatrice, volAbility); PlaySound(sndBeatrice);
                            slot.charges--; if(slot.charges<=0) slot.type=PU_NONE; break;
                        case PU_LARPER: {
                            float tH = player.height * 3.f;
                            larperLadders.push_back({player.x + player.width*0.5f - 20.f, player.y + player.height, 0.f, tH, tH / 0.5f});
                            SetSoundVolume(sndLarper, volAbility); PlaySound(sndLarper);
                            slot.charges--; if (slot.charges<=0) slot.type=PU_NONE; break; }
                        case PU_REINHARD:
                            { LevelData nextLv;
                              if (LoadLevel(nextLv, currentLevelId+1)) {
                                  levelsCleared++; runScore += 300 * ((currentLevelId-1)/5+1);
                                  currentLevelId++; pendingLevelData=nextLv; hasPendingLevel=true;
                                  InitCardSelect(2,false); currentScreen=CARD_SELECT;
                              } else { TryUpdateHighscore(); splashTimer=0.f; currentScreen=GAME_OVER; }
                            }
                            SetSoundVolume(sndReinhard, volAbility); PlaySound(sndReinhard);
                            slot.charges--; if (slot.charges<=0) slot.type=PU_NONE; break;
                        case PU_WHIP: {
                            float reach = player.width * 5.f;
                            Rectangle whipRect = facingRight
                                ? Rectangle{player.x+player.width, player.y, reach, player.height}
                                : Rectangle{player.x-reach, player.y, reach, player.height};
                            for (auto& e : enemies) if (e.active && CheckCollisionRecs(e.hitbox, whipRect)) { e.active=false; score+=300; coins+=15; runScore+=150; SetSoundVolume(sndEnemyKill, volSFX); PlaySound(sndEnemyKill); }
                            { int wi=GetRandomValue(0,2); SetSoundVolume(sndWhip[wi], volAbility); PlaySound(sndWhip[wi]); }
                            // Visual
                            whipActive      = true;
                            whipTimer       = 0.f;
                            whipFacingRight = facingRight;
                            slot.cd = info.maxCD; break; }
                        case PU_EXTRA_LIFE:
                            maxLives = 5; lives = (lives+1 < maxLives) ? lives+1 : maxLives;
                            SetSoundVolume(sndExtraLife, volAbility); PlaySound(sndExtraLife);
                            slot.charges--; if (slot.charges<=0) slot.type=PU_NONE; break;
                        case PU_NUKE_PU: {
                            SetSoundVolume(nukeSound, volSFX); PlaySound(nukeSound);
                            float sc=3.8f*0.85f*1.05f;
                            float nkW=NUKE_NATIVE_W*(NUKE_SCALE*0.25f)*sc, nkH=NUKE_NATIVE_H*(NUKE_SCALE*0.25f)*sc;
                            nukeExplosionPos={player.x+player.width*0.5f-nkW*0.5f, player.y-nkH-2.f};
                            nukeExplosionPlaying=true; nukeExplosionFrame=0; nukeExplosionTimer=0.f;
                            nukeFlashTimer=0.f; nukeExtraDelay=3.f;
                            for (auto& b:barrels){if(b.active){score+=100;coins+=5;runScore+=50;b.active=false;}}
                            for (auto& e:enemies){if(e.active){score+=300;coins+=15;runScore+=150;e.active=false; SetSoundVolume(sndEnemyKill,volSFX); PlaySound(sndEnemyKill);}}
                            regulusIsStunned=true; regulusStunEnding=false; regulusStunFrame=0;
                            regulusStunTimer=0.f; regulusStunLoops=0;
                            slot.charges--; if (slot.charges<=0) slot.type=PU_NONE; break; }
                        case PU_ONE_MORE_LARP:
                            if (lives >= maxLives) {
                                SetSoundVolume(sndError, volAbility); PlaySound(sndError);
                            } else {
                                lives++;
                                SetSoundVolume(sndOneLarp, volAbility); PlaySound(sndOneLarp);
                                slot.charges--; if (slot.charges<=0) slot.type=PU_NONE;
                            }
                            break;
                        case PU_SPEEDRUN:
                            speedrunActive=true; speedrunTimer=5.f;
                            SetSoundVolume(sndSpeedrun, volAbility); PlaySound(sndSpeedrun);
                            slot.cd=info.maxCD; break;
                        case PU_SHIELD:
                            shieldActive=true; shieldTimer=1.2f;
                            SetSoundVolume(sndShield, volAbility); PlaySound(sndShield);
                            slot.cd=info.maxCD; break;
                        case PU_SHOP: {
                            InitCardSelect(5, true);
                            slot.charges--; if(slot.charges<=0) slot.type=PU_NONE;
                            currentScreen=CARD_SELECT; break; }
                        case PU_BEATRICE:
                            playerHasBeatrice = true;
                            beatriceAbilityTimer = BEATRICE_DURATION;
                            beaBulletShootTimer = 0.f;
                            SetSoundVolume(sndBeatrice, volAbility); PlaySound(sndBeatrice);
                            slot.charges--; if(slot.charges<=0) slot.type=PU_NONE; break;
                        default: break;
                        }
                    }
                }
            }

            // ── Elevator children ─────────────────────────────────────────────
            struct ElevPlatSnapshot { int platIndex; float prevY; float newY; float platW; };
            vector<ElevPlatSnapshot> elevSnapshots;

            // Advance backAndForth elevator travel
            if ((int)liveElevTravel.size() < (int)liveElevators.size())
                liveElevTravel.resize(liveElevators.size(), 0.f);
            if ((int)liveElevBounceDir.size() < (int)liveElevators.size())
                liveElevBounceDir.resize(liveElevators.size(), 1);
            for (int ei = 0; ei < (int)liveElevators.size(); ei++) {
                const ElevatorData& el = liveElevators[ei];
                if (!el.backAndForth) continue;
                float& travel = liveElevTravel[ei];
                int&   bdir   = liveElevBounceDir[ei];
                travel += bdir * el.speed * dt;
                if (travel >= el.h) { travel = el.h; bdir = -1; }
                else if (travel <= 0.f) { travel = 0.f; bdir = 1; }
            }

            for (int ri = 0; ri < (int)liveRelations.size(); ri++) {
                const auto& rel = liveRelations[ri];
                if (rel.parent.type != 11) continue;
                int ei = rel.parent.index;
                if (ei < 0 || ei >= (int)liveElevators.size()) continue;
                const ElevatorData& el = liveElevators[ei];
                if ((int)elevChildPhases.size() <= ri) elevChildPhases.resize(ri + 1, 0.f);
                float& phase = elevChildPhases[ri];

                int ci = rel.child.index;
                if (rel.child.type == 4 && ci >= 0 && ci < (int)platforms.size())
                    elevSnapshots.push_back({ ci, platforms[ci].y, 0.f, platforms[ci].width });

                float cx, cy;
                if (el.backAndForth) {
                    float travel = liveElevTravel[ei];
                    if (el.horizontal) {
                        cx = el.x + rel.offsetX + travel * (float)el.direction;
                        cy = el.y + rel.offsetY;
                    } else {
                        cx = el.x + rel.offsetX;
                        cy = el.y + rel.offsetY - travel * (float)el.direction;
                    }
                } else if (el.horizontal) {
                    if (el.direction == 1) { phase -= el.speed * dt; if (phase < 0.f) phase = el.h; }
                    else                   { phase += el.speed * dt; if (phase > el.h) phase = 0.f; }
                    cx = el.x + phase;
                    cy = el.y + rel.offsetY;
                } else {
                    if (el.direction == 1) { phase -= el.speed * dt; if (phase < 0.f) phase = el.h; }
                    else                   { phase += el.speed * dt; if (phase > el.h) phase = 0.f; }
                    cx = el.x + rel.offsetX;
                    cy = el.y + phase;
                }

                if (rel.child.type == 4 && !elevSnapshots.empty())
                    elevSnapshots.back().newY = cy;

                switch (rel.child.type) {
                case 2: // REGULUS (singleton, index=-1)
                    currentLevelData.regulusPos = { cx, cy }; break;
                case 4: if (ci >= 0 && ci < (int)platforms.size())     platforms[ci] = Platform::Make(cx, cy, platforms[ci].width, 0, 0.f); break;
                case 5: if (ci >= 0 && ci < (int)ladders.size())       ladders[ci] = Ladder::Make(cx, cy, ladders[ci].width, ladders[ci].height); break;
                case 6: if (ci >= 0 && ci < (int)beamPositions.size()) { beamPositions[ci].x = cx; beamPositions[ci].y = cy; } break;
                case 8: if (ci >= 0 && ci < (int)nukeSpawnNodes.size())      nukeSpawnNodes[ci] = { cx, cy }; break;
                case 9: if (ci >= 0 && ci < (int)beatriceSpawnNodes.size())   beatriceSpawnNodes[ci] = { cx, cy }; break;
                case 10:if (ci >= 0 && ci < (int)enemySpawnPositions.size())  enemySpawnPositions[ci] = { cx, cy }; break;
                }
            }

            // ── Whip visual timer ─────────────────────────────────────────────
            if (whipActive) {
                whipTimer += dt;
                if (whipTimer >= 0.5f) whipActive = false;
            }

            animationTimer += dt;
            if (ladderCooldown > 0.0f) ladderCooldown -= dt;

            if (invincible)
            {
                invincibleTimer -= dt;
                if (invincibleTimer <= 0.0f) { invincible = false; invincibleTimer = 0.0f; }
            }

            float prevX = player.x, prevY = player.y;
            bool  playerIsMoving = false;
            wasGrounded = isGrounded;

            auto PlayerHitbox = [&]() -> Rectangle {
                float colW = player.width * 0.5f;
                float colH = player.height * 0.5f;
                float offX = (player.width - colW) * 0.5f;
                float offY = player.height - colH;
                return { player.x + offX, player.y + offY, colW, colH };
                };

            if (!isDying)
            {
                for (auto& b : barrels)
                {
                    bool wasActive = b.active;
                    b.reachedEnd = false;
                    UpdateBarrel(b, barrelPath, dt);
                    if (wasActive && !b.active && b.reachedEnd)
                        SpawnEnemyAtEnd(b.hitbox.x + b.hitbox.width * 0.5f);
                }
            }

            // ── Cave timed enemy spawn ────────────────────────────────────────
            if (!isDying && currentLevelData.hasCave && currentLevelData.caveSpawnEnabled)
            {
                caveSpawnTimer += dt;
                if (caveSpawnTimer >= currentLevelData.caveSpawnRate)
                {
                    caveSpawnTimer = 0.f;
                    SpawnEnemyAtEnd(houseX + houseW * 0.5f, houseY + houseH);
                }
            }

            // ── Flying nuke physics ───────────────────────────────────────────
            for (auto& fn : flyingNukes)
            {
                if (!fn.active) continue;
                float prevFnX = fn.rect.x, prevFnY = fn.rect.y;
                fn.vel.y += gravity;
                fn.rect.x += fn.vel.x;
                fn.rect.y += fn.vel.y;
                float fnVx = fn.vel.x, fnVy = fn.vel.y;
                CollisionResult col = CollisionManager::ResolveAll(
                    fn.rect, fnVx, fnVy, platforms, prevFnX, prevFnY);
                fn.vel.x = fnVx;
                fn.vel.y = fnVy;
                if (col.grounded) { nukes.push_back({ { fn.rect.x, fn.rect.y }, true }); fn.active = false; }
                if (fn.rect.y > (float)screenHeight + 120.0f) fn.active = false;
            }

            // ── Beatrice item animation ───────────────────────────────────────
            beatriceItemAnimTimer += dt;
            if (beatriceItemAnimTimer >= 0.35f)
            {
                beatriceItemAnimFrame = (beatriceItemAnimFrame + 1) % 2; beatriceItemAnimTimer = 0.0f;
            }

            // ── Prop fire animation (Dk_Oil_Fire3 <-> Dk_Oil_Fire4) ──────────
            propFireTimer += dt;
            if (propFireTimer >= 0.15f)
            {
                propFireFrame = 1 - propFireFrame; propFireTimer = 0.f;
            }

            // ── Regulus animation ─────────────────────────────────────────────
            if (regulusIsStunned)
            {
                if (regulusStunEnding)
                {
                    regulusStunEndTimer += dt;
                    if (regulusStunEndTimer >= 1.0f / REGULUS_STUN_END_FPS)
                    {
                        regulusStunEndTimer -= 1.0f / REGULUS_STUN_END_FPS;
                        regulusStunEndFrame++;
                        if (regulusStunEndFrame >= 5)
                        {
                            regulusIsStunned = false;
                            regulusStunEnding = false;
                            regulusStunEndFrame = 0;
                            regulusIdleFrame = 0;
                            regulusIdleTimer = 0.0f;
                        }
                    }
                }
                else
                {
                    regulusStunTimer += dt;
                    if (regulusStunTimer >= 1.0f / REGULUS_STUN_FPS)
                    {
                        regulusStunTimer -= 1.0f / REGULUS_STUN_FPS;
                        regulusStunFrame++;
                        if (regulusStunFrame >= 3)
                        {
                            regulusStunFrame = 0;
                            regulusStunLoops++;
                            if (regulusStunLoops >= REGULUS_STUN_LOOPS)
                            {
                                regulusStunEnding = true; regulusStunEndFrame = 0; regulusStunEndTimer = 0.0f;
                            }
                        }
                    }
                }
            }
            else if (!regulusThrowing)
            {
                regulusIdleTimer += dt;
                if (regulusIdleTimer >= 1.0f / REGULUS_IDLE_FPS)
                {
                    regulusIdleTimer -= 1.0f / REGULUS_IDLE_FPS; regulusIdleFrame = (regulusIdleFrame + 1) % 3;
                }
            }
            else
            {
                regulusThrowTimer += dt;
                if (regulusThrowTimer >= 1.0f / REGULUS_THROW_FPS)
                {
                    regulusThrowTimer -= 1.0f / REGULUS_THROW_FPS;
                    regulusThrowFrame++;
                    if (regulusThrowFrame >= 3)
                    {
                        if (regulusSpawnPending)
                        {
                            SpawnBarrelFromPool(barrels, barrelPath, 4.0f, 26.25f, 26.25f, regulusForceBlue);
                            regulusSpawnPending = false;
                            regulusForceBlue = false;
                        }
                        regulusThrowing = false;
                        regulusThrowFrame = 0;
                        regulusIdleFrame = 0;
                        regulusIdleTimer = 0.0f;
                    }
                }
            }

            // ── Nuke pickup (F key → hotbar) ──────────────────────────────────
            if (!isDying && IsKeyPressed(KEY_F))
            {
                float nkW = NUKE_NATIVE_W * NUKE_SCALE, nkH = NUKE_NATIVE_H * NUKE_SCALE;
                for (auto& nk : nukes)
                {
                    if (!nk.active) continue;
                    Rectangle nkRect = { nk.pos.x, nk.pos.y, nkW, nkH };
                    if (CheckCollisionRecs(player, nkRect)) { nk.active = false; AddToHotbar(PU_NUKE_PU); break; }
                }
                // Beatrice pickup (same F key)
                if (!playerHasBeatrice)
                {
                    float bcScale = 2.0f;
                    float bcW = Beatrice_Idle1.width * bcScale;
                    float bcH = Beatrice_Idle1.height * bcScale;
                    for (auto& bc : beatrices)
                    {
                        if (!bc.active) continue;
                        Rectangle bcRect = { bc.pos.x, bc.pos.y - bcH, bcW, bcH };
                        if (CheckCollisionRecs(player, bcRect))
                        {
                            bc.active = false; AddToHotbar(PU_BEATRICE); break;
                        }
                    }
                }
            }

            // ── Beatrice ability ──────────────────────────────────────────────
            if (playerHasBeatrice && !isDying)
            {
                beatriceAbilityTimer -= dt;
                if (beatriceAbilityTimer <= 0.0f)
                {
                    playerHasBeatrice = false;
                    beatriceAbilityTimer = 0.0f;
                    beaBulletShootTimer = 0.0f;
                    for (auto& bb : beaBullets) bb.active = false;
                }
                else if (!onLadder)
                {
                    beaBulletShootTimer += dt;
                    if (beaBulletShootTimer >= BEA_BULLET_SHOOT_INTERVAL)
                    {
                        beaBulletShootTimer = 0.0f;
                        Vector2 mousePos = GetMousePosition();
                        float   startX = player.x + player.width * 0.5f;
                        float   startY = player.y + player.height * 0.30f;
                        Vector2 dir = Vector2Normalize(Vector2Subtract(mousePos, { startX, startY }));
                        for (auto& bb : beaBullets)
                        {
                            if (!bb.active)
                            {
                                bb.pos = { startX, startY };
                                bb.vel = { dir.x * BEA_BULLET_SPEED, dir.y * BEA_BULLET_SPEED };
                                bb.lifetime = 0.0f;
                                bb.active = true;
                                SetSoundVolume(sndBeatriceAtk, volAbility); PlaySound(sndBeatriceAtk);
                                break;
                            }
                        }
                    }
                }
            }

            // ── Bullet update & collisions ────────────────────────────────────
            for (auto& bb : beaBullets)
            {
                if (!bb.active) continue;
                bb.pos.x += bb.vel.x * dt;
                bb.pos.y += bb.vel.y * dt;
                bb.lifetime += dt;
                if (bb.lifetime >= BEA_BULLET_LIFETIME
                    || bb.pos.x < -60 || bb.pos.x > screenWidth + 60
                    || bb.pos.y < -60 || bb.pos.y > screenHeight + 60)
                {
                    bb.active = false; continue;
                }

                float     bbScale = 2.0f;
                float     bbHalfW = texBeaBullet.width * bbScale * 0.5f;
                float     bbHalfH = texBeaBullet.height * bbScale * 0.5f;
                Rectangle bbRect = { bb.pos.x - bbHalfW, bb.pos.y - bbHalfH, bbHalfW * 2.0f, bbHalfH * 2.0f };

                for (auto& b : barrels)
                {
                    if (!b.active) continue;
                    if (CheckCollisionRecs(bbRect, b.hitbox))
                    {
                        b.active = false; bb.active = false; score += 100; coins += 5; runScore += 50; break;
                    }
                }
                if (bb.active) {
                    for (auto& en : enemies)
                    {
                        if (!en.active) continue;
                        if (CheckCollisionRecs(bbRect, en.hitbox))
                        {
                            en.active = false; bb.active = false; score += 300; coins += 15; runScore += 150;
                            SetSoundVolume(sndEnemyKill, volSFX); PlaySound(sndEnemyKill); break;
                        }
                    }
                }
            }

            if (nukeExtraDelay > 0.0f) nukeExtraDelay -= dt;

            if (nukeExplosionPlaying)
            {
                nukeExplosionTimer += dt;
                if (nukeExplosionTimer >= 1.0f / NUKE_EXPL_FPS)
                {
                    nukeExplosionTimer -= 1.0f / NUKE_EXPL_FPS;
                    nukeExplosionFrame++;
                    if (nukeExplosionFrame >= NUKE_EXPL_FRAME_COUNT) nukeExplosionPlaying = false;
                }
            }

            {
                float flashTotal = NUKE_FLASH_IN + NUKE_FLASH_OUT;
                if (nukeFlashTimer < flashTotal) nukeFlashTimer += dt;
                if (nukeFlashTimer >= NUKE_FLASH_IN && nukeFlashTimer < flashTotal)
                {
                    float shakeFade = 1.0f - (nukeFlashTimer - NUKE_FLASH_IN) / NUKE_FLASH_OUT;
                    float shakeMag = NUKE_SHAKE_AMOUNT * shakeFade;
                    nukeShakeOffset = {
                        (float)GetRandomValue((int)-shakeMag, (int)shakeMag),
                        (float)GetRandomValue((int)-shakeMag, (int)shakeMag)
                    };
                }
                else nukeShakeOffset = { 0, 0 };
            }

            // ── Regulus active/inactive state machine ─────────────────────────
            if (!isDying && nukeExtraDelay <= 0.0f && !regulusIsStunned)
            {
                regulusStateTickTimer += dt;
                if (regulusStateTickTimer >= 1.0f)
                {
                    regulusStateTickTimer -= 1.0f;
                    if (regulusIsActive)
                    {
                        int threshold = 20 + regulusActiveFails * 20;
                        if (threshold > 100) threshold = 100;
                        if (GetRandomValue(1, 100) <= threshold)
                        {
                            regulusIsActive = false;
                            regulusInactiveTime = 0.0f;
                            regulusInactiveFails = 0;
                            regulusActiveFails = 0;
                        }
                        else regulusActiveFails++;
                    }
                    else
                    {
                        if (regulusInactiveTime >= 3.0f)
                        {
                            int threshold = 30 + regulusInactiveFails * 15;
                            if (threshold > 100) threshold = 100;
                            if (GetRandomValue(1, 100) <= threshold)
                            {
                                regulusIsActive = true;
                                regulusActiveFails = 0;
                                regulusInactiveFails = 0;
                                regulusActiveSpawnTimer = 0.0f;
                            }
                            else regulusInactiveFails++;
                        }
                    }
                }
                if (!regulusIsActive) regulusInactiveTime += dt;
            }

            // ── Barrel spawn ──────────────────────────────────────────────────
            if (!isDying && regulusIsActive && nukeExtraDelay <= 0.0f && !regulusIsStunned)
            {
                regulusActiveSpawnTimer += dt;
                if (regulusActiveSpawnTimer >= ACTIVE_SPAWN_INTERVAL)
                {
                    regulusActiveSpawnTimer -= ACTIVE_SPAWN_INTERVAL;
                    if (!regulusThrowing)
                    {
                        regulusThrowing = true;
                        regulusThrowFrame = 0;
                        regulusThrowTimer = 0.0f;
                        regulusSpawnPending = true;
                        regulusForceBlue = false;
                    }
                }
            }

            // ── Barrel / player collision ─────────────────────────────────────
            if (!isDying && !invincible)
            {
                for (auto& b : barrels)
                {
                    if (!b.active) continue;
                    if (!CheckCollisionRecs(PlayerHitbox(), b.hitbox)) continue;
                    bool fromBelow = (!b.isFalling && velocityY < 0.0f &&
                        (player.y + player.height * 0.5f) >(b.hitbox.y + b.hitbox.height));
                    if (fromBelow) continue;
                    if (shieldActive) { b.active = false; score += 100; coins += 5; runScore += 50; shieldActive = false; break; }
                    TriggerDeath();
                    break;
                }
            }

            // ── Enemy update & collision ──────────────────────────────────────
            if (!isDying)
            {
                for (auto& en : enemies)
                {
                    UpdateEnemy(en, player, platforms, ladders, dt);
                    if (en.active && !invincible && CheckCollisionRecs(PlayerHitbox(), en.hitbox)) {
                        if (shieldActive) { en.active = false; score += 300; coins += 15; runScore += 150; shieldActive = false; }
                        else TriggerDeath();
                    }
                }
            }

            // ── Jump-over-barrel scoring ──────────────────────────────────────
            if (!isDying && !onLadder && !isGrounded)
            {
                for (auto& b : barrels)
                {
                    if (!b.active || b.jumpScored) continue;
                    float zoneW = (b.hitbox.width + 10.0f) * 1.3f;
                    float zoneH = b.hitbox.height * 1.3f;
                    Rectangle jumpZone = {
                        b.hitbox.x - (zoneW - b.hitbox.width) * 0.5f,
                        b.hitbox.y - zoneH, zoneW, zoneH
                    };
                    if (CheckCollisionRecs(player, jumpZone))
                    {
                        score += 100; coins += 5; runScore += 50; b.jumpScored = true; SetSoundVolume(jumpBrlSound, volSFX); PlaySound(jumpBrlSound); SetSoundVolume(sndMoney, volSFX); PlaySound(sndMoney);
                    }
                }
            }

            // ── Death sequence ────────────────────────────────────────────────
            if (isDying)
            {
                deathTimer += dt;
                if (!hitPlayed) { SetSoundVolume(HitSound, volSFX); PlaySound(HitSound); hitPlayed = true; }
                if (deathTimer > 0.5f && !deathPlayed) { SetSoundVolume(deathSound, volSFX); PlaySound(deathSound); deathPlayed = true; }

                if (deathShakeTimer > 0.0f)
                {
                    deathShakeTimer -= dt;
                    float mag = DEATH_SHAKE_AMOUNT * (deathShakeTimer / DEATH_SHAKE_DURATION);
                    deathShakeOffset = {
                        (float)GetRandomValue((int)-mag, (int)mag),
                        (float)GetRandomValue((int)-mag, (int)mag)
                    };
                }
                else deathShakeOffset = { 0, 0 };
            }

            if (dashShakeTimer > 0.f)
            {
                dashShakeTimer -= dt;
                float mag = DASH_SHAKE_AMOUNT * (dashShakeTimer / DASH_SHAKE_DURATION);
                dashShakeOffset = {
                    (float)GetRandomValue((int)-mag, (int)mag),
                    (float)GetRandomValue((int)-mag, (int)mag)
                };
            }
            else dashShakeOffset = { 0, 0 };

            if (isDying) {
                if (!deathReachedBlack)
                {
                    deathFallVelY += DEATH_FALL_GRAVITY;
                    player.y += deathFallVelY;
                    if (deathTimer >= DEATH_TOTAL_FALL)
                    {
                        deathReachedBlack = true; deathBlackTimer = 0.0f;
                    }
                }
                else
                {
                    deathBlackTimer += dt;
                    if (deathBlackTimer >= (DEATH_BLACK_HOLD + 0.5f))
                    {
                        if (lives <= 0) { TryUpdateHighscore(); currentScreen = GAME_OVER; }
                        else            { ResetRound(); SpawnInitialEnemies(); }
                    }
                }
            }

            // ── Blue barrel / house collision ─────────────────────────────────
            if (!isDying && !houseAnimPlaying)
            {
                for (auto& b : barrels)
                {
                    if (b.active && CheckCollisionRecs(b.hitbox, houseHitbox))
                    {
                        houseAnimPlaying = true; houseAnimFrame = 0; houseAnimTimer = 0.0f; b.active = false; break;
                    }
                }
            }
            if (houseAnimPlaying)
            {
                houseAnimTimer += dt;
                if (houseAnimTimer >= 1.0f / HOUSE_ANIM_FPS)
                {
                    houseAnimTimer -= 1.0f / HOUSE_ANIM_FPS;
                    houseAnimFrame++;
                    if (houseAnimFrame == HOUSE_SWAP_AT_FRAME) houseIsSnowed = true;
                    if (houseAnimFrame >= CAVE_FRAME_COUNT) { houseAnimPlaying = false; houseAnimFrame = CAVE_FRAME_COUNT - 1; }
                }
            }

            // ── Player input ──────────────────────────────────────────────────
            if (!isDying)
            {
                if ((dbgFlight || dbgFlightNoCol) && onLadder) { onLadder = false; currentLadder = -1; }
                if (onLadder)
                {
                    const Ladder& lad = ladders[currentLadder];
                    if (ladderExitPlaying)
                    {
                        float exitTotalDuration = 2.0f * ladderExitFrameDuration;
                        ladderProgress += (0.4f / exitTotalDuration) * dt;
                        ladderProgress = Clamp(ladderProgress, 0.0f, 1.0f);
                        player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                        player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
                        ladderExitTimer += dt;
                        if (ladderExitTimer >= ladderExitFrameDuration) { ladderExitTimer = 0.0f; ladderExitStep++; }
                        if (ladderExitStep == 0) image = &imgMarioClimbEnd1;
                        else if (ladderExitStep == 1) image = &imgMarioClimbEnd2;
                        else
                        {
                            image = &imgMarioClimbDown;
                            bool wants = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                                || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S)
                                || IsKeyPressed(KEY_SPACE);
                            if (wants)
                            {
                                onLadder = false; currentLadder = -1;
                                ladderExitPlaying = false; ladderExitStep = 0; ladderExitTimer = 0;
                                ladderEntryClamp = false;
                                velocityY = 0; isJumping = false;
                            }
                        }
                    }
                    else if (ladderEntryClamp)
                    {
                        ladderEntryClampTimer += dt;
                        float t = Clamp(ladderEntryClampTimer / 0.3f, 0.0f, 1.0f);
                        ladderProgress = ladderEntryClampStart + (0.57f - ladderEntryClampStart) * t;
                        player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                        player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
                        velocityY = 0; velocityX = 0;
                        ladderClimbTimer += dt;
                        if (ladderClimbTimer >= ladderClimbAnimSpeed)
                        {
                            ladderClimbFrame = (ladderClimbFrame + 1) % 2; ladderClimbTimer = 0;
                        }
                        image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        if (t >= 1.0f) ladderEntryClamp = false;
                    }
                    else
                    {
                        float prevLadderY = player.y;
                        bool climbing = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);
                        if (IsKeyDown(KEY_W)) ladderProgress += ladderClimbSpeed * 0.75f / lad.ClimbHeight();
                        if (IsKeyDown(KEY_S)) ladderProgress -= ladderClimbSpeed * 0.75f / lad.ClimbHeight();
                        ladderProgress = Clamp(ladderProgress, 0.0f, 1.0f);
                        player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                        player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
                        // Metalbar step sounds when climbing up (2x faster interval than walking)
                        if (IsKeyDown(KEY_W) && player.y < prevLadderY) {
                            static constexpr float LADDER_STEP_DIST = 19.2f;
                            static constexpr int   METALBAR_IDX = 9;
                            ladderStepDist += prevLadderY - player.y;
                            if (ladderStepDist >= LADDER_STEP_DIST && !footsteps[METALBAR_IDX].walk.empty()) {
                                ladderStepDist = 0.f;
                                int idx = GetRandomValue(0, (int)footsteps[METALBAR_IDX].walk.size() - 1);
                                SetSoundVolume(footsteps[METALBAR_IDX].walk[idx], volSFX * 0.5f);
                                PlaySound(footsteps[METALBAR_IDX].walk[idx]);
                            }
                        } else {
                            ladderStepDist = 0.f;
                        }
                        velocityY = 0; velocityX = 0;
                        if (ladderProgress <= 0.0f)
                        {
                            image = &imgMarioClimbDown;
                            bool wants = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                                || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S)
                                || IsKeyPressed(KEY_SPACE);
                            if (wants)
                            {
                                onLadder = false; currentLadder = -1;
                                ladderEntryClamp = false;
                                isGrounded = true; isJumping = false; ladderCooldown = 0.2f;
                            }
                        }
                        else if (ladderProgress >= 0.6f)
                        {
                            ladderExitPlaying = true;
                            ladderExitStep = 0; ladderExitTimer = 0;
                            image = &imgMarioClimbEnd1;
                        }
                        else if (climbing)
                        {
                            ladderClimbTimer += dt;
                            if (ladderClimbTimer >= ladderClimbAnimSpeed)
                            {
                                ladderClimbFrame = (ladderClimbFrame + 1) % 2; ladderClimbTimer = 0;
                            }
                            image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        }
                    }
                }
                else
                {
                    bool flyMode = dbgFlight || dbgFlightNoCol;
                    { float spd = playerSpeed * (speedrunActive ? 1.5f : 1.f);
                    if (IsKeyDown(KEY_D)) { player.x += spd; playerIsMoving = true; facingRight = true; }
                    if (IsKeyDown(KEY_A)) { player.x -= spd; playerIsMoving = true; facingRight = false; } }

                    if (!flyMode && (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S)))
                    {
                        bool entered = false;
                        if (!isCarryingHeavy && !playerHasBeatrice)
                        {
                            for (int i = 0; i < (int)ladders.size(); i++)
                            {
                                Rectangle _lhb = ladders[i].GetHitbox();
                                float     _lnw = _lhb.width * 0.5f;
                                Rectangle _lnarrow = { _lhb.x + (_lhb.width - _lnw) * 0.5f, _lhb.y, _lnw, _lhb.height - 20.0f };
                                if (ladderCooldown <= 0 && CheckCollisionRecs(player, _lnarrow))
                                {
                                    float ip = ladders[i].ProgressFromPlayerY(player.y, player.height);
                                    ladderProgress = Clamp(ip, 0.01f, 0.99f);
                                    ladderEntryClamp = false;
                                    if (ladderProgress > 0.65f)
                                    {
                                        ladderEntryClamp = true;
                                        ladderEntryClampStart = ladderProgress;
                                        ladderEntryClampTimer = 0.0f;
                                    }
                                    onLadder = true;
                                    currentLadder = i;
                                    ladderExitPlaying = false; ladderExitStep = 0; ladderExitTimer = 0;
                                    ladderClimbFrame = 0;     ladderClimbTimer = 0;
                                    player.x = ladders[i].x + ladders[i].width * 0.5f - player.width * 0.5f;
                                    velocityY = 0; velocityX = 0; isJumping = false; isGrounded = false;
                                    entered = true; break;
                                }
                            }
                        }
                        if (!entered && IsKeyPressed(KEY_W) && !isJumping)
                        {
                            velocityY = jumpForce; isJumping = true; isGrounded = false;
                        }
                    }

                    if (!flyMode && IsKeyPressed(KEY_SPACE) && !isJumping)
                    {
                        velocityY = jumpForce; isJumping = true; isGrounded = false;
                    }

                    if (flyMode) {
                        velocityX = 0; velocityY = 0;
                        if (IsKeyDown(KEY_W)) player.y -= playerSpeed * 2.5f;
                        if (IsKeyDown(KEY_S)) player.y += playerSpeed * 2.5f;
                        isJumping = false;
                    } else {
                        if (!isGrounded) velocityY += gravity;
                        player.y += velocityY;
                    }

                    if (!dbgFlightNoCol)
                    {
                        float colW = player.width * 0.5f;
                        float colH = player.height * 0.5f;
                        float colOffX = (player.width - colW) * 0.5f;
                        float colOffY = player.height - colH;
                        Rectangle colRect = { player.x + colOffX, player.y + colOffY, colW, colH };
                        float     colPrevY = prevY + colOffY;

                        CollisionResult col = CollisionManager::ResolveAll(
                            colRect, velocityX, velocityY, platforms, prevX, colPrevY);

                        player.y = colRect.y - colOffY;
                        isGrounded = col.grounded;
                        if (col.grounded) isJumping = false;
                    }
                    else { isGrounded = false; }

                    // Push player up when an elevator platform rises into them
                    if (!onLadder)
                    {
                        for (const auto& snap : elevSnapshots)
                        {
                            if (snap.newY >= snap.prevY) continue;
                            int pi = snap.platIndex;
                            if (pi < 0 || pi >= (int)platforms.size()) continue;
                            const Platform& movPlat = platforms[pi];

                            float platTop = movPlat.y;
                            float platLeft = movPlat.x;
                            float platRight = movPlat.x + movPlat.width;
                            float playerBottom = player.y + player.height;

                            bool hOverlap = (player.x + player.width) > platLeft && player.x < platRight;
                            if (!hOverlap) continue;

                            float penetration = playerBottom - platTop;
                            if (penetration > 0.f && penetration < player.height * 0.75f)
                            {
                                player.y -= penetration;
                                if (velocityY > 0.f) velocityY = 0.f;
                                isGrounded = true;
                                isJumping = false;
                            }
                        }
                    }

                    // ── Footstep sounds ───────────────────────────────────────
                    if (isGrounded && !onLadder && !isDying) {
                        float stepped = fabsf(player.x - prevX);
                        playerStepDist += stepped;
                        static constexpr float STEP_DIST = 48.f / 1.25f;
                        if (playerStepDist >= STEP_DIST && stepped > 0.1f) {
                            playerStepDist = 0.f;
                            float pcx = player.x + player.width * 0.5f;
                            float pbot = player.y + player.height;
                            int   bestMat = 0;
                            // Check conveyors first using the same feet/belt overlap as the push code
                            {
                                float colW = player.width * 0.5f, colH = player.height * 0.5f;
                                float offX = (player.width - colW) * 0.5f, offY = player.height - colH;
                                Rectangle feet = { player.x + offX, player.y + offY, colW, colH };
                                for (const auto& cv : liveConveyors) {
                                    if (cv.soundMaterial <= 0) continue;
                                    Rectangle belt = { cv.x, cv.y - 4.f, cv.length, cv.beltH + 8.f };
                                    if (CheckCollisionRecs(feet, belt)) { bestMat = cv.soundMaterial; break; }
                                }
                            }
                            // Fall back to beam raycast if not on a conveyor
                            if (bestMat == 0) {
                                static constexpr float STEP_TOL = 12.f;
                                float bestD = -9999.f;
                                for (const auto& bm : beamPositions) {
                                    if (bm.soundMaterial <= 0) continue;
                                    Texture2D* bt = (bm.texVariant >= 1 && bm.texVariant <= 12 && beamVariants[bm.texVariant - 1].id > 0)
                                        ? &beamVariants[bm.texVariant - 1] : &beam;
                                    float bw = bt->width * 4.f, bh = bt->height * 4.f;
                                    if (pcx >= bm.x && pcx <= bm.x + bw) {
                                        float d = bm.y - pbot;
                                        if (d >= -bh && d < STEP_TOL && d > bestD) { bestD = d; bestMat = bm.soundMaterial; }
                                    }
                                }
                            }
                            if (bestMat > 0 && bestMat < MAT_COUNT) {
                                bool useRun = speedrunActive && !footsteps[bestMat].run.empty();
                                const auto& pool = useRun ? footsteps[bestMat].run : footsteps[bestMat].walk;
                                if (!pool.empty()) {
                                    int idx = GetRandomValue(0, (int)pool.size() - 1);
                                    SetSoundVolume(pool[idx], volSFX * 0.5f);
                                    PlaySound(pool[idx]);
                                }
                            }
                        }
                    } else {
                        playerStepDist = 0.f;
                    }

                    // ── Landing SFX ───────────────────────────────────────────
                    // Only fire when coming from a real airborne stretch (≥0.1 s)
                    if (isGrounded && !wasGrounded && airborneTime >= 0.1f && !onLadder && !isDying) {
                        float pcx = player.x + player.width * 0.5f;
                        float pbot = player.y + player.height;
                        int landMat = 0;
                        // Check conveyor first with the same belt overlap the push code uses
                        {
                            float colW = player.width * 0.5f, colH = player.height * 0.5f;
                            float offX = (player.width - colW) * 0.5f, offY = player.height - colH;
                            Rectangle feet = { player.x + offX, player.y + offY, colW, colH };
                            for (const auto& cv : liveConveyors) {
                                if (cv.soundMaterial <= 0) continue;
                                Rectangle belt = { cv.x, cv.y - 4.f, cv.length, cv.beltH + 8.f };
                                if (CheckCollisionRecs(feet, belt)) { landMat = cv.soundMaterial; break; }
                            }
                        }
                        // Fall back to beam raycast
                        if (landMat == 0) {
                            static constexpr float LAND_TOL = 12.f;
                            float bestLD = -9999.f;
                            for (const auto& bm : beamPositions) {
                                if (bm.soundMaterial <= 0) continue;
                                Texture2D* bt = (bm.texVariant >= 1 && bm.texVariant <= 12 && beamVariants[bm.texVariant-1].id > 0)
                                    ? &beamVariants[bm.texVariant-1] : &beam;
                                float bw = bt->width * 4.f, bh = bt->height * 4.f;
                                if (pcx >= bm.x && pcx <= bm.x + bw) {
                                    float d = bm.y - pbot;
                                    if (d >= -bh && d < LAND_TOL && d > bestLD) { bestLD = d; landMat = bm.soundMaterial; }
                                }
                            }
                        }
                        if (landMat > 0 && landMat < MAT_COUNT && !footsteps[landMat].walk.empty()) {
                            int idx = GetRandomValue(0, (int)footsteps[landMat].walk.size()-1);
                            SetSoundVolume(footsteps[landMat].walk[idx], volSFX * 0.7f);
                            PlaySound(footsteps[landMat].walk[idx]);
                        }
                    }
                    // Track time spent airborne (reset on ground)
                    if (isGrounded) airborneTime = 0.f;
                    else            airborneTime += dt;

                    // ── Player animation ──────────────────────────────────────
                    if (IsKeyPressed(KEY_B) && !isDying && isGrounded && !onLadder && !playerIsMoving)
                    {
                        isEmoting = true; emoteFrame = 0; emoteTimer = 0.f;
                    }
                    if (isEmoting && (playerIsMoving || isJumping || isDying))
                        isEmoting = false;
                    if (isEmoting)
                    {
                        emoteTimer += dt;
                        if (emoteTimer >= EMOTE_FRAME_SPD)
                        {
                            emoteTimer = fmod(emoteTimer, EMOTE_FRAME_SPD); emoteFrame = (emoteFrame + 1) % EMOTE_FRAME_COUNT;
                        }
                        image = &imgSubaruDance[emoteFrame];
                    }
                    else if (isJumping)
                    {
                        if (playerHasBeatrice)    image = &Dk_Mario_Jump_Beatrice;
                        else if (isCarryingHeavy) image = &imgMarioJumpNuke;
                        else                      image = &imgMarioJump;
                    }
                    else if (playerIsMoving)
                    {
                        if (animationTimer >= animationSpeed)
                        {
                            walkFrame = (walkFrame + 1) % 2; animationTimer = fmod(animationTimer, animationSpeed);
                        }
                        if (playerHasBeatrice)    image = (walkFrame == 0) ? &Dk_Mario_Walk1_Beatrice : &Dk_Mario_Walk2_Beatrice;
                        else if (isCarryingHeavy) image = (walkFrame == 0) ? &imgMarioWalk1Nuke : &imgMarioWalk2Nuke;
                        else                      image = (walkFrame == 0) ? &imgMarioWalk1 : &imgMarioWalk2;
                    }
                    else
                    {
                        if (playerHasBeatrice)    image = (beatriceItemAnimFrame == 0) ? &Dk_Mario_Idle1_Beatrice : &Dk_Mario_Idle2_Beatrice;
                        else if (isCarryingHeavy) image = &imgMarioIdleNuke;
                        else                      image = &imgMarioIdle;
                        walkFrame = 0;
                    }

                    if (player.y > screenHeight + 40) TriggerDeath();
                }
            }

            if (isDying) { isEmoting = false; image = (deathFallVelY < 0.0f) ? &imgMarioJump : &imgMarioFalling; }

            // ── Kill zone collision ───────────────────────────────────────────
            if (!isDying && !invincible) {
                for (const auto& kz : liveKillZones) {
                    if (CheckCollisionRecs({ kz.x, kz.y, kz.w, kz.h }, PlayerHitbox()))
                    {
                        TriggerDeath(); break;
                    }
                }
            }

            // ── Conveyor push ─────────────────────────────────────────────────
            for (int cvi = 0; cvi < (int)liveConveyors.size(); cvi++) {
                const ConveyorData& cv = liveConveyors[cvi];
                float rad = cv.rotation * DEG2RAD;
                float driftX = cv.direction * cv.speed * dt * cosf(rad);
                float driftY = cv.direction * cv.speed * dt * sinf(rad);
                Rectangle belt = { cv.x, cv.y - 4.f, cv.length, cv.beltH + 8.f };

                if (!isDying && !onLadder && isGrounded) {
                    float colW = player.width * 0.5f, colH = player.height * 0.5f;
                    float offX = (player.width - colW) * 0.5f, offY = player.height - colH;
                    Rectangle feet = { player.x + offX, player.y + offY, colW, colH };
                    if (CheckCollisionRecs(feet, belt)) { player.x += driftX; player.y += driftY; }
                }
                for (auto& en : enemies) {
                    if (!en.active || !en.grounded) continue;
                    if (CheckCollisionRecs(en.hitbox, belt)) { en.hitbox.x += driftX; en.hitbox.y += driftY; }
                }
                for (auto& b : barrels) {
                    if (!b.active || b.isFalling) continue;
                    Rectangle bFeet = { b.hitbox.x, b.hitbox.y + b.hitbox.height - 6.f, b.hitbox.width, 6.f };
                    if (CheckCollisionRecs(bFeet, belt)) { b.hitbox.x += driftX; b.hitbox.y += driftY; }
                }
                EntityRef convRef;
                convRef.type = (int)EditorTool::CONVEYOR;
                convRef.index = cvi;
                for (const auto& rel : liveRelations) {
                    if (rel.parent != convRef) continue;
                    int ci = rel.child.index;
                    switch (rel.child.type) {
                    case (int)EditorTool::NUKE_SPAWN:
                        if (ci >= 0 && ci < (int)nukeSpawnNodes.size())
                        {
                            nukeSpawnNodes[ci].x += driftX; nukeSpawnNodes[ci].y += driftY;
                        } break;
                    case (int)EditorTool::BEATRICE_SPAWN:
                        if (ci >= 0 && ci < (int)beatriceSpawnNodes.size())
                        {
                            beatriceSpawnNodes[ci].x += driftX; beatriceSpawnNodes[ci].y += driftY;
                        } break;
                    case (int)EditorTool::ENEMY_SPAWN:
                        if (ci >= 0 && ci < (int)enemySpawnPositions.size())
                        {
                            enemySpawnPositions[ci].x += driftX; enemySpawnPositions[ci].y += driftY;
                        } break;
                    case (int)EditorTool::BEAM:
                        if (ci >= 0 && ci < (int)beamPositions.size())
                        {
                            beamPositions[ci].x += driftX; beamPositions[ci].y += driftY;
                        } break;
                    default: break;
                    }
                }
            }

            // ── Win condition ─────────────────────────────────────────────────
            if (CheckCollisionRecs(wincondition, player))
            {
                // Priority-weighted random level selection
                // Increment unplayed levels, reset played level
                for (int i = levelRangeMin; i <= levelRangeMax && i <= MAX_LEVEL_ID; i++) {
                    if (i == currentLevelId) { levelPriority[i] = 0; continue; }
                    int p = levelPriority[i];
                    levelPriority[i] = p + (p >= 10 ? 5 : p >= 3 ? 2 : 1);
                }

                // Build weighted candidate list
                struct LvCandidate { int id; int weight; };
                vector<LvCandidate> candidates;
                int totalWeight = 0;
                for (int i = levelRangeMin; i <= levelRangeMax && i <= MAX_LEVEL_ID; i++) {
                    if (i == currentLevelId) continue;
                    LevelData probe;
                    if (!LoadLevel(probe, i)) continue;
                    int w = levelPriority[i] > 0 ? levelPriority[i] : 1;
                    candidates.push_back({ i, w });
                    totalWeight += w;
                }

                int nextId = -1;
                if (!candidates.empty() && totalWeight > 0) {
                    int r = GetRandomValue(0, totalWeight - 1);
                    for (const auto& c : candidates) { r -= c.weight; if (r < 0) { nextId = c.id; break; } }
                    if (nextId < 0) nextId = candidates.back().id;
                }

                // Award level-clear bonus before changing currentLevelId
                levelsCleared++;
                runScore += 300 * ((currentLevelId - 1) / 5 + 1);

                LevelData nextLv;
                if (nextId >= 0 && LoadLevel(nextLv, nextId)) {
                    currentLevelId = nextId;
                    pendingLevelData = nextLv;
                    hasPendingLevel = true;
                    InitCardSelect(2, false);
                    currentScreen = CARD_SELECT;
                }
                else {
                    TryUpdateHighscore();
                    splashTimer = 0.0f;
                    currentScreen = GAME_OVER;
                }
            }

            } // end !isPaused
        } // end GAMEPLAY update

        else if (currentScreen == GAME_OVER)
        {
            splashTimer += dt;
            if (splashTimer >= splashDuration || IsKeyPressed(KEY_ENTER))
            {
                splashTimer = 0.0f; FullReset(); currentScreen = MENU;
            }
        }
        else if (currentScreen == CARD_SELECT)
        {
            // ── Card sizes ────────────────────────────────────────────────────
            float cardW, cardH;
            bool  isShop = (cardSelectMode == CSM_SHOP);
            if (isShop) { cardW = 150.f; cardH = 225.f; }
            else        { cardW = 280.f; cardH = 420.f; }
            float gapX  = 20.f;
            float totalW = numDispCards * cardW + (numDispCards - 1) * gapX;
            float startX = isShop
                ? ((float)screenWidth - totalW) / 2.f + 40.f
                : ((float)screenWidth - totalW) / 2.f;
            float startY = 160.f;  // cards sit lower on screen

            // ── Hotbar slot layout (horizontal, bottom-center) ────────────────
            static const float HB_W = 64.f, HB_H = 96.f, HB_GAP = 8.f;
            float hbTotalW = 3 * HB_W + 2 * HB_GAP;
            float hbStartX = ((float)screenWidth - hbTotalW) * 0.5f;
            float hbSlotY  = (float)screenHeight - 20.f - HB_H;

            Vector2 mouse = GetMousePosition();
            static Vector2 prevMouse = {};
            Vector2 mouseDelta = { mouse.x - prevMouse.x, mouse.y - prevMouse.y };
            prevMouse = mouse;

            // ── Hotbar drag ───────────────────────────────────────────────────
            if (hbDrag.active) {
                if (!hbDrag.falling) {
                    hbDrag.x = mouse.x;
                    hbDrag.y = mouse.y;
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        bool placed = false;
                        for (int i = 0; i < 3; i++) {
                            Rectangle slotR = { hbStartX + i * (HB_W + HB_GAP), hbSlotY, HB_W, HB_H };
                            if (CheckCollisionPointRec(mouse, slotR)) {
                                // Swap: put drag item in slot i, put slot i contents back in srcSlot
                                HotbarSlot displaced = hotbar[i];
                                hotbar[i] = { hbDrag.type, hbDrag.cd, hbDrag.charges };
                                if (hbDrag.srcSlot >= 0 && hbDrag.srcSlot < 3)
                                    hotbar[hbDrag.srcSlot] = displaced;
                                hbDrag.active = false;
                                placed = true;
                                break;
                            }
                        }
                        if (!placed) {
                            hbDrag.falling = true;
                            hbDrag.velX = mouseDelta.x * 60.f;
                            hbDrag.velY = mouseDelta.y * 60.f + 60.f;
                        }
                    }
                } else {
                    hbDrag.velY += 600.f * dt;
                    hbDrag.x += hbDrag.velX * dt;
                    hbDrag.y += hbDrag.velY * dt;
                    if (hbDrag.y > (float)screenHeight + 150.f)
                        hbDrag.active = false;
                }
            } else if (!anyCardPicked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Check hotbar slot for drag start
                for (int i = 0; i < 3; i++) {
                    Rectangle slotR = { hbStartX + i * (HB_W + HB_GAP), hbSlotY, HB_W, HB_H };
                    if (CheckCollisionPointRec(mouse, slotR) && hotbar[i].type != PU_NONE) {
                        hbDrag = { true, hotbar[i].type, hotbar[i].charges, hotbar[i].cd,
                                   i, mouse.x, mouse.y, 0.f, 0.f, false };
                        hotbar[i] = { PU_NONE, 0.f, 0 };
                        break;
                    }
                }
            }

            // ── Update card animations ────────────────────────────────────────
            static const float APPEAR_DUR    = 1.0f;
            static const float EXIT_DUR      = 0.5f;
            bool anyExiting = false;

            for (int i = 0; i < numDispCards; i++) {
                CardDisplay& c = displayCards[i];
                if (!c.appeared) {
                    c.appearT += dt;
                    if (c.appearT > 0.f) {
                        float t = fminf(c.appearT / APPEAR_DUR, 1.f);
                        // ease-out with slight overshoot on height
                        if (t < 0.85f)
                            c.heightFrac = Lerp(0.f, 1.05f, t / 0.85f);
                        else
                            c.heightFrac = Lerp(1.05f, 1.0f, (t - 0.85f) / 0.15f);
                        if (c.appearT >= APPEAR_DUR) {
                            c.heightFrac = 1.0f;
                            c.appeared = true;
                            SetSoundVolume(cardSlideSnd, volUI * 1.2f); PlaySound(cardSlideSnd);
                        }
                    }
                    c.scale = 1.0f;
                }
                if (c.selected || c.dismissed) {
                    c.exitT += dt;
                    anyExiting = true;
                    if (c.selected) {
                        float t = fminf(c.exitT / EXIT_DUR, 1.f);
                        c.scale     = 1.f - t;
                        c.spinPhase = t;
                    } else {
                        c.scale = Lerp(1.f, 0.f, fminf(c.exitT / (EXIT_DUR * 0.7f), 1.f));
                    }
                }
                if (c.shakeTimer > 0.f) c.shakeTimer -= dt;
                if (c.appeared && !c.selected && !c.dismissed) {
                    float cx = startX + i * (cardW + gapX) + cardW * 0.5f;
                    float cy = startY + cardH * 0.5f;
                    Rectangle hitR = { cx - cardW * c.scale * 0.5f, cy - cardH * c.scale * 0.5f,
                                       cardW * c.scale, cardH * c.scale };
                    bool wasHovered = c.hovered;
                    c.hovered = !hbDrag.active && CheckCollisionPointRec(mouse, hitR);
                    if (c.hovered && !wasHovered) {
                        SetSoundVolume(cardHoverSnd, volUI * 1.2f); PlaySound(cardHoverSnd);
                    }
                    float hTarget = c.hovered ? 1.f : 0.f;
                    c.hoverLerp = Lerp(c.hoverLerp, hTarget, fminf(dt * 10.f, 1.f));
                    c.scale = 1.0f + 0.1f * c.hoverLerp;
                }
            }

            // ── Fade to black after pick ──────────────────────────────────────
            bool allDone = anyCardPicked;
            for (int i = 0; i < numDispCards && allDone; i++)
                allDone = allDone && (displayCards[i].exitT >= EXIT_DUR || (!displayCards[i].selected && !displayCards[i].dismissed));
            if (anyCardPicked) {
                cardFadeOut += dt / 0.8f;
                if (cardFadeOut >= 1.f) {
                    cardFadeOut = 1.f;
                    if (isShop) {
                        currentScreen = GAMEPLAY;
                    } else {
                        if (hasPendingLevel) { ApplyLevelData(pendingLevelData); hasPendingLevel = false; }
                        splashTimer = 0.f; subaruFrame = 0; subaruTimer = 0.f;
                        currentScreen = HOW_HIGH;
                    }
                }
            }

            // ── Shop exit button ──────────────────────────────────────────────
            if (isShop && !anyCardPicked) {
                Rectangle exitR = { (float)screenWidth - 130.f, 46.f, 110.f, 32.f };
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, exitR)) {
                    anyCardPicked = true;
                    for (int i = 0; i < numDispCards; i++) displayCards[i].dismissed = true;
                }
            }

            // ── Click to pick a card (only if not dragging) ───────────────────
            if (!anyCardPicked && !hbDrag.active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                for (int i = 0; i < numDispCards; i++) {
                    if (displayCards[i].appeared && displayCards[i].hovered) {
                        if (isShop) {
                            int cost = PU_INFO[(int)displayCards[i].type].cost;
                            if (coins < cost) {
                                // Can't afford — shake and play error sound
                                displayCards[i].shakeTimer = 0.45f;
                                SetSoundVolume(sndError, volUI * 1.5f); PlaySound(sndError);
                                break;
                            }
                            coins -= cost;
                            AddToHotbar(displayCards[i].type);
                        } else {
                            AddToHotbar(displayCards[i].type);
                        }
                        anyCardPicked = true;
                        displayCards[i].selected = true;
                        SetSoundVolume(cardPickSnd, volUI * 1.2f); PlaySound(cardPickSnd);
                        bool anyDismissed = false;
                        for (int j = 0; j < numDispCards; j++)
                            if (j != i) { displayCards[j].dismissed = true; anyDismissed = true; }
                        if (anyDismissed) { SetSoundVolume(cardThrowSnd, volUI * 1.2f); PlaySound(cardThrowSnd); }
                        break;
                    }
                }
            }
        }

        rainScrollY += rainSpeed * dt;
        rain2ScrollY += rain2Speed * dt;
        if (rainScrollY >= Rain.height)  rainScrollY = 0.0f;
        if (rain2ScrollY >= Rain2.height) rain2ScrollY = 0.0f;

        // ─────────────────────────────────────────────────────────────────────
        // DRAW
        // ─────────────────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        if (currentScreen == SPLASH_SCREEN)
        {
            DrawText("A DONKEY KONG GAME PROJECT", 15, 400, 50, WHITE);
            DrawText("Assignatura: Projecte 1 \nCurs: Disseny i Desenvolupament de videojocs 1 \nUniversitat: UPC", 50, 800, 20, GRAY);
        }
        else if (currentScreen == SPLASH_SCREEN2)
        {
            DrawText("Game by:\n\n Alejandro Perez\n Jonathan Bermello\n Marc Flores\n Biel Yubero", 300, 250, 30, WHITE);
            DrawText("Tutor Alejandro Paris", 50, 800, 30, GRAY);
        }
        else if (currentScreen == MENU)
        {
            int   titleFont = 60, menuFont = 28, smallFont = 20, spacing = 30;
            Color dkRed = { 255,  60,   0, 255 };
            Color dkOrange = { 255, 140,   0, 255 };
            Color dkWhite = WHITE;

            const char* title      = "DONKEY KONG";
            const char* playText   = "1 PLAYER GAME";
            const char* exitText   = "EXIT";
            const char* editorText = "LEVEL EDITOR";
            const char* optTxt     = "OPTIONS";
            const char* subtitle   = "1967 Bomboclat Industries LLC";

            int titleW  = MeasureText(title,      titleFont);
            int playW   = MeasureText(playText,   menuFont);
            int exitW   = MeasureText(exitText,   menuFont);
            int editorW = MeasureText(editorText, menuFont);
            int optW    = MeasureText(optTxt,     menuFont);
            int subW    = MeasureText(subtitle,   smallFont);

            // 4 items: play, exit, editor, options
            int startY  = (screenHeight - (titleFont + spacing * 5 + menuFont * 4 + smallFont)) / 2;
            int titleX  = (screenWidth - titleW)  / 2, titleY  = startY;
            int playX   = (screenWidth - playW)   / 2, playY   = titleY + titleFont + spacing;
            int exitX   = (screenWidth - exitW)   / 2, exitY   = playY  + menuFont  + spacing;
            int editorX = (screenWidth - editorW) / 2, editorY = exitY  + menuFont  + spacing;
            int optX    = (screenWidth - optW)    / 2, optY    = editorY + menuFont  + spacing;
            int subX    = (screenWidth - subW)    / 2, subY    = optY   + menuFont  + spacing;

            btnPlay     = { (float)(playX   - 10), (float)playY,   (float)(playW   + 20), (float)(menuFont + 6) };
            btnExit     = { (float)(exitX   - 10), (float)exitY,   (float)(exitW   + 20), (float)(menuFont + 6) };
            btnEditor   = { (float)(editorX - 10), (float)editorY, (float)(editorW + 20), (float)(menuFont + 6) };
            btnSettings = { (float)(optX    - 10), (float)optY,    (float)(optW    + 20), (float)(menuFont + 6) };

            if (selectedOption == 0) DrawText(">", playX   - 40, playY,   menuFont, dkOrange);
            if (selectedOption == 1) DrawText(">", exitX   - 40, exitY,   menuFont, dkOrange);
            if (selectedOption == 2) DrawText(">", editorX - 40, editorY, menuFont, dkOrange);
            if (selectedOption == 3) DrawText(">", optX    - 40, optY,    menuFont, dkOrange);

            DrawText(title,      titleX,  titleY,  titleFont, dkRed);
            DrawText(playText,   playX,   playY,   menuFont,  dkWhite);
            DrawText(exitText,   exitX,   exitY,   menuFont,  dkWhite);
            DrawText(editorText, editorX, editorY, menuFont,  dkOrange);
            DrawText(optTxt,     optX,    optY,    menuFont,  { 120, 220, 120, 255 });
            DrawText(subtitle,   subX,    subY,    smallFont, dkOrange);
        }
        else if (currentScreen == HOW_HIGH)
        {
            // 1.2× scaled background — start with extra 20% above screen, pan down over 2 s
            float imgW  = screenWidth  * 1.2f;
            float imgH  = screenHeight * 1.2f;
            float extra = imgH - screenHeight;          // 190 px
            float imgX  = (screenWidth - imgW) * 0.5f; // -87.5 — centered, sides clipped
            float t     = Clamp(splashTimer / 2.0f, 0.0f, 1.0f);
            float imgY  = -extra + extra * t;           // -190 → 0 as timer runs

            Color bgTint = { 128, 128, 128, 255 };      // 50% brightness
            DrawTexturePro(Subaru_Background,
                { 0, 0, (float)Subaru_Background.width, (float)Subaru_Background.height },
                { imgX, imgY, imgW, imgH }, { 0, 0 }, 0.f, bgTint);

            Texture2D* subTex = subaruFrames[subaruFrame];
            float subScale = (screenHeight * 0.92f * 1.05f) / subTex->height;
            float subW = subTex->width  * subScale;
            float subH = subTex->height * subScale;
            float subX = (screenWidth - subW) * 0.5f;
            float subY = screenHeight - subH + 15.f;   // anchored near bottom
            DrawTexturePro(*subTex,
                { 0, 0, (float)subTex->width, (float)subTex->height },
                { subX, subY, subW, subH }, { 0, 0 }, 0.f, WHITE);
        }
        else if (currentScreen == CONTROLS)
        {
            DrawText("- Move with", 10, 250, 30, WHITE);  DrawText("W, A, S, D", 195, 250, 30, ORANGE);
            DrawText("- Grab Items with", 10, 300, 30, WHITE);  DrawText("E", 285, 300, 30, ORANGE);
            DrawText("- Use items with", 10, 350, 30, WHITE);  DrawText("F", 260, 350, 30, ORANGE);
            DrawText("- Climb stairs with", 10, 400, 30, WHITE);  DrawText("W", 295, 400, 30, ORANGE);
            DrawText("while you are close to them", 330, 400, 30, WHITE);
            DrawText("Return", 750, 900, 30, WHITE);
        }
        else if (currentScreen == SETTINGS)
        {
            Vector2 mouse = GetMousePosition();

            // ── Panel ─────────────────────────────────────────────────────────
            const float panW = 700.f, panH = 580.f;
            const float panX = (screenWidth  - panW) * 0.5f;
            const float panY = (screenHeight - panH) * 0.5f;
            DrawRectangle(0, 0, screenWidth, screenHeight, {10, 10, 20, 200});
            DrawRectangleRounded({panX, panY, panW, panH}, 0.04f, 8, {18, 18, 30, 245});
            DrawRectangleRoundedLinesEx({panX, panY, panW, panH}, 0.04f, 8, 2.f, {100, 160, 255, 200});

            // Title
            const char* titTxt = "OPTIONS";
            DrawText(titTxt, (int)(panX + (panW - MeasureText(titTxt, 38)) * 0.5f), (int)(panY + 20), 38, {120, 220, 120, 255});

            // ── Tab bar ───────────────────────────────────────────────────────
            const char* tabNames[3] = { "CONTROLS", "SETTINGS", "DEBUG" };
            const float tabY = panY + 70.f, tabH = 34.f;
            const float tabW = panW / 3.f;
            for (int t = 0; t < 3; t++) {
                float tx = panX + t * tabW;
                bool active = (settingsTab == t);
                Color tabBg  = active ? Color{50, 100, 200, 220} : Color{30, 30, 55, 200};
                Color tabFg  = active ? WHITE : Color{160, 160, 200, 255};
                DrawRectangleRec({tx, tabY, tabW, tabH}, tabBg);
                DrawRectangleLinesEx({tx, tabY, tabW, tabH}, 1.f, {80, 80, 140, 200});
                int tw = MeasureText(tabNames[t], 18);
                DrawText(tabNames[t], (int)(tx + (tabW - tw) * 0.5f), (int)(tabY + (tabH - 18) * 0.5f), 18, tabFg);
                if (!active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {tx, tabY, tabW, tabH}))
                    settingsTab = t;
            }

            // ── Content area ──────────────────────────────────────────────────
            const float cX = panX + 30.f, cY = tabY + tabH + 20.f;
            const float cW = panW - 60.f;

            // ── TAB 0: CONTROLS ───────────────────────────────────────────────
            if (settingsTab == 0) {
                struct CtrlRow { const char* action; const char* keys; };
                static const CtrlRow rows[] = {
                    { "Move Left / Right",     "A  /  D" },
                    { "Jump",                  "SPACE" },
                    { "Use Ability",           "E  or  Left Click" },
                    { "Pick Up Item",          "F" },
                    { "Enter Ladder",          "W or S  (near ladder)" },
                    { "Climb Up / Down",       "W  /  S  (on ladder)" },
                    { "Exit Ladder",           "A, D, SPACE or W" },
                    { "Select Hotbar Slot",    "1  /  2  /  3" },
                    { "Scroll Hotbar",         "Mouse Wheel" },
                    { "Return to Menu",        "N" },
                    { "Toggle Debug Panel",    "M  (in-game)" },
                };
                const int nRows = (int)(sizeof(rows)/sizeof(rows[0]));
                const int rFS = 20, rGap = 30;
                for (int i = 0; i < nRows; i++) {
                    float ry = cY + i * rGap;
                    DrawText(rows[i].action, (int)cX,         (int)ry, rFS, {200, 200, 220, 255});
                    DrawText(rows[i].keys,   (int)(cX + cW - MeasureText(rows[i].keys, rFS)), (int)ry, rFS, YELLOW);
                    DrawLine((int)cX, (int)(ry + rFS + 3), (int)(cX + cW), (int)(ry + rFS + 3), {60, 60, 90, 180});
                }
            }

            // ── TAB 1: SETTINGS (volume sliders) ──────────────────────────────
            if (settingsTab == 1) {
                // SFX and Ability go 0–4 (0–400%), Music 0–1 (0–100%)
                struct SliderDef { const char* label; float* vol; float maxVal; };
                SliderDef sliders[] = {
                    { "Music",        &volMusic,   1.f },
                    { "Abilities",    &volAbility, 4.f },
                    { "Gameplay SFX", &volSFX,     4.f },
                };
                const float sX = cX + 160.f, sW = 350.f, sH = 14.f;
                const int   lFS = 22;
                const float rowGap = 85.f;
                for (int i = 0; i < 3; i++) {
                    float sy = cY + 30.f + i * rowGap;
                    float& vol = *sliders[i].vol;
                    float  mx  = sliders[i].maxVal;
                    float  frac = vol / mx;  // 0–1 for rendering

                    DrawText(sliders[i].label, (int)cX, (int)(sy - lFS * 0.5f + sH * 0.5f), lFS, RAYWHITE);

                    // Track
                    DrawRectangleRec({sX, sy, sW, sH}, {50, 50, 75, 200});
                    // Fill
                    Color fc = {90, 200, 100, 255};
                    if (frac < 0.25f) fc = {220, 80, 80, 255};
                    else if (frac < 0.5f) fc = {220, 180, 60, 255};
                    DrawRectangleRec({sX, sy, sW * frac, sH}, fc);
                    DrawRectangleLinesEx({sX, sy, sW, sH}, 1.f, {160, 160, 200, 255});
                    // Handle
                    float hx = sX + sW * frac;
                    DrawCircle((int)hx, (int)(sy + sH * 0.5f), 10.f, WHITE);
                    DrawCircle((int)hx, (int)(sy + sH * 0.5f), 7.f, fc);
                    // Value pct
                    DrawText(TextFormat("%d%%", (int)(vol * 100.f + 0.5f)),
                        (int)(sX + sW + 16), (int)(sy - 1), lFS, RAYWHITE);
                    // Input
                    Rectangle hitR = {sX - 12.f, sy - 10.f, sW + 24.f, sH + 20.f};
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, hitR))
                        vol = Clamp((mouse.x - sX) / sW, 0.f, 1.f) * mx;
                }
                SetMusicVolume(music, volMusic);
            }

            // ── TAB 2: DEBUG ──────────────────────────────────────────────────
            if (settingsTab == 2) {
                const float ROW_H     = 33.f;
                const float clipH     = panY + panH - 52.f - cY;
                const float sbW       = 8.f;   // scrollbar width
                const float innerW    = cW - sbW - 4.f;
                const int   lFS       = 17;

                // Scroll with mouse wheel
                if (CheckCollisionPointRec(mouse, {cX, cY, cW, clipH}))
                    settingsDbgScroll -= GetMouseWheelMove() * ROW_H;
                const int TOTAL_DBG_ROWS = 18;
                float totalH = TOTAL_DBG_ROWS * ROW_H;
                float maxDbgScroll = fmaxf(0.f, totalH - clipH);
                settingsDbgScroll = Clamp(settingsDbgScroll, 0.f, maxDbgScroll);

                BeginScissorMode((int)cX, (int)cY, (int)(innerW + 2.f), (int)clipH);
                float ry = cY - settingsDbgScroll;

                // Toggle button (full row)
                auto togBtn = [&](const char* lbl, bool state, Color onCol, Color offCol) -> bool {
                    Color bc = state ? onCol : offCol;
                    Rectangle r = {cX, ry, innerW, ROW_H - 3.f};
                    bool hov = CheckCollisionPointRec(mouse, r);
                    if (hov) { bc.r=(unsigned char)fminf(bc.r+40,255); bc.g=(unsigned char)fminf(bc.g+40,255); bc.b=(unsigned char)fminf(bc.b+40,255); }
                    DrawRectangleRec(r, bc); DrawRectangleLinesEx(r,1.f,{120,120,180,200});
                    int tw=MeasureText(lbl,lFS); DrawText(lbl,(int)(r.x+(r.width-tw)/2),(int)(r.y+(r.height-lFS)/2),lFS,WHITE);
                    ry += ROW_H;
                    return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                };

                // Int stepper (label  < value >)
                auto stepper = [&](const char* lbl, int& val, int minV, int maxV) {
                    DrawText(lbl,(int)(cX+4),(int)(ry+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                    float bw=30.f, bh=ROW_H-5.f;
                    Rectangle mR={cX+innerW-bw*2-50.f,ry+2.f,bw,bh};
                    Rectangle pR={cX+innerW-bw-2.f,   ry+2.f,bw,bh};
                    bool mH=CheckCollisionPointRec(mouse,mR), pH=CheckCollisionPointRec(mouse,pR);
                    DrawRectangleRec(mR,mH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(mR,1.f,{100,100,160,200});
                    DrawRectangleRec(pR,pH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(pR,1.f,{100,100,160,200});
                    DrawText("<",(int)(mR.x+(mR.width-MeasureText("<",lFS))/2),(int)(mR.y+(bh-lFS)/2),lFS,WHITE);
                    DrawText(">",(int)(pR.x+(pR.width-MeasureText(">",lFS))/2),(int)(pR.y+(bh-lFS)/2),lFS,WHITE);
                    const char* vs=TextFormat("%d",val);
                    DrawText(vs,(int)(mR.x-MeasureText(vs,lFS)-6),(int)(ry+(ROW_H-lFS)/2),lFS,YELLOW);
                    if (mH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&val>minV) val--;
                    if (pH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&val<maxV) val++;
                    ry+=ROW_H;
                };

                // Float stepper (label  - value +)
                auto fstepper = [&](const char* lbl, float& val, float minV, float maxV, float step, const char* fmt) {
                    DrawText(lbl,(int)(cX+4),(int)(ry+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                    float bw=30.f, bh=ROW_H-5.f;
                    Rectangle mR={cX+innerW-bw*2-50.f,ry+2.f,bw,bh};
                    Rectangle pR={cX+innerW-bw-2.f,   ry+2.f,bw,bh};
                    bool mH=CheckCollisionPointRec(mouse,mR), pH=CheckCollisionPointRec(mouse,pR);
                    DrawRectangleRec(mR,mH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(mR,1.f,{100,100,160,200});
                    DrawRectangleRec(pR,pH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(pR,1.f,{100,100,160,200});
                    DrawText("-",(int)(mR.x+(mR.width-MeasureText("-",lFS))/2),(int)(mR.y+(bh-lFS)/2),lFS,WHITE);
                    DrawText("+",(int)(pR.x+(pR.width-MeasureText("+",lFS))/2),(int)(pR.y+(bh-lFS)/2),lFS,WHITE);
                    const char* vs=TextFormat(fmt,val);
                    DrawText(vs,(int)(mR.x-MeasureText(vs,lFS)-6),(int)(ry+(ROW_H-lFS)/2),lFS,YELLOW);
                    if (mH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val=fmaxf(minV,val-step);
                    if (pH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val=fminf(maxV,val+step);
                    ry+=ROW_H;
                };

                // Action button — greyed out with note if gameplay-only and not in GAMEPLAY
                auto actionBtn = [&](const char* lbl, Color col, bool gpOnly) -> bool {
                    bool enabled = !gpOnly || (currentScreen == GAMEPLAY);
                    Color bc = enabled ? col : Color{45,45,55,180};
                    Rectangle r = {cX, ry, innerW, ROW_H-3.f};
                    bool hov = enabled && CheckCollisionPointRec(mouse,r);
                    if (hov) { bc.r=(unsigned char)fminf(bc.r+40,255); bc.g=(unsigned char)fminf(bc.g+40,255); bc.b=(unsigned char)fminf(bc.b+40,255); }
                    DrawRectangleRec(r,bc); DrawRectangleLinesEx(r,1.f,{100,100,160,200});
                    const char* txt = (!enabled) ? TextFormat("%s  (in-game only)",lbl) : lbl;
                    int tw=MeasureText(txt,lFS); DrawText(txt,(int)(r.x+(r.width-tw)/2),(int)(r.y+(r.height-lFS)/2),lFS,enabled?WHITE:Color{130,130,140,255});
                    ry+=ROW_H;
                    return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                };

                // ── Rows ──────────────────────────────────────────────────
                if (togBtn(TextFormat("Debug Overlay (M key):  %s", dbgMenuOpen?"ON":"OFF"),
                           dbgMenuOpen,{30,120,30,220},{80,30,30,220}))
                    dbgMenuOpen = !dbgMenuOpen;

                stepper("Lives",     lives,    0, maxLives);
                stepper("Max Lives", maxLives, 1, 10);
                if (maxLives < lives) lives = maxLives;

                if (togBtn(TextFormat("Immortality:  %s", dbgImmortal?"ON":"OFF"),
                           dbgImmortal,{0,115,0,220},{75,0,0,220}))
                    dbgImmortal = !dbgImmortal;

                if (togBtn(TextFormat("Flight:  %s", dbgFlight?"ON":"OFF"),
                           dbgFlight,{0,70,160,220},{30,30,80,220}))
                { dbgFlight=!dbgFlight; if(dbgFlight) dbgFlightNoCol=false; }

                if (togBtn(TextFormat("No Clip:  %s", dbgFlightNoCol?"ON":"OFF"),
                           dbgFlightNoCol,{0,105,145,220},{30,30,80,220}))
                { dbgFlightNoCol=!dbgFlightNoCol; if(dbgFlightNoCol) dbgFlight=false; }

                if (togBtn(TextFormat("Bloom:  %s", dbgBloomEnabled?"ON":"OFF"),
                           dbgBloomEnabled,{80,0,160,220},{30,30,80,220}))
                    dbgBloomEnabled = !dbgBloomEnabled;

                fstepper("Bloom Intensity", dbgBloomIntensity, 0.f, 3.f,  0.1f,  "%.1f");
                fstepper("Bloom Threshold", dbgBloomThreshold, 0.f, 1.f,  0.05f, "%.2f");

                // Give Ability row
                {
                    DrawText("Give Ability:",(int)(cX+4),(int)(ry+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                    float bw=30.f, bh=ROW_H-5.f, giveW=170.f;
                    Rectangle prevR={cX+innerW-bw*2-giveW-6.f, ry+2.f, bw,    bh};
                    Rectangle giveR={cX+innerW-bw-giveW-4.f,   ry+2.f, giveW, bh};
                    Rectangle nextR={cX+innerW-bw-2.f,          ry+2.f, bw,    bh};
                    bool gpEnabled = (currentScreen == GAMEPLAY);
                    bool pH=CheckCollisionPointRec(mouse,prevR), nH=CheckCollisionPointRec(mouse,nextR);
                    bool gH=CheckCollisionPointRec(mouse,giveR)&&gpEnabled;
                    DrawRectangleRec(prevR,pH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(prevR,1.f,{100,100,160,200});
                    DrawRectangleRec(nextR,nH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(nextR,1.f,{100,100,160,200});
                    DrawRectangleRec(giveR,gH?Color{100,60,130,255}:gpEnabled?Color{60,40,85,255}:Color{45,45,55,180}); DrawRectangleLinesEx(giveR,1.f,{100,100,160,200});
                    DrawText("<",(int)(prevR.x+(prevR.width-MeasureText("<",lFS))/2),(int)(prevR.y+(bh-lFS)/2),lFS,WHITE);
                    DrawText(">",(int)(nextR.x+(nextR.width-MeasureText(">",lFS))/2),(int)(nextR.y+(bh-lFS)/2),lFS,WHITE);
                    const char* puName=gpEnabled?PU_INFO[dbgGivePUIdx].name:TextFormat("%s (in-game)",PU_INFO[dbgGivePUIdx].name);
                    int gnw=MeasureText(puName,lFS-2); DrawText(puName,(int)(giveR.x+(giveR.width-gnw)/2),(int)(giveR.y+(bh-(lFS-2))/2),lFS-2,gpEnabled?YELLOW:Color{130,130,140,255});
                    if (pH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbgGivePUIdx=(dbgGivePUIdx+PU_COUNT-1)%PU_COUNT;
                    if (nH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbgGivePUIdx=(dbgGivePUIdx+1)%PU_COUNT;
                    if (gH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) AddToHotbar((PowerupType)dbgGivePUIdx);
                    ry+=ROW_H;
                }

                if (actionBtn("Spawn Barrel",    {50,80,50,220}, true))  SpawnBarrelFromPool(barrels, barrelPath);
                if (actionBtn("Spawn Enemy",     {80,50,50,220}, true)) {
                    for (auto& en:enemies) if(!en.active){ en.active=true; en.hitbox={player.x+(facingRight?80.f:-80.f),player.y-50.f,44.f,44.f}; en.velocity={0,0}; en.type=GRUNT; en.state=ES_IDLE; en.stateTimer=0; en.grounded=false; en.facingRight=facingRight; break; }
                }
                if (actionBtn("Summon Nuke",     {80,70,25,220}, true))  nukes.push_back({{player.x,player.y-60.f},true});
                if (actionBtn("Summon Beatrice", {50,50,100,220},true))  beatrices.push_back({{player.x,player.y-60.f},true});

                fstepper("Music Vol",     volMusic,   0.f, 1.f, 0.05f, "%.2f");
                fstepper("SFX Vol",       volSFX,     0.f, 4.f, 0.05f, "%.2f");
                fstepper("Abilities Vol", volAbility, 0.f, 4.f, 0.1f,  "%.1f");
                fstepper("UI Vol",        volUI,      0.f, 1.f, 0.05f, "%.2f");
                SetMusicVolume(music, volMusic);

                EndScissorMode();

                // Scrollbar
                if (maxDbgScroll > 0.f) {
                    float ratio  = clipH / totalH;
                    float thumbH = fmaxf(clipH * ratio, 20.f);
                    float thumbY = cY + (settingsDbgScroll / maxDbgScroll) * (clipH - thumbH);
                    DrawRectangle((int)(cX+cW-sbW), (int)cY, (int)sbW, (int)clipH, {30,30,50,180});
                    DrawRectangle((int)(cX+cW-sbW), (int)thumbY, (int)sbW, (int)thumbH, {110,110,195,255});
                }
            }

            // ── Back button ───────────────────────────────────────────────────
            const char* backTxt = "[ ESC ]  Back";
            int backW2 = MeasureText(backTxt, 20);
            float backX2 = panX + (panW - backW2) * 0.5f, backY2 = panY + panH - 42.f;
            DrawText(backTxt, (int)backX2, (int)backY2, 20, {160, 160, 210, 255});
            Rectangle backR2 = {backX2 - 10.f, backY2 - 4.f, (float)(backW2 + 20), 28.f};
            if (CheckCollisionPointRec(mouse, backR2)) {
                DrawRectangleLinesEx(backR2, 1.f, {160, 160, 255, 180});
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    SaveGameSettings(levelRangeMin, levelRangeMax, volMusic, volSFX, volAbility, volUI, highScore, highLevels);
                    currentScreen = MENU;
                }
            }
        }
        else if (currentScreen == LEVEL_EDITOR)
        {
            editor.Draw();
        }
        else if (currentScreen == GAMEPLAY)
        {
            Camera2D cam = { 0 };
            cam.zoom = 1.0f;
            cam.offset = { nukeShakeOffset.x + deathShakeOffset.x + dashShakeOffset.x,
                           nukeShakeOffset.y + deathShakeOffset.y + dashShakeOffset.y };

            gameLighting.BeginScene(cam);

            // 1. Background
            DrawTexturePro(background, { 0,0,438,475 }, { 0,0,875,950 }, {}, 0.f, WHITE);

            // 2. Rain (back layer)
            {
                float scaleX = (float)screenWidth / Rain2.width;
                float scaleY = (float)screenHeight / Rain2.height;
                float sW = Rain2.width * scaleX, sH = Rain2.height * scaleY;
                DrawTexturePro(Rain2, { 0, rain2ScrollY, (float)Rain2.width, (float)Rain2.height }, { 0, 0, sW, sH }, {}, 0.f, rain2Tint);
                DrawTexturePro(Rain2, { 0, 0, (float)Rain2.width, (float)Rain2.height }, { 0, -sH + rain2ScrollY, sW, sH }, {}, 0.f, rain2Tint);
            }

            // 3. Ladders (baked)
            DrawTextureRec(ladderLayer.texture, { 0, 0, (float)screenWidth, -(float)screenHeight }, { 0, 0 }, WHITE);

            // 3.1 Elevator-child ladders (live)
            {
                const float lScale = 4.f, tileW = 16.f * lScale, tileH = 16.f * lScale;
                for (int ri = 0; ri < (int)liveRelations.size(); ri++) {
                    const auto& rel = liveRelations[ri];
                    if (rel.parent.type != 11 || rel.child.type != 5) continue;
                    int li = rel.child.index;
                    if (li < 0 || li >= (int)ladders.size()) continue;
                    const Ladder& lad = ladders[li];
                    float drawX = lad.x + lad.width * 0.5f - tileW * 0.5f;
                    for (float y = lad.y; y < lad.y + lad.height; y += tileH) {
                        float dh = fminf(tileH, lad.y + lad.height - y);
                        DrawTexturePro(LadderPart, { 0, 0, 16.f, dh / lScale },
                            { drawX, y, tileW, dh }, { 0, 0 }, 0.f, WHITE);
                    }
                }
            }

            // 3.2 Elevator rope shafts
            {
                const float sc = 4.f;
                for (const auto& el : liveElevators) {
                    if (el.invisible) continue;
                    if (RopeTex.id == 0) { DrawRectangle((int)el.x, (int)el.y, (int)el.w, (int)el.h, { 80,60,40,80 }); continue; }
                    float tw = RopeTex.width * sc, th = RopeTex.height * sc;
                    float drawX = el.x + el.w * 0.5f - tw * 0.5f;
                    float rawPan = (float)GetTime() * el.speed * (el.direction == 1 ? 1.f : -1.f);
                    float panOff = fmodf(rawPan, th); if (panOff < 0.f) panOff += th;
                    float startY = el.y - th + panOff;
                    for (float y = startY; y < el.y + el.h; y += th) {
                        float dy = fmaxf(y, el.y), dyEnd = fminf(y + th, el.y + el.h);
                        if (dy >= dyEnd) continue;
                        float srcYOff = (dy - y) / sc, srcH = (dyEnd - dy) / sc;
                        DrawTexturePro(RopeTex, { 0, srcYOff, (float)RopeTex.width, srcH },
                            { drawX, dy, tw, dyEnd - dy }, {}, 0.f, WHITE);
                    }
                }
            }

            // 4. Beams (baked)
            DrawTextureRec(staticLayer.texture, { 0, 0, (float)screenWidth, -(float)screenHeight }, { 0, 0 }, WHITE);

            // 4.1 Elevator-child beams (live)
            {
                const float bScale = 4.f;
                for (int ri = 0; ri < (int)liveRelations.size(); ri++) {
                    const auto& rel = liveRelations[ri];
                    if (rel.parent.type != 11 || rel.child.type != 6) continue;
                    int bi = rel.child.index;
                    if (bi < 0 || bi >= (int)beamPositions.size()) continue;
                    const auto& b = beamPositions[bi];
                    Texture2D* bTex = (b.texVariant >= 1 && b.texVariant <= 12 && beamVariants[b.texVariant - 1].id > 0)
                        ? &beamVariants[b.texVariant - 1] : &beam;
                    float ecSrcX = b.flipX ? (float)bTex->width : 0.f;
                    float ecSrcW = b.flipX ? -(float)bTex->width : (float)bTex->width;
                    DrawTexturePro(*bTex, { ecSrcX, 0, ecSrcW, (float)bTex->height },
                        { b.x, b.y, (float)bTex->width * bScale, (float)bTex->height * bScale }, { 0,0 }, 0.f, WHITE);
                }
            }

            // 5. Kill zones
            for (const auto& kz : liveKillZones) {
                if (kz.texId == KillZoneTexture::DK_GOLDEN_PISTON && GoldenPistonTex.id > 0) {
                    float tw = (float)GoldenPistonTex.width, th = (float)GoldenPistonTex.height;
                    float scale = fmaxf(1.f, fminf(kz.w / tw, kz.h / th));
                    float dw = tw * scale, dh = th * scale;
                    for (float ty = kz.y; ty < kz.y + kz.h; ty += dh)
                        for (float tx = kz.x; tx < kz.x + kz.w; tx += dw) {
                            float cw = fminf(dw, kz.x + kz.w - tx);
                            float ch = fminf(dh, kz.y + kz.h - ty);
                            DrawTexturePro(GoldenPistonTex, { 0, 0, cw / scale, ch / scale },
                                { tx, ty, cw, ch }, {}, 0.f, WHITE);
                        }
                }
                else {
                    DrawRectangle((int)kz.x, (int)kz.y, (int)kz.w, (int)kz.h, { 255,30,30,60 });
                    DrawRectangleLinesEx({ kz.x,kz.y,kz.w,kz.h }, 2.f, { 255,60,60,200 });
                }
            }

            // 5.1 High-layer beams (above kill zones)
            {
                const float bScale = 4.f;
                for (int bi = 0; bi < (int)beamPositions.size(); bi++) {
                    const auto& b = beamPositions[bi];
                    if (b.renderLayer <= 0) continue;
                    bool isElevChild = false;
                    for (const auto& rel : liveRelations)
                        if (rel.parent.type == 11 && rel.child.type == 6 && rel.child.index == bi)
                        {
                            isElevChild = true; break;
                        }
                    if (isElevChild) continue;
                    Texture2D* bTex = (b.texVariant >= 1 && b.texVariant <= 12 && beamVariants[b.texVariant - 1].id > 0)
                        ? &beamVariants[b.texVariant - 1] : &beam;
                    float hlSrcX = b.flipX ? (float)bTex->width : 0.f;
                    float hlSrcW = b.flipX ? -(float)bTex->width : (float)bTex->width;
                    DrawTexturePro(*bTex, { hlSrcX, 0, hlSrcW, (float)bTex->height },
                        { b.x, b.y, (float)bTex->width * bScale, (float)bTex->height * bScale }, { 0,0 }, 0.f, WHITE);
                }
            }

            // 5.2 Conveyors
            {
                int rawFrame = (int)(GetTime() * 9.0) % 3;
                for (const auto& cv : liveConveyors) {
                    int frameL = (cv.direction == 1) ? rawFrame : (2 - rawFrame);
                    int frameR = 2 - frameL;
                    Texture2D& sideL = ConvSide[frameL], & sideR = ConvSide[frameR], & mid = ConvM[frameL];
                    float rad = cv.rotation * DEG2RAD, ca = cosf(rad), sa = sinf(rad);
                    float ecW = cv.endCapW, bH = cv.beltH, midW = fmaxf(0.f, cv.length - 2.f * ecW);
                    auto DrawSec = [&](Texture2D& tex, float lx, float w, bool flipH) {
                        if (tex.id == 0) return;
                        float srcX = flipH ? (float)tex.width : 0.f, srcW = flipH ? -(float)tex.width : (float)tex.width;
                        DrawTexturePro(tex, { srcX,0,srcW,(float)tex.height },
                            { cv.x + lx * ca, cv.y + lx * sa, w, bH }, {}, cv.rotation, WHITE);
                        };
                    DrawSec(sideL, 0.f, ecW, false);
                    if (mid.id > 0 && midW > 0.f) {
                        float tileDisp = ecW;
                        for (float lx = ecW; lx < ecW + midW; lx += tileDisp) {
                            float drawW = fminf(tileDisp, ecW + midW - lx);
                            float srcCropW = (float)mid.width * (drawW / tileDisp);
                            DrawTexturePro(mid, { 0, 0, srcCropW, (float)mid.height },
                                { cv.x + lx * ca, cv.y + lx * sa, drawW, bH }, {}, cv.rotation, WHITE);
                        }
                    }
                    DrawSec(sideR, cv.length - ecW, ecW, true);
                }
            }

            // 5.3 Props (layer 0, lit)
            for (const auto& pr : currentLevelData.props) {
                if (pr.renderLayer != 0 || pr.lightAffect <= 0.f) continue;
                Texture2D* tex = (pr.texVariant == PROP_FIRE_VARIANT)
                    ? (propFireFrame == 0 ? &propTextures[PROP_FIRE_VARIANT] : &propFireFrame2)
                    : (pr.texVariant >= 0 && pr.texVariant < PROP_TEX_COUNT && propTextures[pr.texVariant].id > 0)
                        ? &propTextures[pr.texVariant] : nullptr;
                if (!tex) DrawRectanglePro({ pr.x - pr.width * .5f, pr.y - pr.height * .5f, pr.width, pr.height }, {}, pr.rotation, { 180,100,220,140 });
                else DrawTexturePro(*tex, { 0,0,(float)tex->width,(float)tex->height },
                    { pr.x, pr.y, pr.width, pr.height }, { pr.width * .5f, pr.height * .5f }, pr.rotation, WHITE);
            }

            // 6. House (only render when cave is visible)
            if (!currentLevelData.hasCave || currentLevelData.caveVisible)
            {
                Texture2D& houseTex = houseIsSnowed ? House2 : House1;
                DrawTexturePro(houseTex, { 0, 0, HOUSE_NATIVE_W, HOUSE_NATIVE_H }, { houseX, houseY, houseW, houseH }, {}, 0.f, WHITE);
                if (houseIsSnowed && currentLevelId == 1) {
                    // Snow floor only on level 1
                    float sfW = FLOOR_NATIVE_W * FLOOR_DRAW_SCALE, sfH = FLOOR_NATIVE_H * FLOOR_DRAW_SCALE;
                    DrawTexturePro(SnowFloor, { 0, 0, FLOOR_NATIVE_W, FLOOR_NATIVE_H }, { houseX, 865.0f, sfW, sfH }, {}, 0.f, WHITE);
                }
                if (houseAnimPlaying && houseAnimFrame < CAVE_FRAME_COUNT) {
                    Texture2D* caveTex = caveFrames[houseAnimFrame];
                    float caveW = CAVE_NATIVE_W * CAVE_DRAW_SCALE, caveH = CAVE_NATIVE_H * CAVE_DRAW_SCALE;
                    float caveX = houseX + houseW - caveW * 0.65f;
                    float animProgress = houseAnimFrame / (float)(CAVE_FRAME_COUNT - 1);
                    float caveY = houseY + houseH * 0.5f - caveH * 0.7f + 10.0f * animProgress;
                    DrawTexturePro(*caveTex, { 0, 0, CAVE_NATIVE_W, CAVE_NATIVE_H }, { caveX, caveY, caveW, caveH }, {}, 0.f, WHITE);
                }
            }

            // 7. Nuke items
            {
                float nkW = NUKE_NATIVE_W * NUKE_SCALE, nkH = NUKE_NATIVE_H * NUKE_SCALE;
                for (const auto& nk : nukes) {
                    if (!nk.active) continue;
                    float bobY = nk.pos.y + sinf((float)GetTime() * 3.0f) * 4.0f;
                    DrawTexturePro(Nuke, { 0,0,NUKE_NATIVE_W,NUKE_NATIVE_H }, { nk.pos.x, bobY, nkW, nkH }, {}, 0.f, WHITE);
                }
            }
            for (const auto& fn : flyingNukes) {
                if (!fn.active) continue;
                DrawTexturePro(Nuke, { 0,0,NUKE_NATIVE_W,NUKE_NATIVE_H },
                    { fn.rect.x, fn.rect.y, fn.rect.width, fn.rect.height }, {}, 0.f, WHITE);
            }

            // 8. Beatrice items
            {
                float      bcScale = 2.0f;
                Texture2D* bcTex = (beatriceItemAnimFrame == 0) ? &Beatrice_Idle1 : &Beatrice_Idle2;
                float bcW = bcTex->width * bcScale, bcH = bcTex->height * bcScale;
                for (const auto& bc : beatrices) {
                    if (!bc.active) continue;
                    float bobY = bc.pos.y - bcH + sinf((float)GetTime() * 2.5f) * 4.0f;
                    DrawTexturePro(*bcTex, { 0,0,(float)bcTex->width,(float)bcTex->height },
                        { bc.pos.x, bobY, bcW, bcH }, {}, 0.f, WHITE);
                }
            }

            // 9. Regulus
            {
                Texture2D* regTex = nullptr;
                if (regulusIsStunned) {
                    regTex = regulusStunEnding
                        ? regulusStunEndFrames[regulusStunEndFrame < 5 ? regulusStunEndFrame : 4]
                        : regulusStunFrames[regulusStunFrame < 3 ? regulusStunFrame : 0];
                }
                else {
                    int throwIdx = Clamp(regulusThrowFrame, 0, 2);
                    regTex = regulusThrowing ? regulusThrowFrames[throwIdx] : regulusIdleFrames[regulusIdleFrame];
                }

                float regW = regTex->width * REGULUS_SCALE;
                float regH = regTex->height * REGULUS_SCALE;
                float baseRegX = currentLevelData.hasRegulus ? currentLevelData.regulusPos.x : REGULUS_X;
                float baseRegY = currentLevelData.hasRegulus ? currentLevelData.regulusPos.y : 225.0f;
                float regY = baseRegY - regH + 20.0f;
                float regX = baseRegX + regW * 0.5f;

                if (!regulusIsStunned && regulusThrowing)
                {
                    int throwIdx = Clamp(regulusThrowFrame, 0, 2);
                    const float handOffX[3] = { 11.0f, 29.0f, 47.0f };
                    const float handOffY[3] = { 40.0f, 19.0f, 40.0f };
                    Texture2D* barrelHandTex = regulusForceBlue ? &BlueBarrelMov1 : &BarrelMov1;
                    float barrelHandScale = REGULUS_SCALE * 0.55f;
                    float barrelHandW = barrelHandTex->width * barrelHandScale;
                    float barrelHandH = barrelHandTex->height * barrelHandScale;
                    float handScrX = regX + handOffX[throwIdx] * REGULUS_SCALE - barrelHandW * 0.5f;
                    float handScrY = regY + handOffY[throwIdx] * REGULUS_SCALE - barrelHandH * 0.5f;

                    if (throwIdx == 0 || throwIdx == 2)
                        DrawTexturePro(*barrelHandTex, { 0,0,(float)barrelHandTex->width,(float)barrelHandTex->height },
                            { handScrX, handScrY, barrelHandW, barrelHandH }, {}, 0.f, WHITE);
                    DrawTexturePro(*regTex, { 0,0,(float)regTex->width,(float)regTex->height },
                        { regX, regY, regW, regH }, {}, 0.f, WHITE);
                    if (throwIdx == 1)
                        DrawTexturePro(*barrelHandTex, { 0,0,(float)barrelHandTex->width,(float)barrelHandTex->height },
                            { handScrX, handScrY, barrelHandW, barrelHandH }, {}, 0.f, WHITE);
                }
                else
                {
                    DrawTexturePro(*regTex, { 0,0,(float)regTex->width,(float)regTex->height },
                        { regX, regY, regW, regH }, {}, 0.f, WHITE);
                }
            }

            // 10. Player
            {
                bool showPlayer = isDying || !invincible || ((int)(invincibleTimer * 10) % 2 == 0);
                if (showPlayer)
                {
                    float scale = 3.8f * 0.85f * 1.05f;
                    float baseH = imgMarioIdle.height * scale;
                    float thisH = image->height * scale;
                    float drawY = player.y + 10.0f + (baseH - thisH);
                    Rectangle src = { 0, 0, (float)image->width, (float)image->height };
                    Rectangle dest = { player.x, drawY, (float)image->width * scale, thisH };
                    if (!facingRight && !onLadder) src.width *= -1;
                    DrawTexturePro(*image, src, dest, {}, 0.f, WHITE);

                    // Heavy item sprite above player
                    if (isCarryingHeavy && !isDying)
                    {
                        Texture2D* heavyTex = nullptr;
                        switch (hotbar[hotbarSlot].type) {
                        case PU_NUKE_PU:       heavyTex = &Nuke;         break;
                        case PU_LARPER:        heavyTex = &LadderPart;   break;
                        case PU_EXTRA_LIFE:    heavyTex = &texGoldHeart; break;
                        case PU_ONE_MORE_LARP: heavyTex = &texHeart;     break;
                        default: break;
                        }
                        if (heavyTex && heavyTex->id > 0) {
                            float itemH = 40.f;
                            float itemW = itemH * (float)heavyTex->width / (float)heavyTex->height;
                            float itemX = player.x + dest.width * 0.5f - itemW * 0.5f;
                            bool  moving = (!onLadder && !isJumping && walkFrame != 0);
                            float bobY = moving ? 10.f : 4.f;
                            float itemY = drawY - itemH - 2.f + bobY;
                            Rectangle itemSrc = { 0, 0, (float)heavyTex->width, (float)heavyTex->height };
                            if (!facingRight) itemSrc.width *= -1;
                            DrawTexturePro(*heavyTex, itemSrc, { itemX, itemY, itemW, itemH }, {}, 0.f, WHITE);
                        }
                    }

                    // Shield animation (drawn while inside scene RT)
                    if (shieldActive && !isDying)
                    {
                        shieldAnimTimer += dt;
                        if (shieldAnimTimer >= 0.1f) {
                            shieldAnimTimer = fmodf(shieldAnimTimer, 0.1f);
                            shieldAnimFrame = 1 - shieldAnimFrame;
                        }
                        Texture2D* shTex = (shieldAnimFrame == 0) ? &texShield1 : &texShield2;
                        if (shTex->id > 0) {
                            float shW = shTex->width * 3.5f, shH = shTex->height * 3.5f;
                            float shX = player.x + dest.width * 0.5f - shW * 0.5f;
                            float shY = player.y + player.height * 0.5f - shH * 0.5f;
                            DrawTexturePro(*shTex, { 0,0,(float)shTex->width,(float)shTex->height },
                                { shX, shY, shW, shH }, {}, 0.f, { 255,255,255,210 });
                        }
                    }
                }

                // Whip visual — position follows player each frame
                if (whipActive) {
                    Texture2D* wTex = (whipTimer < 0.2f) ? &whipTex1 : &whipTex2;
                    if (wTex && wTex->id > 0) {
                        float wW = player.width * 2.f;
                        float wH = player.height;
                        float wx = whipFacingRight ? player.x + player.width : player.x - wW;
                        float wy = player.y;
                        float srcW = whipFacingRight ? (float)wTex->width : -(float)wTex->width;
                        float srcX = whipFacingRight ? 0.f : (float)wTex->width;
                        DrawTexturePro(*wTex, { srcX, 0, srcW, (float)wTex->height },
                            { wx, wy, wW, wH }, { 0, 0 }, 0.f, WHITE);
                    }
                }
            }

            // 11. Barrels
            for (const auto& b : barrels)
            {
                if (!b.active) continue;
                Texture2D** rollSet = b.isBlue ? blueBarrelRoll : barrelRoll;
                Texture2D** fallSet = b.isBlue ? blueBarrelFall : barrelFall;
                Texture2D* tex = nullptr;
                if (b.isFalling) tex = fallSet[b.animFrame % 2];
                else {
                    int frame = b.movingLeft ? (3 - b.animFrame % 4) : (b.animFrame % 4);
                    tex = rollSet[frame];
                }
                float drawW = b.hitbox.width * 2.0f, drawH = b.hitbox.height * 2.0f;
                float drawX = b.hitbox.x - (drawW - b.hitbox.width) * 0.5f;
                float drawY = b.hitbox.y - (drawH - b.hitbox.height) * 0.5f - 2.625f;
                DrawTexturePro(*tex, { 0,0,(float)tex->width,(float)tex->height },
                    { drawX, drawY, drawW, drawH }, {}, 0.f, WHITE);
            }

            // 12. Enemies
            for (const auto& en : enemies)
                DrawEnemy(en, rabbitIdleBlack, rabbitJumpBlack, rabbitIdleWhite, rabbitJumpWhite);

            // 13. Props (layer 1, lit)
            for (const auto& pr : currentLevelData.props) {
                if (pr.renderLayer != 1 || pr.lightAffect <= 0.f) continue;
                Texture2D* tex = (pr.texVariant == PROP_FIRE_VARIANT)
                    ? (propFireFrame == 0 ? &propTextures[PROP_FIRE_VARIANT] : &propFireFrame2)
                    : (pr.texVariant >= 0 && pr.texVariant < PROP_TEX_COUNT && propTextures[pr.texVariant].id > 0)
                        ? &propTextures[pr.texVariant] : nullptr;
                if (!tex) DrawRectanglePro({ pr.x - pr.width * .5f, pr.y - pr.height * .5f, pr.width, pr.height }, {}, pr.rotation, { 180,100,220,140 });
                else DrawTexturePro(*tex, { 0,0,(float)tex->width,(float)tex->height },
                    { pr.x, pr.y, pr.width, pr.height }, { pr.width * .5f, pr.height * .5f }, pr.rotation, WHITE);
            }

            // 14. Beatrice bullets
            {
                float bbScale = 2.0f;
                float bbW = texBeaBullet.width * bbScale, bbH = texBeaBullet.height * bbScale;
                for (const auto& bb : beaBullets) {
                    if (!bb.active) continue;
                    float angle = atan2f(bb.vel.y, bb.vel.x) * RAD2DEG;
                    DrawTexturePro(texBeaBullet, { 0,0,(float)texBeaBullet.width,(float)texBeaBullet.height },
                        { bb.pos.x - bbW * .5f, bb.pos.y - bbH * .5f, bbW, bbH },
                        { bbW * .5f, bbH * .5f }, angle, WHITE);
                }
            }

            // (HUD drawn after lighting composite — see below)

            // 16. Nuke explosion
            if (nukeExplosionPlaying && nukeExplosionFrame < NUKE_EXPL_FRAME_COUNT) {
                Texture2D* exTex = explosionFrames[nukeExplosionFrame];
                float exScale = 4.0f;
                float exW = exTex->width * exScale, exH = exTex->height * exScale;
                DrawTexturePro(*exTex, { 0,0,(float)exTex->width,(float)exTex->height },
                    { nukeExplosionPos.x - exW * .5f, nukeExplosionPos.y - exH * .5f, exW, exH }, {}, 0.f, WHITE);
            }

            // ── Lighting composite ────────────────────────────────────────────
            gameLighting.EndScene();
            gameLighting.BakeOccludersFromLevel(currentLevelData, cam);
            if (shieldActive) {
                LightData shieldGlow;
                shieldGlow.x = player.x + player.width * 0.5f;
                shieldGlow.y = player.y + player.height * 0.5f;
                shieldGlow.radius = 130.f;
                shieldGlow.r = 0.45f; shieldGlow.g = 0.75f; shieldGlow.b = 1.f;
                shieldGlow.intensity = 0.8f;
                shieldGlow.pulseFreq = 3.f; shieldGlow.pulseAmp = 0.25f;
                shieldGlow.enabled = true;
                gameLighting.AddRuntimeLight(shieldGlow);
            }
            gameLighting.SetBloom(dbgBloomEnabled, dbgBloomThreshold, dbgBloomIntensity);
            gameLighting.Composite(currentLevelData, cam);

            // 17. Props (unlit / overlay)
            for (const auto& pr : currentLevelData.props) {
                bool isUnlit = (pr.lightAffect <= 0.f);
                bool isOverlay = (pr.renderLayer == 2);
                if (!isUnlit && !isOverlay) continue;
                Texture2D* tex = (pr.texVariant == PROP_FIRE_VARIANT)
                    ? (propFireFrame == 0 ? &propTextures[PROP_FIRE_VARIANT] : &propFireFrame2)
                    : (pr.texVariant >= 0 && pr.texVariant < PROP_TEX_COUNT && propTextures[pr.texVariant].id > 0)
                        ? &propTextures[pr.texVariant] : nullptr;
                if (!tex) DrawRectanglePro({ pr.x - pr.width * .5f, pr.y - pr.height * .5f, pr.width, pr.height }, {}, pr.rotation, { 180,100,220,140 });
                else DrawTexturePro(*tex, { 0,0,(float)tex->width,(float)tex->height },
                    { pr.x, pr.y, pr.width, pr.height }, { pr.width * .5f, pr.height * .5f }, pr.rotation, WHITE);
            }

            // 17.1 Glow pass (additive)
            {
                bool anyGlow = false;
                for (const auto& pr : currentLevelData.props) if (pr.lightAffect > 1.f) { anyGlow = true; break; }
                if (anyGlow) {
                    BeginBlendMode(BLEND_ADDITIVE);
                    for (const auto& pr : currentLevelData.props) {
                        if (pr.lightAffect <= 1.f) continue;
                        Texture2D* tex = (pr.texVariant == PROP_FIRE_VARIANT)
                            ? (propFireFrame == 0 ? &propTextures[PROP_FIRE_VARIANT] : &propFireFrame2)
                            : (pr.texVariant >= 0 && pr.texVariant < PROP_TEX_COUNT && propTextures[pr.texVariant].id > 0)
                                ? &propTextures[pr.texVariant] : nullptr;
                        if (!tex) continue;
                        unsigned char glowA = (unsigned char)Clamp((pr.lightAffect - 1.f) / 2.f * 255.f, 0.f, 255.f);
                        DrawTexturePro(*tex, { 0,0,(float)tex->width,(float)tex->height },
                            { pr.x, pr.y, pr.width, pr.height }, { pr.width * .5f, pr.height * .5f }, pr.rotation, { 255,255,255,glowA });
                    }
                    EndBlendMode();
                }
            }

            // 18. Rain overlay
            {
                float scaleX = (float)screenWidth / Rain.width;
                float scaleY = (float)screenHeight / Rain.height;
                float sW = Rain.width * scaleX, sH = Rain.height * scaleY;
                DrawTexturePro(Rain, { 0, rainScrollY, (float)Rain.width, (float)Rain.height }, { 0,0,sW,sH }, {}, 0.f, rainTint);
                DrawTexturePro(Rain, { 0, 0, (float)Rain.width, (float)Rain.height }, { 0, -sH + rainScrollY, sW, sH }, {}, 0.f, rainTint);
            }

            // 19. "Press F" prompt near nuke / beatrice ground items
            if (!isDying)
            {
                const float fbW = (FButton.id > 0) ? (float)FButton.width  * 2.f : 24.f;
                const float fbH = (FButton.id > 0) ? (float)FButton.height * 2.f : 24.f;
                auto DrawFPrompt = [&](float cx, float topY) {
                    float dx = cx - fbW * 0.5f, dy = topY - fbH - 4.f;
                    if (FButton.id > 0)
                        DrawTexturePro(FButton,
                            { 0, 0, (float)FButton.width, (float)FButton.height },
                            { dx, dy, fbW, fbH }, { 0, 0 }, 0.f, WHITE);
                    else
                        DrawText("[F]", (int)dx, (int)dy, 18, YELLOW);
                };
                bool shown = false;
                const float nkW = NUKE_NATIVE_W * NUKE_SCALE, nkH = NUKE_NATIVE_H * NUKE_SCALE;
                for (const auto& nk : nukes) {
                    if (!nk.active) continue;
                    if (CheckCollisionRecs(player, { nk.pos.x, nk.pos.y, nkW, nkH })) {
                        DrawFPrompt(nk.pos.x + nkW * 0.5f, nk.pos.y);
                        shown = true; break;
                    }
                }
                if (!shown && !playerHasBeatrice) {
                    const float bcScale2 = 2.0f;
                    const float bcW = Beatrice_Idle1.width * bcScale2, bcH = Beatrice_Idle1.height * bcScale2;
                    for (const auto& bc : beatrices) {
                        if (!bc.active) continue;
                        if (CheckCollisionRecs(player, { bc.pos.x, bc.pos.y - bcH, bcW, bcH })) {
                            DrawFPrompt(bc.pos.x + bcW * 0.5f, bc.pos.y - bcH);
                            break;
                        }
                    }
                }
            }

            // 20. HUD (after lighting — always full brightness)
            {
                // Lives: active hearts full-bright, lost hearts grayed out at 50% opacity
                {
                    const float hW = 30.f, hH = 30.f, hGap = 6.f;
                    for (int i = 0; i < maxLives; i++) {
                        Texture2D* hTex = (i < 3) ? &texHeart : &texGoldHeart;
                        float hx = 14.f + i * (hW + hGap);
                        bool active = (i < lives);
                        Color tint = active ? WHITE : Color{80, 80, 80, 128};
                        if (hTex->id > 0) {
                            DrawTexturePro(*hTex,
                                { 0,0,(float)hTex->width,(float)hTex->height },
                                { hx, 10.f, hW, hH }, {}, 0.f, tint);
                        } else {
                            Color col = active ? (i < 3 ? RED : GOLD) : Color{80, 80, 80, 128};
                            DrawText(i < 3 ? "<3" : "G<3", (int)hx, 10, 24, col);
                        }
                    }
                }
                // Coins (score = coins, displayed with icon)
                {
                    const char* coinTxt = TextFormat("%d", coins);
                    int fontSize = 26;
                    int iconSize = 26;
                    int gap = 6;
                    int txtW = MeasureText(coinTxt, fontSize);
                    int totalW = iconSize + gap + txtW;
                    int startX = screenWidth - totalW - 12;
                    if (texCoin.id > 0) {
                        DrawTexturePro(texCoin,
                            { 0,0,(float)texCoin.width,(float)texCoin.height },
                            { (float)startX, 7.f, (float)iconSize, (float)iconSize }, {}, 0.f, WHITE);
                    } else {
                        DrawCircle(startX + iconSize/2, 10 + iconSize/2, iconSize * 0.45f, GOLD);
                        DrawCircle(startX + iconSize/2, 10 + iconSize/2, iconSize * 0.3f, Color{255,200,50,255});
                    }
                    DrawText(coinTxt, startX + iconSize + gap, 10, fontSize, GOLD);
                }
                // ── Hotbar (bottom-right, vertical) ──────────────────────────
                {
                    static const float HB_W = 64.f, HB_H = 96.f;
                    static const float HB_GAP = 8.f, HB_MARGIN = 10.f;
                    float totalHBH = 3 * HB_H + 2 * HB_GAP;
                    float hbX = (float)screenWidth - HB_MARGIN - HB_W;
                    float hbTopY = (float)screenHeight - HB_MARGIN - totalHBH;
                    for (int i = 0; i < 3; i++) {
                        float slotY = hbTopY + i * (HB_H + HB_GAP);
                        const HotbarSlot& s = hotbar[i];
                        Color bg = (i == hotbarSlot) ? Color{255,220,0,180} : Color{40,40,40,180};
                        DrawRectangle((int)hbX, (int)slotY, (int)HB_W, (int)HB_H, bg);
                        // Rarity-colored border (yellow if selected slot)
                        Color slotBorder = DARKGRAY;
                        if (s.type != PU_NONE) {
                            int ri = PU_INFO[(int)s.type].rarityIdx;
                            if (ri >= 0 && ri < 6) slotBorder = rarityBorderCols[ri];
                        }
                        DrawRectangleLinesEx({hbX, slotY, HB_W, HB_H},
                            (i == hotbarSlot) ? 3.f : 2.f,
                            i == hotbarSlot ? YELLOW : slotBorder);
                        DrawText(TextFormat("%d", i+1), (int)hbX + 4, (int)slotY + 4, 12, WHITE);
                        if (s.type != PU_NONE) {
                            // Item texture only — no card background
                            Texture2D* iTex = GetItemTex(s.type);
                            if (iTex && iTex->id > 0) {
                                float pad = 8.f;
                                Rectangle slotInner = { hbX+pad, slotY+pad, HB_W-pad*2.f, HB_H-pad*2.f };
                                Rectangle fitR = FitTexRect(slotInner, (float)iTex->width, (float)iTex->height);
                                DrawTexturePro(*iTex,
                                    {0.f, 0.f, (float)iTex->width, (float)iTex->height},
                                    fitR, {}, 0.f, WHITE);
                            }
                            const PowerupInfo& info = PU_INFO[(int)s.type];
                            if (info.passive) {
                                DrawText("AUTO", (int)(hbX - 42), (int)(slotY + HB_H/2 - 8), 13, LIME);
                            } else if (info.maxCD > 0.f) {
                                Color cdColor = s.cd <= 0.f ? LIME : ORANGE;
                                const char* cdStr = s.cd <= 0.f ? "READY" : TextFormat("%.0fs", s.cd + 0.5f);
                                int cw = MeasureText(cdStr, 12);
                                DrawText(cdStr, (int)(hbX - cw - 6), (int)(slotY + HB_H/2 - 7), 12, cdColor);
                            } else {
                                DrawText(TextFormat("x%d", s.charges), (int)(hbX - 30), (int)(slotY + HB_H/2 - 7), 14, SKYBLUE);
                            }
                        }
                    }
                }
                // ── Ability bars (bottom-left, stacking upward) ───────────────
                {
                    struct ABar { const char* name; float cur; float maxV; Color col; };
                    ABar abars[6]; int nABars = 0;
                    if (playerHasBeatrice && beatriceAbilityTimer > 0.f)
                        abars[nABars++] = {"BEATRICE", beatriceAbilityTimer, BEATRICE_DURATION, MAGENTA};
                    if (speedrunActive && speedrunTimer > 0.f)
                        abars[nABars++] = {"SPEEDRUN", speedrunTimer, 5.f, LIME};
                    if (shieldActive && shieldTimer > 0.f)
                        abars[nABars++] = {"SHIELD", shieldTimer, 0.3f, SKYBLUE};
                    if (dashActive && dashInvulTimer > 0.f)
                        abars[nABars++] = {"DASH", dashInvulTimer, 0.5f, GOLD};
                    static const float AB_W = 150.f, AB_H = 16.f, AB_GAP = 4.f, AB_X = 10.f;
                    for (int i = 0; i < nABars; i++) {
                        float ay = (float)screenHeight - 95.f - (float)(nABars - i) * (AB_H + AB_GAP);
                        float fill = (abars[i].maxV > 0.f) ? fminf(abars[i].cur / abars[i].maxV, 1.f) : 0.f;
                        DrawRectangle((int)AB_X - 2, (int)ay - 2, (int)AB_W + 4, (int)AB_H + 4, {0,0,0,180});
                        DrawRectangle((int)AB_X, (int)ay, (int)AB_W, (int)AB_H, {40,40,40,200});
                        DrawRectangle((int)AB_X, (int)ay, (int)(AB_W * fill), (int)AB_H, abars[i].col);
                        DrawText(abars[i].name, (int)AB_X + 4, (int)ay + 2, 11, WHITE);
                        const char* ts = TextFormat("%.1fs", abars[i].cur);
                        int tw = MeasureText(ts, 10);
                        DrawText(ts, (int)(AB_X + AB_W - tw - 4), (int)(ay + 3), 10, WHITE);
                    }
                }
                // ── RBD fade overlay ──────────────────────────────────────────
                if (rbdFading) {
                    float t = 1.f - rbdFadeTimer / 2.f;
                    unsigned char a = (unsigned char)(t * 255.f);
                    DrawRectangle(0, 0, screenWidth, screenHeight, {0,0,0,a});
                }
                if (debugPath) DrawBarrelPathDebug(barrelPath, barrels, screenHeight);
            }

            // 21. Nuke flash
            {
                float flashTotal = NUKE_FLASH_IN + NUKE_FLASH_OUT;
                if (nukeFlashTimer < flashTotal) {
                    unsigned char alpha = 0;
                    if (nukeFlashTimer < NUKE_FLASH_IN)
                        alpha = (unsigned char)(255.0f * (nukeFlashTimer / NUKE_FLASH_IN));
                    else
                        alpha = (unsigned char)(255.0f * (1.0f - (nukeFlashTimer - NUKE_FLASH_IN) / NUKE_FLASH_OUT));
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 255,255,255,alpha });
                }
            }

            // 22. Death overlay
            if (isDying || deathReachedBlack)
            {
                if (deathReachedBlack) {
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 0,0,0,255 });
                }
                else if (deathTimer < DEATH_FLASH_DURATION) {
                    float t = deathTimer / DEATH_FLASH_DURATION;
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 255,255,255,(unsigned char)(120.0f * t) });
                }
                else {
                    float t = Clamp((deathTimer - DEATH_FLASH_DURATION) / DEATH_FADE_DURATION, 0.f, 1.f);
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 0,0,0,(unsigned char)(t * 255.0f) });
                }
            }

            // ── Pause overlay ─────────────────────────────────────────────────
            if (isPaused)
            {
                Vector2 pauseMouse = GetMousePosition();

                // Darkened background
                DrawRectangle(0, 0, screenWidth, screenHeight, {0, 0, 0, 172});

                // Panel
                const float pW = 700.f, pH2 = 580.f;
                const float pX = (screenWidth  - pW)  * 0.5f;
                const float pY = (screenHeight - pH2) * 0.5f;
                DrawRectangleRounded({pX, pY, pW, pH2}, 0.04f, 8, {18, 18, 30, 252});
                DrawRectangleRoundedLinesEx({pX, pY, pW, pH2}, 0.04f, 8, 2.f, {100, 160, 255, 220});

                // Title
                const char* pTitle = "PAUSED";
                DrawText(pTitle, (int)(pX + (pW - MeasureText(pTitle, 38)) * 0.5f), (int)(pY + 18), 38, {120, 220, 120, 255});

                // Tab bar
                const char* pTabNames[3] = { "CONTROLS", "SETTINGS", "DEBUG" };
                const float pTabY = pY + 68.f, pTabH = 34.f, pTabW = pW / 3.f;
                for (int t = 0; t < 3; t++) {
                    float tx = pX + t * pTabW;
                    bool active = (settingsTab == t);
                    DrawRectangleRec({tx, pTabY, pTabW, pTabH}, active ? Color{50,100,200,220} : Color{30,30,55,200});
                    DrawRectangleLinesEx({tx, pTabY, pTabW, pTabH}, 1.f, {80,80,140,200});
                    int tw = MeasureText(pTabNames[t], 18);
                    DrawText(pTabNames[t], (int)(tx+(pTabW-tw)*0.5f), (int)(pTabY+(pTabH-18)*0.5f), 18, active?WHITE:Color{160,160,200,255});
                    if (!active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(pauseMouse,{tx,pTabY,pTabW,pTabH}))
                        settingsTab = t;
                }

                const float pcX = pX + 30.f, pcY = pTabY + pTabH + 20.f, pcW = pW - 60.f;

                // ── Controls tab ──────────────────────────────────────────────
                if (settingsTab == 0) {
                    struct CR { const char* action; const char* keys; };
                    static const CR cr[] = {
                        {"Move Left / Right",  "A  /  D"},
                        {"Jump",               "SPACE"},
                        {"Use Ability",        "E  or  Left Click"},
                        {"Pick Up Item",       "F"},
                        {"Enter Ladder",       "W or S  (near ladder)"},
                        {"Climb Up / Down",    "W  /  S  (on ladder)"},
                        {"Exit Ladder",        "A, D, SPACE or W"},
                        {"Select Hotbar Slot", "1  /  2  /  3"},
                        {"Scroll Hotbar",      "Mouse Wheel"},
                        {"Pause / Settings",   "ESC"},
                        {"Toggle Debug Panel", "M  (in-game)"},
                    };
                    const int nCR = (int)(sizeof(cr)/sizeof(cr[0]));
                    const int rFS = 20, rGap = 30;
                    for (int i = 0; i < nCR; i++) {
                        float ry = pcY + i * rGap;
                        DrawText(cr[i].action, (int)pcX, (int)ry, rFS, {200,200,220,255});
                        DrawText(cr[i].keys, (int)(pcX+pcW-MeasureText(cr[i].keys,rFS)), (int)ry, rFS, YELLOW);
                        DrawLine((int)pcX,(int)(ry+rFS+3),(int)(pcX+pcW),(int)(ry+rFS+3),{60,60,90,180});
                    }
                }

                // ── Settings tab (volume sliders) ─────────────────────────────
                if (settingsTab == 1) {
                    struct SD { const char* label; float* vol; float maxVal; };
                    SD sldrs[] = {{"Music",&volMusic,1.f},{"Abilities",&volAbility,4.f},{"Gameplay SFX",&volSFX,4.f}};
                    const float sX = pcX+160.f, sW = 350.f, sH = 14.f;
                    const float rowGap = 85.f;
                    for (int i = 0; i < 3; i++) {
                        float sy = pcY + 30.f + i * rowGap;
                        float& vol = *sldrs[i].vol;
                        float  mx  = sldrs[i].maxVal;
                        float  frac = vol / mx;
                        DrawText(sldrs[i].label, (int)pcX, (int)(sy - 11.f + sH * 0.5f), 22, RAYWHITE);
                        DrawRectangleRec({sX,sy,sW,sH},{50,50,75,200});
                        Color fc = frac<0.25f?Color{220,80,80,255}:frac<0.5f?Color{220,180,60,255}:Color{90,200,100,255};
                        DrawRectangleRec({sX,sy,sW*frac,sH},fc);
                        DrawRectangleLinesEx({sX,sy,sW,sH},1.f,{160,160,200,255});
                        float hx = sX+sW*frac;
                        DrawCircle((int)hx,(int)(sy+sH*0.5f),10.f,WHITE);
                        DrawCircle((int)hx,(int)(sy+sH*0.5f),7.f,fc);
                        DrawText(TextFormat("%d%%",(int)(vol*100.f+0.5f)),(int)(sX+sW+16),(int)(sy-1),22,RAYWHITE);
                        Rectangle hitR={sX-12.f,sy-10.f,sW+24.f,sH+20.f};
                        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(pauseMouse,hitR))
                            vol = Clamp((pauseMouse.x-sX)/sW,0.f,1.f)*mx;
                    }
                    SetMusicVolume(music,volMusic);
                }

                // ── Debug tab ─────────────────────────────────────────────────
                if (settingsTab == 2) {
                    const float ROW_H  = 33.f;
                    const float clipH  = pY + pH2 - 52.f - pcY;
                    const float sbW    = 8.f;
                    const float innerW = pcW - sbW - 4.f;
                    const int   lFS    = 17;
                    if (CheckCollisionPointRec(pauseMouse,{pcX,pcY,pcW,clipH}))
                        settingsDbgScroll -= GetMouseWheelMove() * ROW_H;
                    const int TOTAL_ROWS = 18;
                    float totalH2 = TOTAL_ROWS * ROW_H;
                    float maxScroll2 = fmaxf(0.f, totalH2 - clipH);
                    settingsDbgScroll = Clamp(settingsDbgScroll, 0.f, maxScroll2);
                    BeginScissorMode((int)pcX,(int)pcY,(int)(innerW+2.f),(int)clipH);
                    float ry2 = pcY - settingsDbgScroll;

                    auto togBtn2 = [&](const char* lbl, bool state, Color onC, Color offC) -> bool {
                        Color bc = state ? onC : offC;
                        Rectangle r = {pcX, ry2, innerW, ROW_H-3.f};
                        bool hov = CheckCollisionPointRec(pauseMouse,r);
                        if (hov){bc.r=(unsigned char)fminf(bc.r+40,255);bc.g=(unsigned char)fminf(bc.g+40,255);bc.b=(unsigned char)fminf(bc.b+40,255);}
                        DrawRectangleRec(r,bc); DrawRectangleLinesEx(r,1.f,{120,120,180,200});
                        int tw=MeasureText(lbl,lFS); DrawText(lbl,(int)(r.x+(r.width-tw)/2),(int)(r.y+(r.height-lFS)/2),lFS,WHITE);
                        ry2+=ROW_H; return hov&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                    };
                    auto step2 = [&](const char* lbl, int& val, int mn, int mx) {
                        DrawText(lbl,(int)(pcX+4),(int)(ry2+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                        float bw=30.f,bh=ROW_H-5.f;
                        Rectangle mR={pcX+innerW-bw*2-50.f,ry2+2.f,bw,bh}, pR={pcX+innerW-bw-2.f,ry2+2.f,bw,bh};
                        bool mH=CheckCollisionPointRec(pauseMouse,mR),pH=CheckCollisionPointRec(pauseMouse,pR);
                        DrawRectangleRec(mR,mH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(mR,1.f,{100,100,160,200});
                        DrawRectangleRec(pR,pH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(pR,1.f,{100,100,160,200});
                        DrawText("<",(int)(mR.x+(mR.width-MeasureText("<",lFS))/2),(int)(mR.y+(bh-lFS)/2),lFS,WHITE);
                        DrawText(">",(int)(pR.x+(pR.width-MeasureText(">",lFS))/2),(int)(pR.y+(bh-lFS)/2),lFS,WHITE);
                        const char* vs=TextFormat("%d",val);
                        DrawText(vs,(int)(mR.x-MeasureText(vs,lFS)-6),(int)(ry2+(ROW_H-lFS)/2),lFS,YELLOW);
                        if (mH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&val>mn) val--;
                        if (pH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&val<mx) val++;
                        ry2+=ROW_H;
                    };
                    auto fstep2 = [&](const char* lbl, float& val, float mn, float mx, float step, const char* fmt) {
                        DrawText(lbl,(int)(pcX+4),(int)(ry2+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                        float bw=30.f,bh=ROW_H-5.f;
                        Rectangle mR={pcX+innerW-bw*2-50.f,ry2+2.f,bw,bh}, pR={pcX+innerW-bw-2.f,ry2+2.f,bw,bh};
                        bool mH=CheckCollisionPointRec(pauseMouse,mR),pH=CheckCollisionPointRec(pauseMouse,pR);
                        DrawRectangleRec(mR,mH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(mR,1.f,{100,100,160,200});
                        DrawRectangleRec(pR,pH?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(pR,1.f,{100,100,160,200});
                        DrawText("-",(int)(mR.x+(mR.width-MeasureText("-",lFS))/2),(int)(mR.y+(bh-lFS)/2),lFS,WHITE);
                        DrawText("+",(int)(pR.x+(pR.width-MeasureText("+",lFS))/2),(int)(pR.y+(bh-lFS)/2),lFS,WHITE);
                        const char* vs=TextFormat(fmt,val);
                        DrawText(vs,(int)(mR.x-MeasureText(vs,lFS)-6),(int)(ry2+(ROW_H-lFS)/2),lFS,YELLOW);
                        if (mH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val=fmaxf(mn,val-step);
                        if (pH&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val=fminf(mx,val+step);
                        ry2+=ROW_H;
                    };
                    auto actBtn2 = [&](const char* lbl, Color col) -> bool {
                        Color bc=col; Rectangle r={pcX,ry2,innerW,ROW_H-3.f};
                        bool hov=CheckCollisionPointRec(pauseMouse,r);
                        if (hov){bc.r=(unsigned char)fminf(bc.r+40,255);bc.g=(unsigned char)fminf(bc.g+40,255);bc.b=(unsigned char)fminf(bc.b+40,255);}
                        DrawRectangleRec(r,bc); DrawRectangleLinesEx(r,1.f,{100,100,160,200});
                        int tw=MeasureText(lbl,lFS); DrawText(lbl,(int)(r.x+(r.width-tw)/2),(int)(r.y+(r.height-lFS)/2),lFS,WHITE);
                        ry2+=ROW_H; return hov&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                    };

                    if (togBtn2(TextFormat("Debug Overlay (M key):  %s",dbgMenuOpen?"ON":"OFF"),dbgMenuOpen,{30,120,30,220},{80,30,30,220})) dbgMenuOpen=!dbgMenuOpen;
                    step2("Lives",lives,0,maxLives);
                    step2("Max Lives",maxLives,1,10);
                    if (maxLives<lives) lives=maxLives;
                    if (togBtn2(TextFormat("Immortality:  %s",dbgImmortal?"ON":"OFF"),dbgImmortal,{0,115,0,220},{75,0,0,220})) dbgImmortal=!dbgImmortal;
                    if (togBtn2(TextFormat("Flight:  %s",dbgFlight?"ON":"OFF"),dbgFlight,{0,70,160,220},{30,30,80,220})) {dbgFlight=!dbgFlight;if(dbgFlight)dbgFlightNoCol=false;}
                    if (togBtn2(TextFormat("No Clip:  %s",dbgFlightNoCol?"ON":"OFF"),dbgFlightNoCol,{0,105,145,220},{30,30,80,220})) {dbgFlightNoCol=!dbgFlightNoCol;if(dbgFlightNoCol)dbgFlight=false;}
                    if (togBtn2(TextFormat("Bloom:  %s",dbgBloomEnabled?"ON":"OFF"),dbgBloomEnabled,{80,0,160,220},{30,30,80,220})) dbgBloomEnabled=!dbgBloomEnabled;
                    fstep2("Bloom Intensity",dbgBloomIntensity,0.f,3.f,0.1f,"%.1f");
                    fstep2("Bloom Threshold",dbgBloomThreshold,0.f,1.f,0.05f,"%.2f");
                    {
                        DrawText("Give Ability:",(int)(pcX+4),(int)(ry2+(ROW_H-lFS)/2),lFS,{200,200,220,255});
                        float bw=30.f,bh=ROW_H-5.f,gW=170.f;
                        Rectangle prv={pcX+innerW-bw*2-gW-6.f,ry2+2.f,bw,bh};
                        Rectangle gvR={pcX+innerW-bw-gW-4.f,  ry2+2.f,gW,bh};
                        Rectangle nxt={pcX+innerW-bw-2.f,      ry2+2.f,bw,bh};
                        bool pH2=CheckCollisionPointRec(pauseMouse,prv),nH2=CheckCollisionPointRec(pauseMouse,nxt),gH2=CheckCollisionPointRec(pauseMouse,gvR);
                        DrawRectangleRec(prv,pH2?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(prv,1.f,{100,100,160,200});
                        DrawRectangleRec(nxt,nH2?Color{80,80,140,255}:Color{50,50,80,255}); DrawRectangleLinesEx(nxt,1.f,{100,100,160,200});
                        DrawRectangleRec(gvR,gH2?Color{100,60,130,255}:Color{60,40,85,255}); DrawRectangleLinesEx(gvR,1.f,{100,100,160,200});
                        DrawText("<",(int)(prv.x+(prv.width-MeasureText("<",lFS))/2),(int)(prv.y+(bh-lFS)/2),lFS,WHITE);
                        DrawText(">",(int)(nxt.x+(nxt.width-MeasureText(">",lFS))/2),(int)(nxt.y+(bh-lFS)/2),lFS,WHITE);
                        const char* pn=PU_INFO[dbgGivePUIdx].name;
                        int gnw=MeasureText(pn,lFS-2); DrawText(pn,(int)(gvR.x+(gvR.width-gnw)/2),(int)(gvR.y+(bh-(lFS-2))/2),lFS-2,YELLOW);
                        if (pH2&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbgGivePUIdx=(dbgGivePUIdx+PU_COUNT-1)%PU_COUNT;
                        if (nH2&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbgGivePUIdx=(dbgGivePUIdx+1)%PU_COUNT;
                        if (gH2&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) AddToHotbar((PowerupType)dbgGivePUIdx);
                        ry2+=ROW_H;
                    }
                    if (actBtn2("Spawn Barrel",   {50,80,50,220}))  SpawnBarrelFromPool(barrels,barrelPath);
                    if (actBtn2("Spawn Enemy",    {80,50,50,220}))  { for (auto& en:enemies) if(!en.active){en.active=true;en.hitbox={player.x+(facingRight?80.f:-80.f),player.y-50.f,44.f,44.f};en.velocity={0,0};en.type=GRUNT;en.state=ES_IDLE;en.stateTimer=0;en.grounded=false;en.facingRight=facingRight;break;} }
                    if (actBtn2("Summon Nuke",    {80,70,25,220}))  nukes.push_back({{player.x,player.y-60.f},true});
                    if (actBtn2("Summon Beatrice",{50,50,100,220})) beatrices.push_back({{player.x,player.y-60.f},true});
                    fstep2("Music Vol",    volMusic,  0.f,1.f,0.05f,"%.2f");
                    fstep2("SFX Vol",      volSFX,    0.f,4.f,0.05f,"%.2f");
                    fstep2("Ability Vol",  volAbility,0.f,4.f,0.1f, "%.1f");
                    fstep2("UI Vol",       volUI,     0.f,1.f,0.05f,"%.2f");
                    SetMusicVolume(music,volMusic);
                    EndScissorMode();
                    if (maxScroll2 > 0.f) {
                        float ratio=clipH/totalH2, thumbH=fmaxf(clipH*ratio,20.f);
                        float thumbY=pcY+(settingsDbgScroll/maxScroll2)*(clipH-thumbH);
                        DrawRectangle((int)(pcX+pcW-sbW),(int)pcY,(int)sbW,(int)clipH,{30,30,50,180});
                        DrawRectangle((int)(pcX+pcW-sbW),(int)thumbY,(int)sbW,(int)thumbH,{110,110,195,255});
                    }
                }

                // Bottom buttons
                const float btnBH = 34.f, btnBW = 200.f, btnBY = pY + pH2 - 46.f;
                Rectangle resumeR = {pX + pW*0.25f - btnBW*0.5f, btnBY, btnBW, btnBH};
                Rectangle toMenuR = {pX + pW*0.75f - btnBW*0.5f, btnBY, btnBW, btnBH};
                bool resHov  = CheckCollisionPointRec(pauseMouse, resumeR);
                bool menHov  = CheckCollisionPointRec(pauseMouse, toMenuR);
                DrawRectangleRec(resumeR, resHov?Color{40,160,40,245}:Color{25,100,25,220});
                DrawRectangleLinesEx(resumeR, 1.f, {100,255,100,200});
                const char* resTxt = "RESUME  (ESC)";
                DrawText(resTxt,(int)(resumeR.x+(resumeR.width-MeasureText(resTxt,18))*0.5f),(int)(resumeR.y+(btnBH-18)*0.5f),18,WHITE);
                DrawRectangleRec(toMenuR, menHov?Color{160,40,40,245}:Color{100,25,25,220});
                DrawRectangleLinesEx(toMenuR, 1.f, {255,100,100,200});
                const char* menTxt = "RETURN TO MENU";
                DrawText(menTxt,(int)(toMenuR.x+(toMenuR.width-MeasureText(menTxt,18))*0.5f),(int)(toMenuR.y+(btnBH-18)*0.5f),18,WHITE);
                if (resHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) isPaused = false;
                if (menHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { isPaused = false; currentScreen = MENU; }
            }
        }
        else if (currentScreen == GAME_OVER)
        {
            if (lives > 0)
            {
                subaruTimer += dt;
                if (subaruTimer >= 1.0f / SUBARU_ANIM_FPS)
                {
                    subaruTimer -= 1.0f / SUBARU_ANIM_FPS; subaruFrame = (subaruFrame + 1) % SUBARU_FRAME_COUNT;
                }

                DrawTexturePro(Subaru_Background,
                    { 0,0,(float)Subaru_Background.width,(float)Subaru_Background.height },
                    { 0,0,(float)screenWidth,(float)screenHeight }, { 0,0 }, 0.f, WHITE);
                Texture2D* subTex = subaruFrames[subaruFrame];
                DrawTexturePro(*subTex, { 0,0,(float)subTex->width,(float)subTex->height },
                    { 0,0,(float)screenWidth,(float)screenHeight }, { 0,0 }, 0.f, WHITE);
                DrawText("HOW HIGH CAN YOU GET?", 225, 900, 30, WHITE);
                DrawText("25", 200, 800, 30, WHITE);
                DrawText("50", 200, 700, 30, WHITE);
            }
            else
            {
                DrawText("GAME OVER", 300, 380, 50, RED);

                // ── Run score ────────────────────────────────────────────────
                const int fsSc = 34, centerX = screenWidth / 2;
                const char* scoreTxt = TextFormat("Score: %u", runScore);
                DrawText(scoreTxt, centerX - MeasureText(scoreTxt, fsSc) / 2, 450, fsSc, YELLOW);

                // ── Levels cleared this run ──────────────────────────────────
                const char* lvlTxt = TextFormat("Levels cleared: %d", levelsCleared);
                DrawText(lvlTxt, centerX - MeasureText(lvlTxt, 26) / 2, 500, 26, SKYBLUE);

                // ── Highscore ────────────────────────────────────────────────
                bool newRecord = (runScore > 0 && runScore == highScore);
                Color hscCol = newRecord ? Color{255, 220, 50, 255} : Color{180, 180, 220, 255};
                const char* hsTxt = TextFormat("Best Score: %u", highScore);
                DrawText(hsTxt, centerX - MeasureText(hsTxt, 26) / 2, 545, 26, hscCol);
                if (newRecord) DrawText("NEW RECORD!", centerX - MeasureText("NEW RECORD!", 22) / 2, 578, 22, ORANGE);

                const char* hlTxt = TextFormat("Best Levels: %d", highLevels);
                DrawText(hlTxt, centerX - MeasureText(hlTxt, 24) / 2, 610, 24, Color{160, 220, 180, 255});

                // ── Coins ────────────────────────────────────────────────────
                {
                    const char* coinTxt = TextFormat("%d", coins);
                    int fontSize = 26, iconSize = 26, gap = 8;
                    int txtW = MeasureText(coinTxt, fontSize);
                    int totalW = iconSize + gap + txtW;
                    int sx = centerX - totalW / 2;
                    if (texCoin.id > 0)
                        DrawTexturePro(texCoin, {0,0,(float)texCoin.width,(float)texCoin.height},
                            {(float)sx, 650.f, (float)iconSize, (float)iconSize}, {}, 0.f, WHITE);
                    else { DrawCircle(sx+iconSize/2, 650+iconSize/2, iconSize*0.45f, GOLD);
                           DrawCircle(sx+iconSize/2, 650+iconSize/2, iconSize*0.3f, {255,200,50,255}); }
                    DrawText(coinTxt, sx + iconSize + gap, 650, fontSize, GOLD);
                }
            }
        }
        else if (currentScreen == CARD_SELECT)
        {
            bool  isShop = (cardSelectMode == CSM_SHOP);
            float cardW  = isShop ? 150.f : 280.f;
            float cardH  = isShop ? 225.f : 420.f;
            float gapX   = 20.f;
            float totalW = numDispCards * cardW + (numDispCards - 1) * gapX;
            float startX = isShop
                ? ((float)screenWidth - totalW) / 2.f + 40.f
                : ((float)screenWidth - totalW) / 2.f;
            float startY = 160.f;

            // Hotbar layout (matches update block)
            static const float HB_W2 = 64.f, HB_H2 = 96.f, HB_GAP2 = 8.f;
            float hbTotalW2 = 3 * HB_W2 + 2 * HB_GAP2;
            float hbStartX2 = ((float)screenWidth - hbTotalW2) * 0.5f;
            float hbSlotY2  = (float)screenHeight - 20.f - HB_H2;

            // Title
            const char* title = isShop ? "SHOP - CHOOSE TO BUY" : "CHOOSE YOUR POWER";
            int tw = MeasureText(title, 28);
            DrawText(title, screenWidth / 2 - tw / 2, 50, 28, WHITE);
            if (isShop) {
                // Exit button (top-right)
                Rectangle exitR = { (float)screenWidth - 130.f, 46.f, 110.f, 32.f };
                bool exitHov = CheckCollisionPointRec(GetMousePosition(), exitR) && !anyCardPicked;
                DrawRectangleRec(exitR, exitHov ? Color{180,40,40,230} : Color{100,25,25,200});
                DrawRectangleLinesEx(exitR, 2.f, exitHov ? Color{255,100,100,255} : Color{180,60,60,200});
                int etw = MeasureText("EXIT SHOP", 13);
                DrawText("EXIT SHOP", (int)(exitR.x + exitR.width*0.5f - etw*0.5f), (int)(exitR.y + 9.f), 13, WHITE);

                // Coin icon + count next to title
                int iconSz = 22, gapSz = 4;
                float iconX = (float)(screenWidth / 2 + tw / 2 + 20);
                if (texCoin.id > 0)
                    DrawTexturePro(texCoin, {0,0,(float)texCoin.width,(float)texCoin.height},
                        {iconX, 52.f, (float)iconSz, (float)iconSz}, {}, 0.f, WHITE);
                else { DrawCircle((int)iconX+iconSz/2, 63, iconSz/2, GOLD);
                       DrawCircle((int)iconX+iconSz/2, 63, iconSz*5/14, {255,200,50,255}); }
                DrawText(TextFormat("%d", coins), (int)(iconX + iconSz + gapSz), 52, 22, GOLD);
            }

            // Draw each card
            for (int i = 0; i < numDispCards; i++) {
                const CardDisplay& c = displayCards[i];
                if (c.scale <= 0.001f) continue;
                float shakeOff = (c.shakeTimer > 0.f)
                    ? sinf(c.shakeTimer * 22.f) * 9.f * (c.shakeTimer / 0.45f) : 0.f;
                float cx = startX + i * (cardW + gapX) + cardW * 0.5f + shakeOff;
                float cy = startY + cardH * 0.5f;

                float scaleX = c.scale;
                bool  flipped = false;
                if (c.selected && c.exitT > 0.f) {
                    float spinF = cosf(c.spinPhase * 3.14159f * 3.f);
                    scaleX = fabsf(spinF) * c.scale;
                    flipped = spinF < 0.f;
                }

                // Scale pop-in on all axes: 0 → 1.x → 1 during appear, full size after
                bool isAppearing = (!c.appeared && c.heightFrac < 1.f);
                float effScale = isAppearing ? c.heightFrac : c.scale;
                float dW = cardW * scaleX * (isAppearing ? effScale : 1.f);
                float dH = cardH * effScale;
                if (dW < 1.f) dW = 1.f;
                if (dH < 1.f) dH = 1.f;
                // Always centered
                float destX = cx - dW * 0.5f;
                float destY = cy - dH * 0.5f;
                Rectangle dest = { destX, destY, dW, dH };

                // Draw card texture
                int ri = c.rarity;
                if (ri >= 0 && ri < CARD_TEX_COUNT && cardTextures[ri].id > 0) {
                    float srcWtex = flipped ? -(float)cardTextures[ri].width : (float)cardTextures[ri].width;
                    float srcXtex = flipped ? (float)cardTextures[ri].width : 0.f;
                    DrawTexturePro(cardTextures[ri],
                        { srcXtex, 0.f, srcWtex, (float)cardTextures[ri].height },
                        dest, { 0, 0 }, 0.f, WHITE);
                } else {
                    Color rc = (ri>=0&&ri<6) ? rarityBorderCols[ri] : GRAY;
                    DrawRectangleRec(dest, rc);
                }

                // Item texture overlay: shadow + bobbing icon on top of card
                if (c.type != PU_NONE && effScale > 0.1f) {
                    Texture2D* iTex = GetItemTex(c.type);
                    if (iTex && iTex->id > 0) {
                        float bob = isAppearing ? 0.f : sinf((float)GetTime() * 2.f) * 3.f;
                        float pad = dW * 0.15f;
                        Rectangle itemArea = { destX + pad, destY + pad + bob, dW - pad*2.f, dH - pad*2.f };
                        Rectangle fitR = FitTexRect(itemArea, (float)iTex->width, (float)iTex->height);
                        Rectangle fitRShadow = { fitR.x + 4.f, fitR.y + 6.f, fitR.width, fitR.height };
                        Rectangle src = {0.f, 0.f, (float)iTex->width, (float)iTex->height};
                        // Shadow: black duplicate offset below-right
                        DrawTexturePro(*iTex, src, fitRShadow, {}, 0.f, {0,0,0,110});
                        // Item
                        DrawTexturePro(*iTex, src, fitR, {}, 0.f, WHITE);
                    }
                }

                // Hover highlight
                if (c.hovered && !c.dismissed && !c.selected)
                    DrawRectangleLinesEx(dest, 3, YELLOW);

                // Text OUTSIDE card — name above, desc+cd below
                if (effScale > 0.2f && !c.selected) {
                    float alpha = fminf((effScale - 0.2f) / 0.5f, 1.f);
                    unsigned char a = (unsigned char)(alpha * 255.f);
                    int nameFS = isShop ? 13 : 17;
                    int descFS = isShop ? 10 : 13;

                    // Name above the card
                    const char* nm = PU_INFO[(int)c.type].name;
                    int nw = MeasureText(nm, nameFS);
                    DrawText(nm, (int)(cx - nw * 0.5f), (int)(dest.y - nameFS - 6.f), nameFS, {255,255,255,a});

                    // Desc below the card
                    float belowY = dest.y + dH + 6.f;
                    const char* ds = PU_INFO[(int)c.type].desc;
                    int dw2 = MeasureText(ds, descFS);
                    DrawText(ds, (int)(cx - dw2 * 0.5f), (int)belowY, descFS, {210,210,210,a});

                    // CD info
                    const char* cd = PU_INFO[(int)c.type].cdInfo;
                    int cw = MeasureText(cd, descFS);
                    DrawText(cd, (int)(cx - cw * 0.5f), (int)(belowY + descFS + 3.f), descFS, {200,200,100,a});

                    // Cost (shop only)
                    if (isShop) {
                        const char* costStr = TextFormat("%dc", PU_INFO[(int)c.type].cost);
                        int costW = MeasureText(costStr, descFS);
                        Color costColor = (coins >= PU_INFO[(int)c.type].cost)
                            ? Color{100,255,100,a} : Color{255,80,80,a};
                        DrawText(costStr, (int)(cx - costW * 0.5f), (int)(belowY + (descFS + 3.f) * 2), descFS, costColor);
                    }
                }
            }

            // ── Hotbar (horizontal, bottom-center) ───────────────────────────
            DrawText("YOUR ITEMS", (int)(hbStartX2), (int)(hbSlotY2 - 22), 14, GRAY);
            for (int i = 0; i < 3; i++) {
                float slotX = hbStartX2 + i * (HB_W2 + HB_GAP2);
                const HotbarSlot& s = hotbar[i];
                Color bg = (i == hotbarSlot) ? Color{255,220,0,130} : Color{40,40,40,150};
                DrawRectangle((int)slotX, (int)hbSlotY2, (int)HB_W2, (int)HB_H2, bg);
                // Rarity-colored border
                Color slotBorder2 = DARKGRAY;
                if (s.type != PU_NONE) {
                    int ri = PU_INFO[(int)s.type].rarityIdx;
                    if (ri >= 0 && ri < 6) slotBorder2 = rarityBorderCols[ri];
                }
                DrawRectangleLinesEx({slotX, hbSlotY2, HB_W2, HB_H2},
                    (i == hotbarSlot) ? 3.f : 2.f,
                    i == hotbarSlot ? YELLOW : slotBorder2);
                DrawText(TextFormat("%d", i+1), (int)slotX + 4, (int)hbSlotY2 + 4, 12, WHITE);
                if (s.type != PU_NONE) {
                    // Item texture only — no card background
                    Texture2D* iTex = GetItemTex(s.type);
                    if (iTex && iTex->id > 0) {
                        float pad2 = 8.f;
                        Rectangle slotInner2 = { slotX+pad2, hbSlotY2+pad2, HB_W2-pad2*2.f, HB_H2-pad2*2.f };
                        Rectangle fitR2 = FitTexRect(slotInner2, (float)iTex->width, (float)iTex->height);
                        DrawTexturePro(*iTex,
                            {0.f, 0.f, (float)iTex->width, (float)iTex->height},
                            fitR2, {}, 0.f, WHITE);
                    }
                }
            }

            // Dragged item (follows mouse or falls)
            if (hbDrag.active) {
                int ri = PU_INFO[(int)hbDrag.type].rarityIdx;
                float dX = hbDrag.x - HB_W2 * 0.5f, dY = hbDrag.y - HB_H2 * 0.5f;
                Color dragBorder = (ri >= 0 && ri < 6) ? rarityBorderCols[ri] : WHITE;
                // Item texture for dragged card
                Texture2D* dTex = GetItemTex(hbDrag.type);
                if (dTex && dTex->id > 0) {
                    Rectangle dragInner = { dX, dY, HB_W2, HB_H2 };
                    Rectangle dragFit = FitTexRect(dragInner, (float)dTex->width, (float)dTex->height);
                    DrawTexturePro(*dTex,
                        {0.f, 0.f, (float)dTex->width, (float)dTex->height},
                        dragFit, {}, 0.f, {255,255,255,200});
                }
                else
                    DrawRectangle((int)dX, (int)dY, (int)HB_W2, (int)HB_H2, {120,80,200,200});
                DrawRectangleLinesEx({dX, dY, HB_W2, HB_H2}, 2.f, dragBorder);
                int nw = MeasureText(PU_INFO[(int)hbDrag.type].name, 9);
                DrawText(PU_INFO[(int)hbDrag.type].name, (int)(hbDrag.x - nw * 0.5f), (int)(hbDrag.y + HB_H2 * 0.5f - 12), 9, WHITE);
            }

            // Fade overlay
            if (cardFadeOut > 0.f) {
                unsigned char fa = (unsigned char)(fminf(cardFadeOut, 1.f) * 255.f);
                DrawRectangle(0, 0, screenWidth, screenHeight, { 0, 0, 0, fa });
            }
        }

        // ── Debug menu overlay ────────────────────────────────────────────────────
        if (dbgMenuOpen)
        {
            const int  DBG_X = 5, DBG_Y = 5, DBG_W = 235, DBG_H = 355;
            const int  DBG_TITLE_H = 22;
            const int  DBG_ROW_H   = 30;
            const int  DBG_PAD     = 4;
            const int  DBG_CONT_Y  = DBG_Y + DBG_TITLE_H;
            const int  DBG_CONT_H  = DBG_H - DBG_TITLE_H;

            Vector2 dbgMouse = GetMousePosition();
            float   mwheel   = GetMouseWheelMove();

            if (CheckCollisionPointRec(dbgMouse, { (float)DBG_X,(float)DBG_Y,(float)DBG_W,(float)DBG_H }))
                dbgMenuScroll -= mwheel * (float)DBG_ROW_H;

            const int numRows    = 16;
            const int totalContH = numRows * DBG_ROW_H + DBG_PAD * 2;
            int maxScroll  = totalContH - DBG_CONT_H;
            if (maxScroll < 0) maxScroll = 0;
            if (dbgMenuScroll < 0.f) dbgMenuScroll = 0.f;
            if (dbgMenuScroll > (float)maxScroll) dbgMenuScroll = (float)maxScroll;

            // Panel background
            DrawRectangle(DBG_X, DBG_Y, DBG_W, DBG_H, { 18, 18, 28, 230 });
            DrawRectangleLines(DBG_X, DBG_Y, DBG_W, DBG_H, { 90, 90, 140, 255 });

            // Title bar
            DrawRectangle(DBG_X, DBG_Y, DBG_W, DBG_TITLE_H, { 35, 35, 75, 255 });
            DrawText("DEBUG MENU", DBG_X + DBG_PAD + 2, DBG_Y + 4, 13, YELLOW);

            // Close [X] button
            Rectangle dbgCloseBtn = { (float)(DBG_X + DBG_W - 22), (float)(DBG_Y + 1), 21.f, (float)(DBG_TITLE_H - 2) };
            bool dbgCloseHov = CheckCollisionPointRec(dbgMouse, dbgCloseBtn);
            DrawRectangle((int)dbgCloseBtn.x,(int)dbgCloseBtn.y,(int)dbgCloseBtn.width,(int)dbgCloseBtn.height,
                dbgCloseHov ? Color{200,50,50,255} : Color{110,30,30,255});
            DrawText("X",(int)(dbgCloseBtn.x+5),(int)(dbgCloseBtn.y+3),13,WHITE);
            if (dbgCloseHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbgMenuOpen = false;

            // Clip content
            BeginScissorMode(DBG_X, DBG_CONT_Y, DBG_W, DBG_CONT_H);

            auto dbgRowY = [&](int r) -> int {
                return DBG_CONT_Y + DBG_PAD + r * DBG_ROW_H - (int)dbgMenuScroll;
            };
            auto dbgBtn = [&](Rectangle r, const char* txt, Color bg, Color fg) {
                Rectangle clip = { (float)DBG_X,(float)DBG_CONT_Y,(float)DBG_W,(float)DBG_CONT_H };
                bool hov = CheckCollisionPointRec(dbgMouse, r) && CheckCollisionPointRec(dbgMouse, clip);
                auto addC = [](unsigned char v, int a) -> unsigned char { int x=(int)v+a; return x>255?255:(x<0?0:(unsigned char)x); };
                Color bc = hov ? Color{addC(bg.r,50),addC(bg.g,50),addC(bg.b,50),bg.a} : bg;
                DrawRectangle((int)r.x,(int)r.y,(int)r.width,(int)r.height,bc);
                DrawRectangleLines((int)r.x,(int)r.y,(int)r.width,(int)r.height,{70,70,110,200});
                int tw = MeasureText(txt,10);
                DrawText(txt,(int)(r.x+r.width/2-tw/2),(int)(r.y+r.height/2-5),10,fg);
            };
            auto dbgClicked = [&](Rectangle r) -> bool {
                Rectangle clip = { (float)DBG_X,(float)DBG_CONT_Y,(float)DBG_W,(float)DBG_CONT_H };
                return IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
                    && CheckCollisionPointRec(dbgMouse,r)
                    && CheckCollisionPointRec(dbgMouse,clip);
            };

            // Row 0 – Lives
            { int ry = dbgRowY(0);
              DrawText("Lives:", DBG_X+DBG_PAD+2, ry+9, 11, WHITE);
              Rectangle mR = {(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR = {(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"<",{55,55,75,255},WHITE); dbgBtn(pR,">",{55,55,75,255},WHITE);
              DrawText(TextFormat("%d",lives),DBG_X+139,ry+9,12,YELLOW);
              if (dbgClicked(mR) && lives>0) lives--;
              if (dbgClicked(pR) && lives<maxLives) lives++; }

            // Row 1 – Max Lives
            { int ry = dbgRowY(1);
              DrawText("Max Lives:", DBG_X+DBG_PAD+2, ry+9, 11, WHITE);
              Rectangle mR = {(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR = {(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"<",{55,55,75,255},WHITE); dbgBtn(pR,">",{55,55,75,255},WHITE);
              DrawText(TextFormat("%d",maxLives),DBG_X+139,ry+9,12,YELLOW);
              if (dbgClicked(mR) && maxLives>1) { maxLives--; if(lives>maxLives) lives=maxLives; }
              if (dbgClicked(pR) && maxLives<10) maxLives++; }

            // Row 2 – Spawn Barrel
            { int ry = dbgRowY(2);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,"Spawn Barrel",{50,80,50,255},WHITE);
              if (dbgClicked(r) && currentScreen==GAMEPLAY) SpawnBarrelFromPool(barrels,barrelPath); }

            // Row 3 – Spawn Enemy
            { int ry = dbgRowY(3);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,"Spawn Enemy",{80,50,50,255},WHITE);
              if (dbgClicked(r) && currentScreen==GAMEPLAY) {
                  for (auto& en:enemies) if(!en.active){
                      en.active=true;
                      en.hitbox={player.x+(facingRight?80.f:-80.f),player.y-50.f,44.f,44.f};
                      en.velocity={0,0}; en.type=GRUNT; en.state=ES_IDLE;
                      en.stateTimer=0; en.grounded=false; en.facingRight=facingRight; break;
                  }
              } }

            // Row 4 – Summon Nuke
            { int ry = dbgRowY(4);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,"Summon Nuke",{80,70,25,255},WHITE);
              if (dbgClicked(r) && currentScreen==GAMEPLAY) nukes.push_back({{player.x,player.y-60.f},true}); }

            // Row 5 – Summon Beatrice
            { int ry = dbgRowY(5);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,"Summon Beatrice",{50,50,100,255},WHITE);
              if (dbgClicked(r) && currentScreen==GAMEPLAY) beatrices.push_back({{player.x,player.y-60.f},true}); }

            // Row 6 – Give Ability
            { int ry = dbgRowY(6);
              float bw = 22.f;
              Rectangle prevR = {(float)(DBG_X+DBG_PAD),(float)ry,bw,(float)(DBG_ROW_H-2)};
              Rectangle nextR = {(float)(DBG_X+DBG_W-DBG_PAD-bw),(float)ry,bw,(float)(DBG_ROW_H-2)};
              Rectangle giveR = {(float)(DBG_X+DBG_PAD+bw+2),(float)ry,(float)(DBG_W-DBG_PAD*2-bw*2-4),(float)(DBG_ROW_H-2)};
              dbgBtn(prevR,"<",{55,55,75,255},WHITE);
              dbgBtn(nextR,">",{55,55,75,255},WHITE);
              dbgBtn(giveR,PU_INFO[dbgGivePUIdx].name,{60,40,85,255},YELLOW);
              if (dbgClicked(prevR)) dbgGivePUIdx=(dbgGivePUIdx+PU_COUNT-1)%PU_COUNT;
              if (dbgClicked(nextR)) dbgGivePUIdx=(dbgGivePUIdx+1)%PU_COUNT;
              if (dbgClicked(giveR)&&currentScreen==GAMEPLAY) AddToHotbar((PowerupType)dbgGivePUIdx); }

            // Row 7 – Immortality
            { int ry = dbgRowY(7);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,TextFormat("Immortality: %s",dbgImmortal?"ON":"OFF"),
                  dbgImmortal?Color{0,115,0,255}:Color{75,0,0,255},WHITE);
              if (dbgClicked(r)) dbgImmortal=!dbgImmortal; }

            // Row 8 – Flight
            { int ry = dbgRowY(8);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,TextFormat("Flight: %s",dbgFlight?"ON":"OFF"),
                  dbgFlight?Color{0,70,160,255}:Color{30,30,80,255},WHITE);
              if (dbgClicked(r)) { dbgFlight=!dbgFlight; if(dbgFlight) dbgFlightNoCol=false; } }

            // Row 9 – No Clip
            { int ry = dbgRowY(9);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,TextFormat("No Clip: %s",dbgFlightNoCol?"ON":"OFF"),
                  dbgFlightNoCol?Color{0,105,145,255}:Color{30,30,80,255},WHITE);
              if (dbgClicked(r)) { dbgFlightNoCol=!dbgFlightNoCol; if(dbgFlightNoCol) dbgFlight=false; } }

            // Row 10 – Bloom ON/OFF
            { int ry = dbgRowY(10);
              Rectangle r = {(float)(DBG_X+DBG_PAD),(float)ry,(float)(DBG_W-DBG_PAD*2),(float)(DBG_ROW_H-2)};
              dbgBtn(r,TextFormat("Bloom: %s",dbgBloomEnabled?"ON":"OFF"),
                  dbgBloomEnabled?Color{80,0,160,255}:Color{30,30,80,255},WHITE);
              if (dbgClicked(r)) dbgBloomEnabled=!dbgBloomEnabled; }

            // Row 11 – Bloom Intensity
            { int ry = dbgRowY(11);
              DrawText("Bloom Inten:", DBG_X+DBG_PAD+2, ry+9, 10, WHITE);
              Rectangle mR={(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR={(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"-",{55,55,75,255},WHITE); dbgBtn(pR,"+",{55,55,75,255},WHITE);
              DrawText(TextFormat("%.1f",dbgBloomIntensity),DBG_X+134,ry+9,11,YELLOW);
              if (dbgClicked(mR)) dbgBloomIntensity=fmaxf(0.f,dbgBloomIntensity-0.1f);
              if (dbgClicked(pR)) dbgBloomIntensity=fminf(3.f,dbgBloomIntensity+0.1f); }

            // Row 12 – Bloom Threshold
            { int ry = dbgRowY(12);
              DrawText("Bloom Thr:", DBG_X+DBG_PAD+2, ry+9, 10, WHITE);
              Rectangle mR={(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR={(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"-",{55,55,75,255},WHITE); dbgBtn(pR,"+",{55,55,75,255},WHITE);
              DrawText(TextFormat("%.2f",dbgBloomThreshold),DBG_X+130,ry+9,11,YELLOW);
              if (dbgClicked(mR)) dbgBloomThreshold=fmaxf(0.f,dbgBloomThreshold-0.05f);
              if (dbgClicked(pR)) dbgBloomThreshold=fminf(1.f,dbgBloomThreshold+0.05f); }

            // Row 13 – Music Volume
            { int ry = dbgRowY(13);
              DrawText("Music Vol:", DBG_X+DBG_PAD+2, ry+9, 10, {180,220,255,255});
              Rectangle mR={(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR={(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"-",{55,55,75,255},WHITE); dbgBtn(pR,"+",{55,55,75,255},WHITE);
              DrawText(TextFormat("%.2f",volMusic),DBG_X+130,ry+9,11,{180,220,255,255});
              if (dbgClicked(mR)) { volMusic=fmaxf(0.f,volMusic-0.05f); SetMusicVolume(music,volMusic); }
              if (dbgClicked(pR)) { volMusic=fminf(1.f,volMusic+0.05f); SetMusicVolume(music,volMusic); } }

            // Row 14 – SFX Volume (gameplay sounds: death, hit, nuke, barrel jump, RBD)
            { int ry = dbgRowY(14);
              DrawText("SFX Vol:", DBG_X+DBG_PAD+2, ry+9, 10, {255,200,160,255});
              Rectangle mR={(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR={(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"-",{55,55,75,255},WHITE); dbgBtn(pR,"+",{55,55,75,255},WHITE);
              DrawText(TextFormat("%.2f",volSFX),DBG_X+130,ry+9,11,{255,200,160,255});
              if (dbgClicked(mR)) volSFX=fmaxf(0.f,volSFX-0.05f);
              if (dbgClicked(pR)) volSFX=fminf(4.f,volSFX+0.05f); }

            // Row 15 – UI Volume (card sounds)
            { int ry = dbgRowY(15);
              DrawText("UI Vol:", DBG_X+DBG_PAD+2, ry+9, 10, {200,255,200,255});
              Rectangle mR={(float)(DBG_X+112),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              Rectangle pR={(float)(DBG_X+172),(float)ry,22.f,(float)(DBG_ROW_H-2)};
              dbgBtn(mR,"-",{55,55,75,255},WHITE); dbgBtn(pR,"+",{55,55,75,255},WHITE);
              DrawText(TextFormat("%.2f",volUI),DBG_X+130,ry+9,11,{200,255,200,255});
              if (dbgClicked(mR)) volUI=fmaxf(0.f,volUI-0.05f);
              if (dbgClicked(pR)) volUI=fminf(1.f,volUI+0.05f); }

            EndScissorMode();

            // Scrollbar
            if (maxScroll > 0) {
                float ratio  = (float)DBG_CONT_H / (float)totalContH;
                float thumbH = (float)DBG_CONT_H * ratio;
                float thumbY = (float)DBG_CONT_Y + (dbgMenuScroll/(float)maxScroll)*((float)DBG_CONT_H - thumbH);
                DrawRectangle(DBG_X+DBG_W-6, DBG_CONT_Y, 5, DBG_CONT_H, {35,35,55,200});
                DrawRectangle(DBG_X+DBG_W-6,(int)thumbY,5,(int)thumbH,{110,110,195,255});
            }
        }

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    UnloadMusicStream(music);
    UnloadSound(deathSound);
    UnloadSound(HitSound);
    UnloadSound(nukeSound);
    UnloadSound(jumpBrlSound);
    UnloadSound(rbdSound);
    UnloadSound(sndShield);
    UnloadSound(sndDash);
    UnloadSound(sndBeatrice);
    UnloadSound(sndBeatriceAtk);
    UnloadSound(sndLarper);
    UnloadSound(sndSpeedrun);
    for (int wi = 0; wi < 3; wi++) UnloadSound(sndWhip[wi]);
    UnloadSound(sndEnemyKill);
    UnloadSound(sndExtraLife);
    UnloadSound(sndOneLarp);
    UnloadSound(sndError);
    UnloadSound(sndReinhard);
    UnloadSound(sndBunnyJump);
    UnloadSound(sndMoney);
    for (int mi = 1; mi < MAT_COUNT; mi++) {
        for (auto& s : footsteps[mi].walk) UnloadSound(s);
        for (auto& s : footsteps[mi].run)  UnloadSound(s);
    }
    UnloadSound(cardFanSnd);
    UnloadSound(cardSlideSnd);
    UnloadSound(cardHoverSnd);
    UnloadSound(cardPickSnd);
    UnloadSound(cardThrowSnd);

    UnloadTexture(imgMarioIdle);      UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);     UnloadTexture(imgMarioJump);
    UnloadTexture(imgMarioFalling);   UnloadTexture(RopeTex);
    UnloadTexture(imgMarioClimb1);    UnloadTexture(imgMarioClimb2);
    UnloadTexture(imgMarioClimbEnd1); UnloadTexture(imgMarioClimbEnd2);
    UnloadTexture(imgMarioClimbDown); UnloadTexture(background);
    UnloadTexture(beam);
    for (int i = 0; i < 12; i++) if (beamVariants[i].id > 0) UnloadTexture(beamVariants[i]);
    UnloadTexture(LadderPart);
    UnloadTexture(BarrelMov1);        UnloadTexture(BarrelMov2);
    UnloadTexture(BarrelMov3);        UnloadTexture(BarrelMov4);
    UnloadTexture(BarrelFall1);       UnloadTexture(BarrelFall2);
    UnloadTexture(BlueBarrelMov1);    UnloadTexture(BlueBarrelMov2);
    UnloadTexture(BlueBarrelMov3);    UnloadTexture(BlueBarrelMov4);
    UnloadTexture(BlueBarrelFall1);   UnloadTexture(BlueBarrelFall2);
    UnloadTexture(House1);            UnloadTexture(House2);
    UnloadTexture(SnowFloor);         UnloadTexture(Nuke);
    UnloadTexture(imgMarioIdleNuke);  UnloadTexture(imgMarioWalk1Nuke);
    UnloadTexture(imgMarioWalk2Nuke); UnloadTexture(imgMarioJumpNuke);
    UnloadTexture(Explosion1); UnloadTexture(Explosion2); UnloadTexture(Explosion3);
    UnloadTexture(Explosion4); UnloadTexture(Explosion5); UnloadTexture(Explosion6);
    UnloadTexture(cave1);  UnloadTexture(cave2);  UnloadTexture(cave3);
    UnloadTexture(cave4);  UnloadTexture(cave5);  UnloadTexture(cave6);
    UnloadTexture(cave7);  UnloadTexture(cave8);  UnloadTexture(cave9);
    UnloadTexture(cave10); UnloadTexture(cave11);
    UnloadTexture(Rain);   UnloadTexture(Rain2);
    UnloadTexture(RegulusGrab1);     UnloadTexture(RegulusGrab2);     UnloadTexture(RegulusGrab3);
    UnloadTexture(RegulusIdle1);     UnloadTexture(RegulusIdle2);     UnloadTexture(RegulusIdle3);
    UnloadTexture(Regulus_Stun1);    UnloadTexture(Regulus_Stun2);    UnloadTexture(Regulus_Stun3);
    UnloadTexture(Regulus_StunEnd1); UnloadTexture(Regulus_StunEnd2); UnloadTexture(Regulus_StunEnd3);
    UnloadTexture(Regulus_StunEnd4); UnloadTexture(Regulus_StunEnd5);
    UnloadTexture(Dk_Mario_Idle1_Beatrice); UnloadTexture(Dk_Mario_Idle2_Beatrice);
    UnloadTexture(Dk_Mario_Jump_Beatrice);
    UnloadTexture(Dk_Mario_Walk1_Beatrice); UnloadTexture(Dk_Mario_Walk2_Beatrice);
    UnloadTexture(Beatrice_Idle1);          UnloadTexture(Beatrice_Idle2);
    UnloadTexture(texBeaBullet);
    UnloadTexture(Subaru1); UnloadTexture(Subaru2); UnloadTexture(Subaru3);
    UnloadTexture(Subaru4); UnloadTexture(Subaru5); UnloadTexture(Subaru_Background);
    UnloadTexture(rabbitIdleBlack); UnloadTexture(rabbitJumpBlack);
    UnloadTexture(rabbitIdleWhite); UnloadTexture(rabbitJumpWhite);
    UnloadTexture(FButton);
    UnloadTexture(texGoldHeart); UnloadTexture(texHeart);
    UnloadTexture(texShield1);   UnloadTexture(texShield2);
    for (int i = 0; i < PROP_TEX_COUNT; i++)
        if (propTextures[i].id > 0) UnloadTexture(propTextures[i]);
    if (propFireFrame2.id > 0) UnloadTexture(propFireFrame2);
    for (int i = 0; i < CARD_TEX_COUNT; i++) UnloadTexture(cardTextures[i]);
    if (texCoin.id > 0) UnloadTexture(texCoin);
    UnloadTexture(itemTex_ReturnByDeath); UnloadTexture(itemTex_Dash);
    UnloadTexture(itemTex_Reinhard);      UnloadTexture(itemTex_Whip);
    UnloadTexture(itemTex_Speedrun);      UnloadTexture(itemTex_Shield);
    UnloadTexture(itemTex_Shop);
    UnloadTexture(whipTex1); UnloadTexture(whipTex2);

    UnloadRenderTexture(ladderLayer);
    UnloadRenderTexture(staticLayer);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}