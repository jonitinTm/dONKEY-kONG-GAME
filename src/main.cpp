#include "raylib.h"
#include "raymath.h"
#include "Collision.h"
#include "Ladder.h"

enum GameScreen { MENU = 0, GAMEPLAY };

// ── Barrel path graph ─────────────────────────────────────────────────────
//
//  All node Y values are calculated as:  barrel.y = platform_surface_y - barrel_height
//  Platform surface formula:  y_surface(x) = plat.y + tan(rot) * (x - plat.cx)
//  where plat.cx = plat.x + plat.width/2,  tan(3°) ≈ 0.05241
//
//  Platforms (from the code):
//    L1a  x= 60 w=400 y=240 rot=0°   cx=260   barrel_y = 240-40 = 200
//    L1b  x=460 w=320 y=246 rot=+3°  cx=620   barrel_y = (246+0.05241*(xp-620)) - 40
//    L2   x=110 w=720 y=360 rot=-3°  cx=470   barrel_y = (360-0.05241*(xp-470)) - 40
//    L3   x= 60 w=720 y=490 rot=+3°  cx=420   barrel_y = (490+0.05241*(xp-420)) - 40
//    L4   x=110 w=720 y=620 rot=-3°  cx=470   barrel_y = (620-0.05241*(xp-470)) - 40
//    L5   x= 60 w=720 y=750 rot=+3°  cx=420   barrel_y = (750+0.05241*(xp-420)) - 40
//    BOTa x= 27 w=412 y=880 rot=0°             barrel_y = 840
//    BOTb x=430 w=420 y=870 rot=-3°  cx=640   barrel_y = (870-0.05241*(xp-640)) - 40
//
//  Ladders (x, top_y, width, height → center_x = x+20, bottom_y = y+height):
//    L1→L2: x=675 top=245 h=104  cx=695  bottom_y=349
//    L2→L3: x=300 top=365 h=117  cx=320  bottom_y=482   (also x=160 top=375 h=102)
//    L3→L4: x=680 top=495 h=110  cx=700  bottom_y=605   (also x=430 top=489 h=128)
//    L4→L5: x=160 top=632 h=101  cx=180  bottom_y=733   (also x=380 top=621 h=124)
//    L5→BOT: x=670 top=760 h=105 cx=690  bottom_y=865
//
//  next[0] = stair / fall branch  → isFalling = true   (Fall anim)
//  next[1] = flat  / roll branch  → isFalling = false  (Roll anim)
//
//  rollThreshold (out of 10):
//    5  → 50/50 split (green nodes)
//    9  → 90/10 split (start node)
//   10  → always fall (edge-fall nodes: isSplitNode=true forces fall anim)
//
//  isSplitNode:
//    true  + bothValid  → random choice, updates isFalling
//    true  + oneValid   → forced choice, still updates isFalling (edge nodes)
//    false              → pass-through, no anim state change
//
//  next={-1,-1} → barrel destroyed
//
struct PathNode
{
    Vector2 pos;
    int     next[2] = { -1, -1 };
    int     rollThreshold = 5;
    bool    isSplitNode = false;
};

// ── Barrel ────────────────────────────────────────────────────────────────
struct Barrel
{
    Rectangle hitbox = { 0, 0, 40, 40 };
    int       currentNode = 0;
    float     speed = 2.5f;
    bool      active = true;

    bool  isBlue = false;
    bool  isFalling = false;
    bool  movingLeft = false;

    float animTimer = 0.0f;
    int   animFrame = 0;
};

// ── SpawnBarrel ───────────────────────────────────────────────────────────
// Creates a barrel at startNode. Always spawns at node 0 (same world pos).
// Mid-game: barrels[0] = SpawnBarrel(barrelPath);
Barrel SpawnBarrel(const vector<PathNode>& path, int startNode = 0,
    float spd = 2.5f, float w = 40.0f, float h = 40.0f)
{
    Barrel b;
    b.currentNode = startNode;
    b.speed = spd;
    b.isBlue = (GetRandomValue(0, 9) == 0);
    b.isFalling = false;
    b.movingLeft = false;
    b.animFrame = 0;
    b.animTimer = 0.0f;
    b.active = true;
    b.hitbox = { path[startNode].pos.x,
                      path[startNode].pos.y, w, h };
    return b;
}

