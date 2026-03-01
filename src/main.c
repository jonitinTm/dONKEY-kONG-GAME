

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
    const int screenWidth = 800;
    const int screenHeight = 450;

    //Creatin of the character
    // Creacion del personaje a partir de un cuadrado {x, y, width, height}
    Rectangle player = { 100, 350, 30, 30 };
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

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------


        // moverse de derecha a izquierda
        if (IsKeyDown(KEY_RIGHT)) player.x += playerSpeed;
        if (IsKeyDown(KEY_LEFT)) player.x -= playerSpeed;

        // salto

        if (IsKeyPressed(KEY_SPACE) && !isJumping)
        {
            velocityY = jumpForce;
            isJumping = true;
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
        DrawRectangleRec(player, RED);

        DrawText("Prueba de Donkey Kong_1", 10, 10, 20, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}