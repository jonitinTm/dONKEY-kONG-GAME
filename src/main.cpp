#include "raylib.h"
#include "raymath.h"
#include "Collision.h"
#include "Ladder.h"

int main(void)
{
    const int screenWidth = 875;
    const int screenHeight = 950;

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

    // ── Ladder state ──────────────────────────────────────────────────────
    bool  onLadder = false;
    int   currentLadder = -1;
    float ladderProgress = 0.0f;   // 0 = bottom, 1 = top
    float ladderClimbSpeed = 2.1f;  // 70% of original 3.0

    // Climb animation (Climb1 <-> Climb2 while moving)
    int   ladderClimbFrame = 0;
    float ladderClimbTimer = 0.0f;
    float ladderClimbAnimSpeed = 0.15f;  // seconds per frame

    // Top-exit animation sequence: End1 -> End2 -> ClimbDown -> exit
    // step: 0 = End1, 1 = End2, 2 = ClimbDown (then exit)
    bool  ladderExitPlaying = false;
    int   ladderExitStep = 0;
    float ladderExitTimer = 0.0f;
    float ladderExitFrameDuration = 0.12f;  // seconds each step holds
    float ladderCooldown = 0.0f;               // blocks re-entry right after exiting

    // ── Platforms ─────────────────────────────────────────────────────────
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

    // ── Beam positions {x, y} ─────────────────────────────────────────────
    vector<Vector2> beamPositions = {
        //First Layer
        { 50, 225 }, { 114, 225 }, { 178, 225 }, { 212, 225 },
        { 276, 225 }, { 340, 225 }, { 372, 225 }, { 436, 230 },
        { 468, 230 }, { 532, 235 }, { 564, 235 }, { 628, 240 },
        { 660, 240 }, { 724, 245 },
        //Second Layer
        { 110, 370 }, { 142, 370 }, { 206, 365 }, { 238, 365 },
        { 302, 360 }, { 334, 360 }, { 398, 350 }, { 430, 350 },
        { 494, 345 }, { 526, 345 }, { 590, 340 }, { 622, 340 },
        { 686, 335 }, { 718, 335 }, { 782, 330 },
        //Third Layer
        { 54,  460 }, { 86,  460 }, { 150, 465 }, { 182, 465 },
        { 246, 470 }, { 278, 470 }, { 342, 475 }, { 374, 475 },
        { 438, 480 }, { 470, 480 }, { 534, 485 }, { 566, 485 },
        { 630, 490 }, { 662, 490 }, { 726, 495 },
        //Fourth Layer
        { 110, 625 }, { 142, 625 }, { 206, 620 }, { 238, 620 },
        { 302, 615 }, { 334, 615 }, { 398, 605 }, { 430, 605 },
        { 494, 600 }, { 526, 600 }, { 590, 595 }, { 622, 595 },
        { 686, 590 }, { 718, 590 }, { 782, 585 },
        //Fifth Layer
        { 54,  715 }, { 86,  715 }, { 150, 720 }, { 182, 720 },
        { 246, 725 }, { 278, 725 }, { 342, 730 }, { 374, 730 },
        { 438, 735 }, { 470, 735 }, { 534, 740 }, { 566, 740 },
        { 630, 745 }, { 662, 745 }, { 726, 750 },
        //Sixth Layer
        { 30,  865 }, { 94,  865 }, { 158, 865 }, { 192, 865 },
        { 256, 865 }, { 320, 865 }, { 352, 865 }, { 416, 860 },
        { 448, 860 }, { 512, 855 }, { 544, 855 }, { 608, 850 },
        { 640, 850 }, { 704, 845 }, { 768, 845 },
        //Princess Beam
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

    // ── Bake all beams into a static texture ONCE ─────────────────────────
    RenderTexture2D staticLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(staticLayer);
    ClearBackground(BLANK);
    float beamScale = 4.0f;
    for (auto& pos : beamPositions)
    {
        DrawTexturePro(beam,
            { 0, 0, (float)beam.width, (float)beam.height },
            { pos.x, pos.y, beam.width * beamScale, beam.height * beamScale },
            { 0, 0 }, 0.f, WHITE);
    }
    EndTextureMode();
    UnloadTexture(beam);

    // ── Bake all ladders into a static texture (drawn BEHIND beams) ───────
    float ladderScale = 4.0f;
    float ladderTileH = 16 * ladderScale;  // 64px per tile
    float ladderTileW = 16 * ladderScale;  // 64px wide

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
            // Draw full tile always — no clipping, so the bottom stump is visible
            DrawTexturePro(LadderPart,
                { 0, 0, 16.0f, 16.0f },
                { drawX, y, ladderTileW, ladderTileH },
                { 0, 0 }, 0.f, WHITE);
            y += ladderTileH;
        }
    }
    EndTextureMode();
    UnloadTexture(LadderPart);  // no longer needed after baking

    Texture2D image = imgMarioIdle;
    SetTargetFPS(60);

    float animationTimer = 0.0f;
    float animationSpeed = 0.15f;
    int   walkFrame = 0;

    // ── Game loop ─────────────────────────────────────────────────────────
    while (!WindowShouldClose())
    {
        float deltaTime = GetFrameTime();
        animationTimer += deltaTime;
        if (ladderCooldown > 0.0f) ladderCooldown -= deltaTime;

        float prevX = player.x;
        float prevY = player.y;

        bool playerIsMoving = false;

        // ══════════════════════════════════════════════════════════════════
        //  LADDER STATE
        // ══════════════════════════════════════════════════════════════════
        if (onLadder)
        {
            const Ladder& lad = ladders[currentLadder];

            // ── Top-exit animation playing: freeze movement, run sequence ──
            if (ladderExitPlaying)
            {
                ladderExitTimer += deltaTime;
                if (ladderExitTimer >= ladderExitFrameDuration)
                {
                    ladderExitTimer = 0.0f;
                    ladderExitStep++;
                }

                if (ladderExitStep == 0) image = imgMarioClimbEnd1;
                else if (ladderExitStep == 1) image = imgMarioClimbEnd2;
                else
                {
                    // step 2: hold ClimbDown until the player starts moving
                    image = imgMarioClimbDown;
                    bool wantsToMove = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                        || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S) || IsKeyPressed(KEY_SPACE);
                    if (wantsToMove)
                    {
                        onLadder = false;
                        currentLadder = -1;
                        ladderExitPlaying = false;
                        ladderExitStep = 0;
                        ladderExitTimer = 0.0f;
                        velocityY = 0.0f;
                        isJumping = false;
                    }
                }
            }
            // ── Normal climbing ────────────────────────────────────────────
            else
            {
                bool climbing = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);

                if (IsKeyDown(KEY_W)) ladderProgress += ladderClimbSpeed / lad.height;
                if (IsKeyDown(KEY_S)) ladderProgress -= ladderClimbSpeed / lad.height;

                if (ladderProgress < 0.0f) ladderProgress = 0.0f;
                if (ladderProgress > 1.0f) ladderProgress = 1.0f;

                // Snap player position to ladder
                player.x = lad.x + lad.width * 0.5f - player.width * 0.5f;
                player.y = lad.PlayerYAtProgress(ladderProgress, player.height);
                velocityY = 0.0f;
                velocityX = 0.0f;

                if (ladderProgress <= 0.0f)
                {
                    // Bottom: hold ClimbDown until the player starts moving
                    image = imgMarioClimbDown;
                    bool wantsToMove = IsKeyDown(KEY_A) || IsKeyDown(KEY_D)
                        || IsKeyPressed(KEY_W) || IsKeyDown(KEY_S) || IsKeyPressed(KEY_SPACE);
                    if (wantsToMove)
                    {
                        onLadder = false;
                        currentLadder = -1;
                        isGrounded = true;
                        isJumping = false;
                        ladderCooldown = 0.2f;  // prevent instant re-entry
                    }
                }
                else if (ladderProgress >= 1.0f)
                {
                    // Top: kick off exit animation sequence
                    ladderExitPlaying = true;
                    ladderExitStep = 0;
                    ladderExitTimer = 0.0f;
                    image = imgMarioClimbEnd1;
                }
                else if (climbing)
                {
                    // Mid-ladder moving: alternate Climb1 / Climb2
                    ladderClimbTimer += deltaTime;
                    if (ladderClimbTimer >= ladderClimbAnimSpeed)
                    {
                        ladderClimbFrame = (ladderClimbFrame + 1) % 2;
                        ladderClimbTimer = 0.0f;
                    }
                    image = (ladderClimbFrame == 0) ? imgMarioClimb1 : imgMarioClimb2;
                }
                // Not moving mid-ladder: hold current frame (no change to image)
            }
        }
        // ══════════════════════════════════════════════════════════════════
        //  NORMAL STATE
        // ══════════════════════════════════════════════════════════════════
        else
        {
            // ── Horizontal movement ───────────────────────────────────────
            if (IsKeyDown(KEY_D)) { player.x += playerSpeed; playerIsMoving = true; facingRight = true; }
            if (IsKeyDown(KEY_A)) { player.x -= playerSpeed; playerIsMoving = true; facingRight = false; }

            // ── W / S: try to enter a ladder, then jump if not found ──────
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S))
            {
                bool enteredLadder = false;
                for (int i = 0; i < (int)ladders.size(); i++)
                {
                    if (ladderCooldown <= 0.0f && CheckCollisionRecs(player, ladders[i].GetHitbox()))
                    {
                        float initProgress = ladders[i].ProgressFromPlayerY(
                            player.y, player.height);
                        if (initProgress < 0.01f) initProgress = 0.01f;
                        if (initProgress > 0.99f) initProgress = 0.99f;
                        ladderProgress = initProgress;

                        onLadder = true;
                        currentLadder = i;
                        ladderExitPlaying = false;
                        ladderExitStep = 0;
                        ladderExitTimer = 0.0f;
                        ladderClimbFrame = 0;
                        ladderClimbTimer = 0.0f;

                        player.x = ladders[i].x + ladders[i].width * 0.5f - player.width * 0.5f;
                        velocityY = 0.0f;
                        velocityX = 0.0f;
                        isJumping = false;
                        isGrounded = false;
                        enteredLadder = true;
                        break;
                    }
                }

                // No ladder found: only W jumps, S does nothing
                if (!enteredLadder && IsKeyPressed(KEY_W) && !isJumping)
                {
                    velocityY = jumpForce;
                    isJumping = true;
                    isGrounded = false;
                }
            }

            // SPACE always jumps
            if (IsKeyPressed(KEY_SPACE) && !isJumping)
            {
                velocityY = jumpForce;
                isJumping = true;
                isGrounded = false;
            }

            // ── Physics ───────────────────────────────────────────────────
            if (!isGrounded) velocityY += gravity;
            player.y += velocityY;

            // ── Platform collision (fully skipped when onLadder) ──────────
            CollisionResult col = CollisionManager::ResolveAll(
                player, velocityX, velocityY, platforms, prevX, prevY);

            isGrounded = col.grounded;
            if (col.grounded) isJumping = false;

            // ── Normal animation ──────────────────────────────────────────
            if (isJumping)
            {
                image = imgMarioJump;
            }
            else if (playerIsMoving)
            {
                if (animationTimer >= animationSpeed)
                {
                    walkFrame = (walkFrame + 1) % 2;
                    animationTimer = 0.0f;
                }
                image = (walkFrame == 0) ? imgMarioWalk1 : imgMarioWalk2;
            }
            else
            {
                image = imgMarioIdle;
                walkFrame = 0;
            }
        }

        // ── Out-of-bounds reset ───────────────────────────────────────────
        if (player.y > 900)
        {
            player.y = 0;
            player.x = 440;
            onLadder = false;
            currentLadder = -1;
            ladderExitPlaying = false;
        }

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(background,
            { 0, 0, 548, 308 }, { 0, 0, 875, 950 }, {}, 0.f, WHITE);

        // Ladders drawn first (behind beams)
        DrawTextureRec(ladderLayer.texture,
            { 0, 0, (float)screenWidth, -(float)screenHeight },
            { 0, 0 }, WHITE);

        DrawTextureRec(staticLayer.texture,
            { 0, 0, (float)screenWidth, -(float)screenHeight },
            { 0, 0 }, WHITE);

        CollisionManager::DrawAll(platforms);

        // Debug: ladder hitboxes — remove when done
        //for (auto& lad : ladders) lad.DrawDebug();

        // ── Player sprite ─────────────────────────────────────────────────
        float     scale = 3.8f;
        Rectangle src = { 0, 0, (float)image.width, (float)image.height };
        Rectangle dest = { player.x, player.y, image.width * scale, image.height * scale };
        if (!facingRight && !onLadder) src.width *= -1;  // no flip while climbing
        DrawTexturePro(image, src, dest, {}, 0.f, WHITE);

        DrawText("Prueba de Donkey Kong_1", 10, 10, 20, WHITE);

        if (onLadder)
            DrawText(TextFormat("Ladder: %.2f", ladderProgress), 10, 35, 18, YELLOW);

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    UnloadTexture(imgMarioIdle);
    UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);
    UnloadTexture(imgMarioJump);
    UnloadTexture(imgMarioClimb1);
    UnloadTexture(imgMarioClimb2);
    UnloadTexture(imgMarioClimbEnd1);
    UnloadTexture(imgMarioClimbEnd2);
    UnloadTexture(imgMarioClimbDown);
    UnloadTexture(background);
    UnloadRenderTexture(ladderLayer);
    UnloadRenderTexture(staticLayer);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}