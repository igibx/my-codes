// ========================================================================================
// SUPERGEMINI CREATOR ENGINE - V1.0 (GOD MODE)
// Jogo de Exploração em 1ª Pessoa com Geração Procedural e IA Dinâmica usando Raylib
// ========================================================================================

#include "raylib.h"
#include "raymath.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include <memory>
#include <atomic>

using namespace std::chrono_literals;

// ==============================================================================
// 1. MESTRE DA MATEMÁTICA PURA: GERAÇÃO FRACTAL PROCEDURAL
// Usando Fractal Brownian Motion (fBM) customizado para terreno infinito
// ==============================================================================
float GerarAlturaTerreno(float x, float z) {
    float total = 0.0f;
    float frequencia = 0.1f;
    float amplitude = 5.0f;
    
    // 3 oitavas de ruído puramente matemático (sem libs externas de ruído)
    for(int i = 0; i < 3; i++) {
        total += (sin(x * frequencia) + cos(z * frequencia)) * amplitude;
        amplitude *= 0.5f;
        frequencia *= 2.0f;
    }
    return total;
}

Color ObterCorBioma(float altura) {
    if (altura < -2.0f) return DARKBLUE;     // Águas profundas/magia
    if (altura < 0.0f)  return SKYBLUE;      // Raso
    if (altura < 3.0f)  return LIME;         // Planícies
    if (altura < 6.0f)  return DARKGREEN;    // Florestas Altas
    if (altura < 8.0f)  return GRAY;         // Rochas
    return WHITE;                            // Neve
}

// ==============================================================================
// 2. O COMPANHEIRO IA (ORBE MÁGICO QUE FALA E PENSA)
// ==============================================================================
struct MensagemChat {
    std::string remetente;
    std::string texto;
    Color cor;
};

class CompanheiroMagico {
public:
    Vector3 posicao;
    std::string nome;
    float tempoOrbita;
    
    std::vector<MensagemChat> historico;
    std::mutex chatMutex;
    std::atomic<bool> pensando;

    CompanheiroMagico() : posicao({0, 15, 0}), nome("Aura"), tempoOrbita(0.0f), pensando(false) {
        AdicionarMensagem(nome, "Ola, explorador! Aperte 'T' para falar comigo.", GOLD);
    }

    void AtualizarOrbita(Vector3 posJogador, float deltaTime) {
        tempoOrbita += deltaTime * 2.0f; // Velocidade da órbita
        // Matemática Pura: Órbita em formato de Lissajous flutuante
        posicao.x = posJogador.x + sin(tempoOrbita * 0.5f) * 4.0f;
        posicao.y = posJogador.y + sin(tempoOrbita * 2.0f) * 1.5f + 1.0f; // Flutua pra cima e baixo
        posicao.z = posJogador.z + cos(tempoOrbita * 0.5f) * 4.0f;
    }

    void Desenhar3D() {
        // Desenha o orbe central e anéis de energia girando
        DrawSphere(posicao, 0.6f, { 255, 215, 0, 200 }); // Ouro transparente
        DrawSphereWires(posicao, 0.8f, 16, 16, ORANGE);
        DrawSphereWires(posicao, 1.0f + sin(tempoOrbita)*0.2f, 8, 8, RED);
    }

    void AdicionarMensagem(std::string remetente, std::string texto, Color cor) {
        std::lock_guard<std::mutex> lock(chatMutex);
        historico.push_back({remetente, texto, cor});
        if (historico.size() > 8) historico.erase(historico.begin()); // Mantém só as últimas 8
    }

    void ProcessarConversa(std::string inputJogador, float alturaAtual) {
        AdicionarMensagem("Voce", inputJogador, LIGHTGRAY);
        pensando = true;

        // Thread separada para não travar os gráficos enquanto a "IA" pensa
        std::thread([this, inputJogador, alturaAtual]() {
            std::this_thread::sleep_for(1500ms); // Simula o tempo de rede/LLM
            
            std::string resposta;
            if (inputJogador == "onde estamos?") {
                if (alturaAtual > 6.0f) resposta = "Estamos em uma area montanhosa elevada! Cuidado para nao cair.";
                else if (alturaAtual < 0.0f) resposta = "Sinto muita umidade... estamos em um vale profundo.";
                else resposta = "Em um campo tranquilo gerado por algoritmos fractais.";
            } else if (inputJogador.length() > 20) {
                resposta = "Isso e muito profundo! A matematica deste mundo concorda com voce.";
            } else {
                resposta = "Interessante... vamos continuar explorando o infinito!";
            }

            AdicionarMensagem(nome, resposta, GOLD);
            pensando = false;
        }).detach();
    }
};

