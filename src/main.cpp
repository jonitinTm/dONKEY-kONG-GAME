    #include "raylib.h"
    #include "raymath.h"
    #include "Collision.h"
    #include "Ladder.h"

    enum GameScreen { MENU = 0, GAMEPLAY };

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
    };

    struct NukeItem
    {
        Vector2   pos;
        bool      active = true;
    };

    Barrel SpawnBarrel(const vector<PathNode>& path, int startNode = 0,
        float spd = 2.5f, float w = 30.0f, float h = 30.0f)
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
        b.hitbox = { path[startNode].pos.x, path[startNode].pos.y, w, h };
        return b;
    }

    bool SpawnBarrelFromPool(vector<Barrel>& barrels, const vector<PathNode>& path,
        float spd = 2.5f, float w = 30.0f, float h = 30.0f, bool forceBlue = false)
    {
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

            if (!node.isSplitNode)
                b.isFalling = false;

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
            else { b.active = false; }
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

    int main(void)
    {
        const int screenWidth = 875;
        const int screenHeight = 950;

        GameScreen currentScreen = MENU;
        int        selectedOption = 0;
        Rectangle  btnPlay = { 340, 450, 200, 40 };
        Rectangle  btnExit = { 340, 500, 200, 40 };
        bool       debugPath = false;

        Rectangle player = { 35.0f + 64.0f * 3.5f + 10.0f, 820.0f, 60, 60 }; // next to house
        float     playerSpeed = 2.0f;
        float     jumpForce = -8.0f;
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

        // ── House / explosion state ──────────────────────────────────────────────
        const float HOUSE_ANIM_FPS = 10.0f;   // EDITABLE: cave anim speed (frames/sec)
        const int   HOUSE_SWAP_AT_FRAME = 4;        // EDITABLE: 0-based frame that swaps house (4 = cave5)
        const float HOUSE_DRAW_SCALE = 3.5f;     // EDITABLE: house draw scale (native 64x32)
        const float CAVE_DRAW_SCALE = 4.335f;  // 5.1 * 0.85     // EDITABLE: explosion draw scale (native 64x32)
        const float FLOOR_DRAW_SCALE = 4.0f;     // EDITABLE: snow floor tile scale (native 48x16)

        // Native sprite dimensions (do not change unless assets change)
        const float HOUSE_NATIVE_W = 64.0f;
        const float HOUSE_NATIVE_H = 32.0f;
        const float CAVE_NATIVE_W = 64.0f;
        const float CAVE_NATIVE_H = 32.0f;
        const float FLOOR_NATIVE_W = 80.0f;
        const float FLOOR_NATIVE_H = 16.0f;

        float houseW = HOUSE_NATIVE_W * HOUSE_DRAW_SCALE;
        float houseH = HOUSE_NATIVE_H * HOUSE_DRAW_SCALE;
        float houseX = 35.0f;
        float houseY = 880.0f - houseH;   // sits on the bottom platform surface

        Rectangle houseHitbox = { houseX, houseY, houseW, houseH };
        bool      houseAnimPlaying = false;
        int       houseAnimFrame = 0;
        float     houseAnimTimer = 0.0f;
        bool      houseIsSnowed = false;

        // ── Nuke state ───────────────────────────────────────────────────────────
        const float NUKE_SCALE = 1.5f;
        const float NUKE_NATIVE_W = 74.0f;
        const float NUKE_NATIVE_H = 35.0f;
        const float NUKE_EXPL_FPS = 10.0f;  // EDITABLE: explosion anim speed
        const float NUKE_FLASH_IN = 1.0f;   // EDITABLE: seconds to go full white
        const float NUKE_FLASH_OUT = 4.0f;   // EDITABLE: seconds to fade back
        const float NUKE_SHAKE_AMOUNT = 12.0f;  // EDITABLE: max shake pixels
        bool        playerHasNuke = false;
        float       nukeExtraDelay = 0.0f;
        bool        nukeExplosionPlaying = false;
        int         nukeExplosionFrame = 0;
        float       nukeExplosionTimer = 0.0f;
        Vector2     nukeExplosionPos = { 0, 0 };
        float       nukeFlashTimer = 0.0f;  // counts 0 → FLASH_IN+FLASH_OUT
        Vector2     nukeShakeOffset = { 0, 0 };
        // ────────────────────────────────────────────────────────────────────────

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

        vector<Barrel> barrels(100);
        for (auto& b : barrels) b.active = false;
        SpawnBarrelFromPool(barrels, barrelPath, 2.5f, 30.0f, 30.0f, true); // first barrel always blue

        // ── Nuke: one item spawned at a random position from the candidate list ──
        vector<Vector2> nukeSpawnNodes = {
            { 150.0f, 845.0f }, { 330.0f, 845.0f },
            { 180.0f, 693.0f }, { 480.0f, 693.0f }, { 650.0f, 693.0f },
            { 250.0f, 563.0f }, { 570.0f, 563.0f },
            { 150.0f, 433.0f }, { 500.0f, 433.0f },
            { 300.0f, 303.0f },
        };
        vector<NukeItem> nukes;
        {
            int idx = GetRandomValue(0, (int)nukeSpawnNodes.size() - 1);
            nukes.push_back({ nukeSpawnNodes[idx], true });
        }

        float spawnTimer = 0.0f;
        float spawnInterval = 10.0f;
        float minuteTimer = 0.0f;
        const float minSpawnInterval = 1.0f;

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

        Texture2D House1 = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_House_1.png");    Texture2D House2 = LoadTexture("Assets/Textures/Characters/FireSprites/Dk_House_Snowed.png");
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
            &cave1, &cave2, &cave3, &cave4, &cave5,
            &cave6, &cave7, &cave8, &cave9, &cave10, &cave11
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


        const int NUKE_EXPL_FRAME_COUNT = 6;
        Texture2D* explosionFrames[NUKE_EXPL_FRAME_COUNT] = {
            &Explosion1, &Explosion2, &Explosion3,
            &Explosion4, &Explosion5, &Explosion6
        };

        Texture2D* barrelRoll[4] = { &BarrelMov1,     &BarrelMov2,     &BarrelMov3,     &BarrelMov4 };
        Texture2D* barrelFall[2] = { &BarrelFall1,    &BarrelFall2 };
        Texture2D* blueBarrelRoll[4] = { &BlueBarrelMov1, &BlueBarrelMov2, &BlueBarrelMov3, &BlueBarrelMov4 };
        Texture2D* blueBarrelFall[2] = { &BlueBarrelFall1,&BlueBarrelFall2 };

        // ── Static beam + snow floor layer (baked once) ───────────────────────────
        float floorTileW = FLOOR_NATIVE_W * FLOOR_DRAW_SCALE;  // 48 * 4 = 192
        float floorTileH = FLOOR_NATIVE_H * FLOOR_DRAW_SCALE;  // 16 * 4 = 64
        float floorY = 880.0f - floorTileH;                 // bottom platform surface

        RenderTexture2D staticLayer = LoadRenderTexture(screenWidth, screenHeight);
        BeginTextureMode(staticLayer);
        ClearBackground(BLANK);

        // Beam tiles
        float beamScale = 4.0f;
        for (auto& pos : beamPositions)
            DrawTexturePro(beam,
                { 0, 0, (float)beam.width, (float)beam.height },
                { pos.x, pos.y, beam.width * beamScale, beam.height * beamScale },
                { 0, 0 }, 0.f, WHITE);



        EndTextureMode();
        UnloadTexture(beam);
        // SnowFloor is drawn live each frame, do not unload here

        // ── Ladder layer ─────────────────────────────────────────────────────────
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
        UnloadTexture(LadderPart);

        Texture2D* image = &imgMarioIdle;
        SetTargetFPS(60);

        float animationTimer = 0.0f;
        float animationSpeed = 0.15f;
        int   walkFrame = 0;

        // Rain animation parameters
        float rainScrollY = 0.0f;
        float rain2ScrollY = 0.0f;

        float rainSpeed = -1230.0f;
        float rain2Speed = rainSpeed * 0.8f; // slower layer

        Color rainTint = { 255, 255, 255, 80 }; // semi-transparent white
        Color rain2Tint = { 255, 255, 255, 50 }; // lighter = further away

        while (!WindowShouldClose())
        {
            if (IsKeyPressed(KEY_F1)) debugPath = !debugPath;

            // ── MENU ──────────────────────────────────────────────────────────────
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

            // ── GAMEPLAY ──────────────────────────────────────────────────────────
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

                for (auto& b : barrels) UpdateBarrel(b, barrelPath, dt);

                spawnTimer += dt;
                minuteTimer += dt;

                if (minuteTimer >= 60.0f)
                {
                    minuteTimer = 0.0f;
                    spawnInterval = max(minSpawnInterval, spawnInterval - 3.0f);
                }
                // ── Nuke pickup ────────────────────────────────────────────────
                if (!playerHasNuke)
                {
                    float nkW = NUKE_NATIVE_W * NUKE_SCALE;
                    float nkH = NUKE_NATIVE_H * NUKE_SCALE;
                    for (auto& nk : nukes)
                    {
                        if (!nk.active) continue;
                        Rectangle nkRect = { nk.pos.x, nk.pos.y, nkW, nkH };
                        if (CheckCollisionRecs(player, nkRect))
                        {
                            nk.active = false;
                            playerHasNuke = true;
                            break;
                        }
                    }
                }

                // ── Nuke detonation ────────────────────────────────────────────
                if (playerHasNuke && IsKeyPressed(KEY_F))
                {
                    // Record nuke position (above player, matching draw code)
                    float scale = 3.8f;
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
                    for (auto& b : barrels) b.active = false;
                    spawnTimer = 0.0f;
                    nukeExtraDelay = 3.0f;
                }
                if (nukeExtraDelay > 0.0f) nukeExtraDelay -= dt;

                // Explosion animation tick
                if (nukeExplosionPlaying)
                {
                    nukeExplosionTimer += dt;
                    if (nukeExplosionTimer >= 1.0f / NUKE_EXPL_FPS)
                    {
                        nukeExplosionTimer -= 1.0f / NUKE_EXPL_FPS;
                        nukeExplosionFrame++;
                        if (nukeExplosionFrame >= NUKE_EXPL_FRAME_COUNT)
                            nukeExplosionPlaying = false;
                    }
                }

                // Flash + shake tick
                float flashTotal = NUKE_FLASH_IN + NUKE_FLASH_OUT;
                if (nukeFlashTimer < flashTotal) nukeFlashTimer += dt;

                // Shake: only during fade-out, decreases over time
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

                // Apply nuke delay on top of normal spawn interval
                if (spawnTimer >= spawnInterval + max(0.0f, nukeExtraDelay))
                {
                    spawnTimer = 0.0f;
                    SpawnBarrelFromPool(barrels, barrelPath);
                }
                if (IsKeyPressed(KEY_E))
                    SpawnBarrelFromPool(barrels, barrelPath);

                // ── Barrel / player collision ──────────────────────────────────
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

                // ── Barrel / house collision ───────────────────────────────────
                if (!houseAnimPlaying)
                {
                    for (auto& b : barrels)
                    {
                        if (b.active && b.isBlue && CheckCollisionRecs(b.hitbox, houseHitbox))
                        {
                            houseAnimPlaying = true;
                            houseAnimFrame = 0;
                            houseAnimTimer = 0.0f;
                            b.active = false;
                            break;
                        }
                    }
                }

                // ── House explosion animation tick ────────────────────────────
                if (houseAnimPlaying)
                {
                    houseAnimTimer += dt;
                    float frameDuration = 1.0f / HOUSE_ANIM_FPS;
                    if (houseAnimTimer >= frameDuration)
                    {
                        houseAnimTimer -= frameDuration;
                        houseAnimFrame++;
                        if (houseAnimFrame == HOUSE_SWAP_AT_FRAME)
                            houseIsSnowed = true;
                        if (houseAnimFrame >= CAVE_FRAME_COUNT)
                        {
                            houseAnimPlaying = false;
                            houseAnimFrame = CAVE_FRAME_COUNT - 1;
                        }
                    }
                }

                // ── Ladder update ─────────────────────────────────────────────
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
                        if (ladderExitTimer >= ladderExitFrameDuration)
                        {
                            ladderExitTimer = 0.0f;
                            ladderExitStep++;
                        }
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
                            ladderClimbFrame = (ladderClimbFrame + 1) % 2;
                            ladderClimbTimer = 0;
                        }
                        image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        if (t >= 1.0f) ladderEntryClamp = false;
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
                                ladderClimbFrame = (ladderClimbFrame + 1) % 2;
                                ladderClimbTimer = 0;
                            }
                            image = (ladderClimbFrame == 0) ? &imgMarioClimb1 : &imgMarioClimb2;
                        }
                    }
                }
                else
                {
                    // ── Normal movement ─────────────────────────────────────────
                    if (IsKeyDown(KEY_D)) { player.x += playerSpeed; playerIsMoving = true; facingRight = true; }
                    if (IsKeyDown(KEY_A)) { player.x -= playerSpeed; playerIsMoving = true; facingRight = false; }

                    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S))
                    {
                        bool entered = false;
                        for (int i = 0; i < (int)ladders.size(); i++)
                        {
                            Rectangle _lhb = ladders[i].GetHitbox();
                            float     _lnw = _lhb.width * 0.5f;
                            Rectangle _lnarrow = { _lhb.x + (_lhb.width - _lnw) * 0.5f, _lhb.y, _lnw, _lhb.height };

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

                                onLadder = true; currentLadder = i;
                                ladderExitPlaying = false; ladderExitStep = 0; ladderExitTimer = 0;
                                ladderClimbFrame = 0; ladderClimbTimer = 0;
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

                    if (isJumping)          image = playerHasNuke ? &imgMarioJumpNuke : &imgMarioJump;
                    else if (playerIsMoving)
                    {
                        if (animationTimer >= animationSpeed)
                        {
                            walkFrame = (walkFrame + 1) % 2;
                            animationTimer = fmod(animationTimer, animationSpeed); // don't skip frames
                        }
                        if (playerHasNuke)
                            image = (walkFrame == 0) ? &imgMarioWalk1Nuke : &imgMarioWalk2Nuke;
                        else
                            image = (walkFrame == 0) ? &imgMarioWalk1 : &imgMarioWalk2;
                    }
                    else { image = playerHasNuke ? &imgMarioIdleNuke : &imgMarioIdle; walkFrame = 0; }
                }

                if (player.y > 900)
                {
                    player.y = 0; player.x = 440;
                    onLadder = false; currentLadder = -1;
                    ladderExitPlaying = false; ladderEntryClamp = false;
                }
            }

            
            //Rain

            rainScrollY += rainSpeed * GetFrameTime();
            rain2ScrollY += rain2Speed * GetFrameTime();

            // Loop both
            if (rainScrollY >= Rain.height)
                rainScrollY = 0.0f;

            if (rain2ScrollY >= Rain2.height)
                rain2ScrollY = 0.0f;



            // ── Draw ─────────────────────────────────────────────────────────────
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
                // Apply screen shake via camera
                Camera2D cam = { 0 };
                cam.zoom = 1.0f;
                cam.offset = nukeShakeOffset;
                BeginMode2D(cam);

                // 1. Background
                DrawTexturePro(background, { 0,0,438,475 }, { 0,0,875,950 }, {}, 0.f, WHITE);

                // 1.5 Rain2 (behind ladders, above background)
                {
                    float scaleX = (float)screenWidth / Rain2.width;
                    float scaleY = (float)screenHeight / Rain2.height;
                    float scaledW = Rain2.width * scaleX;
                    float scaledH = Rain2.height * scaleY;

                    DrawTexturePro(Rain2,
                        { 0, rain2ScrollY, (float)Rain2.width, (float)Rain2.height },
                        { 0, 0, scaledW, scaledH },
                        { 0, 0 }, 0.f, rain2Tint);

                    DrawTexturePro(Rain2,
                        { 0, 0, (float)Rain2.width, (float)Rain2.height },
                        { 0, -scaledH + rain2ScrollY, scaledW, scaledH },
                        { 0, 0 }, 0.f, rain2Tint);
                }

                // 2. Ladders
                DrawTextureRec(ladderLayer.texture, { 0,0,(float)screenWidth,-(float)screenHeight }, { 0,0 }, WHITE);

                // 3. Beams + snow floor (baked)
                DrawTextureRec(staticLayer.texture, { 0,0,(float)screenWidth,-(float)screenHeight }, { 0,0 }, WHITE);
                CollisionManager::DrawAll(platforms);

                // 4. House (64x32 native, sits on bottom-left platform)
                {
                    Texture2D& houseTex = houseIsSnowed ? House2 : House1;
                    DrawTexturePro(houseTex,
                        { 0, 0, HOUSE_NATIVE_W, HOUSE_NATIVE_H },
                        { houseX, houseY, houseW, houseH },
                        { 0, 0 }, 0.f, WHITE);

                    // Snow floor — appears over the house only after it turns snowed
                    if (houseIsSnowed)
                    {
                        float sfW = FLOOR_NATIVE_W * FLOOR_DRAW_SCALE;
                        float sfH = FLOOR_NATIVE_H * FLOOR_DRAW_SCALE;
                        float sfY = 865.0f;  // aligned to bottom beam row
                        DrawTexturePro(SnowFloor,
                            { 0, 0, FLOOR_NATIVE_W, FLOOR_NATIVE_H },
                            { houseX, sfY, sfW, sfH }, { 0,0 }, 0.f, WHITE);
                    }

                    // Explosion to the right of the house, vertically centred on it
                    if (houseAnimPlaying && houseAnimFrame < CAVE_FRAME_COUNT)
                    {
                        Texture2D* caveTex = caveFrames[houseAnimFrame];
                        float caveW = CAVE_NATIVE_W * CAVE_DRAW_SCALE;
                        float caveH = CAVE_NATIVE_H * CAVE_DRAW_SCALE;
                        float caveX = houseX + houseW - caveW * 0.65f;           // centred just inside the right edge
                        float animProgress = houseAnimFrame / (float)(CAVE_FRAME_COUNT - 1);
                        float caveY = houseY + houseH * 0.5f - caveH * 0.7f + 10.0f * animProgress; // drifts 10px down over full anim
                        DrawTexturePro(*caveTex,
                            { 0, 0, CAVE_NATIVE_W, CAVE_NATIVE_H },
                            { caveX, caveY, caveW, caveH },
                            { 0, 0 }, 0.f, WHITE);
                    }
                }

                // 4.5 Nuke items on map
                {
                    float nkW = NUKE_NATIVE_W * NUKE_SCALE;
                    float nkH = NUKE_NATIVE_H * NUKE_SCALE;
                    for (const auto& nk : nukes)
                    {
                        if (!nk.active) continue;
                        float bobY = nk.pos.y + sinf((float)GetTime() * 3.0f) * 4.0f; // ±4px bob
                        DrawTexturePro(Nuke,
                            { 0, 0, NUKE_NATIVE_W, NUKE_NATIVE_H },
                            { nk.pos.x, bobY, nkW, nkH },
                            { 0, 0 }, 0.f, WHITE);
                    }
                }

                // 5. Player
                {
                    bool showPlayer = !invincible || ((int)(invincibleTimer * 10) % 2 == 0);
                    if (showPlayer)
                    {
                        float     scale = 3.8f;
                        Rectangle src = { 0, 0, (float)image->width, (float)image->height };
                        Rectangle dest = { player.x, player.y, image->width * scale, image->height * scale };
                        if (!facingRight && !onLadder) src.width *= -1;
                        DrawTexturePro(*image, src, dest, {}, 0.f, WHITE);

                        // Nuke held above player
                        if (playerHasNuke)
                        {
                            float nkW = NUKE_NATIVE_W * (NUKE_SCALE * 0.25f) * scale;
                            float nkH = NUKE_NATIVE_H * (NUKE_SCALE * 0.25f) * scale;
                            float nkX = player.x + dest.width * 0.5f - nkW * 0.5f;
                            float nukeOffsetY = 5.0f;

                            // Recreate "moving" condition
                            bool moving = (!onLadder && !isJumping && walkFrame != 0);

                            if (moving)
                            {
                                nukeOffsetY = (walkFrame == 0) ? 5.0f : 10.0f;
                            }

                            float nkY = player.y - nkH - 2.0f + nukeOffsetY;
                            Rectangle nukeSrc = { 0, 0, NUKE_NATIVE_W, NUKE_NATIVE_H };
                            if (!facingRight) nukeSrc.width *= -1; // mirror with player
                            DrawTexturePro(Nuke, nukeSrc,
                                { nkX, nkY, nkW, nkH },
                                { 0, 0 }, 0.f, WHITE);
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
                    if (b.isFalling)
                        tex = fallSet[b.animFrame % 2];
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
                        { 0, 0, (float)tex->width, (float)tex->height },
                        { drawX, drawY, drawW, drawH },
                        { 0, 0 }, 0.f, WHITE);
                }

                // ── HUD ──────────────────────────────────────────────────────────
                for (int i = 0; i < lives; i++) DrawText("<3", 20 + i * 40, 10, 30, RED);
                if (death)      DrawText("DEATH", 10, 45, 20, ORANGE);
                if (lives <= 0) DrawText("GAME OVER", 270, 450, 50, MAROON);
                DrawText("Prueba de Donkey Kong_1", 10, screenHeight - 30, 20, WHITE);
                if (onLadder)
                    DrawText(TextFormat("Ladder: %.2f", ladderProgress), 10, screenHeight - 55, 18, YELLOW);

                int activeCount = 0;
                for (const auto& b : barrels) if (b.active) activeCount++;
                DrawText(TextFormat("Barrels: %d  Next: %.1fs  Interval: %.1fs",
                    activeCount, spawnInterval - spawnTimer, spawnInterval),
                    10, screenHeight - 80, 16, GRAY);

                if (debugPath) DrawBarrelPathDebug(barrelPath, barrels, screenHeight);

                // 8. Nuke explosion animation (drawn in world space before EndMode2D)
                if (nukeExplosionPlaying && nukeExplosionFrame < NUKE_EXPL_FRAME_COUNT)
                {
                    Texture2D* exTex = explosionFrames[nukeExplosionFrame];
                    float exScale = 4.0f;  // EDITABLE: explosion draw size
                    float exW = exTex->width * exScale;
                    float exH = exTex->height * exScale;
                    DrawTexturePro(*exTex,
                        { 0, 0, (float)exTex->width, (float)exTex->height },
                        { nukeExplosionPos.x - exW * 0.5f,
                          nukeExplosionPos.y - exH * 0.5f, exW, exH },
                        { 0, 0 }, 0.f, WHITE);
                }

                EndMode2D();

                
                // 9. Rain overlay (scrolling and looping)
                {
                    float scaleX = (float)screenWidth / Rain.width;
                    float scaleY = (float)screenHeight / Rain.height;
                    float scaledW = Rain.width * scaleX;
                    float scaledH = Rain.height * scaleY;

                    // Draw two copies vertically to create a seamless loop
                    DrawTexturePro(Rain,
                        { 0, rainScrollY, (float)Rain.width, (float)Rain.height },
                        { 0, 0, scaledW, scaledH },
                        { 0, 0 }, 0.f, rainTint);

                    DrawTexturePro(Rain,
                        { 0, 0, (float)Rain.width, (float)Rain.height },
                        { 0, -scaledH + rainScrollY, scaledW, scaledH },
                        { 0, 0 }, 0.f, rainTint);
                }
                // 10. Flash overlay — drawn outside camera so it covers full screen cleanly
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

            }

            EndDrawing();
        }

        // ── Cleanup ───────────────────────────────────────────────────────────────
        UnloadTexture(imgMarioIdle);      UnloadTexture(imgMarioWalk1);
        UnloadTexture(imgMarioWalk2);     UnloadTexture(imgMarioJump);
        UnloadTexture(imgMarioClimb1);    UnloadTexture(imgMarioClimb2);
        UnloadTexture(imgMarioClimbEnd1); UnloadTexture(imgMarioClimbEnd2);
        UnloadTexture(imgMarioClimbDown); UnloadTexture(background);
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
        UnloadTexture(cave1);             UnloadTexture(cave2);
        UnloadTexture(cave3);             UnloadTexture(cave4);
        UnloadTexture(cave5);             UnloadTexture(cave6);
        UnloadTexture(cave7);             UnloadTexture(cave8);
        UnloadTexture(cave9);             UnloadTexture(cave10);
        UnloadTexture(cave11);
        UnloadTexture(Rain);
        UnloadTexture(Rain2);
        UnloadRenderTexture(ladderLayer);
        UnloadRenderTexture(staticLayer);
        CloseAudioDevice();
        CloseWindow();
        return 0;
    }