// ── UpdateBarrel ──────────────────────────────────────────────────────────
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

        bool bothValid = (node.next[0] != -1 && node.next[1] != -1);
        bool oneValid = (node.next[0] != -1 || node.next[1] != -1);

        if (bothValid)
        {
            int roll = GetRandomValue(0, 9);
            int choice = (roll < node.rollThreshold) ? 0 : 1;

            if (node.isSplitNode)
            {
                b.isFalling = (choice == 0);   // 0 = stair/fall, 1 = flat/roll
                b.animFrame = 0;
            }
            b.currentNode = node.next[choice];
        }
        else if (oneValid)
        {
            int nextNode = (node.next[0] != -1) ? node.next[0] : node.next[1];
            bool takingPath0 = (node.next[0] != -1);

            // Edge-fall nodes (isSplitNode=true, only one valid branch) force
            // the fall animation so the barrel looks like it's tumbling off the edge
            if (node.isSplitNode)
            {
                b.isFalling = takingPath0;   // path 0 = fall
                b.animFrame = 0;
            }
            b.currentNode = nextNode;
        }
        else
        {
            b.active = false;   // end node — barrel destroyed
        }
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
    if (b.animTimer >= frameTime)
    {
        b.animFrame = (b.animFrame + 1) % frameCount;
        b.animTimer = 0.0f;
    }
}

// ── DrawBarrelPathDebug ───────────────────────────────────────────────────
// Toggle with F1 in-game.
void DrawBarrelPathDebug(const vector<PathNode>& path,
    const vector<Barrel>& barrels,
    int screenHeight)
{
    // Connection lines
    for (int i = 0; i < (int)path.size(); i++)
    {
        const PathNode& n = path[i];
        for (int b = 0; b < 2; b++)
        {
            if (n.next[b] == -1) continue;
            Color c = (b == 0)
                ? Color{ 255, 140, 0, 200 }   // orange = stair/fall
            : Color{ 0, 200, 255, 200 }; // cyan   = flat/roll
            DrawLineEx(n.pos, path[n.next[b]].pos, 2.5f, c);
        }
    }

    // Nodes
    for (int i = 0; i < (int)path.size(); i++)
    {
        const PathNode& n = path[i];
        Color fill;
        if (i == 0)                               fill = WHITE;
        else if (n.next[0] == -1 && n.next[1] == -1)       fill = RED;
        else if (n.isSplitNode && n.rollThreshold == 10)  fill = ORANGE;  // forced-fall edge
        else if (n.isSplitNode)                         fill = GREEN;
        else                                            fill = YELLOW;

        DrawCircleV(n.pos, 11.0f, BLACK);
        DrawCircleV(n.pos, 9.0f, fill);
        const char* lbl = TextFormat("%d", i);
        int tw = MeasureText(lbl, 9);
        DrawText(lbl, (int)n.pos.x - tw / 2, (int)n.pos.y - 4, 9, BLACK);
    }

    // Active barrel dots
    for (int i = 0; i < (int)barrels.size(); i++)
    {
        const Barrel& b = barrels[i];
        if (!b.active) continue;
        Vector2 c = { b.hitbox.x + b.hitbox.width * 0.5f,
                      b.hitbox.y + b.hitbox.height * 0.5f };
        DrawCircleV(c, 7.0f, BLACK);
        DrawCircleV(c, 5.0f, b.isBlue ? BLUE : ORANGE);
        DrawText(TextFormat("B%d N%d", i, b.currentNode),
            (int)c.x + 8, (int)c.y - 5, 9, WHITE);
    }

    // Legend
    int lx = 10;
    int ly = screenHeight - 125;
    DrawRectangle(6, ly, 190, 120, { 0,0,0,180 });
    DrawText("[F1] toggle debug", lx, ly + 4, 9, GRAY);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 20) }, 5, WHITE);   DrawText("Start", lx + 14, ly + 15, 9, WHITE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 34) }, 5, GREEN);   DrawText("Split 50/50", lx + 14, ly + 29, 9, GREEN);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 48) }, 5, ORANGE);  DrawText("Edge (fall)", lx + 14, ly + 43, 9, ORANGE);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 62) }, 5, YELLOW);  DrawText("Obligatory", lx + 14, ly + 57, 9, YELLOW);
    DrawCircleV({ (float)(lx + 5),(float)(ly + 76) }, 5, RED);     DrawText("End", lx + 14, ly + 71, 9, RED);
    DrawLineEx({ (float)lx,(float)(ly + 90) }, { (float)(lx + 18),(float)(ly + 90) }, 2, { 255,140,0,255 });
    DrawText("Stair/Fall", lx + 22, ly + 85, 9, { 255,140,0,255 });
    DrawLineEx({ (float)lx,(float)(ly + 104) }, { (float)(lx + 18),(float)(ly + 104) }, 2, { 0,200,255,255 });
    DrawText("Flat/Roll", lx + 22, ly + 99, 9, { 0,200,255,255 });
    DrawText("Dot=barrel", lx, ly + 113, 9, GRAY);
}