// ==============================================================================
// 3. MOTOR PRINCIPAL & GAME LOOP
// ==============================================================================
int main(void) {
    // Inicializa a Janela
    const int larguraTela = 1280;
    const int alturaTela = 720;
    InitWindow(larguraTela, alturaTela, "SUPERGEMINI CREATOR ENGINE - GOD MODE");
    SetTargetFPS(60);

    // Configuração da Câmera 1ª Pessoa
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 15.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 15.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    DisableCursor(); // Trava o mouse para girar a visão

    CompanheiroMagico companheiro;
    bool modoChat = false;
    std::string inputAtual = "";

    // Distância de renderização (em blocos)
    const int RENDER_DIST = 20; 

    // Loop do Motor
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ----------------------------------------------------------------------
        // LÓGICA DE INPUT E ESTADOS
        // ----------------------------------------------------------------------
        if (IsKeyPressed(KEY_T) && !modoChat) {
            modoChat = true;
            EnableCursor(); // Libera o mouse para digitar
        } else if ((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) && modoChat) {
            if (IsKeyPressed(KEY_ENTER) && !inputAtual.empty()) {
                float alturaLocal = GerarAlturaTerreno(camera.position.x, camera.position.z);
                companheiro.ProcessarConversa(inputAtual, alturaLocal);
            }
            modoChat = false;
            inputAtual = "";
            DisableCursor(); // Retorna ao jogo
        }

        // Se estiver no chat, captura o teclado. Se não, move o jogador.
        if (modoChat) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && inputAtual.length() < 50) {
                    inputAtual += (char)key;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && inputAtual.length() > 0) {
                inputAtual.pop_back();
            }
        } else {
            UpdateCamera(&camera, CAMERA_FIRST_PERSON);
            // Mantém o jogador na superfície usando a matemática fractal
            float alturaChao = GerarAlturaTerreno(camera.position.x, camera.position.z);
            if (camera.position.y < alturaChao + 2.0f) {
                camera.position.y = alturaChao + 2.0f; // Altura do olho do jogador
            }
        }

        // Atualiza a IA
        companheiro.AtualizarOrbita(camera.position, dt);

        // ----------------------------------------------------------------------
        // RENDERIZAÇÃO
        // ----------------------------------------------------------------------
        BeginDrawing();
            ClearBackground((Color){ 20, 20, 30, 255 }); // Céu noturno limpo

            BeginMode3D(camera);
                
                // Renderização Procedural de Chunks ao redor do jogador
                int playerChunkX = round(camera.position.x);
                int playerChunkZ = round(camera.position.z);

                for (int x = -RENDER_DIST; x <= RENDER_DIST; x++) {
                    for (int z = -RENDER_DIST; z <= RENDER_DIST; z++) {
                        // Otimização: Renderiza em um formato circular (distância Euclidiana)
                        if (x*x + z*z > RENDER_DIST*RENDER_DIST) continue;

                        float worldX = playerChunkX + x;
                        float worldZ = playerChunkZ + z;
                        float altura = GerarAlturaTerreno(worldX, worldZ);
                        
                        // Arredonda para dar o visual "Voxel/Minecraft"
                        int blocosAltura = round(altura);
                        
                        Vector3 posBloco = { worldX, (float)blocosAltura, worldZ };
                        Color corBloco = ObterCorBioma(altura);
                        
                        // Desenha o bloco e um leve contorno para profundidade
                        DrawCube(posBloco, 1.0f, 1.0f, 1.0f, corBloco);
                        DrawCubeWires(posBloco, 1.0f, 1.0f, 1.0f, Fade(BLACK, 0.2f));
                    }
                }

                // Desenha nosso amigo voador
                companheiro.Desenhar3D();

            EndMode3D();

            // ----------------------------------------------------------------------
            // INTERFACE 2D (HUD)
            // ----------------------------------------------------------------------
            // Mira
            if (!modoChat) {
                DrawLine(larguraTela/2 - 10, alturaTela/2, larguraTela/2 + 10, alturaTela/2, WHITE);
                DrawLine(larguraTela/2, alturaTela/2 - 10, larguraTela/2, alturaTela/2 + 10, WHITE);
                DrawText("Pressione 'T' para Falar com a IA", 20, 20, 20, LIGHTGRAY);
            }

            // Interface de Chat
            std::lock_guard<std::mutex> lock(companheiro.chatMutex);
            int startY = alturaTela - 250;
            
            // Fundo do chat
            DrawRectangle(10, startY - 10, 600, 250, Fade(BLACK, 0.6f));
            
            // Desenha Histórico
            for (const auto& msg : companheiro.historico) {
                DrawText(TextFormat("%s: %s", msg.remetente.c_str(), msg.texto.c_str()), 20, startY, 20, msg.cor);
                startY += 25;
            }

            if (companheiro.pensando) {
                DrawText("Aura esta pensando...", 20, startY, 18, GRAY);
            }

            // Caixa de Input
            if (modoChat) {
                DrawRectangle(10, alturaTela - 40, 600, 30, Fade(DARKBLUE, 0.8f));
                DrawText(TextFormat("Voce: %s_", inputAtual.c_str()), 20, alturaTela - 35, 20, WHITE);
                DrawText("[ENTER] para enviar | [ESC] para cancelar", 10, alturaTela - 60, 15, GRAY);
            }

        EndDrawing();
    }

    CloseWindow(); // Fecha o contexto OpenGL e a janela da Raylib
    return 0;
}