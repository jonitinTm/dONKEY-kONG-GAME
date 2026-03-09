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
        Platform::Make(60,  750, 700, 0,  3.0f),
        Platform::Make(110, 620, 700, 0, -3.0f),
        Platform::Make(60,  490, 700, 0,  3.0f),
        Platform::Make(110, 360, 700, 0, -3.0f),
        Platform::Make(430, 250, 340, 0,  3.0f),
        Platform::Make(60,  240, 400, 0,  0.0f),
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

    float beamScale = 2.0f; // change this to whatever size you want

    DrawTexturePro(beam,
        { 0, 0, (float)beam.width, (float)beam.height },          // source rect (full texture)
        { 100, 200, beam.width * beamScale, beam.height * beamScale }, // dest rect (position + size)
        { 0, 0 }, 0.f, WHITE);

    EndTextureMode();
    UnloadTexture(beam); // source no longer needed after baking

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

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(background,
            { 0, 0, 204, 179 }, { 0, 0, 875, 950 }, {}, 0.f, WHITE);

        // Draw baked beams — single draw call, no per-frame cost
        DrawTextureRec(staticLayer.texture,
            { 0, 0, (float)screenWidth, -(float)screenHeight }, // negative H = Y flip
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