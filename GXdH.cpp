#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// DISJUNTOR DE SEGURANÇA DA CÂMERA REAL DO ANDROID
// ============================================================================
// Amigo, para mudar o valor abaixo de 0 para 1, você DEVE PRIMEIRO abrir o 
// seu ficheiro "app/src/main/cpp/CMakeLists.txt" e procurar por: target_link_libraries
// Você tem de adicionar: camera2ndk e mediandk
// Exemplo: target_link_libraries(raymob log android EGL GLESv2 camera2ndk mediandk)
// Se colocar 1 sem fazer isso, a compilação do Android vai dar ERRO FATAL.
#define ATIVAR_CAMERA_REAL 0

#if ATIVAR_CAMERA_REAL && defined(__ANDROID__)
    #include <camera/NdkCameraManager.h>
    #include <camera/NdkCameraDevice.h>
    #include <media/NdkImageReader.h>
    // Aqui ficaria toda a implementação assíncrona complexa do NDK
    // AImageReader* imageReader = nullptr; ...
#endif

// ============================================================================
// MOTOR PRINCIPAL
// ============================================================================
int main(void)
{
    InitWindow(0, 0, "Contador de Buraco Pro");
    SetTargetFPS(60);

    // --- ESTADO DOS JOGADORES ---
    int pontosLili = 0; 
    int pontosXaxa = 0; 
    
    bool mortoLili = false;
    bool mortoXaxa = false;
    int mortosDisponiveis = 2;

    // --- VARIÁVEIS VISUAIS ---
    float scanlineY = 0.0f;
    Color corFundo = { 240, 240, 245, 255 }; // Cinza bem claro e moderno

    // Loop Principal
    while (!WindowShouldClose()) 
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        // Tamanhos dinâmicos baseados no tamanho do seu telemóvel
        int fTitle = sw / 18;
        int fName = sw / 8;     // Nomes bem grandes
        int fScore = sw / 5;    // Pontuação gigante
        int fLabel = sw / 22;
        int fBtn = sw / 20;

        Vector2 touchPoint = GetMousePosition();
        bool touched = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // --- ÁREAS DE INTERFACE ---
        int camHeight = (int)(sh * 0.40f); // Câmera ocupa 40% da tela agora

        // Botão Central de IA
        Rectangle btnIA = { (float)sw*0.1f, camHeight + (sh*0.02f), (float)sw*0.8f, (float)sh*0.07f };

        // Botões Gigantes dos Mortos na parte inferior
        int btnWidth = (int)(sw * 0.42f);
        int btnHeight = (int)(sh * 0.09f);
        Rectangle btnLili = { (float)sw * 0.04f, (float)sh * 0.88f, (float)btnWidth, (float)btnHeight };
        Rectangle btnXaxa = { (float)sw * 0.54f, (float)sh * 0.88f, (float)btnWidth, (float)btnHeight };

        // --- LÓGICA DOS TOQUES ---
        if (touched) {
            // Pegar Morto: LILI
            if (CheckCollisionPointRec(touchPoint, btnLili) && !mortoLili && mortosDisponiveis > 0) {
                mortoLili = true;
                mortosDisponiveis--;
            }
            // Pegar Morto: XAXÁ
            if (CheckCollisionPointRec(touchPoint, btnXaxa) && !mortoXaxa && mortosDisponiveis > 0) {
                mortoXaxa = true;
                mortosDisponiveis--;
            }
            // Botão de IA (Simulação de Contagem de Cartas da Câmera)
            if (CheckCollisionPointRec(touchPoint, btnIA)) {
                // Como não temos a IA do YOLO treinada ainda, 
                // simulamos que a câmera leu cartas na mesa e calculou os pontos
                pontosLili += GetRandomValue(10, 100); 
                pontosXaxa += GetRandomValue(10, 100);
            }
        }

        // Animação da Câmera (Radar verde)
        scanlineY += 4.0f;
        if (scanlineY > camHeight) scanlineY = 0.0f;

        // ====================================================================
        // DESENHO NO ECRÃ
        // ====================================================================
        BeginDrawing();

            ClearBackground(corFundo); 

            // ----------------------------------------------------------------
            // 1. ÁREA DA CÂMERA DE VISÃO COMPUTACIONAL
            // ----------------------------------------------------------------
            DrawRectangle(0, 0, sw, camHeight, BLACK);
            
            if (ATIVAR_CAMERA_REAL == 0) {
                // Modo Simulação (Para não dar erro no Android)
                const char* camTxt = "CAMERA REAL DESATIVADA NO CODIGO";
                DrawText(camTxt, sw/2 - MeasureText(camTxt, fLabel)/2, camHeight/2 - 10, fLabel, RED);
                const char* iaTxt = "(Aguardando Integracao de IA - YOLOv8)";
                DrawText(iaTxt, sw/2 - MeasureText(iaTxt, fLabel*0.8f)/2, camHeight/2 + 20, (int)(fLabel*0.8f), GRAY);
                
                // Desenha a linha verde de Scan
                DrawLineEx((Vector2){0, scanlineY}, (Vector2){(float)sw, scanlineY}, 3.0f, LIME);
            } else {
                // Aqui seria renderizada a Textura com os bytes do AImageReader
                const char* camTxt = "LENDO IMAGEM DA CAMERA...";
                DrawText(camTxt, sw/2 - MeasureText(camTxt, fLabel)/2, camHeight/2, fLabel, GREEN);
            }
            
            DrawLineEx((Vector2){0, (float)camHeight}, (Vector2){(float)sw, (float)camHeight}, 8.0f, DARKGRAY);

            // ----------------------------------------------------------------
            // 2. BOTÃO DE ACIONAR A CONTAGEM (IA)
            // ----------------------------------------------------------------
            DrawRectangleRounded(btnIA, 0.3f, 10, DARKGREEN);
            DrawRectangleRoundedLines(btnIA, 0.3f, 10, GREEN);
            const char* txtIA = "SOLICITAR CONTAGEM DA MESA";
            DrawText(txtIA, btnIA.x + btnIA.width/2 - MeasureText(txtIA, fLabel)/2, btnIA.y + btnIA.height/2 - fLabel/2, fLabel, WHITE);

            // ----------------------------------------------------------------
            // 3. PLACAR PRINCIPAL (LILI VS XAXÁ)
            // ----------------------------------------------------------------
            int centerY = camHeight + (sh * 0.16f);

            // -> LILI (ESQUERDA - AZUL)
            DrawText("LILI", (sw/4) - MeasureText("LILI", fName)/2, centerY, fName, BLUE);
            char strLili[32];
            sprintf(strLili, "%d", pontosLili);
            DrawText(strLili, (sw/4) - MeasureText(strLili, fScore)/2, centerY + (sh*0.06f), fScore, DARKBLUE);
            DrawText("pontos", (sw/4) - MeasureText("pontos", fLabel)/2, centerY + (sh*0.17f), fLabel, GRAY);

            // -> XAXÁ (DIREITA - VERMELHO)
            DrawText("XAXÁ", (sw*3/4) - MeasureText("XAXÁ", fName)/2, centerY, fName, RED);
            char strXaxa[32];
            sprintf(strXaxa, "%d", pontosXaxa);
            DrawText(strXaxa, (sw*3/4) - MeasureText(strXaxa, fScore)/2, centerY + (sh*0.06f), fScore, MAROON);
            DrawText("pontos", (sw*3/4) - MeasureText("pontos", fLabel)/2, centerY + (sh*0.17f), fLabel, GRAY);

            // ----------------------------------------------------------------
            // 4. STATUS DOS MORTOS
            // ----------------------------------------------------------------
            char statusMorto[64];
            sprintf(statusMorto, "MORTOS RESTANTES: %d", mortosDisponiveis);
            int mortoTextWidth = MeasureText(statusMorto, fTitle);
            
            // Fundo pretinho atrás do texto dos mortos para dar destaque
            Rectangle recMorto = { (float)sw/2 - mortoTextWidth/2 - 10, (float)sh * 0.77f - 5, (float)mortoTextWidth + 20, (float)fTitle + 10 };
            DrawRectangleRounded(recMorto, 0.5f, 10, BLACK);
            DrawText(statusMorto, sw/2 - mortoTextWidth/2, sh * 0.77f, fTitle, YELLOW);

            // ----------------------------------------------------------------
            // 5. BOTÕES DE PEGAR O MORTO
            // ----------------------------------------------------------------
            // LILI
            DrawRectangleRounded(btnLili, 0.2f, 10, mortoLili ? LIGHTGRAY : BLUE);
            DrawRectangleRoundedLines(btnLili, 0.2f, 10, mortoLili ? GRAY : DARKBLUE);
            const char* tLili = mortoLili ? "LILI PEGOU" : "LILI: MORTO";
            DrawText(tLili, btnLili.x + btnLili.width/2 - MeasureText(tLili, fBtn)/2, btnLili.y + btnLili.height/2 - fBtn/2, fBtn, mortoLili ? GRAY : WHITE);

            // XAXÁ
            DrawRectangleRounded(btnXaxa, 0.2f, 10, mortoXaxa ? LIGHTGRAY : RED);
            DrawRectangleRoundedLines(btnXaxa, 0.2f, 10, mortoXaxa ? GRAY : MAROON);
            const char* tXaxa = mortoXaxa ? "XAXA PEGOU" : "XAXA: MORTO";
            DrawText(tXaxa, btnXaxa.x + btnXaxa.width/2 - MeasureText(tXaxa, fBtn)/2, btnXaxa.y + btnXaxa.height/2 - fBtn/2, fBtn, mortoXaxa ? GRAY : WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}