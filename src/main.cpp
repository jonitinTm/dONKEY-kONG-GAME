#include "raylib.h"
#include "raymath.h"
#include "Collision.h"
#include "Ladder.h"
#include "LevelData.h"
#include "LevelEditor.h"
#include "CinematicPlayer.h"
#include <ctime>

enum GameScreen { SPLASH_SCREEN = 0, SPLASH_SCREEN2, MENU, CONTROLS, GAMEPLAY, GAME_OVER, HOW_HIGH, LEVEL_EDITOR };

static constexpr float DEATH_FLASH_DURATION = 0.40f;
static constexpr float DEATH_FADE_DURATION = 1.10f;
static constexpr float DEATH_TOTAL_FALL = DEATH_FLASH_DURATION + DEATH_FADE_DURATION;
static constexpr float DEATH_BLACK_HOLD = 0.50f;
static constexpr float DEATH_FALL_GRAVITY = 0.50f;
static constexpr float DEATH_FALL_INITIAL_VY = -5.0f;

static constexpr float DEATH_SHAKE_DURATION = 0.80f;
static constexpr float DEATH_SHAKE_AMOUNT = 9.0f;

static constexpr float ACTIVE_SPAWN_INTERVAL = 1.2f;

struct PathNode
{
    Vector2 pos;
    int     next[2] = { -1, -1 };
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
    bool      reachedEnd = false;   // true when barrel dies at terminal node
};

struct NukeItem
{
    Vector2 pos;
    bool    active = true;
};

struct FlyingNuke
{
    Rectangle rect = { 0, 0, 0, 0 };
    Vector2   vel = { 0, 0 };
    bool      active = false;
};

struct BeatriceItem
{
    Vector2 pos;
    bool    active = false;
};

struct BeaBullet
{
    Vector2 pos = { 0, 0 };
    Vector2 vel = { 0, 0 };
    float   lifetime = 0.0f;
    bool    active = false;
};


//Struct enemies
// ── Enemy types ───────────────────────────────────────────────────────────────
enum EnemyType { GRUNT = 0, SPECTER };
enum EnemyState { ES_IDLE, ES_JUMP_TOWARD, ES_LAND_PAUSE, ES_JUMP_BACK };

struct Enemy {
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
};