int main(void)
{
    const int screenWidth = 875;
    const int screenHeight = 950;

    // ── Menu ──────────────────────────────────────────────────────────────
    GameScreen currentScreen = MENU;
    int        selectedOption = 0;
    Rectangle  btnPlay = { 340, 450, 200, 40 };
    Rectangle  btnExit = { 340, 500, 200, 40 };
    bool       debugPath = false;

    // ── Player ────────────────────────────────────────────────────────────
    Rectangle player = { 100, 150, 60, 60 };
    float     playerSpeed = 4.0f;
    float     jumpForce = -8.0f;
    float     gravity = 0.4f;
    float     velocityX = 0.0f;
    float     velocityY = 0.0f;
    bool      isJumping = false;
    bool      facingRight = true;
    bool      isGrounded = false;

    // ── Lives & death ─────────────────────────────────────────────────────
    int  lives = 3;
    bool death = false;

    bool  invincible = false;
    float invincibleTimer = 0.0f;
    const float invincibleDuration = 1.5f;

    // ── Ladder state ──────────────────────────────────────────────────────
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

    // ── Platforms ─────────────────────────────────────────────────────────
    vector<Platform> platforms = {
        Platform::Make(27,  880, 412, 0,  0.0f),   // BOTa
        Platform::Make(430, 870, 420, 0, -3.0f),   // BOTb
        Platform::Make(60,  750, 720, 0,  3.0f),   // L5
        Platform::Make(110, 620, 720, 0, -3.0f),   // L4
        Platform::Make(60,  490, 720, 0,  3.0f),   // L3
        Platform::Make(110, 360, 720, 0, -3.0f),   // L2
        Platform::Make(460, 246, 320, 0,  3.0f),   // L1b
        Platform::Make(60,  240, 400, 0,  0.0f),   // L1a
    };

    // ── Ladders ───────────────────────────────────────────────────────────
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

    // ── Barrel path graph ─────────────────────────────────────────────────
    //
    //  Y values computed from platform surface formula, barrel sits ON surface.
    //  tan(3°) = 0.05241.
    //
    //  Rolling direction per layer:
    //    L1  (+3° right portion) → rolls RIGHT  → x increases, y increases slightly
    //    L2  (-3°)               → rolls LEFT   → x decreases, y increases slightly
    //    L3  (+3°)               → rolls RIGHT
    //    L4  (-3°)               → rolls LEFT
    //    L5  (+3°)               → rolls RIGHT
    //    BOT (mostly flat/−3°)   → rolls LEFT
    //
    //  Node types  (debug color):
    //    WHITE   = start (node 0)
    //    GREEN   = 50/50 random split
    //    ORANGE  = forced-fall edge node (rolls off platform edge)
    //    YELLOW  = obligatory pass-through
    //    RED     = end (barrel destroyed → auto-respawn after 2 s)
    //
    //  Idx  Pos          next0  next1  thresh  split  Notes
    vector<PathNode> barrelPath = {
        // ── Layer 1 (rolls RIGHT) ─────────────────────────────────────────
        /*  0 */ { {125, 200},  {  1, -1 },  9, false }, // Start: 90% normal, 10% destroy (unimplemented path)
        /*  1 */ { {438, 200},  {  2, -1 },  5, false }, // L1a obligatory (flat surface y=200)
        /*  2 */ { {675, 209},  {  3,  4 },  5, true  }, // L1b GREEN split near ladder x=675
        //   0→stair(3)  1→flat(4)

// ── Stair L1→L2 (ladder x=675, cx=695, bottom y=349) ─────────────
/*  3 */ { {695, 309},  {  6, -1 },  5, false }, // L2 stair-landing

// ── Flat L1→L2 (barrel rolls off right edge of L1b at x≈820) ─────
/*  4 */ { {820, 222},  {  5, -1 }, 10, true  }, // L1b EDGE-FALL (forced fall anim)
/*  5 */ { {780, 304},  {  6, -1 },  5, false }, // L2 edge-landing

// ── Layer 2 (rolls LEFT) ──────────────────────────────────────────
/*  6 */ { {550, 316},  {  7, -1 },  5, false }, // L2 obligatory merge
/*  7 */ { {318, 328},  {  8,  9 },  5, true  }, // L2 GREEN split near ladder x=300
//   0→stair(8)  1→flat(9)

// ── Stair L2→L3 (ladder x=300, cx=320, bottom y=482) ─────────────
/*  8 */ { {320, 445},  { 11, -1 },  5, false }, // L3 stair-landing

// ── Flat L2→L3 (barrel rolls off left edge of L2 at x≈110) ───────
/*  9 */ { {110, 339},  { 10, -1 }, 10, true  }, // L2 EDGE-FALL
/* 10 */ { {110, 431},  { 11, -1 },  5, false }, // L3 edge-landing

// ── Layer 3 (rolls RIGHT) ─────────────────────────────────────────
/* 11 */ { {430, 451},  { 12, -1 },  5, false }, // L3 obligatory merge
/* 12 */ { {680, 464},  { 13, 14 },  5, true  }, // L3 GREEN split near ladder x=680
//   0→stair(13) 1→flat(14)

// ── Stair L3→L4 (ladder x=680, cx=700, bottom y=605) ─────────────
/* 13 */ { {700, 568},  { 16, -1 },  5, false }, // L4 stair-landing

// ── Flat L3→L4 (barrel rolls off right edge of L3 at x≈780) ──────
/* 14 */ { {780, 469},  { 15, -1 }, 10, true  }, // L3 EDGE-FALL
/* 15 */ { {780, 564},  { 16, -1 },  5, false }, // L4 edge-landing

// ── Layer 4 (rolls LEFT) ──────────────────────────────────────────
/* 16 */ { {550, 576},  { 17, -1 },  5, false }, // L4 obligatory merge
/* 17 */ { {160, 596},  { 18, 19 },  5, true  }, // L4 GREEN split near ladder x=160
//   0→stair(18) 1→flat(19)

// ── Stair L4→L5 (ladder x=160, cx=180, bottom y=733) ─────────────
/* 18 */ { {180, 697},  { 21, -1 },  5, false }, // L5 stair-landing

// ── Flat L4→L5 (barrel rolls off left edge of L4 at x≈110) ───────
/* 19 */ { {110, 599},  { 20, -1 }, 10, true  }, // L4 EDGE-FALL
/* 20 */ { {110, 691},  { 21, -1 },  5, false }, // L5 edge-landing

// ── Layer 5 (rolls RIGHT) ─────────────────────────────────────────
/* 21 */ { {430, 711},  { 22, -1 },  5, false }, // L5 obligatory merge
/* 22 */ { {670, 723},  { 23, 24 },  5, true  }, // L5 GREEN split near ladder x=670
//   0→stair(23) 1→flat(24)

// ── Stair L5→BOT (ladder x=670, cx=690, bottom y=865) ────────────
/* 23 */ { {690, 827},  { 26, -1 },  5, false }, // BOT stair-landing

// ── Flat L5→BOT (barrel rolls off right edge of L5 at x≈780) ─────
/* 24 */ { {780, 729},  { 25, -1 }, 10, true  }, // L5 EDGE-FALL
/* 25 */ { {780, 823},  { 26, -1 },  5, false }, // BOT edge-landing

// ── Bottom layer (rolls LEFT) ─────────────────────────────────────
/* 26 */ { {400, 840},  { 27, -1 },  5, false }, // BOT obligatory merge
/* 27 */ { {148, 840},  { -1, -1 },  5, false }, // END — barrel destroyed, respawn in 2 s
    };

    // ── Single barrel (auto-respawns after reaching end) ──────────────────
    vector<Barrel> barrels = { SpawnBarrel(barrelPath) };
    float barrelRespawnTimer = 0.0f;
    const float barrelRespawnDelay = 2.0f;

    // ── Beam positions ────────────────────────────────────────────────────
    vector<Vector2> beamPositions = {
        {  50, 225 }, { 114, 225 }, { 178, 225 }, { 212, 225 },
        { 276, 225 }, { 340, 225 }, { 372, 225 }, { 436, 230 },
        { 468, 230 }, { 532, 235 }, { 564, 235 }, { 628, 240 },
        { 660, 240 }, { 724, 245 },
        { 110, 370 }, { 142, 370 }, { 206, 365 }, { 238, 365 },
        { 302, 360 }, { 334, 360 }, { 398, 350 }, { 430, 350 },
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

    // ── Window & assets ───────────────────────────────────────────────────
    InitWindow(screenWidth, screenHeight, "Donkey Kong");
    InitAudioDevice();

    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Texture2D background = LoadTexture("Wiki/SubaruStairs.png");
    Texture2D beam = LoadTexture("Assets/Textures/Architecture/Dk_FloorPart.png");
    Texture2D imgMarioClimb1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder1.png");
    Texture2D imgMarioClimb2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Ladder2.png");
    Texture2D imgMarioClimbEnd1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd1.png");
    Texture2D imgMarioClimbEnd2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_LadderEnd2.png");
    Texture2D imgMarioClimbDown = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_IdleBack.png");
    Texture2D LadderPart = LoadTexture("Assets/Textures/Architecture/Dk_Ladder.png");

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

    Texture2D* barrelRoll[4] = { &BarrelMov1,     &BarrelMov2,     &BarrelMov3,     &BarrelMov4 };
    Texture2D* barrelFall[2] = { &BarrelFall1,    &BarrelFall2 };
    Texture2D* blueBarrelRoll[4] = { &BlueBarrelMov1, &BlueBarrelMov2, &BlueBarrelMov3, &BlueBarrelMov4 };
    Texture2D* blueBarrelFall[2] = { &BlueBarrelFall1,&BlueBarrelFall2 };

    // ── Bake beams ────────────────────────────────────────────────────────
    RenderTexture2D staticLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(staticLayer);
    ClearBackground(BLANK);
    float beamScale = 4.0f;
    for (auto& pos : beamPositions)
        DrawTexturePro(beam,
            { 0,0,(float)beam.width,(float)beam.height },
            { pos.x, pos.y, beam.width * beamScale, beam.height * beamScale },
            { 0,0 }, 0.f, WHITE);
    EndTextureMode();
    UnloadTexture(beam);

    // ── Bake ladders ──────────────────────────────────────────────────────
    float ladderScale = 4.0f;
    float ladderTileH = 16 * ladderScale;
    float ladderTileW = 16 * ladderScale;

    RenderTexture2D ladderLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(ladderLayer);
    ClearBackground(BLANK);
    for (auto& lad : ladders)
    {
        float drawX = lad.x + lad.width * 0.5f - ladderTileW * 0.5f;
        float y = lad.y;
        float bottom = lad.y + lad.height;
        while (y < bottom)
        {
            DrawTexturePro(LadderPart,
                { 0,0,16.0f,16.0f },
                { drawX, y, ladderTileW, ladderTileH },
                { 0,0 }, 0.f, WHITE);
            y += ladderTileH;
        }
    }
    EndTextureMode();
    UnloadTexture(LadderPart);

    Texture2D image = imgMarioIdle;
    SetTargetFPS(60);

    float animationTimer = 0.0f;
    float animationSpeed = 0.15f;
    int   walkFrame = 0;

    // ── Game loop ─────────────────────────────────────────────────────────
    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F1)) debugPath = !debugPath;

        // ── Menu input ────────────────────────────────────────────────────
        if (currentScreen == MENU)
        {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnPlay))
            {
                selectedOption = 0;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentScreen = GAMEPLAY;
            }
            if (CheckCollisionPointRec(mouse, btnExit))
            {
                selectedOption = 1;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) break;
            }
        }

        // ── Gameplay logic ────────────────────────────────────────────────
        if (currentScreen == GAMEPLAY)
        {
            float dt = GetFrameTime();
            animationTimer += dt;
            if (ladderCooldown > 0.0f) ladderCooldown -= dt;

            if (invincible)
            {
                invincibleTimer -= dt;
                if (invincibleTimer <= 0.0f) { invincible = false; invincibleTimer = 0.0f; }
            }

            float prevX = player.x, prevY = player.y;
            bool  playerIsMoving = false;

            // ── Barrel update & auto-respawn ──────────────────────────────
            for (auto& b : barrels) UpdateBarrel(b, barrelPath, dt);

            if (!barrels[0].active)
            {
                barrelRespawnTimer += dt;
                if (barrelRespawnTimer >= barrelRespawnDelay)
                {
                    barrels[0] = SpawnBarrel(barrelPath);
                    barrelRespawnTimer = 0.0f;
                }
            }

            // ── Barrel collision ──────────────────────────────────────────
            if (!invincible)
            {
                for (const auto& b : barrels)
                {
                    if (b.active && CheckCollisionRecs(player, b.hitbox))
                    {
                        lives--;
                        death = true;
                        invincible = true;
                        invincibleTimer = invincibleDuration;
                        break;
                    }
                }
            }

            // ── Ladder state ──────────────────────────────────────────────
            if (onLadder)
            {
                const Ladder& lad = ladders[currentLadder];

                if (ladderExitPlaying)
                {
                    ladderExitTimer += dt;
                    if (ladderExitTimer >= ladderExitFrameDuration)
                    {
                        ladderExitTimer = 0.0f; ladderExitStep++;
                    }

                    if (ladderExitStep == 0) image = imgMarioClimbEnd1;
                    else if (ladderExitStep == 1) image = imgMarioClimbEnd2;
                    else
                    {
                        image = imgMarioClimbDown;
                        bool wants = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                            || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S) || IsKeyPressed(KEY_SPACE);
                        if (wants)
                        {
                            onLadder = false; currentLadder = -1; ladderExitPlaying = false;
                            ladderExitStep = 0; ladderExitTimer = 0; velocityY = 0; isJumping = false;
                        }
                    }
                }
                else
                {
                    bool climbing = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);
                    if (IsKeyDown(KEY_W)) ladderProgress += ladderClimbSpeed / lad.height;
                    if (IsKeyDown(KEY_S)) ladderProgress -= ladderClimbSpeed / lad.height;
                    ladderProgress = Clamp(ladderProgress, 0.0f, 1.0f);

                    player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                    player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
                    velocityY = 0; velocityX = 0;

                    if (ladderProgress <= 0.0f)
                    {
                        image = imgMarioClimbDown;
                        bool wants = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                            || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S) || IsKeyPressed(KEY_SPACE);
                        if (wants) { onLadder = false; currentLadder = -1; isGrounded = true; isJumping = false; ladderCooldown = 0.2f; }
                    }
                    else if (ladderProgress >= 1.0f)
                    {
                        ladderExitPlaying = true; ladderExitStep = 0; ladderExitTimer = 0; image = imgMarioClimbEnd1;
                    }
                    else if (climbing)
                    {
                        ladderClimbTimer += dt;
                        if (ladderClimbTimer >= ladderClimbAnimSpeed)
                        {
                            ladderClimbFrame = (ladderClimbFrame + 1) % 2; ladderClimbTimer = 0;
                        }
                        image = (ladderClimbFrame == 0) ? imgMarioClimb1 : imgMarioClimb2;
                    }
                }
            }
            // ── Normal state ──────────────────────────────────────────────
            else
            {
                if (IsKeyDown(KEY_D)) { player.x += playerSpeed; playerIsMoving = true; facingRight = true; }
                if (IsKeyDown(KEY_A)) { player.x -= playerSpeed; playerIsMoving = true; facingRight = false; }

                if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S))
                {
                    bool entered = false;
                    for (int i = 0; i < (int)ladders.size(); i++)
                    {
                        if (ladderCooldown <= 0 && CheckCollisionRecs(player, ladders[i].GetHitbox()))
                        {
                            float ip = ladders[i].ProgressFromPlayerY(player.y, player.height);
                            ladderProgress = Clamp(ip, 0.01f, 0.99f);
                            onLadder = true; currentLadder = i; ladderExitPlaying = false;
                            ladderExitStep = 0; ladderExitTimer = 0; ladderClimbFrame = 0; ladderClimbTimer = 0;
                            player.x = ladders[i].x + ladders[i].width * 0.5f - player.width * 0.5f;
                            velocityY = 0; velocityX = 0; isJumping = false; isGrounded = false;
                            entered = true; break;
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

                CollisionResult col = CollisionManager::ResolveAll(
                    player, velocityX, velocityY, platforms, prevX, prevY);
                isGrounded = col.grounded;
                if (col.grounded) isJumping = false;

                if (isJumping)      image = imgMarioJump;
                else if (playerIsMoving)
                {
                    if (animationTimer >= animationSpeed)
                    {
                        walkFrame = (walkFrame + 1) % 2; animationTimer = 0;
                    }
                    image = (walkFrame == 0) ? imgMarioWalk1 : imgMarioWalk2;
                }
                else { image = imgMarioIdle; walkFrame = 0; }
            }

            if (player.y > 900)
            {
                player.y = 0; player.x = 440; onLadder = false; currentLadder = -1; ladderExitPlaying = false;
            }
        }

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        if (currentScreen == MENU)
        {
            DrawText("DONKEY KONG", 250, 250, 60, RED);
            DrawRectangleLinesEx(btnPlay, 2, selectedOption == 0 ? YELLOW : WHITE);
            DrawRectangleLinesEx(btnExit, 2, selectedOption == 1 ? YELLOW : WHITE);
            DrawText("1 PLAYER", btnPlay.x + 20, btnPlay.y + 5, 28, selectedOption == 0 ? YELLOW : WHITE);
            DrawText("EXIT", btnExit.x + 20, btnExit.y + 5, 28, selectedOption == 1 ? YELLOW : WHITE);
        }

        if (currentScreen == GAMEPLAY)
        {
            DrawTexturePro(background, { 0,0,548,308 }, { 0,0,875,950 }, {}, 0.f, WHITE);
            DrawTextureRec(ladderLayer.texture, { 0,0,(float)screenWidth,-(float)screenHeight }, { 0,0 }, WHITE);
            DrawTextureRec(staticLayer.texture, { 0,0,(float)screenWidth,-(float)screenHeight }, { 0,0 }, WHITE);
            CollisionManager::DrawAll(platforms);

            // ── Barrels ───────────────────────────────────────────────────
            for (const auto& b : barrels)
            {
                if (!b.active) continue;

                Texture2D** rollSet = b.isBlue ? blueBarrelRoll : barrelRoll;
                Texture2D** fallSet = b.isBlue ? blueBarrelFall : barrelFall;
                Texture2D* tex;

                if (b.isFalling)
                {
                    tex = fallSet[b.animFrame % 2];
                }
                else
                {
                    int frame = b.movingLeft ? (3 - b.animFrame % 4) : (b.animFrame % 4);
                    tex = rollSet[frame];
                }

                float drawW = b.hitbox.width * 2.0f;
                float drawH = b.hitbox.height * 2.0f;
                float drawX = b.hitbox.x - (drawW - b.hitbox.width) * 0.5f;
                float drawY = b.hitbox.y - (drawH - b.hitbox.height) * 0.5f;

                DrawTexturePro(*tex,
                    { 0,0,(float)tex->width,(float)tex->height },
                    { drawX,drawY,drawW,drawH },
                    { 0,0 }, 0.f, WHITE);
            }

            // ── Player ────────────────────────────────────────────────────
            bool showPlayer = !invincible || ((int)(invincibleTimer * 10) % 2 == 0);
            if (showPlayer)
            {
                float scale = 3.8f;
                Rectangle src = { 0,0,(float)image.width,(float)image.height };
                Rectangle dest = { player.x,player.y,image.width * scale,image.height * scale };
                if (!facingRight && !onLadder) src.width *= -1;
                DrawTexturePro(image, src, dest, {}, 0.f, WHITE);
            }

            // ── HUD ───────────────────────────────────────────────────────
            for (int i = 0; i < lives; i++) DrawText("<3", 20 + i * 40, 10, 30, RED);
            if (death)   DrawText("DEATH", 10, 45, 20, ORANGE);
            if (lives <= 0) DrawText("GAME OVER", 270, 450, 50, MAROON);
            DrawText("Prueba de Donkey Kong_1", 10, screenHeight - 30, 20, WHITE);
            if (onLadder)
                DrawText(TextFormat("Ladder: %.2f", ladderProgress), 10, screenHeight - 55, 18, YELLOW);
            if (!barrels[0].active)
                DrawText(TextFormat("Barrel respawn: %.1fs",
                    barrelRespawnDelay - barrelRespawnTimer),
                    10, screenHeight - 80, 16, GRAY);

            if (debugPath) DrawBarrelPathDebug(barrelPath, barrels, screenHeight);
        }

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    UnloadTexture(imgMarioIdle);   UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);  UnloadTexture(imgMarioJump);
    UnloadTexture(imgMarioClimb1); UnloadTexture(imgMarioClimb2);
    UnloadTexture(imgMarioClimbEnd1); UnloadTexture(imgMarioClimbEnd2);
    UnloadTexture(imgMarioClimbDown); UnloadTexture(background);
    UnloadTexture(BarrelMov1);     UnloadTexture(BarrelMov2);
    UnloadTexture(BarrelMov3);     UnloadTexture(BarrelMov4);
    UnloadTexture(BarrelFall1);    UnloadTexture(BarrelFall2);
    UnloadTexture(BlueBarrelMov1); UnloadTexture(BlueBarrelMov2);
    UnloadTexture(BlueBarrelMov3); UnloadTexture(BlueBarrelMov4);
    UnloadTexture(BlueBarrelFall1); UnloadTexture(BlueBarrelFall2);
    UnloadRenderTexture(ladderLayer);
    UnloadRenderTexture(staticLayer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}