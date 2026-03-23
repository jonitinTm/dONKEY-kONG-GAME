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
    bool  isBlue = false;
    bool  isFalling = false;
    bool  movingLeft = false;
    float animTimer = 0.0f;
    int   animFrame = 0;
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
    float spd = 2.5f, float w = 30.0f, float h = 30.0f)
{
    for (auto& b : barrels)
    {
        if (!b.active)
        {
            b = SpawnBarrel(path, 0, spd, w, h);
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
            if (node.isSplitNode)
            {
                b.isFalling = (choice == 0);
                b.animFrame = 0;
            }
            b.currentNode = node.next[choice];
        }
        else if (oneValid)
        {
            int nextNode = (node.next[0] != -1) ? node.next[0] : node.next[1];
            if (node.isSplitNode)
            {
                b.isFalling = false;
                b.animFrame = 0;
            }
            b.currentNode = nextNode;
        }
        else
        {
            b.active = false;
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

    Rectangle player = { 100, 150, 60, 60 };
    float     playerSpeed = 4.0f;
    float     jumpForce = -8.0f;
    float     gravity = 0.4f;
    float     velocityX = 0.0f;
    float     velocityY = 0.0f;
    bool      isJumping = false;
    bool      facingRight = true;
    bool      isGrounded = false;

    int  lives = 3;
    bool death = false;
    bool  invincible = false;
    float invincibleTimer = 0.0f;
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
    SpawnBarrelFromPool(barrels, barrelPath);

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

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F1)) debugPath = !debugPath;

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

            if (spawnTimer >= spawnInterval)
            {
                spawnTimer = 0.0f;
                SpawnBarrelFromPool(barrels, barrelPath);
            }

            if (IsKeyPressed(KEY_E))
                SpawnBarrelFromPool(barrels, barrelPath);

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

                if (isJumping)           image = imgMarioJump;
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

            bool showPlayer = !invincible || ((int)(invincibleTimer * 10) % 2 == 0);
            if (showPlayer)
            {
                float scale = 3.8f;
                Rectangle src = { 0,0,(float)image.width,(float)image.height };
                Rectangle dest = { player.x,player.y,image.width * scale,image.height * scale };
                if (!facingRight && !onLadder) src.width *= -1;
                DrawTexturePro(image, src, dest, {}, 0.f, WHITE);
            }

            for (int i = 0; i < lives; i++) DrawText("<3", 20 + i * 40, 10, 30, RED);
            if (death)    DrawText("DEATH", 10, 45, 20, ORANGE);
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
        }

        EndDrawing();
    }

    UnloadTexture(imgMarioIdle);    UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);   UnloadTexture(imgMarioJump);
    UnloadTexture(imgMarioClimb1);  UnloadTexture(imgMarioClimb2);
    UnloadTexture(imgMarioClimbEnd1); UnloadTexture(imgMarioClimbEnd2);
    UnloadTexture(imgMarioClimbDown); UnloadTexture(background);
    UnloadTexture(BarrelMov1);      UnloadTexture(BarrelMov2);
    UnloadTexture(BarrelMov3);      UnloadTexture(BarrelMov4);
    UnloadTexture(BarrelFall1);     UnloadTexture(BarrelFall2);
    UnloadTexture(BlueBarrelMov1);  UnloadTexture(BlueBarrelMov2);
    UnloadTexture(BlueBarrelMov3);  UnloadTexture(BlueBarrelMov4);
    UnloadTexture(BlueBarrelFall1); UnloadTexture(BlueBarrelFall2);
    UnloadRenderTexture(ladderLayer);
    UnloadRenderTexture(staticLayer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}