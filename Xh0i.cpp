#include "raylib.h"
#include <stdio.h>
#include <string>
#include <vector>

// ============================================================================
// INCLUDES REAIS DE VISÃO COMPUTACIONAL E IA (OPENCV + DNN)
// Requer OpenCV for Android SDK configurado no CMakeLists.txt
// ============================================================================
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

using namespace cv;
using namespace dnn;
using namespace std;

// Função real para converter o frame da Câmera (OpenCV) para a Tela (Raylib)
Texture2D MatToRaylibTexture(const Mat& frame) {
    Mat rgbFrame;
    // O OpenCV lê em BGR, o Raylib precisa de RGB
    cvtColor(frame, rgbFrame, COLOR_BGR2RGB);

    Image img = { 0 };
    img.data = rgbFrame.data;
    img.width = rgbFrame.cols;
    img.height = rgbFrame.rows;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    img.mipmaps = 1;

    return LoadTextureFromImage(img);
}

int main(void)
{
    InitWindow(0, 0, "Contador de Buraco Pro - IA Real");
    SetTargetFPS(30); // 30 FPS é o ideal para processamento de Câmera

    // --- ESTADO DOS JOGADORES ---
    int pontosLili = 0; 
    int pontosXaxa = 0; 
    bool mortoLili = false;
    bool mortoXaxa = false;
    int mortosDisponiveis = 2;

    // ========================================================================
    // INICIALIZAÇÃO DA CÂMERA REAL E DA INTELIGÊNCIA ARTIFICIAL
    // ========================================================================
    
    // Abre a câmera traseira do celular (ID 0 ou 1 dependendo do aparelho)
    VideoCapture cap(CAP_ANDROID); 
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);

    // Carrega a Rede Neural Real (YOLOv8 treinada para Baralho)
    // ATENÇÃO: Você precisará colocar este arquivo .onnx na pasta 'assets'
    Net aiModel;
    bool iaCarregada = false;
    try {
        aiModel = readNetFromONNX("assets/cartas_buraco_yolov8.onnx");
        aiModel.setPreferableBackend(DNN_BACKEND_OPENCV);
        aiModel.setPreferableTarget(DNN_TARGET_CPU);
        iaCarregada = true;
    } catch (...) {
        iaCarregada = false; // Arquivo da IA não encontrado
    }

    Texture2D cameraTexture = { 0 };
    Mat frame;

    // Loop Principal
    while (!WindowShouldClose()) 
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        // Variáveis de interface proporcionais
        int fTitle = sw / 18;
        int fName = sw / 8;     
        int fScore = sw / 5;    
        int fLabel = sw / 22;
        int fBtn = sw / 20;

        Vector2 touchPoint = GetMousePosition();
        bool touched = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Áreas de Interface
        int camHeight = (int)(sh * 0.40f); 
        Rectangle btnIA = { (float)sw*0.1f, camHeight + (sh*0.02f), (float)sw*0.8f, (float)sh*0.07f };
        int btnWidth = (int)(sw * 0.42f);
        int btnHeight = (int)(sh * 0.09f);
        Rectangle btnLili = { (float)sw * 0.04f, (float)sh * 0.88f, (float)btnWidth, (float)btnHeight };
        Rectangle btnXaxa = { (float)sw * 0.54f, (float)sh * 0.88f, (float)btnWidth, (float)btnHeight };

        // ====================================================================
        // CAPTURA DA CÂMERA EM TEMPO REAL
        // ====================================================================
        if (cap.isOpened()) {
            cap.read(frame); // Puxa o frame físico da lente do celular
            
            if (!frame.empty()) {
                // Se já havia uma textura da câmera no frame anterior, nós a descarregamos da GPU
                if (cameraTexture.id != 0) UnloadTexture(cameraTexture);
                
                // Converte a imagem da câmera para ser desenhada no ecrã
                cameraTexture = MatToRaylibTexture(frame);
            }
        }

        // --- LÓGICA DOS TOQUES ---
        if (touched) {
            if (CheckCollisionPointRec(touchPoint, btnLili) && !mortoLili && mortosDisponiveis > 0) { mortoLili = true; mortosDisponiveis--; }
            if (CheckCollisionPointRec(touchPoint, btnXaxa) && !mortoXaxa && mortosDisponiveis > 0) { mortoXaxa = true; mortosDisponiveis--; }
            
            // ================================================================
            // PROCESSAMENTO REAL DA IA (INFERÊNCIA YOLO)
            // ================================================================
            if (CheckCollisionPointRec(touchPoint, btnIA) && iaCarregada && !frame.empty()) {
                
                // 1. Prepara a imagem da câmera para a Inteligência Artificial ler
                Mat blob;
                blobFromImage(frame, blob, 1.0/255.0, Size(640, 640), Scalar(), true, false);
                aiModel.setInput(blob);

                // 2. Roda a Rede Neural e pega as cartas detectadas
                vector<Mat> outputDetections;
                aiModel.forward(outputDetections, aiModel.getUnconnectedOutLayersNames());

                // 3. Lógica de contagem de pontos (Exemplo Real extraindo dados do tensor)
                // Cada linha do output representa uma carta detectada [x, y, w, h, confianca, classe_id...]
                int pontosDetectados = 0;
                float *data = (float *)outputDetections[0].data;
                const int dimensions = outputDetections[0].size[2]; 
                const int rows = outputDetections[0].size[1]; 

                for (int i = 0; i < rows; ++i) {
                    float confidence = data[4]; // Índice da confiança no YOLO
                    if (confidence > 0.6) {     // Se a IA tiver 60%+ de certeza que é uma carta
                        int class_id = (int)data[5]; // ID da carta (ex: 0 = "3", 1 = "A", 2 = "K")
                        
                        // Lógica Padrão do Buraco Baseado na Classe Identificada
                        if (class_id == 0) pontosDetectados += 5;       // Cartas 3 a 7
                        else if (class_id == 1) pontosDetectados += 10; // Cartas 8 a K + Coringa
                        else if (class_id == 2) pontosDetectados += 15; // Ás
                    }
                    data += dimensions;
                }

                // Aplica os pontos à Lili ou ao Xaxá (Para um app avançado, você pode dividir a 
                // câmera ao meio, X < Width/2 é ponto pra um, X > Width/2 é ponto pro outro)
                pontosLili += pontosDetectados; 
            }
        }

        // ====================================================================
        // DESENHO NO ECRÃ
        // ====================================================================
        BeginDrawing();

            ClearBackground((Color){ 240, 240, 245, 255 }); 

            // 1. DESENHA A CÂMERA REAL 
            DrawRectangle(0, 0, sw, camHeight, BLACK);
            if (cameraTexture.id != 0) {
                // Escala a câmera para caber na parte de cima da tela
                float scale = (float)sw / cameraTexture.width;
                DrawTextureEx(cameraTexture, (Vector2){0, 0}, 0.0f, scale, WHITE);
            } else {
                DrawText("ERRO: CAMERA NAO INICIADA", sw/2 - MeasureText("ERRO: CAMERA NAO INICIADA", fLabel)/2, camHeight/2, fLabel, RED);
            }

            if (!iaCarregada) {
                DrawText("FALTA ARQUIVO DA IA (cartas_buraco_yolov8.onnx)", 10, 10, fLabel*0.8f, RED);
            }

            DrawLineEx((Vector2){0, (float)camHeight}, (Vector2){(float)sw, (float)camHeight}, 8.0f, DARKGRAY);

            // 2. BOTÃO IA
            DrawRectangleRounded(btnIA, 0.3f, 10, DARKGREEN);
            const char* txtIA = "SOLICITAR CONTAGEM DA MESA";
            DrawText(txtIA, btnIA.x + btnIA.width/2 - MeasureText(txtIA, fLabel)/2, btnIA.y + btnIA.height/2 - fLabel/2, fLabel, WHITE);

            // 3. PLACAR LILI VS XAXÁ
            int centerY = camHeight + (sh * 0.16f);

            DrawText("LILI", (sw/4) - MeasureText("LILI", fName)/2, centerY, fName, BLUE);
            char strLili[32]; sprintf(strLili, "%d", pontosLili);
            DrawText(strLili, (sw/4) - MeasureText(strLili, fScore)/2, centerY + (sh*0.06f), fScore, DARKBLUE);

            DrawText("XAXÁ", (sw*3/4) - MeasureText("XAXÁ", fName)/2, centerY, fName, RED);
            char strXaxa[32]; sprintf(strXaxa, "%d", pontosXaxa);
            DrawText(strXaxa, (sw*3/4) - MeasureText(strXaxa, fScore)/2, centerY + (sh*0.06f), fScore, MAROON);

            // 4. MORTOS
            char statusMorto[64]; sprintf(statusMorto, "MORTOS RESTANTES: %d", mortosDisponiveis);
            int mortoTextWidth = MeasureText(statusMorto, fTitle);
            DrawRectangleRounded((Rectangle){ (float)sw/2 - mortoTextWidth/2 - 10, (float)sh * 0.77f - 5, (float)mortoTextWidth + 20, (float)fTitle + 10 }, 0.5f, 10, BLACK);
            DrawText(statusMorto, sw/2 - mortoTextWidth/2, sh * 0.77f, fTitle, YELLOW);

            // 5. BOTÕES MORTOS
            DrawRectangleRounded(btnLili, 0.2f, 10, mortoLili ? LIGHTGRAY : BLUE);
            const char* tLili = mortoLili ? "LILI PEGOU" : "LILI: MORTO";
            DrawText(tLili, btnLili.x + btnLili.width/2 - MeasureText(tLili, fBtn)/2, btnLili.y + btnLili.height/2 - fBtn/2, fBtn, mortoLili ? GRAY : WHITE);

            DrawRectangleRounded(btnXaxa, 0.2f, 10, mortoXaxa ? LIGHTGRAY : RED);
            const char* tXaxa = mortoXaxa ? "XAXA PEGOU" : "XAXA: MORTO";
            DrawText(tXaxa, btnXaxa.x + btnXaxa.width/2 - MeasureText(tXaxa, fBtn)/2, btnXaxa.y + btnXaxa.height/2 - fBtn/2, fBtn, mortoXaxa ? GRAY : WHITE);

        EndDrawing();
    }

    if (cameraTexture.id != 0) UnloadTexture(cameraTexture);
    cap.release(); // Libera a câmera ao fechar
    CloseWindow();
    return 0;
}