

#include "raylib.h"
//Pare que no se queje de los booleanos
#include <stdbool.h>

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 875;
    const int screenHeight = 950;


    //Creatin of the character
    // Creacion del personaje a partir de un cuadrado {x, y, width, height}
    Rectangle player = { 100, 150, 30, 30 };

    // Velocidad del jugador al presionar alguna tecla
    float playerSpeed = 4.0f;
    // Ahora vamos a  definir las variables de salto (Tener en cuenta que para ir hacia arriba del eje Y es nagativo y para abajo el positivo
    float jumpForce = -8.0f;
    // para simular la grabedad creamos otra variable que hara la velocidad que ira hacia abajop por cada frame
    float gravity = 0.4f;
    //esta variable hace el salto mas realista, haciendo que vaya bajando poco a poco progresivamente
    float velocityY = 0;
    //para evitar el do0ble salto hacemos una variable boleana para saber el estado de salto
    bool isJumping = false;
    //creacion del suelo {x, y, ancho, alto}
    Rectangle ground = { 0, 400, 800, 50 };
    







    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    Texture2D imgMarioIdle = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");
    Texture2D imgMarioWalk1 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk1.png");
    Texture2D imgMarioWalk2 = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Walk2.png");
    Texture2D imgMarioJump = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Jump.png");
    Vector2 marioPos{ player.x, player.y };
    Texture2D image = LoadTexture("Assets/Textures/Characters/Mario/Dk_Mario_Idle1.png");

    Texture2D background = LoadTexture("Assets/stage1.png");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    //Initialize DeltaTime
    float timer = 0.0f;

    //Initialize animation Frames (Player)
    float animationTimer = 0.0f;
    float animationSpeed = 0.15f;   // change frame every 0.15 seconds
    int walkFrame = 0;

    //Player Mov Bool
    bool facingRight = true; // true = right, false = left

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------

        //Get DeltaTime
        float deltaTime = GetFrameTime();  // time since last frame
        animationTimer += deltaTime;
        timer += deltaTime;

        //init inside bool mov vars for player
        bool PlayerisMoving = false;

        // moverse de derecha a izquierda
        if (IsKeyDown(KEY_D)) {
            player.x += playerSpeed;
            PlayerisMoving = true;
            facingRight = true; 
        }

        if (IsKeyDown(KEY_A)) {
            player.x -= playerSpeed;
            PlayerisMoving = true;
            facingRight = false;
        }

        // salto

        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W)) && !isJumping)
        {
            velocityY = jumpForce;
            isJumping = true;
        }

        //Playing Player Animations
        if (isJumping) {
            image = imgMarioJump;
        }
        else {
            if (PlayerisMoving)
            {
                if (animationTimer >= animationSpeed)
                {
                    walkFrame++;
                    if (walkFrame > 1) walkFrame = 0;  // only 2 frames (0 and 1)
                    animationTimer = 0.0f;
                }

                if (walkFrame == 0)
                    image = imgMarioWalk1;
                else
                    image = imgMarioWalk2;
            }
            else
            {
                image = imgMarioIdle;
                walkFrame = 0;
            }
        }
        
        
        
        


        //poner gravedad
        velocityY += gravity;
        player.y += velocityY;


        //tener colision con el suelo

        if (CheckCollisionRecs(player, ground))
        {
            player.y = ground.y - player.height;
            velocityY = 0;
            isJumping = false;
        }








        BeginDrawing();

        ClearBackground(BLACK);



        //poner suelo
        DrawRectangleRec(ground, DARKBROWN);
        //poner al jugador
        DrawRectangleRec(player, {0,0,0,0});

        DrawText("Prueba de Donkey Kong_1", 10, 10, 20, WHITE);
        //poner imagen
        float scale = 2.0f;

        Vector2 origin = { 0.0f, 0.0f };
        Rectangle dest = { player.x , player.y ,image.width * scale, image.height * scale };
        Rectangle src = { 0, 0, image.width, image.height };

        if (!facingRight) src.width *= -1;

        DrawTexturePro(image, src, dest, origin, 0.0f, WHITE);


        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(image);
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}