// ─────────────────────────────────────────────────────────────────────────────
Barrel SpawnBarrel(const vector<PathNode>& path, int startNode = 0,
    float spd = 4.0f, float w = 26.25f, float h = 26.25f)
{
    Barrel b;
    if (path.empty()) { b.active = false; return b; }   // no path → dead barrel
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

bool SpawnBarrelFromPool(vector<Barrel>& barrels, const vector<PathNode>& path,
    float spd = 4.0f, float w = 26.25f, float h = 26.25f,
    bool forceBlue = false)
{
    if (path.empty()) return false;   // no path defined for this level
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

void UpdateBarrel(Barrel& b, const vector<PathNode>& path, float delta)
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

        bool bothValid = (node.next[0] != -1 && node.next[1] != -1);
        bool oneValid = (node.next[0] != -1 || node.next[1] != -1);

        if (bothValid)
        {
            int roll = GetRandomValue(0, 9);
            int choice = (roll < node.rollThreshold) ? 0 : 1;
            if (node.isSplitNode) { b.isFalling = (choice == 0); b.animFrame = 0; }
            b.currentNode = node.next[choice];
        }
        else if (oneValid)
        {
            int nextNode = (node.next[0] != -1) ? node.next[0] : node.next[1];
            if (node.isSplitNode) { b.isFalling = false; b.animFrame = 0; }
            b.currentNode = nextNode;
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

//Funcion para updatear los enemigos


void UpdateEnemy(Enemy& e, const Rectangle& playerRect,
    vector<Platform>& platforms, float delta)
{
    if (!e.active) return;

    // Parámetros por tipo
    float jumpForce = (e.type == GRUNT) ? -5.5f : -5.0f;//cambio altura de salto//cambio altura de salto
    float speedToward = (e.type == GRUNT) ? 4.0f : 4.0f; //cambio tiempo en el aire
    float speedBack = (e.type == GRUNT) ? 1.8f : 2.5f;
    float idleTime = (e.type == GRUNT) ? 2.0f : 1.65f;
    float pauseTime = (e.type == GRUNT) ? 1.45f : 0.28f;
    float animSpeed = (e.type == GRUNT) ? 0.22f : 0.14f;

    float playerCX = playerRect.x + playerRect.width * 0.5f;
    float enemyCX = e.hitbox.x + e.hitbox.width * 0.5f;
    bool  playerRight = (playerCX > enemyCX);

    // ── Máquina de estados ────────────────────────────────────────────────
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
            e.velocity.y = jumpForce * 0.55f;         // salto pequeño atrás
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

    // ── Física ───────────────────────────────────────────────────────────
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

    if (e.hitbox.y > 1050.0f) e.active = false;   // cayó fuera de pantalla

    // ── Animación ─────────────────────────────────────────────────────────
    e.animTimer += delta;
    if (e.animTimer >= animSpeed)
    {
        e.animTimer = 0.0f;
        e.animFrame = (e.animFrame + 1) % 2;
    }
}





//Dibujarlo en la pantallla

void DrawEnemy(const Enemy& e,
    Texture2D& walkGrunt,   // textura andar  del conejo negro (GRUNT)
    Texture2D& jumpGrunt,   // textura saltar del conejo negro (GRUNT)
    Texture2D& walkSpecter, // textura andar  del conejo blanco (SPECTER)
    Texture2D& jumpSpecter) // textura saltar del conejo blanco (SPECTER)
{
    // Si el enemigo no está activo no dibujamos nada
    if (!e.active) return;

    // ── Elegir qué textura usar ───────────────────────────────────────────
    Texture2D* tex = nullptr;

    if (e.type == GRUNT)
    {
        // Conejo negro: si está en el aire usa la de salto, si no la de andar
        if (!e.grounded)
            tex = &jumpGrunt;
        else
            tex = &walkGrunt;
    }
    else // SPECTER
    {
        // Conejo blanco: igual pero con sus propias texturas
        if (!e.grounded)
            tex = &jumpSpecter;
        else
            tex = &walkSpecter;
    }

    // ── Tamaño en pantalla ────────────────────────────────────────────────
    float scale = 2.5f;                    // cuánto agrandar la textura (3x su tamaño original)
    float drawW = tex->width * scale;     // ancho  final en pantalla
    float drawH = tex->height * scale;     // altura final en pantalla

    // ── Posición: centrado horizontalmente, pies tocando el suelo del hitbox
    float drawX = e.hitbox.x + e.hitbox.width * 0.5f - drawW * 0.5f;
    float drawY = e.hitbox.y + e.hitbox.height - drawH;

    // ── Voltear la imagen si mira a la izquierda ──────────────────────────
    // src es el "recorte" de la textura original que vamos a dibujar
    Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };

    if (!e.facingRight)
        src.width *= -1; // ancho negativo = raylib espeja la imagen horizontalmente

    // ── Dibujar ───────────────────────────────────────────────────────────
    // DrawTexturePro(textura, recorte_origen, rectangulo_destino, origen_rotacion, angulo, color)
    DrawTexturePro(*tex, src, { drawX, drawY, drawW, drawH }, {}, 0.f, WHITE);
}












void DrawBarrelPathDebug(const vector<PathNode>& path, const vector<Barrel>& barrels, int screenHeight)
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
        Color fill;
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
    DrawCircleV({ (float)(lx + 5),(float)(ly + 20) }, 5, WHITE);  DrawText("Start", lx + 14, ly + 15, 9, WHITE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 34) }, 5, GREEN);  DrawText("Split 50/50", lx + 14, ly + 29, 9, GREEN);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 48) }, 5, ORANGE); DrawText("Edge (fall)", lx + 14, ly + 43, 9, ORANGE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 62) }, 5, YELLOW); DrawText("Obligatory", lx + 14, ly + 57, 9, YELLOW);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 76) }, 5, RED);    DrawText("End", lx + 14, ly + 71, 9, RED);
    DrawLineEx({ (float)lx,(float)(ly + 90) }, { (float)(lx + 18),(float)(ly + 90) }, 2, { 255,140,0,255 }); DrawText("Stair/Fall", lx + 22, ly + 85, 9, { 255,140,0,255 });
    DrawLineEx({ (float)lx,(float)(ly + 104) }, { (float)(lx + 18),(float)(ly + 104) }, 2, { 0,200,255,255 }); DrawText("Flat/Roll", lx + 22, ly + 99, 9, { 0,200,255,255 });
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
    Rectangle btnPlay = { 340, 450, 200, 40 };
    Rectangle btnExit = { 340, 500, 200, 40 };
    Rectangle btnCtrl = { 340, 550, 200, 40 };
    Rectangle btnEditor = { 340, 600, 200, 40 };

    bool      debugPath = false;

    int score = 0;
    int currentLevelId = 1;   // which level is currently loaded / playing

    Rectangle wincondition = { 400, 150, 40, 40 };

    // ── Dynamic player spawn (updated by level data) ──────────────────────────
    float playerSpawnX = 35.0f + 64.0f * 3.5f + 10.0f;
    float playerSpawnY = 817.0f;

    LevelEditor editor;

    // ── Subaru animation state ────────────────────────────────────────────────
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

    int         lives = 3;
    bool        death = false;
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
    bool  ladderEntryClamp = false;
    float ladderEntryClampTimer = 0.0f;
    float ladderEntryClampStart = 0.0f;

    // ── Death sequence state ──────────────────────────────────────────────────
    bool  isDying = false;
    float deathTimer = 0.0f;
    bool  hitPlayed = false;
    bool  deathPlayed = false;
    float deathFallVelY = 0.0f;
    bool  deathReachedBlack = false;
    float deathBlackTimer = 0.0f;
    float   deathShakeTimer = 0.0f;
    Vector2 deathShakeOffset = { 0, 0 };

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

    // ── Nuke state ────────────────────────────────────────────────────────────
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

    // ── Regulus state ─────────────────────────────────────────────────────────
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
    // ── Elevators & parent-child relations ─────────────────────────────────
    vector<ElevatorData>        liveElevators;
    vector<ParentChildRelation> liveRelations;
    vector<float>               elevChildPhases;
    vector<KillZoneData>        liveKillZones;

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
        Ladder::Make(675, 245,  40, 104),
        Ladder::Make(160, 375,  40, 102),
        Ladder::Make(300, 365,  40, 117),
        Ladder::Make(680, 495,  40, 110),
        Ladder::Make(430, 489,  40, 128),
        Ladder::Make(380, 621,  40, 124),
        Ladder::Make(160, 632,  40, 101),
        Ladder::Make(670, 760,  40, 105),
    };



    //enemies

    // ── Enemies ───────────────────────────────────────────────────────────────
    vector<Vector2> enemySpawnPositions = {
        { 180.0f, 706.0f }, { 620.0f, 706.0f },   // Floor 1  (plataforma y=750)
        { 200.0f, 576.0f }, { 550.0f, 576.0f },   // Floor 2  (plataforma y=620)
        { 160.0f, 446.0f }, { 520.0f, 446.0f },   // Floor 3  (plataforma y=490)
        { 200.0f, 316.0f }, { 500.0f, 316.0f },   // Floor 4  (plataforma y=360)
    };

    vector<Enemy> enemies(8);
    for (auto& en : enemies) en.active = false;

    // Lambda que inicializa/respawnea los dos enemigos en posiciones aleatorias
    auto SpawnRandomEnemies = [&]()
        {
            for (auto& en : enemies) en.active = false;
            if (enemySpawnPositions.size() < 2) return;   // not enough spawn points

            int idx1 = GetRandomValue(0, (int)enemySpawnPositions.size() - 1);
            int idx2;
            do { idx2 = GetRandomValue(0, (int)enemySpawnPositions.size() - 1); } while (idx2 == idx1);

            auto initEnemy = [](Enemy& en, Vector2 pos, EnemyType t, float w, float h, float timerOffset)
                {
                    en.hitbox = { pos.x, pos.y, w, h };
                    en.type = t;
                    en.state = ES_IDLE;
                    en.stateTimer = timerOffset;   // desfase para que no se sincronicen
                    en.velocity = { 0.0f, 0.0f };
                    en.grounded = true;
                    en.animFrame = 0;
                    en.animTimer = 0.0f;
                    en.facingRight = true;
                    en.active = true;
                };

            // hitbox is 70% of original; DrawEnemy stays centred on it automatically
            initEnemy(enemies[0], enemySpawnPositions[idx1], GRUNT, 30.8f, 30.8f, 0.0f);
            initEnemy(enemies[1], enemySpawnPositions[idx2], SPECTER, 26.6f, 35.0f, 0.4f);
        };

    // enemies now spawn only when barrels reach the end node







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

    float spawnTimer = 0.0f;
    float spawnInterval = 10.0f;
    float minuteTimer = 0.0f;

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
    vector<Vector2> beamPositions = {
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

    // ── Initial Regulus throw ─────────────────────────────────────────────────
    regulusThrowing = true;
    regulusSpawnPending = true;
    regulusForceBlue = true;
    regulusThrowFrame = 0;
    regulusThrowTimer = 0.0f;

    // ── Window / audio / textures ─────────────────────────────────────────────
    InitWindow(screenWidth, screenHeight, "Donkey Kong");
    editor.Init(screenWidth, screenHeight);
    Cinematic::Global.LoadAll();   // load all saved sequences from Cinematics/
    SetRandomSeed((unsigned int)time(NULL));   // truly random each run
    InitAudioDevice();

    TraceLog(LOG_INFO, TextFormat("Working Directory: %s", GetWorkingDirectory()));

    Music music = LoadMusicStream("Assets/Nuevo audio/mp3/PerfectLoopSubaru.mp3");
    Sound deathSound = LoadSound("Assets/Nuevo audio/mp3/20. Dead.mp3");
    Sound HitSound = LoadSound("Assets/Nuevo audio/mp3/19. Bonus.mp3");
    Sound nukeSound = LoadSound("Assets/Nuevo audio/mp3/Flash.mp3");
    Sound jumpBrlSound = LoadSound("Assets/Nuevo audio/mp3/19. Bonus.mp3");

    SetMasterVolume(1.0f);
    SetMusicVolume(music, 1.0f);
    SetMusicPan(music, 0.0f);
    PlayMusicStream(music);

    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Texture2D imgMarioFalling = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Falling.png");
    Texture2D background = LoadTexture("Wiki/SubaruStairs.png");
    Texture2D beam = LoadTexture("Assets/Textures/Architecture/Dk_FloorPart.png");
    Texture2D imgMarioClimb1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder1.png");
    Texture2D imgMarioClimb2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder2.png");
    Texture2D imgMarioClimbEnd1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd1.png");
    Texture2D imgMarioClimbEnd2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd2.png");
    Texture2D imgMarioClimbDown = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_IdleBack.png");
    Texture2D LadderPart = LoadTexture("Assets/Textures/Architecture/Dk_Ladder.png");
    Texture2D RopeTex = LoadTexture("Assets/Textures/Architecture/Rope.png");
    Texture2D GoldenPistonTex = LoadTexture("Assets/Textures/Items/GoldenPiston.png");
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

    // ── Regulus textures ──────────────────────────────────────────────────────
    Texture2D RegulusGrab1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab1.png");
    Texture2D RegulusGrab2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab2.png");
    Texture2D RegulusGrab3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Grab3.png");
    Texture2D RegulusIdle1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle1.png");
    Texture2D RegulusIdle2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle2.png");
    Texture2D RegulusIdle3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Idle3.png");
    Texture2D RegulusStairs1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stairs1.png");
    Texture2D RegulusStairs2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stairs2.png");
    Texture2D Regulus_Stun1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun1.png");
    Texture2D Regulus_Stun2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun2.png");
    Texture2D Regulus_Stun3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_Stun3.png");
    Texture2D Regulus_StunEnd1 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd1.png");
    Texture2D Regulus_StunEnd2 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd2.png");
    Texture2D Regulus_StunEnd3 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd3.png");
    Texture2D Regulus_StunEnd4 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd4.png");
    Texture2D Regulus_StunEnd5 = LoadTexture("Assets/Textures/Characters/Regulus/Regulus_StunEnd5.png");

    // ── Beatrice / Mario-with-Beatrice textures ───────────────────────────────
    Texture2D Dk_Mario_Idle1_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1_Beatrice.png");
    Texture2D Dk_Mario_Idle2_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle2_Beatrice.png");
    Texture2D Dk_Mario_Jump_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump_Beatrice.png");
    Texture2D Dk_Mario_Walk1_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1_Beatrice.png");
    Texture2D Dk_Mario_Walk2_Beatrice = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2_Beatrice.png");
    Texture2D Beatrice_Idle1 = LoadTexture("Assets/Textures/Characters/Beatrice/Beatrice_Idle1.png");
    Texture2D Beatrice_Idle2 = LoadTexture("Assets/Textures/Characters/Beatrice/Beatrice_Idle2.png");
    Texture2D texBeaBullet = LoadTexture("Assets/Textures/Characters/Beatrice/BeaBullet.png");

    // ── Subaru textures ───────────────────────────────────────────────────────
    Texture2D Subaru1 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru1.png");
    Texture2D Subaru2 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru2.png");
    Texture2D Subaru3 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru3.png");
    Texture2D Subaru4 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru4.png");
    Texture2D Subaru5 = LoadTexture("Assets/Textures/Characters/Subaru/Subaru5.png");
    Texture2D Subaru_Background = LoadTexture("Assets/Textures/Characters/Subaru/Subaru_Background.png");

    // ──Enemies─────────────────────────────────────────────
    //black rabbit
    Texture2D rabbitWalkBlack = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Blue1.png");
    Texture2D rabbitJumpBlack = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Jump_Blue1.png");
    //White rabbit---------
    Texture2D rabbitWalkWhite = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite1.png");
    Texture2D rabbitJumpWhite = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_FireSprite_Jump1.png");


    Texture2D EButton = LoadTexture("Assets/Textures/UI/EButton.png");

    // Array for easy indexed access
    Texture2D* subaruFrames[SUBARU_FRAME_COUNT] = {
        &Subaru1, &Subaru2, &Subaru3, &Subaru4, &Subaru5
    };

    Texture2D* regulusIdleFrames[3] = { &RegulusIdle1,  &RegulusIdle2,  &RegulusIdle3 };
    Texture2D* regulusThrowFrames[3] = { &RegulusGrab1,  &RegulusGrab2,  &RegulusGrab3 };
    Texture2D* regulusStunFrames[3] = { &Regulus_Stun1, &Regulus_Stun2, &Regulus_Stun3 };
    Texture2D* regulusStunEndFrames[5] = { &Regulus_StunEnd1, &Regulus_StunEnd2, &Regulus_StunEnd3,
                                           &Regulus_StunEnd4, &Regulus_StunEnd5 };

    const int NUKE_EXPL_FRAME_COUNT = 6;
    Texture2D* explosionFrames[NUKE_EXPL_FRAME_COUNT] = {
        &Explosion1,&Explosion2,&Explosion3,
        &Explosion4,&Explosion5,&Explosion6
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
    for (auto& pos : beamPositions)
        DrawTexturePro(beam,
            { 0, 0, (float)beam.width, (float)beam.height },
            { pos.x, pos.y, beam.width * beamScale, beam.height * beamScale },
            { 0, 0 }, 0.f, WHITE);
    EndTextureMode();
    // NOTE: beam texture is kept alive so RebuildLayers can rebake staticLayer

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
            DrawTexturePro(LadderPart,
                { 0, 0, 16.0f, 16.0f },
                { drawX, y, ladderTileW, ladderTileH },
                { 0, 0 }, 0.f, WHITE);
            y += ladderTileH;
        }
    }
    EndTextureMode();
    // NOTE: LadderPart texture is kept alive so RebuildLayers can rebake ladderLayer

    // ── Wire textures into the editor ─────────────────────────────────────────
    editor.SetGameTextures(&background, &beam, &LadderPart,
        &imgMarioIdle, &RegulusIdle1, &House1, &RopeTex, &GoldenPistonTex);

    // ── RebuildLayers: rebakes staticLayer + ladderLayer from current data ────
    auto RebuildLayers = [&]()
        {
            // Beam layer — elevator-child beams are excluded and drawn live instead
            float bScale = 4.0f;
            BeginTextureMode(staticLayer);
            ClearBackground(BLANK);
            for (int bi = 0; bi < (int)beamPositions.size(); bi++)
            {
                bool isElevChild = false;
                for (const auto& rel : liveRelations)
                    if (rel.parent.type == 11 && rel.child.type == 6 && rel.child.index == bi)
                    {
                        isElevChild = true; break;
                    }
                if (isElevChild) continue;
                const auto& pos = beamPositions[bi];
                DrawTexturePro(beam,
                    { 0, 0, (float)beam.width, (float)beam.height },
                    { pos.x, pos.y, beam.width * bScale, beam.height * bScale },
                    { 0, 0 }, 0.f, WHITE);
            }
            EndTextureMode();

            // Ladder layer (uniform tiling) — elevator-child ladders are excluded
            // and drawn live in the gameplay loop instead, since their position
            // changes every frame.
            float lScale = 4.0f;
            float tileW = 16.f * lScale;
            float tileH = 16.f * lScale;
            BeginTextureMode(ladderLayer);
            ClearBackground(BLANK);
            for (int li = 0; li < (int)ladders.size(); li++)
            {
                // Skip ladders parented to an elevator — drawn live, not baked
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
                    DrawTexturePro(LadderPart,
                        { 0, 0, 16.f, srh },
                        { drawX, y, tileW, dh },
                        { 0, 0 }, 0.f, WHITE);
                }
            }
            EndTextureMode();
        };

    // ── ApplyLevelData: push a LevelData into all live game vectors ───────────
    auto ApplyLevelData = [&](const LevelData& lv)
        {
            // Platforms
            platforms.clear();
            for (const auto& pd : lv.platforms)
                platforms.push_back(Platform::Make(pd.x, pd.y, pd.w, pd.h, pd.tilt));

            // Ladders
            ladders.clear();
            for (const auto& ld : lv.ladders)
                ladders.push_back(Ladder::Make(ld.x, ld.y, ld.w, ld.h));

            // Barrel path
            barrelPath.clear();
            for (const auto& nd : lv.pathNodes) {
                PathNode n;
                n.pos = { nd.x, nd.y };
                n.next[0] = nd.next[0];
                n.next[1] = nd.next[1];
                n.rollThreshold = nd.rollThreshold;
                n.isSplitNode = nd.isSplitNode;
                barrelPath.push_back(n);
            }

            // Spawn nodes
            nukeSpawnNodes.clear();
            for (const auto& v : lv.nukeSpawns)     nukeSpawnNodes.push_back(v);
            beatriceSpawnNodes.clear();
            for (const auto& v : lv.beatriceSpawns)  beatriceSpawnNodes.push_back(v);
            enemySpawnPositions.clear();
            for (const auto& v : lv.enemySpawns)     enemySpawnPositions.push_back(v);

            // Beam positions + rebuild baked layers
            beamPositions.clear();
            for (const auto& v : lv.beams)           beamPositions.push_back(v);

            // Cave / house position
            if (lv.hasCave) {
                houseX = lv.cavePos.x;
                houseY = lv.cavePos.y;
                houseHitbox = { houseX, houseY, houseW, houseH };
            }

            // Player spawn
            if (lv.hasPlayerSpawn) {
                playerSpawnX = lv.playerSpawn.x;
                playerSpawnY = lv.playerSpawn.y;
            }

            // Elevators + relations
            liveElevators = lv.elevators;
            liveRelations = lv.relations;
            // Build phase vector: each relation gets an initial phase from offsetY
            elevChildPhases.resize(liveRelations.size());
            for (int ri = 0; ri < (int)liveRelations.size(); ri++)
                elevChildPhases[ri] = liveRelations[ri].offsetY;

            // Rebuild render textures with new data
            RebuildLayers();

            // Win zone
            if (lv.hasWinZone)
                wincondition = { lv.winZone.x, lv.winZone.y, lv.winZone.w, lv.winZone.h };

            // Kill zones
            liveKillZones.clear();
            for (const auto& kz : lv.killZones)
                liveKillZones.push_back(kz);
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

    // ── Helper: respawn items on death ────────────────────────────────────────
    auto RespawnItems = [&]()
        {
            if (nukeRespawnNodes.empty()) return;
            if (GetRandomValue(1, 100) <= 30)
            {
                int idx = GetRandomValue(0, (int)nukeRespawnNodes.size() - 1);
                bool blocked = false;
                for (const auto& nk : nukes)
                    if (nk.active && Vector2Distance(nk.pos, nukeRespawnNodes[idx]) < 50.0f)
                        blocked = true;
                if (!blocked)
                    nukes.push_back({ nukeRespawnNodes[idx], true });
            }
        };

    // ── Helper lambdas ────────────────────────────────────────────────────────
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

    auto ClearRoundEntities = [&]()
        {
            for (auto& b : barrels)     b.active = false;
            for (auto& bb : beaBullets)  bb.active = false;
            for (auto& en : enemies)     en.active = false;
            for (auto& fn : flyingNukes) fn.active = false;
            flyingNukes.clear();
            spawnTimer = 0.0f;
            playerHasNuke = false;
            playerHasBeatrice = false;
            beatriceAbilityTimer = 0.0f;
            beaBulletShootTimer = 0.0f;
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
            PauseMusicStream(music);
            ResetRegulus();
            RespawnItems();
        };

    //resets

    auto ResetRound = [&]()
        {
            ClearDeathState();
            ClearRoundEntities();
            ResetPlayerPos();
            ResetRegulus();
            invincible = true;
            invincibleTimer = invincibleDuration;
            ResumeMusicStream(music);
        };

    auto FullReset = [&]()
        {
            // Reload level 1 data if we've moved past it
            if (currentLevelId != 1) {
                currentLevelId = 1;
                LevelData lv1;
                if (LoadLevel(lv1, 1)) ApplyLevelData(lv1);
                else                   ApplyLevelData(GetDefaultLevel1());
            }
            ClearDeathState();
            ClearRoundEntities();
            ResetPlayerPos();
            ResetRegulus();

            lives = 3;
            death = false;
            invincible = false;
            invincibleTimer = 0.0f;
            score = 0;
            spawnInterval = 10.0f;
            minuteTimer = 0.0f;
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

            // Reset Subaru animation
            subaruFrame = 0;
            subaruTimer = 0.0f;

            ResumeMusicStream(music);
        };

    // ── Spawn one enemy at the cave end when a barrel completes its path ─────
    auto SpawnEnemyAtEnd = [&](float barrelCX)
        {
            for (auto& en : enemies)
            {
                if (en.active) continue;
                EnemyType t = (GetRandomValue(0, 1) == 0) ? GRUNT : SPECTER;
                float     hw = (t == GRUNT) ? 30.8f : 26.6f;
                float     hh = (t == GRUNT) ? 30.8f : 35.0f;
                // feet flush with the bottom platform (y = 880)
                en.hitbox = { barrelCX - hw * 0.5f, 880.0f - hh, hw, hh };
                en.type = t;
                en.state = ES_IDLE;
                en.stateTimer = 0.0f;
                en.velocity = { 0.0f, 0.0f };
                en.grounded = true;
                en.animFrame = 0;
                en.animTimer = 0.0f;
                en.facingRight = (GetRandomValue(0, 1) == 1);
                en.active = true;
                break;
            }
        };

    // ── Cinematic player apply callback — patches live game objects directly ──
    // Call CinematicSequencer::Play("Name") anywhere to trigger a sequence.
    Cinematic::Global.SetApplyCallback([&](const CinematicEntityState& st) {
        int i = st.entityIndex;
        switch (st.entityType) {
        case 4: // PLATFORM — rebuild from cinematic state
            if (i >= 0 && i < (int)platforms.size())
                platforms[i] = Platform::Make(st.x, st.y,
                    st.width > 0.f ? st.width : 128.f,
                    0, st.tilt);
            break;
        case 5: // LADDER
            if (i >= 0 && i < (int)ladders.size())
                ladders[i] = Ladder::Make(st.x, st.y,
                    ladders[i].width,
                    st.height > 0.f ? st.height : ladders[i].height);
            break;
        case 6: // BEAM
            if (i >= 0 && i < (int)beamPositions.size())
                beamPositions[i] = { st.x, st.y };
            break;
        case 7: // PATH NODE
            if (i >= 0 && i < (int)barrelPath.size()) {
                barrelPath[i].pos.x = st.x;
                barrelPath[i].pos.y = st.y;
            } break;
        case 3: // CAVE / HOUSE
            houseX = st.x; houseY = st.y;
            houseHitbox = { houseX, houseY, houseW, houseH };
            break;
        case 8:  if (i >= 0 && i < (int)nukeSpawnNodes.size())      nukeSpawnNodes[i] = { st.x,st.y };     break;
        case 9:  if (i >= 0 && i < (int)beatriceSpawnNodes.size())   beatriceSpawnNodes[i] = { st.x,st.y }; break;
        case 10: if (i >= 0 && i < (int)enemySpawnPositions.size())  enemySpawnPositions[i] = { st.x,st.y }; break;
            // Types 1 (PLAYER_SPAWN) and 2 (REGULUS) are spawn points — not moved mid-game
        default: break;
        }
        });

    // ─────────────────────────────────────────────────────────────────────────
    // MAIN LOOP
    // ─────────────────────────────────────────────────────────────────────────
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // B = back to menu from anywhere, always
        if (IsKeyPressed(KEY_B) && currentScreen != MENU) {
            editor.ClearFlags();
            currentScreen = MENU;
        }

        if (IsKeyPressed(KEY_F1)) debugPath = !debugPath;

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
            if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER)) { selectedOption = 0; currentScreen = HOW_HIGH; splashTimer = 0.0f; subaruFrame = 0; subaruTimer = 0.0f; }
            if (CheckCollisionPointRec(mouse, btnPlay)) { selectedOption = 0; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentScreen = HOW_HIGH; splashTimer = 0.0f; subaruFrame = 0; subaruTimer = 0.0f; } }
            if (CheckCollisionPointRec(mouse, btnCtrl)) { selectedOption = 2; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentScreen = CONTROLS; splashTimer = 0.0f; subaruFrame = 0; subaruTimer = 0.0f; } }
            if (CheckCollisionPointRec(mouse, btnEditor)) { selectedOption = 3; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { editor.ClearFlags(); currentScreen = LEVEL_EDITOR; } }
            if (CheckCollisionPointRec(mouse, btnExit)) { selectedOption = 1; if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) break; }
        }
        else if (currentScreen == CONTROLS)
        {
            Rectangle btnsalida = { 750, 900, 200, 40 };
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnsalida))
            {
                DrawText(">", 725, 900, 40, ORANGE);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    currentScreen = MENU;

                }
            }

        }
        // ── LEVEL EDITOR ──────────────────────────────────────────────────────
        else if (currentScreen == LEVEL_EDITOR)
        {
            // B key checked here FIRST, before any editor code runs
            if (IsKeyPressed(KEY_B)) {
                editor.ClearFlags();
                Cinematic::Global.LoadAll();   // pick up any new sequences saved in editor
                currentScreen = MENU;
            }
            else {
                editor.Update(dt);
                if (editor.WantsMenu()) { editor.ClearFlags(); currentScreen = MENU; }
                if (editor.WantsPlay()) {
                    editor.ClearFlags();
                    // Apply the editor's current level to the live game data
                    ApplyLevelData(editor.GetLevel());
                    currentLevelId = editor.GetCurrentLevelId();
                    // Reload cinematics — user may have created new sequences in editor
                    Cinematic::Global.LoadAll();
                    // Full reset without reloading level 1 (we just applied editor data)
                    ClearDeathState();
                    ClearRoundEntities();
                    ResetPlayerPos();
                    ResetRegulus();
                    lives = 3; death = false; score = 0;
                    invincible = true; invincibleTimer = invincibleDuration;
                    spawnInterval = 10.0f; minuteTimer = 0.0f;
                    playerHasBeatrice = false; beatriceAbilityTimer = 0.0f;
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
                    ResumeMusicStream(music);
                    currentScreen = GAMEPLAY;
                } // end WantsPlay
            } // end else (not KEY_B)
        }
        // ── HOW HIGH screen (update) ──────────────────────────────────────────
        else if (currentScreen == HOW_HIGH)
        {
            splashTimer += dt;

            // Advance Subaru animation at 5 fps
            subaruTimer += dt;
            if (subaruTimer >= 1.0f / SUBARU_ANIM_FPS)
            {
                subaruTimer -= 1.0f / SUBARU_ANIM_FPS;
                subaruFrame = (subaruFrame + 1) % SUBARU_FRAME_COUNT;
            }

            if (IsKeyPressed(KEY_ENTER) || splashTimer >= splashDuration)
            {
                splashTimer = 0.0f;
                currentScreen = GAMEPLAY;
            }
        }

        // ── GAMEPLAY ──────────────────────────────────────────────────────────
        else if (currentScreen == GAMEPLAY)
        {
            UpdateMusicStream(music);

            // ── Cinematic sequences ───────────────────────────────────────────
            // The SetApplyCallback set above patches live game objects directly.
            {
                static LevelData _cinematicDummy;
                Cinematic::Global.Update(dt, _cinematicDummy);
            }

            // ── Elevator children movement ─────────────────────────────────────
            // Each child of an elevator loops along the shaft: phase 0=top, h=bottom
            //
            // Snapshot platform Y values BEFORE moving them so we can detect
            // upward motion and push the player up when a platform rises into them.
            // Without this, the one-way collision guard in CollisionManager::Resolve
            // fires incorrectly: the platform teleports up, prevY still has the
            // player "below" the old top, and the guard passes the player through.
            struct ElevPlatSnapshot { int platIndex; float prevY; float newY; float platW; };
            vector<ElevPlatSnapshot> elevSnapshots;

            for (int ri = 0; ri < (int)liveRelations.size(); ri++) {
                const auto& rel = liveRelations[ri];
                if (rel.parent.type != 11) continue; // 11 = ELEVATOR tool enum
                int ei = rel.parent.index;
                if (ei < 0 || ei >= (int)liveElevators.size()) continue;
                const ElevatorData& el = liveElevators[ei];
                if ((int)elevChildPhases.size() <= ri) elevChildPhases.resize(ri + 1, 0.f);
                float& phase = elevChildPhases[ri];

                // Snapshot platform Y before update (platform children only)
                int ci = rel.child.index;
                if (rel.child.type == 4 && ci >= 0 && ci < (int)platforms.size())
                    elevSnapshots.push_back({ ci, platforms[ci].y, 0.f, platforms[ci].width });

                if (el.direction == 1) { phase -= el.speed * dt; if (phase < 0.f)  phase = el.h; }
                else { phase += el.speed * dt; if (phase > el.h) phase = 0.f; }
                float cx = el.x + rel.offsetX;
                float cy = el.y + phase;

                // Record the new Y into the snapshot entry we just pushed
                if (rel.child.type == 4 && !elevSnapshots.empty())
                    elevSnapshots.back().newY = cy;

                switch (rel.child.type) {
                case 4: if (ci >= 0 && ci < (int)platforms.size()) platforms[ci] = Platform::Make(cx, cy, platforms[ci].width, 0, 0.f); break;
                case 5: if (ci >= 0 && ci < (int)ladders.size())   ladders[ci] = Ladder::Make(cx, cy, ladders[ci].width, ladders[ci].height); break;
                case 6: if (ci >= 0 && ci < (int)beamPositions.size()) beamPositions[ci] = { cx, cy }; break;
                case 8: if (ci >= 0 && ci < (int)nukeSpawnNodes.size())      nukeSpawnNodes[ci] = { cx,cy }; break;
                case 9: if (ci >= 0 && ci < (int)beatriceSpawnNodes.size())   beatriceSpawnNodes[ci] = { cx,cy }; break;
                case 10:if (ci >= 0 && ci < (int)enemySpawnPositions.size())  enemySpawnPositions[ci] = { cx,cy }; break;
                }
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

            // Shared half-height hitbox anchored to player feet, used for
            // barrel/enemy/killzone damage checks so the collision feel matches
            // the platform collision box.
            auto PlayerHitbox = [&]() -> Rectangle {
                float colW = player.width * 0.5f;
                float colH = player.height * 0.5f;
                float offX = (player.width - colW) * 0.5f;  // centred horizontally
                float offY = player.height - colH;           // anchored to feet
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
                spawnTimer += dt;
                minuteTimer += dt;
            }

            // ── Flying nuke physics ───────────────────────────────────────────
            for (auto& fn : flyingNukes)
            {
                if (!fn.active) continue;
                float prevFnX = fn.rect.x, prevFnY = fn.rect.y;
                fn.vel.y += gravity;          // same gravity constant as player
                fn.rect.x += fn.vel.x;
                fn.rect.y += fn.vel.y;
                float fnVx = fn.vel.x, fnVy = fn.vel.y;
                CollisionResult col = CollisionManager::ResolveAll(
                    fn.rect, fnVx, fnVy, platforms, prevFnX, prevFnY);
                fn.vel.x = fnVx;
                fn.vel.y = fnVy;
                if (col.grounded)
                {
                    nukes.push_back({ { fn.rect.x, fn.rect.y }, true });
                    fn.active = false;
                }
                if (fn.rect.y > (float)screenHeight + 120.0f) fn.active = false;
            }

            // ── Beatrice item animation ───────────────────────────────────────
            beatriceItemAnimTimer += dt;
            if (beatriceItemAnimTimer >= 0.35f)
            {
                beatriceItemAnimFrame = (beatriceItemAnimFrame + 1) % 2;
                beatriceItemAnimTimer = 0.0f;
            }

            // ── Regulus animation update ──────────────────────────────────────
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
                                regulusStunEnding = true;
                                regulusStunEndFrame = 0;
                                regulusStunEndTimer = 0.0f;
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
                    regulusIdleTimer -= 1.0f / REGULUS_IDLE_FPS;
                    regulusIdleFrame = (regulusIdleFrame + 1) % 3;
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

            // ── Nuke pickup ───────────────────────────────────────────────────
            if (!isDying && !playerHasNuke && IsKeyPressed(KEY_E))
            {
                float nkW = NUKE_NATIVE_W * NUKE_SCALE;
                float nkH = NUKE_NATIVE_H * NUKE_SCALE;
                for (auto& nk : nukes)
                {
                    if (!nk.active) continue;
                    Rectangle nkRect = { nk.pos.x, nk.pos.y, nkW, nkH };
                    if (CheckCollisionRecs(player, nkRect)) { nk.active = false; playerHasNuke = true; break; }
                }
            }

            // ── Beatrice pickup ───────────────────────────────────────────────
            if (!isDying && !playerHasBeatrice && IsKeyPressed(KEY_E))
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
                        bc.active = false;
                        playerHasBeatrice = true;
                        beatriceAbilityTimer = BEATRICE_DURATION;
                        beaBulletShootTimer = 0.0f;
                        break;
                    }
                }
            }

            // ── Beatrice ability timer & bullets ──────────────────────────────
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
                                break;
                            }
                        }
                    }
                }
            }

            // ── Bullet update & barrel collision ──────────────────────────────
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
                Rectangle bbRect = { bb.pos.x - bbHalfW, bb.pos.y - bbHalfH,
                                      bbHalfW * 2.0f, bbHalfH * 2.0f };
                for (auto& b : barrels)
                {
                    if (!b.active) continue;
                    if (CheckCollisionRecs(bbRect, b.hitbox))
                    {
                        b.active = false;
                        bb.active = false;
                        score += 100;
                        break;
                    }
                }
                // Colisión bala Beatrice con enemigos
                if (bb.active) // solo si la bala sigue viva tras golpear barriles
                {
                    for (auto& en : enemies)
                    {
                        if (!en.active) continue;
                        if (CheckCollisionRecs(bbRect, en.hitbox))
                        {
                            en.active = false;   // matar enemigo
                            bb.active = false;   // destruir bala
                            score += 300;
                            break;
                        }
                    }
                }
            }

            // ── Drop nuke with G — place it back in the world at player feet ──
            // ── Throw nuke with G — arc in facing direction, lands on platforms ─
            if (playerHasNuke && IsKeyPressed(KEY_G))
            {
                playerHasNuke = false;
                float     nkW = NUKE_NATIVE_W * NUKE_SCALE;
                float     nkH = NUKE_NATIVE_H * NUKE_SCALE;
                FlyingNuke fn;
                fn.rect = { player.x + player.width * 0.5f - nkW * 0.5f,
                              player.y + player.height * 0.5f - nkH * 0.5f,
                              nkW, nkH };
                fn.vel = { facingRight ? 7.0f : -7.0f, -5.5f };  // forward arc
                fn.active = true;
                flyingNukes.push_back(fn);
            }

            // ── Nuke detonation ───────────────────────────────────────────────
            if (!isDying && playerHasNuke && IsKeyPressed(KEY_F))
            {
                PlaySound(nukeSound);
                float scale = 3.8f * 0.85f * 1.05f;
                float nkW = NUKE_NATIVE_W * (NUKE_SCALE * 0.25f) * scale;
                float nkH = NUKE_NATIVE_H * (NUKE_SCALE * 0.25f) * scale;
                nukeExplosionPos = {
                    player.x + image->width * scale * 0.5f - nkW * 0.5f,
                    player.y - nkH - 2.0f
                };
                playerHasNuke = false;
                nukeExplosionPlaying = true;
                nukeExplosionFrame = 0;
                nukeExplosionTimer = 0.0f;
                nukeFlashTimer = 0.0f;
                nukeExtraDelay = 3.0f;

                for (auto& b : barrels) { if (b.active) { score += 100; b.active = false; } }
                // Nuke mata enemigos 
                int enemiesKilled = 0;
                for (auto& en : enemies)
                {
                    if (en.active) { en.active = false; score += 300; enemiesKilled++; }
                }
                spawnTimer = 0.0f;

                regulusIsStunned = true;
                regulusStunEnding = false;
                regulusStunFrame = 0;
                regulusStunTimer = 0.0f;
                regulusStunLoops = 0;
                regulusStunEndFrame = 0;
                regulusStunEndTimer = 0.0f;
                regulusThrowing = false;
                regulusSpawnPending = false;
                regulusForceBlue = false;
                regulusThrowFrame = 0;
                regulusActiveSpawnTimer = 0.0f;
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

            // ── Regulus active/inactive machine ───────────────────────────────
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
                        else { regulusActiveFails++; }
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
                            else { regulusInactiveFails++; }
                        }
                    }
                }
                if (!regulusIsActive) regulusInactiveTime += dt;
            }

            // ── Barrel spawn (active mode) ────────────────────────────────────
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

            if (!isDying && IsKeyPressed(KEY_E) && !regulusThrowing && !regulusIsStunned)
            {
                regulusThrowing = true;
                regulusThrowFrame = 0;
                regulusThrowTimer = 0.0f;
                regulusSpawnPending = true;
                regulusForceBlue = false;
            }

            // ── Barrel / player collision ─────────────────────────────────────
            if (!isDying && !invincible)
            {
                for (const auto& b : barrels)
                {
                    if (!b.active) continue;
                    if (!CheckCollisionRecs(PlayerHitbox(), b.hitbox)) continue;
                    bool fromBelow = (!b.isFalling && velocityY < 0.0f &&
                        (player.y + player.height * 0.5f) >(b.hitbox.y + b.hitbox.height));
                    if (fromBelow) continue;
                    TriggerDeath();
                    break;
                }
            }

            //Rabbit/ player collision
            // ── Actualizar enemigos y colisión con jugador ────────────────────────────
            if (!isDying)
            {
                for (auto& en : enemies)
                {
                    UpdateEnemy(en, player, platforms, dt);
                    if (en.active && !invincible && CheckCollisionRecs(PlayerHitbox(), en.hitbox))
                        TriggerDeath();
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
                        b.hitbox.y - zoneH,
                        zoneW, zoneH
                    };
                    if (CheckCollisionRecs(player, jumpZone)) { score += 100; b.jumpScored = true; PlaySound(jumpBrlSound); }
                }
            }

            // ── Death sequence update ─────────────────────────────────────────
            if (isDying)
            {
                deathTimer += dt;
                if (!hitPlayed) { PlaySound(HitSound);   hitPlayed = true; }
                if (deathTimer > 0.5f && !deathPlayed) { PlaySound(deathSound); deathPlayed = true; }

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

                if (!deathReachedBlack)
                {
                    deathFallVelY += DEATH_FALL_GRAVITY;
                    player.y += deathFallVelY;
                    if (deathTimer >= DEATH_TOTAL_FALL)
                    {
                        deathReachedBlack = true;
                        deathBlackTimer = 0.0f;
                    }
                }
                else
                {
                    deathBlackTimer += dt;
                    if (deathBlackTimer >= (DEATH_BLACK_HOLD + 0.5f))
                    {
                        if (lives <= 0) currentScreen = GAME_OVER;
                        else            ResetRound();
                    }
                }
            }

            // ── Barrel / house collision ──────────────────────────────────────
            if (!isDying && !houseAnimPlaying)
            {
                for (auto& b : barrels)
                {
                    if (b.active && b.isBlue && CheckCollisionRecs(b.hitbox, houseHitbox))
                    {
                        houseAnimPlaying = true; houseAnimFrame = 0; houseAnimTimer = 0.0f;
                        b.active = false; break;
                    }
                }
            }
            if (houseAnimPlaying)
            {
                houseAnimTimer += dt;
                float frameDuration = 1.0f / HOUSE_ANIM_FPS;
                if (houseAnimTimer >= frameDuration)
                {
                    houseAnimTimer -= frameDuration;
                    houseAnimFrame++;
                    if (houseAnimFrame == HOUSE_SWAP_AT_FRAME) houseIsSnowed = true;
                    if (houseAnimFrame >= CAVE_FRAME_COUNT) { houseAnimPlaying = false; houseAnimFrame = CAVE_FRAME_COUNT - 1; }
                }
            }

            // ── Player input ──────────────────────────────────────────────────
            if (!isDying)
            {
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
                        if (ladderExitStep == 0)      image = &imgMarioClimbEnd1;
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
                        if (ladderClimbTimer >= ladderClimbAnimSpeed) { ladderClimbFrame = (ladderClimbFrame + 1) % 2; ladderClimbTimer = 0; }
                        image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        if (t >= 1.0f) ladderEntryClamp = false;
                    }
                    else
                    {
                        bool climbing = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);
                        if (IsKeyDown(KEY_W)) ladderProgress += ladderClimbSpeed / lad.ClimbHeight();
                        if (IsKeyDown(KEY_S)) ladderProgress -= ladderClimbSpeed / lad.ClimbHeight();
                        ladderProgress = Clamp(ladderProgress, 0.0f, 1.0f);
                        player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                        player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
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
                            if (ladderClimbTimer >= ladderClimbAnimSpeed) { ladderClimbFrame = (ladderClimbFrame + 1) % 2; ladderClimbTimer = 0; }
                            image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        }
                    }
                }
                else
                {
                    if (IsKeyDown(KEY_D)) { player.x += playerSpeed; playerIsMoving = true; facingRight = true; }
                    if (IsKeyDown(KEY_A)) { player.x -= playerSpeed; playerIsMoving = true; facingRight = false; }

                    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S))
                    {
                        bool entered = false;
                        if (!playerHasNuke && !playerHasBeatrice)
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
                                    ladderClimbFrame = 0;      ladderClimbTimer = 0;
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

                    if (IsKeyPressed(KEY_SPACE) && !isJumping)
                    {
                        velocityY = jumpForce; isJumping = true; isGrounded = false;
                    }

                    if (!isGrounded) velocityY += gravity;
                    player.y += velocityY;

                    // Shrunk collision box anchored to player feet (50% height, bottom-aligned)
                    {
                        float colW = player.width * 0.5f;
                        float colH = player.height * 0.5f;
                        float colOffX = (player.width - colW) * 0.5f;
                        float colOffY = player.height - colH;
                        Rectangle colRect = { player.x + colOffX, player.y + colOffY, colW, colH };
                        float     colPrevY = prevY + colOffY;

                        CollisionResult col = CollisionManager::ResolveAll(
                            colRect, velocityX, velocityY, platforms, prevX, colPrevY);

                        // Mirror Y correction back so visual rect stays bottom-anchored
                        player.y = colRect.y - colOffY;  // mirror Y correction back to visual rect

                        isGrounded = col.grounded;
                        if (col.grounded) isJumping = false;
                    }

                    // ── Upward-moving platform push ───────────────────────────
                    // CollisionManager uses a one-way top-surface guard based on
                    // the player's previous position. When a platform moves UP
                    // into the player this frame, prevY still shows the player
                    // "below" the old top, so the guard wrongly skips resolution
                    // and the player phases through. We fix it here: for every
                    // elevator-child platform that moved upward, if the platform
                    // top has risen into the player's bottom half, push the player
                    // up flush with the new top and kill downward velocity.
                    if (!onLadder)
                    {
                        for (const auto& snap : elevSnapshots)
                        {
                            // Only care about upward movement
                            if (snap.newY >= snap.prevY) continue;
                            int pi = snap.platIndex;
                            if (pi < 0 || pi >= (int)platforms.size()) continue;
                            const Platform& movPlat = platforms[pi];

                            // Build an AABB for the platform top surface (thin strip)
                            // using its current (new) position.
                            float platTop = movPlat.y;  // y is the top edge for Make(x,y,w,0)
                            float platLeft = movPlat.x;
                            float platRight = movPlat.x + movPlat.width;

                            float playerBottom = player.y + player.height;
                            float playerLeft = player.x;
                            float playerRight = player.x + player.width;

                            // Horizontal overlap check
                            bool hOverlap = playerRight > platLeft && playerLeft < platRight;
                            if (!hOverlap) continue;

                            // The platform top has risen into (or past) the player's
                            // lower half. Tolerance of 4 px avoids accidental triggers
                            // when the player is well above the platform.
                            float penetration = playerBottom - platTop;
                            if (penetration > 0.f && penetration < player.height * 0.75f)
                            {
                                player.y -= penetration;   // push up flush with platform top
                                if (velocityY > 0.f) velocityY = 0.f;  // kill downward velocity
                                isGrounded = true;
                                isJumping = false;
                            }
                        }
                    }

                    // ── Player animation selection ────────────────────────────
                    if (isJumping)
                    {
                        if (playerHasBeatrice)      image = &Dk_Mario_Jump_Beatrice;
                        else if (playerHasNuke)     image = &imgMarioJumpNuke;
                        else                        image = &imgMarioJump;
                    }
                    else if (playerIsMoving)
                    {
                        if (animationTimer >= animationSpeed)
                        {
                            walkFrame = (walkFrame + 1) % 2;
                            animationTimer = fmod(animationTimer, animationSpeed);
                        }
                        if (playerHasBeatrice)      image = (walkFrame == 0) ? &Dk_Mario_Walk1_Beatrice : &Dk_Mario_Walk2_Beatrice;
                        else if (playerHasNuke)     image = (walkFrame == 0) ? &imgMarioWalk1Nuke : &imgMarioWalk2Nuke;
                        else                        image = (walkFrame == 0) ? &imgMarioWalk1 : &imgMarioWalk2;
                    }
                    else
                    {
                        if (playerHasBeatrice)      image = (beatriceItemAnimFrame == 0) ? &Dk_Mario_Idle1_Beatrice : &Dk_Mario_Idle2_Beatrice;
                        else if (playerHasNuke)     image = &imgMarioIdleNuke;
                        else                        image = &imgMarioIdle;
                        walkFrame = 0;
                    }

                    if (player.y > screenHeight + 40) TriggerDeath();
                }
            }

            if (isDying) image = (deathFallVelY < 0.0f) ? &imgMarioJump : &imgMarioFalling;

            // ── Kill Zone collision ───────────────────────────────────────────
            if (!isDying && !invincible) {
                for (const auto& kz : liveKillZones) {
                    if (CheckCollisionRecs({ kz.x, kz.y, kz.w, kz.h }, PlayerHitbox())) {
                        TriggerDeath();
                        break;
                    }
                }
            }

            // ── Win Condition ─────────────────────────────────────────────────
            if (CheckCollisionRecs(wincondition, player))
            {
                LevelData nextLv;
                if (LoadLevel(nextLv, currentLevelId + 1)) {
                    currentLevelId++;
                    ApplyLevelData(nextLv);
                    // Do NOT call FullReset() — it reverts currentLevelId to 1
                    // and reloads level 1 data, corrupting the new level.
                    ClearDeathState();
                    ClearRoundEntities();
                    ResetPlayerPos();
                    ResetRegulus();
                    invincible = true;
                    invincibleTimer = invincibleDuration;
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
                    currentScreen = GAMEPLAY;
                }
                else {
                    splashTimer = 0.0f;
                    currentScreen = GAME_OVER;
                }
            }

        } // end GAMEPLAY update

        else if (currentScreen == GAME_OVER)
        {
            splashTimer += dt;
            if (splashTimer >= splashDuration || IsKeyPressed(KEY_ENTER)) { splashTimer = 0.0f; FullReset(); currentScreen = MENU; }
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

            const char* title = "DONKEY KONG";
            const char* playText = "1 PLAYER GAME";
            const char* exitText = "EXIT";
            const char* controlText = "CONTROLS";
            const char* editorText = "LEVEL EDITOR";
            const char* subtitle = "1967 Bomboclat Industries LLC";

            int titleW = MeasureText(title, titleFont);
            int playW = MeasureText(playText, menuFont);
            int exitW = MeasureText(exitText, menuFont);
            int controlW = MeasureText(controlText, menuFont);
            int editorW = MeasureText(editorText, menuFont);
            int subW = MeasureText(subtitle, smallFont);

            int startY = (screenHeight - (titleFont + spacing * 5 + menuFont * 4 + smallFont)) / 2;
            int titleX = (screenWidth - titleW) / 2, titleY = startY;
            int playX = (screenWidth - playW) / 2, playY = titleY + titleFont + spacing;
            int exitX = (screenWidth - exitW) / 2, exitY = playY + menuFont + spacing;
            int controlX = (screenWidth - controlW) / 2, controlY = exitY + menuFont + spacing;
            int editorX = (screenWidth - editorW) / 2, editorY = controlY + menuFont + spacing;
            int subX = (screenWidth - subW) / 2, subY = editorY + menuFont + spacing;

            // Keep button rects in sync with computed positions
            btnPlay = { (float)(playX - 10), (float)playY,    (float)(playW + 20), (float)(menuFont + 6) };
            btnExit = { (float)(exitX - 10), (float)exitY,    (float)(exitW + 20), (float)(menuFont + 6) };
            btnCtrl = { (float)(controlX - 10), (float)controlY, (float)(controlW + 20), (float)(menuFont + 6) };
            btnEditor = { (float)(editorX - 10), (float)editorY,  (float)(editorW + 20), (float)(menuFont + 6) };

            if (selectedOption == 0) DrawText(">", playX - 40, playY, menuFont, dkOrange);
            if (selectedOption == 1) DrawText(">", exitX - 40, exitY, menuFont, dkOrange);
            if (selectedOption == 2) DrawText(">", controlX - 40, controlY, menuFont, dkOrange);
            if (selectedOption == 3) DrawText(">", editorX - 40, editorY, menuFont, dkOrange);

            DrawText(title, titleX, titleY, titleFont, dkRed);
            DrawText(playText, playX, playY, menuFont, dkWhite);
            DrawText(exitText, exitX, exitY, menuFont, dkWhite);
            DrawText(controlText, controlX, controlY, menuFont, dkWhite);
            DrawText(editorText, editorX, editorY, menuFont, dkOrange);
            DrawText(subtitle, subX, subY, smallFont, dkOrange);
        }
        // ── HOW HIGH screen (draw) ────────────────────────────────────────────
        // ── HOW HIGH screen (update) ──────────────────────────────────────────
        else if (currentScreen == HOW_HIGH)
        {
            // 1. Background stretched to fill screen
            DrawTexturePro(Subaru_Background,
                { 0, 0, (float)Subaru_Background.width, (float)Subaru_Background.height },
                { 0, 0, (float)screenWidth, (float)screenHeight },
                { 0, 0 }, 0.f, WHITE);

            // 2. Current Subaru animation frame, also full-screen
            Texture2D* subTex = subaruFrames[subaruFrame];
            DrawTexturePro(*subTex,
                { 0, 0, (float)subTex->width, (float)subTex->height },
                { 0, 0, (float)screenWidth, (float)screenHeight },
                { 0, 0 }, 0.f, WHITE);

            // 3. Text on top
            const char* howHighTxt = "HOW HIGH CAN YOU GET?";
            int hwW = MeasureText(howHighTxt, 50);
            DrawText(howHighTxt, (screenWidth - hwW) / 2, screenHeight / 2 - 60, 50, YELLOW);

            const char* pressEnter = "PRESS ENTER TO PLAY";
            int peW = MeasureText(pressEnter, 28);
            DrawText(pressEnter, (screenWidth - peW) / 2, screenHeight / 2 + 20, 28, WHITE);
        }
        else if (currentScreen == CONTROLS)
        {

            DrawText("- Move with", 10, 250, 30, WHITE);
            DrawText("W, A, S, D", 195, 250, 30, ORANGE);
            DrawText("E", 285, 300, 30, ORANGE);
            DrawText("- Grab Items with", 10, 300, 30, WHITE);
            DrawText("- Use items with", 10, 350, 30, WHITE);
            DrawText("F", 260, 350, 30, ORANGE);
            DrawText("- Climb stairs with ", 10, 400, 30, WHITE);
            DrawText("W", 295, 400, 30, ORANGE);
            DrawText("while you are close to them", 330, 400, 30, WHITE);

            DrawText("Return", 750, 900, 30, WHITE);
        }

        // ── LEVEL EDITOR draw ─────────────────────────────────────────────────
        else if (currentScreen == LEVEL_EDITOR)
        {
            editor.Draw();
        }

        else if (currentScreen == GAMEPLAY)
        {
            Camera2D cam = { 0 };
            cam.zoom = 1.0f;
            cam.offset = { nukeShakeOffset.x + deathShakeOffset.x, nukeShakeOffset.y + deathShakeOffset.y };
            BeginMode2D(cam);

            // 1. Background
            DrawTexturePro(background, { 0,0,438,475 }, { 0,0,875,950 }, {}, 0.f, WHITE);

            // 1.5 Rain2
            {
                float scaleX = (float)screenWidth / Rain2.width;
                float scaleY = (float)screenHeight / Rain2.height;
                float sW = Rain2.width * scaleX, sH = Rain2.height * scaleY;
                DrawTexturePro(Rain2, { 0, rain2ScrollY, (float)Rain2.width, (float)Rain2.height }, { 0, 0, sW, sH }, {}, 0.f, rain2Tint);
                DrawTexturePro(Rain2, { 0, 0, (float)Rain2.width, (float)Rain2.height }, { 0, -sH + rain2ScrollY, sW, sH }, {}, 0.f, rain2Tint);
            }

            // 2. Ladders (static — pre-baked, excludes elevator children)
            DrawTextureRec(ladderLayer.texture, { 0, 0, (float)screenWidth, -(float)screenHeight }, { 0, 0 }, WHITE);

            // 2.1 Elevator-child ladders — drawn live every frame because their
            //     position is updated by the elevator each tick (not static).
            {
                const float lScale = 4.f;
                const float tileW = 16.f * lScale;
                const float tileH = 16.f * lScale;
                for (int ri = 0; ri < (int)liveRelations.size(); ri++)
                {
                    const auto& rel = liveRelations[ri];
                    if (rel.parent.type != 11 || rel.child.type != 5) continue; // only ladder children of elevators
                    int li = rel.child.index;
                    if (li < 0 || li >= (int)ladders.size()) continue;
                    const Ladder& lad = ladders[li];
                    float drawX = lad.x + lad.width * 0.5f - tileW * 0.5f;
                    for (float y = lad.y; y < lad.y + lad.height; y += tileH)
                    {
                        float dh = fminf(tileH, lad.y + lad.height - y);
                        float srh = dh / lScale;
                        DrawTexturePro(LadderPart,
                            { 0, 0, 16.f, srh },
                            { drawX, y, tileW, dh },
                            { 0, 0 }, 0.f, WHITE);
                    }
                }
            }

            // 2.5 Elevators — rope shaft tiled + panned toward direction
            {
                const float sc = 4.f;
                for (const auto& el : liveElevators)
                {
                    if (RopeTex.id == 0)
                    {
                        // Fallback: plain tinted rect so the shaft is still visible
                        DrawRectangle((int)el.x, (int)el.y, (int)el.w, (int)el.h,
                            { 80, 60, 40, 80 });
                        continue;
                    }

                    float tw = RopeTex.width * sc;
                    float th = RopeTex.height * sc;
                    float drawX = el.x + el.w * 0.5f - tw * 0.5f;

                    // Pan: direction=1 (UP) scrolls rope upward, -1 (DOWN) downward.
                    // Speed is in world-px/s; th is one tile in world-px at scale sc.
                    float rawPan = (float)GetTime() * el.speed
                        * (el.direction == 1 ? 1.f : -1.f);
                    float panOff = fmodf(rawPan, th);
                    if (panOff < 0.f) panOff += th;

                    // One extra tile above the top hides the seam as it scrolls in.
                    float startY = el.y - th + panOff;

                    for (float y = startY; y < el.y + el.h; y += th)
                    {
                        float dy = fmaxf(y, el.y);
                        float dyEnd = fminf(y + th, el.y + el.h);
                        if (dy >= dyEnd) continue;
                        float srcYOff = (dy - y) / sc;
                        float srcH = (dyEnd - dy) / sc;
                        DrawTexturePro(RopeTex,
                            { 0, srcYOff, (float)RopeTex.width, srcH },
                            { drawX, dy, tw, dyEnd - dy },
                            {}, 0.f, WHITE);
                    }
                }
            }

            // 3. Beams (static — pre-baked, excludes elevator children)
            DrawTextureRec(staticLayer.texture, { 0, 0, (float)screenWidth, -(float)screenHeight }, { 0, 0 }, WHITE);

            // 3.0 Kill zones
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
                    DrawRectangle((int)kz.x, (int)kz.y, (int)kz.w, (int)kz.h, { 255, 30, 30, 60 });
                    DrawRectangleLinesEx({ kz.x, kz.y, kz.w, kz.h }, 2.f, { 255, 60, 60, 200 });
                }
            }

            // 3.1 Elevator-child beams — drawn live every frame
            {
                const float bScale = 4.f;
                for (int ri = 0; ri < (int)liveRelations.size(); ri++)
                {
                    const auto& rel = liveRelations[ri];
                    if (rel.parent.type != 11 || rel.child.type != 6) continue;
                    int bi = rel.child.index;
                    if (bi < 0 || bi >= (int)beamPositions.size()) continue;
                    const Vector2& pos = beamPositions[bi];
                    DrawTexturePro(beam,
                        { 0, 0, (float)beam.width, (float)beam.height },
                        { pos.x, pos.y, beam.width * bScale, beam.height * bScale },
                        { 0, 0 }, 0.f, WHITE);
                }
            }

            // 4. House
            {
                Texture2D& houseTex = houseIsSnowed ? House2 : House1;
                DrawTexturePro(houseTex, { 0, 0, HOUSE_NATIVE_W, HOUSE_NATIVE_H }, { houseX, houseY, houseW, houseH }, {}, 0.f, WHITE);
                if (houseIsSnowed)
                {
                    float sfW = FLOOR_NATIVE_W * FLOOR_DRAW_SCALE, sfH = FLOOR_NATIVE_H * FLOOR_DRAW_SCALE;
                    DrawTexturePro(SnowFloor, { 0, 0, FLOOR_NATIVE_W, FLOOR_NATIVE_H }, { houseX, 865.0f, sfW, sfH }, {}, 0.f, WHITE);
                }
                if (houseAnimPlaying && houseAnimFrame < CAVE_FRAME_COUNT)
                {
                    Texture2D* caveTex = caveFrames[houseAnimFrame];
                    float caveW = CAVE_NATIVE_W * CAVE_DRAW_SCALE, caveH = CAVE_NATIVE_H * CAVE_DRAW_SCALE;
                    float caveX = houseX + houseW - caveW * 0.65f;
                    float animProgress = houseAnimFrame / (float)(CAVE_FRAME_COUNT - 1);
                    float caveY = houseY + houseH * 0.5f - caveH * 0.7f + 10.0f * animProgress;
                    DrawTexturePro(*caveTex, { 0, 0, CAVE_NATIVE_W, CAVE_NATIVE_H }, { caveX, caveY, caveW, caveH }, {}, 0.f, WHITE);
                }
            }

            // 4.5 Nuke items
            {
                float nkW = NUKE_NATIVE_W * NUKE_SCALE, nkH = NUKE_NATIVE_H * NUKE_SCALE;
                for (const auto& nk : nukes)
                {
                    if (!nk.active) continue;
                    float bobY = nk.pos.y + sinf((float)GetTime() * 3.0f) * 4.0f;
                    DrawTexturePro(Nuke, { 0, 0, NUKE_NATIVE_W, NUKE_NATIVE_H }, { nk.pos.x, bobY, nkW, nkH }, {}, 0.f, WHITE);
                }
            }

            // 4.55 Flying nukes (in-air)
            for (const auto& fn : flyingNukes)
            {
                if (!fn.active) continue;
                DrawTexturePro(Nuke,
                    { 0, 0, NUKE_NATIVE_W, NUKE_NATIVE_H },
                    { fn.rect.x, fn.rect.y, fn.rect.width, fn.rect.height },
                    {}, 0.f, WHITE);
            }

            // 4.6 Beatrice items
            {
                float      bcScale = 2.0f;
                Texture2D* bcTex = (beatriceItemAnimFrame == 0) ? &Beatrice_Idle1 : &Beatrice_Idle2;
                float bcW = bcTex->width * bcScale;
                float bcH = bcTex->height * bcScale;
                for (const auto& bc : beatrices)
                {
                    if (!bc.active) continue;
                    float bobY = bc.pos.y - bcH + sinf((float)GetTime() * 2.5f) * 4.0f;
                    DrawTexturePro(*bcTex,
                        { 0, 0, (float)bcTex->width, (float)bcTex->height },
                        { bc.pos.x, bobY, bcW, bcH },
                        {}, 0.f, WHITE);
                }
            }

            // 4.7 Regulus
            {
                Texture2D* regTex = nullptr;
                if (regulusIsStunned)
                {
                    if (regulusStunEnding)
                    {
                        int idx = regulusStunEndFrame < 5 ? regulusStunEndFrame : 4;
                        regTex = regulusStunEndFrames[idx];
                    }
                    else
                    {
                        int idx = regulusStunFrame < 3 ? regulusStunFrame : 0;
                        regTex = regulusStunFrames[idx];
                    }
                }
                else
                {
                    int throwIdx = regulusThrowFrame < 0 ? 0 : (regulusThrowFrame > 2 ? 2 : regulusThrowFrame);
                    regTex = regulusThrowing ? regulusThrowFrames[throwIdx] : regulusIdleFrames[regulusIdleFrame];
                }

                float regW = regTex->width * REGULUS_SCALE;
                float regH = regTex->height * REGULUS_SCALE;
                float regY = 225.0f - regH + 20.0f;
                float regX = REGULUS_X + regW * 0.5f;

                if (!regulusIsStunned && regulusThrowing)
                {
                    int throwIdx = regulusThrowFrame < 0 ? 0 : (regulusThrowFrame > 2 ? 2 : regulusThrowFrame);
                    const float handOffX[3] = { 11.0f, 29.0f, 47.0f };
                    const float handOffY[3] = { 40.0f, 19.0f, 40.0f };
                    Texture2D* barrelHandTex = regulusForceBlue ? &BlueBarrelMov1 : &BarrelMov1;
                    float barrelHandScale = REGULUS_SCALE * 0.55f;
                    float barrelHandW = barrelHandTex->width * barrelHandScale;
                    float barrelHandH = barrelHandTex->height * barrelHandScale;
                    float handScrX = regX + handOffX[throwIdx] * REGULUS_SCALE - barrelHandW * 0.5f;
                    float handScrY = regY + handOffY[throwIdx] * REGULUS_SCALE - barrelHandH * 0.5f;

                    if (throwIdx == 0 || throwIdx == 2)
                        DrawTexturePro(*barrelHandTex,
                            { 0, 0, (float)barrelHandTex->width, (float)barrelHandTex->height },
                            { handScrX, handScrY, barrelHandW, barrelHandH }, {}, 0.f, WHITE);
                    DrawTexturePro(*regTex,
                        { 0, 0, (float)regTex->width, (float)regTex->height },
                        { regX, regY, regW, regH }, {}, 0.f, WHITE);
                    if (throwIdx == 1)
                        DrawTexturePro(*barrelHandTex,
                            { 0, 0, (float)barrelHandTex->width, (float)barrelHandTex->height },
                            { handScrX, handScrY, barrelHandW, barrelHandH }, {}, 0.f, WHITE);
                }
                else
                {
                    DrawTexturePro(*regTex,
                        { 0, 0, (float)regTex->width, (float)regTex->height },
                        { regX, regY, regW, regH }, {}, 0.f, WHITE);
                }
            }

            // 5. Player
            {
                bool showPlayer = isDying || !invincible || ((int)(invincibleTimer * 10) % 2 == 0);
                if (showPlayer)
                {
                    float scale = 3.8f * 0.85f * 1.05f;
                    // Feet-aligned: shift Beatrice sprites up so feet stay at same Y
                    float baseH = imgMarioIdle.height * scale;
                    float thisH = image->height * scale;
                    float drawY = player.y + 10.0f + (baseH - thisH);
                    Rectangle src = { 0, 0, (float)image->width, (float)image->height };
                    Rectangle dest = { player.x, drawY, image->width * scale, thisH };
                    if (!facingRight && !onLadder) src.width *= -1;
                    DrawTexturePro(*image, src, dest, {}, 0.f, WHITE);

                    if (playerHasNuke && !isDying)
                    {
                        float nkW = NUKE_NATIVE_W * (NUKE_SCALE * 0.25f) * scale;
                        float nkH = NUKE_NATIVE_H * (NUKE_SCALE * 0.25f) * scale;
                        float nkX = player.x + dest.width * 0.5f - nkW * 0.5f;
                        float nukeOffsetY = 5.0f;
                        bool  moving = (!onLadder && !isJumping && walkFrame != 0);
                        if (moving) nukeOffsetY = (walkFrame == 0) ? 5.0f : 10.0f;
                        float nkY = drawY - nkH - 2.0f + nukeOffsetY;
                        Rectangle nukeSrc = { 0, 0, NUKE_NATIVE_W, NUKE_NATIVE_H };
                        if (!facingRight) nukeSrc.width *= -1;
                        DrawTexturePro(Nuke, nukeSrc, { nkX, nkY, nkW, nkH }, {}, 0.f, WHITE);
                    }
                }
            }

            // 6. Barrels
            for (const auto& b : barrels)
            {
                if (!b.active) continue;
                Texture2D** rollSet = b.isBlue ? blueBarrelRoll : barrelRoll;
                Texture2D** fallSet = b.isBlue ? blueBarrelFall : barrelFall;
                Texture2D* tex;
                if (b.isFalling) tex = fallSet[b.animFrame % 2];
                else
                {
                    int frame = b.movingLeft ? (3 - b.animFrame % 4) : (b.animFrame % 4);
                    tex = rollSet[frame];
                }
                float drawW = b.hitbox.width * 2.0f;
                float drawH = b.hitbox.height * 2.0f;
                float drawX = b.hitbox.x - (drawW - b.hitbox.width) * 0.5f;
                float drawY = b.hitbox.y - (drawH - b.hitbox.height) * 0.5f - 2.625f;
                DrawTexturePro(*tex, { 0, 0, (float)tex->width, (float)tex->height }, { drawX, drawY, drawW, drawH }, {}, 0.f, WHITE);
            }

            // 6.1 Enemies
            for (const auto& en : enemies)
                DrawEnemy(en, rabbitWalkBlack, rabbitJumpBlack, rabbitWalkWhite, rabbitJumpWhite);


            // 6.5 Beatrice bullets
            {
                float bbScale = 2.0f;
                float bbW = texBeaBullet.width * bbScale;
                float bbH = texBeaBullet.height * bbScale;
                for (const auto& bb : beaBullets)
                {
                    if (!bb.active) continue;
                    float angle = atan2f(bb.vel.y, bb.vel.x) * RAD2DEG;
                    DrawTexturePro(texBeaBullet,
                        { 0, 0, (float)texBeaBullet.width, (float)texBeaBullet.height },
                        { bb.pos.x - bbW * 0.5f, bb.pos.y - bbH * 0.5f, bbW, bbH },
                        { bbW * 0.5f, bbH * 0.5f }, angle, WHITE);
                }
            }

            // 7. HUD
            for (int i = 0; i < lives; i++) DrawText("<3", 20 + i * 40, 10, 30, RED);
            {
                const char* scoreTxt = TextFormat("SCORE: %d", score);
                int sw = MeasureText(scoreTxt, 26);
                DrawText(scoreTxt, screenWidth - sw - 12, 10, 26, YELLOW);
            }
            DrawText("Prueba de Donkey Kong_1", 10, screenHeight - 30, 20, WHITE);
            if (onLadder) DrawText(TextFormat("Ladder: %.2f", ladderProgress), 10, screenHeight - 55, 18, YELLOW);
            {
                int ac = 0; for (const auto& b : barrels) if (b.active) ac++;
                DrawText(TextFormat("Barrels: %d  Interval: %.1fs", ac, (float)ACTIVE_SPAWN_INTERVAL), 10, screenHeight - 80, 16, GRAY);
            }
            if (playerHasBeatrice)
                DrawText(TextFormat("BEATRICE: %.1fs", beatriceAbilityTimer),
                    10, screenHeight - 105, 18, MAGENTA);
            if (debugPath) DrawBarrelPathDebug(barrelPath, barrels, screenHeight);

            // 8. Nuke explosion
            if (nukeExplosionPlaying && nukeExplosionFrame < NUKE_EXPL_FRAME_COUNT)
            {
                Texture2D* exTex = explosionFrames[nukeExplosionFrame];
                float exScale = 4.0f;
                float exW = exTex->width * exScale, exH = exTex->height * exScale;
                DrawTexturePro(*exTex, { 0, 0, (float)exTex->width, (float)exTex->height },
                    { nukeExplosionPos.x - exW * 0.5f, nukeExplosionPos.y - exH * 0.5f, exW, exH }, {}, 0.f, WHITE);
            }

            EndMode2D();

            // 9. Rain overlay
            {
                float scaleX = (float)screenWidth / Rain.width;
                float scaleY = (float)screenHeight / Rain.height;
                float sW = Rain.width * scaleX, sH = Rain.height * scaleY;
                DrawTexturePro(Rain, { 0, rainScrollY, (float)Rain.width, (float)Rain.height }, { 0, 0, sW, sH }, {}, 0.f, rainTint);
                DrawTexturePro(Rain, { 0, 0, (float)Rain.width, (float)Rain.height }, { 0, -sH + rainScrollY, sW, sH }, {}, 0.f, rainTint);
            }

            // 9.5 "Press E" prompt — drawn after rain so it's above everything
            if (!isDying)
            {
                const float ebScale = 1.75f;
                const float ebW = EButton.width * ebScale;
                const float ebH = EButton.height * ebScale;
                bool        shown = false;

                // ── Nuke item ─────────────────────────────────────────────
                if (!playerHasNuke)
                {
                    const float nkW = NUKE_NATIVE_W * NUKE_SCALE;
                    const float nkH = NUKE_NATIVE_H * NUKE_SCALE;
                    for (const auto& nk : nukes)
                    {
                        if (!nk.active) continue;
                        Rectangle nkRect = { nk.pos.x, nk.pos.y, nkW, nkH };
                        if (CheckCollisionRecs(player, nkRect))
                        {
                            // centre horizontally over item, sit just above it
                            float cx = nk.pos.x + nkW * 0.5f - ebW * 0.5f;
                            float cy = nk.pos.y - ebH + 5.0f;
                            DrawTexturePro(EButton,
                                { 0, 0, (float)EButton.width, (float)EButton.height },
                                { cx, cy, ebW, ebH }, {}, 0.f, WHITE);
                            shown = true;
                            break;
                        }
                    }
                }

                // ── Beatrice item ─────────────────────────────────────────
                if (!shown && !playerHasBeatrice)
                {
                    const float bcScale2 = 2.0f;
                    const float bcW = Beatrice_Idle1.width * bcScale2;
                    const float bcH = Beatrice_Idle1.height * bcScale2;
                    for (const auto& bc : beatrices)
                    {
                        if (!bc.active) continue;
                        // beatrice rect is drawn feet-up, so top = pos.y - bcH
                        Rectangle bcRect = { bc.pos.x, bc.pos.y - bcH, bcW, bcH };
                        if (CheckCollisionRecs(player, bcRect))
                        {
                            float cx = bc.pos.x + bcW * 0.5f - ebW * 0.5f;
                            float cy = bc.pos.y - bcH - ebH - 1.0f;
                            DrawTexturePro(EButton,
                                { 0, 0, (float)EButton.width, (float)EButton.height },
                                { cx, cy, ebW, ebH }, {}, 0.f, WHITE);
                            break;
                        }
                    }
                }
            }

            // 9.6 Beatrice ability bar (above rain, bottom-left)
            if (playerHasBeatrice)
            {
                const float barMaxW = 180.0f;
                const float barH = 14.0f;
                const float barX = 20.0f;
                const float barY = (float)screenHeight - 58.0f;
                float       frac = Clamp(beatriceAbilityTimer / BEATRICE_DURATION, 0.0f, 1.0f);

                // outer border
                DrawRectangle((int)barX - 2, (int)barY - 2,
                    (int)barMaxW + 4, (int)barH + 4, BLACK);
                // dark background track
                DrawRectangle((int)barX, (int)barY,
                    (int)barMaxW, (int)barH, { 60, 0, 60, 220 });
                // fill — colour shifts red when low
                Color fillCol = (frac > 0.3f) ? MAGENTA : RED;
                DrawRectangle((int)barX, (int)barY,
                    (int)(barMaxW * frac), (int)barH, fillCol);
                // label above bar
                DrawText("BEATRICE", (int)barX, (int)barY - 18, 14, MAGENTA);
            }

            // 10. Nuke flash
            {
                float flashTotal = NUKE_FLASH_IN + NUKE_FLASH_OUT;
                if (nukeFlashTimer < flashTotal)
                {
                    unsigned char alpha = 0;
                    if (nukeFlashTimer < NUKE_FLASH_IN)
                        alpha = (unsigned char)(255.0f * (nukeFlashTimer / NUKE_FLASH_IN));
                    else
                        alpha = (unsigned char)(255.0f * (1.0f - (nukeFlashTimer - NUKE_FLASH_IN) / NUKE_FLASH_OUT));
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 255, 255, 255, alpha });
                }
            }

            // 11. Death overlay
            if (isDying || deathReachedBlack)
            {
                if (deathReachedBlack)
                {
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 0, 0, 0, 255 });
                }
                else if (deathTimer < DEATH_FLASH_DURATION)
                {
                    float t = deathTimer / DEATH_FLASH_DURATION;
                    unsigned char alpha = (unsigned char)(120.0f * t);
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 255, 255, 255, alpha });
                }
                else
                {
                    float t = (deathTimer - DEATH_FLASH_DURATION) / DEATH_FADE_DURATION;
                    if (t > 1.0f) t = 1.0f;
                    unsigned char alpha = (unsigned char)(t * 255.0f);
                    DrawRectangle(0, 0, screenWidth, screenHeight, { 0, 0, 0, alpha });
                }
            }
        }
        else if (currentScreen == GAME_OVER)
        {
            if (lives > 0)
            {
                splashTimer += dt;

                // Advance Subaru animation at 5 fps
                subaruTimer += dt;
                if (subaruTimer >= 1.0f / SUBARU_ANIM_FPS)
                {
                    subaruTimer -= 1.0f / SUBARU_ANIM_FPS;
                    subaruFrame = (subaruFrame + 1) % SUBARU_FRAME_COUNT;
                }

                if (IsKeyPressed(KEY_ENTER) || splashTimer >= splashDuration)
                {
                    splashTimer = 0.0f;
                    currentScreen = GAMEPLAY;
                }
                // 1. Background stretched to fill screen
                DrawTexturePro(Subaru_Background,
                    { 0, 0, (float)Subaru_Background.width, (float)Subaru_Background.height },
                    { 0, 0, (float)screenWidth, (float)screenHeight },
                    { 0, 0 }, 0.f, WHITE);

                // 2. Current Subaru animation frame, also full-screen
                Texture2D* subTex = subaruFrames[subaruFrame];
                DrawTexturePro(*subTex,
                    { 0, 0, (float)subTex->width, (float)subTex->height },
                    { 0, 0, (float)screenWidth, (float)screenHeight },
                    { 0, 0 }, 0.f, WHITE);

                DrawText("HOW HIGH CAN YOU GET?", 225, 900, 30, WHITE);
                DrawText("25", 200, 800, 30, WHITE);
                DrawText("50", 200, 700, 30, WHITE);
            }
            else
            {
                DrawText("GAME OVER", 300, 380, 50, RED);
                const char* scoreTxt = TextFormat("SCORE: %d", score);
                int sw = MeasureText(scoreTxt, 32);
                DrawText(scoreTxt, screenWidth / 2 - sw / 2, 450, 32, YELLOW);
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

    UnloadTexture(imgMarioIdle);      UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);     UnloadTexture(imgMarioJump);
    UnloadTexture(imgMarioFalling);
    UnloadTexture(RopeTex);
    UnloadTexture(imgMarioClimb1);    UnloadTexture(imgMarioClimb2);
    UnloadTexture(imgMarioClimbEnd1); UnloadTexture(imgMarioClimbEnd2);
    UnloadTexture(imgMarioClimbDown); UnloadTexture(background);
    UnloadTexture(beam);              UnloadTexture(LadderPart);
    UnloadTexture(BarrelMov1);        UnloadTexture(BarrelMov2);
    UnloadTexture(BarrelMov3);        UnloadTexture(BarrelMov4);
    UnloadTexture(BarrelFall1);       UnloadTexture(BarrelFall2);
    UnloadTexture(BlueBarrelMov1);    UnloadTexture(BlueBarrelMov2);
    UnloadTexture(BlueBarrelMov3);    UnloadTexture(BlueBarrelMov4);
    UnloadTexture(BlueBarrelFall1);   UnloadTexture(BlueBarrelFall2);
    UnloadTexture(House1);            UnloadTexture(House2);
    UnloadTexture(SnowFloor);
    UnloadTexture(Nuke);
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
    UnloadTexture(RegulusStairs1);   UnloadTexture(RegulusStairs2);
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

    UnloadRenderTexture(ladderLayer);
    UnloadRenderTexture(staticLayer);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}