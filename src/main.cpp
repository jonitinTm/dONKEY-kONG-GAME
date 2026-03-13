#include "raylib.h"
#include "Collision.h"

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

    // ── Beam positions {x, y} ─────────────────────────────────────────────
    vector<Vector2> beamPositions = {
        //First Layer
        { 50, 225 },
        { 114, 225 },
        { 178, 225 },
        { 212, 225 },
        { 276, 225 },
        { 340, 225 },
        { 372, 225 },
        { 436, 230 },
        { 468, 230 },
        { 532, 235 },
        { 564, 235 },
        { 628, 240 },
        { 660, 240 },
        { 724, 245 },
        //Second Layer
        { 110, 370 },
        { 142, 370 },
        { 206, 365 },
        { 238, 365 },
        { 302, 360 },
        { 334, 360 },
        { 398, 350 },
        { 430, 350 },
        { 494, 345 },
        { 526, 345 },
        { 590, 340 },
        { 622, 340 },
        { 686, 335 },
        { 718, 335 },
        { 782, 330 },
        //Third Layer
        { 54,  460 },
        { 86,  460 },
        { 150, 465 },
        { 182, 465 },
        { 246, 470 },
        { 278, 470 },
        { 342, 475 },
        { 374, 475 },
        { 438, 480 },
        { 470, 480 },
        { 534, 485 },
        { 566, 485 },
        { 630, 490 },
        { 662, 490 },
        { 726, 495 },
        //Fourth Layer (like second, goes down)
        { 110, 625 },
        { 142, 625 },
        { 206, 620 },
        { 238, 620 },
        { 302, 615 },
        { 334, 615 },
        { 398, 605 },
        { 430, 605 },
        { 494, 600 },
        { 526, 600 },
        { 590, 595 },
        { 622, 595 },
        { 686, 590 },
        { 718, 590 },
        { 782, 585 },
        //Fifth Layer (like third, goes up)
        { 54,  715 },
        { 86,  715 },
        { 150, 720 },
        { 182, 720 },
        { 246, 725 },
        { 278, 725 },
        { 342, 730 },
        { 374, 730 },
        { 438, 735 },
        { 470, 735 },
        { 534, 740 },
        { 566, 740 },
        { 630, 745 },
        { 662, 745 },
        { 726, 750 },
        //Sixth Layer (like first, goes up)
        { 30,  865 },
        { 94,  865 },
        { 158, 865 },
        { 192, 865 },
        { 256, 865 },
        { 320, 865 },
        { 352, 865 },
        { 416, 860 },
        { 448, 860 },
        { 512, 855 },
        { 544, 855 },
        { 608, 850 },
        { 640, 850 },
        { 704, 845 },
        { 768, 845 },
        //Princess Beam
        { 360, 120 },
        { 424, 120 },
        { 456, 120 },
        { 296, 150 },
        { 264, 150 },



        // keep adding as many as you want here
    };

    // ── Window & assets ───────────────────────────────────────────────────
    InitWindow(screenWidth, screenHeight, "Donkey Kong");
    InitAudioDevice();

    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Texture2D background = LoadTexture("Wiki/stage1_P.png");
    Texture2D beam = LoadTexture("Assets/Textures/Architecture/Dk_FloorPart.png");

    // ── Bake all beams into a static texture ONCE before the loop ─────────
    RenderTexture2D staticLayer = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(staticLayer);
    ClearBackground(BLANK);

    float beamScale = 4.0f; // doubled from 2.0f → twice as large

    for (auto& pos : beamPositions)
    {
        DrawTexturePro(beam,
            { 0, 0, (float)beam.width, (float)beam.height },
            { pos.x, pos.y, beam.width * beamScale, beam.height * beamScale },
            { 0, 0 }, 0.f, WHITE);
    }

    EndTextureMode();
    UnloadTexture(beam); // no longer needed after baking

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

        float prevX = player.x;
        float prevY = player.y;

        // ── Input ─────────────────────────────────────────────────────────
        bool playerIsMoving = false;
        if (IsKeyDown(KEY_D)) { player.x += playerSpeed; playerIsMoving = true; facingRight = true; }
        if (IsKeyDown(KEY_A)) { player.x -= playerSpeed; playerIsMoving = true; facingRight = false; }

        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W)) && !isJumping)
        {
            velocityY = jumpForce;
            isJumping = true;
            isGrounded = false;
        }

        // ── Physics ───────────────────────────────────────────────────────
        if (!isGrounded) velocityY += gravity;
        player.y += velocityY;

        // ── Collision ─────────────────────────────────────────────────────
        CollisionResult col = CollisionManager::ResolveAll(
            player, velocityX, velocityY, platforms, prevX, prevY);

        isGrounded = col.grounded;
        if (col.grounded) isJumping = false;

        // ── Animation ─────────────────────────────────────────────────────
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

        if (player.y > 900) 
        { 
            player.y = 0;
            player.x = 440;
        }

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(background,
            { 0, 0, 204, 179 }, { 0, 0, 875, 950 }, {}, 0.f, WHITE);

        // Draw baked beams — single draw call, no per-frame cost
        DrawTextureRec(staticLayer.texture,
            { 0, 0, (float)screenWidth, -(float)screenHeight },
            { 0, 0 }, WHITE);

        CollisionManager::DrawAll(platforms);

        float     scale = 3.8f;
        Rectangle src = { 0, 0, (float)image.width, (float)image.height };
        Rectangle dest = { player.x, player.y, image.width * scale, image.height * scale };
        if (!facingRight) src.width *= -1;
        DrawTexturePro(image, src, dest, {}, 0.f, WHITE);

        DrawText("Prueba de Donkey Kong_1", 10, 10, 20, WHITE);
        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    UnloadTexture(imgMarioIdle);
    UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);
    UnloadTexture(imgMarioJump);
    UnloadTexture(background);
    UnloadRenderTexture(staticLayer);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}