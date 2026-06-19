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
#include <fstream>
#include <cstdlib>

using namespace std::chrono_literals;

// --- CONFIGURAÇÃO DE IA (GOD MODE) ---
// Coloca a tua chave de API do Gemini aqui! (É gratuita no Google AI Studio)
const std::string GEMINI_API_KEY = "AIzaSyDlNoZhsNw_bbMsv9aGKAO89f68LtwRLik";

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
// 1.5 INJEÇÃO GOD MODE: LIGAÇÃO À API REAL (GEMINI FREE)
// Usa chamadas de sistema locais para conectar à internet sem bibliotecas extra!
// ==============================================================================
std::string PerguntarIA_Real(std::string inputJogador, float alturaAtual) {
    if (GEMINI_API_KEY == "COLA_A_TUA_CHAVE_AQUI") {
        return "Erro: Precisas de criar uma chave no Google AI Studio e colar no codigo!";
    }

    // 1. Limpar aspas do input do jogador para não corromper o JSON
    std::string inputLimpo = "";
    for (char c : inputJogador) {
        if (c == '\"') inputLimpo += "'";
        else inputLimpo += c;
    }

    // Criar o contexto do mundo para a IA
    std::string contexto = "Estamos num mundo infinito de blocos. A minha altura atual e " + std::to_string(alturaAtual) + ". Responde como um companheiro de aventura magico e curto (maximo 2 frases). O jogador disse: ";
    std::string promptFinal = contexto + inputLimpo;

    // God Mode Hack: Usar C++ nativo para gerar um ficheiro JSON e chamar o cURL
    std::ofstream outfile("prompt_temp.json");
    outfile << "{\"contents\":[{\"parts\":[{\"text\":\"" << promptFinal << "\"}]}]}";
    outfile.close();

    // 2. CORREÇÃO GOD MODE: Atualizando para o modelo Gemini 2.5 Flash que está ativo na API!
    std::string cmd = "curl -k -s -H \"Content-Type: application/json\" -d @prompt_temp.json \"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + GEMINI_API_KEY + "\" > resp_temp.json";
    system(cmd.c_str());

    // Ler a resposta e extrair o texto
    std::ifstream infile("resp_temp.json");
    std::string responseData((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    
    std::string resposta = "";
    
    // 3. Parser nível GOD MODE: Simplificado e muito mais agressivo
    std::string token_busca = "\"text\": \"";
    size_t pos = responseData.find(token_busca);
    
    if (pos != std::string::npos) {
        size_t inicio_texto = pos + token_busca.length();
        // A resposta termina na primeira aspa que não tenha um escape (backslash) antes
        size_t fim_texto = inicio_texto;
        while(fim_texto < responseData.length()) {
             if(responseData[fim_texto] == '\"' && responseData[fim_texto-1] != '\\') {
                 break;
             }
             fim_texto++;
        }
        
        if(fim_texto < responseData.length()) {
            resposta = responseData.substr(inicio_texto, fim_texto - inicio_texto);
            
            // Limpar quebras de linha JSON (\n) e aspas escapadas (\")
            size_t n_pos;
            while ((n_pos = resposta.find("\\n")) != std::string::npos) {
                resposta.replace(n_pos, 2, " ");
            }
            while ((n_pos = resposta.find("\\\"")) != std::string::npos) {
                resposta.replace(n_pos, 2, "'");
            }
        }
    }
    
    // 4. Se a resposta continuar vazia, vamos imprimir os primeiros 100 caracteres do JSON para debug!
    if (resposta.empty()) {
        if (responseData.empty()) {
            resposta = "ERRO: O ficheiro de resposta esta vazio. Sem internet ou falha no cURL.";
        } else {
             // Imprime o início do erro para o chat para percebermos o que falhou
             std::string excerto = responseData.substr(0, std::min((size_t)80, responseData.length()));
             // Limpa aspas e \n do excerto para não quebrar a nossa UI de chat
             for (char& c : excerto) { if (c == '\n' || c == '\"') c = ' '; }
             resposta = "ERRO API: " + excerto + "... (ve o log_api.txt)";
        }
        
        std::ofstream log("log_api.txt");
        log << responseData;
        log.close();
    }
    
    // Agora nao apagamos os temporários para podermos inspecionar se algo der errado!
    return resposta;
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

        // Thread separada para não travar os gráficos enquanto a IA processa na rede
        std::thread([this, inputJogador, alturaAtual]() {
            std::string resposta = PerguntarIA_Real(inputJogador, alturaAtual);
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

    // Distância de renderização (em blocos) AUMENTADA A PERDER DE VISTA!
    const int RENDER_DIST = 45; 

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
        
        // Corrige o "Voo": Matemática para colar o jogador ao chão e ajustar a mira
        float alturaChao = GerarAlturaTerreno(camera.position.x, camera.position.z);
        float alturaDesejada = alturaChao + 2.5f; // Altura dos olhos do jogador
        
        float diffY = alturaDesejada - camera.position.y;
        camera.position.y = alturaDesejada;
        camera.target.y += diffY; // Move a mira na mesma proporção para não desalinhar
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