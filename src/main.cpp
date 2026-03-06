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

    // FIX: track grounded across frames so we can skip gravity when on ground.
    // Without this, gravity pushes the player down every frame, the collision
    // corrects them back up+sideways, and that sideways push causes sliding.
    bool isGrounded = false;

    // ── Platforms ─────────────────────────────────────────────────────────
    // Platform::Make(x, y, width, height)                    flat
    // Platform::Make(x, y, width, height, 20.0f)             tilted right
    // Platform::Make(x, y, width, height, -15.0f, RED)       tilted left + color
    vector<Platform> platforms = {
        Platform::Make(0,  880, 875, 30,   0.0f, DARKBROWN),  // ground
        Platform::Make(200,  825, 200, 20,   -30.0f),  // flat platform
        Platform::Make(500,  790, 150, 20,   0.0f),  // flat platform
    };

    // ── Window & assets ───────────────────────────────────────────────────
    InitWindow(screenWidth, screenHeight, "Donkey Kong");

    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Texture2D image = imgMarioIdle;
    Texture2D background = LoadTexture("Wiki/stage1_P.png");

    SetTargetFPS(60);

    float animationTimer = 0.0f;
    float animationSpeed = 0.15f;
    int   walkFrame = 0;

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
        // Only apply gravity when airborne – this stops the per-frame
        // gravity+correction cycle that causes sliding on rotated platforms
        if (!isGrounded)
            velocityY += gravity;

        player.y += velocityY;

        // ── Collision ─────────────────────────────────────────────────────
        CollisionResult col = CollisionManager::ResolveAll(
            player, velocityX, velocityY,
            platforms,
            prevX, prevY
        );

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
            { 0, 0, 204, 179 }, { 0, 0, 875, 950 },
            { 0.0f, 0.0f }, 0.0f, WHITE);

        CollisionManager::DrawAll(platforms);

        float     scale = 3.8f;
        Rectangle src = { 0, 0, (float)image.width, (float)image.height };
        Rectangle dest = { player.x, player.y, image.width * scale, image.height * scale };
        if (!facingRight) src.width *= -1;
        DrawTexturePro(image, src, dest, { 0.0f, 0.0f }, 0.0f, WHITE);

        DrawText("Prueba de Donkey Kong_1", 10, 10, 20, WHITE);

        EndDrawing();
    }

    UnloadTexture(imgMarioIdle);
    UnloadTexture(imgMarioWalk1);
    UnloadTexture(imgMarioWalk2);
    UnloadTexture(imgMarioJump);
    UnloadTexture(background);
    CloseWindow();

    return 0;
}