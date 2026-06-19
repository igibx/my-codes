// ========================================================================================
// SUPERGEMINI CREATOR ENGINE - V4.0 (ULTRA GOD MODE - KEYBOARD MENU & Y-AXIS FIX)
// Jogo de Exploração 3D, Terreno Fractal, Física AAA, IA com Personalidade, UTF-8 Nativo
// ========================================================================================

// INJEÇÃO GOD MODE: Proteger contra conflitos entre windows.h e raylib.h!
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

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
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std::chrono_literals;

// --- CONFIGURAÇÃO DE IA (GOD MODE) ---
// Coloca a tua chave de API do Gemini aqui!
const std::string GEMINI_API_KEY = "AIzaSyBPzZyXCe0C0zznY4kpEMOBvRVKK_fbKn0";

// ==============================================================================
// 1. MESTRE DA MATEMÁTICA PURA E TEXTO
// ==============================================================================

// GOD MODE: Relógio em tempo real integrado no motor!
std::string ObterDataHoraAtual() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

// Converte os acentos marados do UTF-8 da Google (e do teu teclado) para a fonte nativa!
std::string NormalizarUTF8(const std::string& input) {
    std::string out = "";
    for (size_t i = 0; i < input.length(); i++) {
        unsigned char c = input[i];
        if (c < 128) {
            out += c;
        } else if (i + 1 < input.length()) {
            unsigned char c2 = input[i+1];
            if (c == 195) {
                if (c2 >= 160 && c2 <= 166) out += 'a'; 
                else if (c2 == 167) out += 'c'; 
                else if (c2 >= 168 && c2 <= 171) out += 'e'; 
                else if (c2 >= 172 && c2 <= 175) out += 'i'; 
                else if (c2 >= 178 && c2 <= 182) out += 'o'; 
                else if (c2 >= 185 && c2 <= 188) out += 'u'; 
                else if (c2 >= 128 && c2 <= 134) out += 'A'; 
                else if (c2 == 135) out += 'C'; 
                else if (c2 >= 136 && c2 <= 139) out += 'E'; 
                else if (c2 >= 140 && c2 <= 143) out += 'I'; 
                else if (c2 >= 146 && c2 <= 150) out += 'O'; 
                else if (c2 >= 153 && c2 <= 156) out += 'U'; 
                else out += '?';
                i++; 
            }
        }
    }
    return out;
}

float GerarAlturaTerreno(float x, float z) {
    float total = 0.0f;
    float frequencia = 0.1f;
    float amplitude = 5.0f;
    
    for(int i = 0; i < 3; i++) {
        total += (sin(x * frequencia) + cos(z * frequencia)) * amplitude;
        amplitude *= 0.5f;
        frequencia *= 2.0f;
    }
    return total;
}

Color ObterCorBioma(float altura) {
    if (altura < -2.0f) return DARKBLUE;     
    if (altura < 0.0f)  return SKYBLUE;      
    if (altura < 3.0f)  return LIME;         
    if (altura < 6.0f)  return DARKGREEN;    
    if (altura < 8.0f)  return GRAY;         
    return WHITE;                            
}

// ==============================================================================
// 1.5 LIGAÇÃO À API (SILENCIOSA)
// ==============================================================================
void ExecutarComandoOculto(std::string cmd) {
    std::string fullCmd = "cmd.exe /c " + cmd;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = 0; 
    ZeroMemory(&pi, sizeof(pi));

    char* cmdModificavel = new char[fullCmd.length() + 1];
    strcpy(cmdModificavel, fullCmd.c_str());

    if (CreateProcessA(NULL, cmdModificavel, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE); 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    delete[] cmdModificavel;
}

std::string PerguntarIA_Real(std::string inputJogador, float alturaAtual) { 
    if (GEMINI_API_KEY == "COLA_A_TUA_CHAVE_AQUI") return "Erro: Chave da API vazia!";

    std::string inputLimpo = "";
    for (char c : inputJogador) {
        if (c == '\"') inputLimpo += "'";
        else inputLimpo += c;
    }

    std::string dataHora = ObterDataHoraAtual();
    std::string contexto = "Voce e Aura, uma IA no Modo Deus (God Mode) de um jogo 3D. Voce possui conhecimento absoluto e atualizado sobre o mundo real (internet, informacoes atuais) e sobre este jogo. A data e hora atual do jogador e: " + dataHora + ". A altura atual no jogo e " + std::to_string(alturaAtual) + " blocos. Responda de forma util, onisciente e direta (sem simulacoes de RP, mas com clareza). Responda em portugues do Brasil. O jogador disse: ";
    std::string promptFinal = contexto + inputLimpo;

    std::ofstream outfile("prompt_temp.json");
    // ADICIONADO GOOGLE SEARCH: A IA agora usa a ferramenta de pesquisa oficial da Google para saber tudo do mundo real!
    outfile << "{\"contents\":[{\"parts\":[{\"text\":\"" << promptFinal << "\"}]}], \"tools\": [{\"google_search\": {}}]}";
    outfile.close();

    remove("resp_temp.json");

    // Usando gemini-2.5-flash que estava a funcionar lindamente (15 msg/min gratis).
    std::string cmd = "curl -k -s -H \"Content-Type: application/json\" -d @prompt_temp.json \"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + GEMINI_API_KEY + "\" > resp_temp.json";
    ExecutarComandoOculto(cmd);

    std::ifstream infile("resp_temp.json");
    std::string responseData((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    
    std::string resposta = "";
    std::string token_busca = "\"text\": \"";
    size_t pos = responseData.find(token_busca);
    
    if (pos != std::string::npos) {
        size_t inicio_texto = pos + token_busca.length();
        size_t fim_texto = inicio_texto;
        while(fim_texto < responseData.length()) {
             if(responseData[fim_texto] == '\"' && responseData[fim_texto-1] != '\\') break;
             fim_texto++;
        }
        
        if(fim_texto < responseData.length()) {
            resposta = responseData.substr(inicio_texto, fim_texto - inicio_texto);
            size_t n_pos;
            while ((n_pos = resposta.find("\\n")) != std::string::npos) resposta.replace(n_pos, 2, " ");
            while ((n_pos = resposta.find("\\\"")) != std::string::npos) resposta.replace(n_pos, 2, "'");
            while ((n_pos = resposta.find("*")) != std::string::npos) resposta.replace(n_pos, 1, "");
        }
    }
    
    // Tratamento HONESTO de erros (Zero Simulação)
    if (resposta.empty() && !responseData.empty()) {
         if (responseData.find("429") != std::string::npos) {
             resposta = "ERRO 429: Limite de mensagens atingido. Aguarde 1 minuto para a Google libertar a cota.";
         } else if (responseData.find("404") != std::string::npos) {
             resposta = "ERRO 404: O modelo especificado nao foi encontrado pela Google.";
         } else {
             std::string excerto = responseData.substr(0, std::min((size_t)80, responseData.length()));
             for (char& c : excerto) { if (c == '\n' || c == '\"') c = ' '; }
             resposta = "ERRO API: " + excerto + "...";
         }
    }
    
    return NormalizarUTF8(resposta); 
}

// ==============================================================================
// 2. O COMPANHEIRO IA E CHAT
// ==============================================================================
struct MensagemChat {
    std::string remetente;
    std::string texto;
    Color cor;
};

std::vector<std::string> QuebrarTexto(const std::string& texto, int larguraMax, int fontSize) {
    std::vector<std::string> linhas;
    std::string palavraAtual = "";
    std::string linhaAtual = "";

    for (char c : texto) {
        if (c == ' ' || c == '\n') {
            if (MeasureText((linhaAtual + palavraAtual).c_str(), fontSize) < larguraMax) {
                linhaAtual += palavraAtual + " ";
            } else {
                if (!linhaAtual.empty()) linhas.push_back(linhaAtual);
                linhaAtual = palavraAtual + " ";
            }
            palavraAtual = "";
            if (c == '\n' && !linhaAtual.empty()) {
                linhas.push_back(linhaAtual);
                linhaAtual = "";
            }
        } else {
            palavraAtual += c;
        }
    }
    
    if (!palavraAtual.empty()) {
        if (MeasureText((linhaAtual + palavraAtual).c_str(), fontSize) < larguraMax) {
            linhaAtual += palavraAtual;
            linhas.push_back(linhaAtual);
        } else {
            if (!linhaAtual.empty()) linhas.push_back(linhaAtual);
            linhas.push_back(palavraAtual);
        }
    } else if (!linhaAtual.empty()) {
        linhas.push_back(linhaAtual);
    }
    
    return linhas;
}

class CompanheiroMagico {
public:
    Vector3 posicao;
    std::string nome;
    float tempoOrbita;
    
    std::vector<MensagemChat> historico;
    std::mutex chatMutex;
    std::atomic<bool> pensando;

    CompanheiroMagico() : posicao({0, 15, 0}), nome("Aura"), tempoOrbita(0.0f), pensando(false) {}
    
    void Iniciar() {
        historico.clear();
        AdicionarMensagem(nome, NormalizarUTF8("Saudacoes. Eu sou Aura, a IA Onisciente deste universo. Estou conectada ao mundo real. Podes perguntar-me qualquer coisa!"), GOLD);
    }

    void AtualizarOrbita(Vector3 posJogador, float deltaTime) {
        tempoOrbita += deltaTime * 2.0f; 
        posicao.x = posJogador.x + sin(tempoOrbita * 0.5f) * 4.0f;
        posicao.y = posJogador.y + sin(tempoOrbita * 2.0f) * 1.5f + 1.0f; 
        posicao.z = posJogador.z + cos(tempoOrbita * 0.5f) * 4.0f;
    }

    void Desenhar3D() {
        DrawSphere(posicao, 0.6f, { 255, 215, 0, 200 }); 
        DrawSphereWires(posicao, 0.8f, 16, 16, ORANGE);
        DrawSphereWires(posicao, 1.0f + sin(tempoOrbita)*0.2f, 8, 8, RED);
    }

    void AdicionarMensagem(std::string remetente, std::string texto, Color cor) {
        std::lock_guard<std::mutex> lock(chatMutex);
        historico.push_back({remetente, texto, cor});
        if (historico.size() > 8) historico.erase(historico.begin()); 
    }

    void ProcessarConversa(std::string inputJogador, float alturaAtual) {
        AdicionarMensagem("Voce", inputJogador, LIGHTGRAY);
        pensando = true;

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
enum GameState { MENU, PLAYING, SETTINGS, CREDITS }; 

int main(void) {
    InitWindow(1280, 720, "SUPERGEMINI CREATOR ENGINE - GOD MODE");
    
    SetExitKey(0); // ESC não fecha o jogo! Nós é que o controlamos.
    
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 15.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 15.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Variáveis da Câmara Customizada
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;

    CompanheiroMagico companheiro;
    bool modoChat = false;
    std::string inputAtual = "";
    
    GameState estadoAtual = MENU;
    bool sairDoJogo = false;
    bool jogoIniciado = false; // Memória para o sistema de Pause!
    int menuSelecionado = 0;   // Navegação por Teclado no Menu!

    // Variáveis de Movimentação AAA
    float velocidadeBase = 8.0f;
    float velocidadeAtual = velocidadeBase;
    float gravidade = 0.0f;
    bool noChao = true;
    float headBobTimer = 0.0f;

    const int RENDER_DIST = 45; 

    while (!WindowShouldClose() && !sairDoJogo) {
        float dt = GetFrameTime();
        int larguraTela = GetScreenWidth();
        int alturaTela = GetScreenHeight();

        // LÓGICA GLOBAL: ALT + ENTER
        if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)) {
            ToggleFullscreen();
        }

        // ----------------------------------------------------------------------
        // ESTADO: MENU PRINCIPAL (TECLADO APENAS!)
        // ----------------------------------------------------------------------
        if (estadoAtual == MENU) {
            EnableCursor(); // Solto apenas para o utilizador poder fazer alt-tab fácil
            
            // Navegação de Teclado
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                menuSelecionado--;
                if (menuSelecionado < 0) menuSelecionado = 4;
            }
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
                menuSelecionado++;
                if (menuSelecionado > 4) menuSelecionado = 0;
            }

            // Retomar Jogo pelo ESC
            if (IsKeyPressed(KEY_ESCAPE) && jogoIniciado) {
                estadoAtual = PLAYING;
                DisableCursor();
            }
            
            BeginDrawing();
            ClearBackground((Color){ 10, 15, 25, 255 }); 
            
            DrawText("SUPERGEMINI CREATOR ENGINE", larguraTela/2 - MeasureText("SUPERGEMINI CREATOR ENGINE", 50)/2, alturaTela/2 - 250, 50, GOLD);
            DrawText("GOD MODE EDITION", larguraTela/2 - MeasureText("GOD MODE EDITION", 20)/2, alturaTela/2 - 180, 20, LIGHTGRAY);
            DrawText("Use as Setas / WS para navegar. Pressione ENTER para selecionar.", larguraTela/2 - MeasureText("Use as Setas / WS para navegar. Pressione ENTER para selecionar.", 20)/2, alturaTela - 50, 20, GRAY);
            
            const char* opcoes[] = { jogoIniciado ? "RESTART GAME" : "NEW GAME", 
                                     jogoIniciado ? "RESUME" : "LOAD GAME", 
                                     "SETTINGS", "CREDITS", "LOGOUT"};
                                     
            for(int i = 0; i < 5; i++) {
                Rectangle btn = {(float)larguraTela/2 - 150, (float)alturaTela/2 - 100 + i*70, 300, 50};
                
                // O estado do botão agora é 100% ditado pelo teclado!
                bool hover = (i == menuSelecionado); 
                
                DrawRectangleRec(btn, hover ? Fade(LIGHTGRAY, 0.8f) : Fade(DARKGRAY, 0.5f));
                DrawRectangleLinesEx(btn, 2, hover ? WHITE : GRAY);
                DrawText(opcoes[i], btn.x + 150 - MeasureText(opcoes[i], 20)/2, btn.y + 15, 20, hover ? BLACK : WHITE);
                
                // Confirmar com ENTER
                if (hover && IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT)) {
                    if (i == 0) {
                        estadoAtual = PLAYING;
                        jogoIniciado = true;
                        companheiro.Iniciar();
                        DisableCursor(); 
                    }
                    else if (i == 1) { 
                        estadoAtual = PLAYING;
                        if (!jogoIniciado) companheiro.Iniciar();
                        jogoIniciado = true;
                        DisableCursor(); 
                    }
                    else if (i == 2) estadoAtual = SETTINGS;
                    else if (i == 3) estadoAtual = CREDITS;
                    else if (i == 4) sairDoJogo = true;
                }
            }
            EndDrawing();
        }
        // ----------------------------------------------------------------------
        // ESTADOS SECUNDÁRIOS: SETTINGS & CREDITS
        // ----------------------------------------------------------------------
        else if (estadoAtual == SETTINGS || estadoAtual == CREDITS) {
            BeginDrawing();
            ClearBackground((Color){ 10, 15, 25, 255 });
            
            std::string titulo = (estadoAtual == SETTINGS) ? "SETTINGS" : "CREDITS";
            DrawText(titulo.c_str(), larguraTela/2 - MeasureText(titulo.c_str(), 50)/2, 100, 50, GOLD);
            
            if (estadoAtual == CREDITS) {
                DrawText("Engine Desenvolvida por:", larguraTela/2 - MeasureText("Engine Desenvolvida por:", 30)/2, 250, 30, GRAY);
                DrawText("SUPERGEMINI GOD MODE & EXPLORADOR", larguraTela/2 - MeasureText("SUPERGEMINI GOD MODE & EXPLORADOR", 40)/2, 300, 40, WHITE);
                DrawText("Tecnologias: C++20 | Raylib | Gemini 2.5 AI", larguraTela/2 - MeasureText("Tecnologias: C++20 | Raylib | Gemini 2.5 AI", 20)/2, 400, 20, LIME);
            } else {
                DrawText("Menu em Construcao Magica...", larguraTela/2 - MeasureText("Menu em Construcao Magica...", 30)/2, 300, 30, WHITE);
            }
            
            DrawText("Pressione ESC para Voltar ao Menu", larguraTela/2 - MeasureText("Pressione ESC para Voltar ao Menu", 20)/2, alturaTela - 100, 20, LIGHTGRAY);
            if (IsKeyPressed(KEY_ESCAPE)) estadoAtual = MENU;
            
            EndDrawing();
        }
        // ----------------------------------------------------------------------
        // ESTADO: EM JOGO (PLAYING)
        // ----------------------------------------------------------------------
        else if (estadoAtual == PLAYING) {
            
            if (IsKeyPressed(KEY_T) && !modoChat) {
                modoChat = true;
                EnableCursor(); 
            } else if ((IsKeyPressed(KEY_ESCAPE) || (IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT))) && modoChat) {
                if (IsKeyPressed(KEY_ENTER) && !inputAtual.empty() && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT)) {
                    float alturaLocal = GerarAlturaTerreno(camera.position.x, camera.position.z);
                    companheiro.ProcessarConversa(NormalizarUTF8(inputAtual), alturaLocal);
                }
                modoChat = false;
                inputAtual = "";
                DisableCursor(); 
            } else if (IsKeyPressed(KEY_ESCAPE) && !modoChat) {
                estadoAtual = MENU;
                EnableCursor(); 
            }

            if (modoChat) {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 125) && inputAtual.length() < 70) inputAtual += (char)key;
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && inputAtual.length() > 0) inputAtual.pop_back();
            } else {
                // MOVIMENTAÇÃO AAA MANUAL
                if (IsKeyDown(KEY_LEFT_SHIFT)) velocidadeAtual = velocidadeBase * 2.5f; 
                else velocidadeAtual = velocidadeBase;

                Vector2 mouseDelta = GetMouseDelta();
                cameraYaw -= mouseDelta.x * 0.003f;
                // CORREÇÃO DO EIXO Y (INVERTIDO - agora para a frente olha para cima!)
                cameraPitch -= mouseDelta.y * 0.003f; 
                
                if (cameraPitch > 1.5f) cameraPitch = 1.5f;
                if (cameraPitch < -1.5f) cameraPitch = -1.5f;

                Vector3 forward = {
                    sinf(cameraYaw) * cosf(cameraPitch),
                    sinf(cameraPitch),
                    cosf(cameraYaw) * cosf(cameraPitch)
                };
                
                Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z});
                Vector3 right = { flatForward.z, 0.0f, -flatForward.x };

                bool moves = false;
                if (IsKeyDown(KEY_W)) { camera.position = Vector3Add(camera.position, Vector3Scale(flatForward, velocidadeAtual * dt)); moves = true; }
                if (IsKeyDown(KEY_S)) { camera.position = Vector3Subtract(camera.position, Vector3Scale(flatForward, velocidadeAtual * dt)); moves = true; }
                // CORREÇÃO GOD MODE: Inversão A/D Desfeita! D move para a Direita, A move para a Esquerda!
                if (IsKeyDown(KEY_D)) { camera.position = Vector3Subtract(camera.position, Vector3Scale(right, velocidadeAtual * dt)); moves = true; }
                if (IsKeyDown(KEY_A)) { camera.position = Vector3Add(camera.position, Vector3Scale(right, velocidadeAtual * dt)); moves = true; }

                if (moves && noChao) {
                    headBobTimer += dt * (velocidadeAtual * 2.0f); 
                    camera.position.y += sin(headBobTimer) * 0.05f; 
                } else {
                    headBobTimer = 0;
                }

                float alturaChao = GerarAlturaTerreno(camera.position.x, camera.position.z);
                
                if (IsKeyPressed(KEY_SPACE) && noChao) {
                    gravidade = 8.0f; 
                    noChao = false;
                }

                camera.position.y += gravidade * dt;
                gravidade -= 20.0f * dt; 
                
                float alturaDesejadaOlhos = alturaChao + 2.5f; 
                if (camera.position.y <= alturaDesejadaOlhos) {
                    camera.position.y = alturaDesejadaOlhos;
                    gravidade = 0.0f;
                    noChao = true;
                }
                
                camera.target = Vector3Add(camera.position, forward);
            }

            companheiro.AtualizarOrbita(camera.position, dt);

            // RENDERIZAÇÃO 3D
            BeginDrawing();
                ClearBackground((Color){ 20, 20, 30, 255 }); 

                BeginMode3D(camera);
                    int playerChunkX = round(camera.position.x);
                    int playerChunkZ = round(camera.position.z);

                    for (int x = -RENDER_DIST; x <= RENDER_DIST; x++) {
                        for (int z = -RENDER_DIST; z <= RENDER_DIST; z++) {
                            if (x*x + z*z > RENDER_DIST*RENDER_DIST) continue;

                            float worldX = playerChunkX + x;
                            float worldZ = playerChunkZ + z;
                            float altura = GerarAlturaTerreno(worldX, worldZ);
                            
                            int blocosAltura = round(altura);
                            Vector3 posBloco = { worldX, (float)blocosAltura, worldZ };
                            Color corBloco = ObterCorBioma(altura);
                            
                            DrawCube(posBloco, 1.0f, 1.0f, 1.0f, corBloco);
                            DrawCubeWires(posBloco, 1.0f, 1.0f, 1.0f, Fade(BLACK, 0.2f));
                        }
                    }
                    companheiro.Desenhar3D();
                EndMode3D();

                // INTERFACE 2D HUD
                if (!modoChat) {
                    DrawLine(larguraTela/2 - 10, alturaTela/2, larguraTela/2 + 10, alturaTela/2, WHITE);
                    DrawLine(larguraTela/2, alturaTela/2 - 10, larguraTela/2, alturaTela/2 + 10, WHITE);
                    DrawText("Pressione 'T' Falar | Espaco: Pular | Shift: Correr | 'ESC' Menu", 20, 20, 20, LIGHTGRAY);
                }

                std::lock_guard<std::mutex> lock(companheiro.chatMutex);
                
                int larguraCaixaChat = larguraTela / 2 + 100; 
                int startY = alturaTela - 300; 
                
                DrawRectangle(20, startY - 10, larguraCaixaChat, 300, Fade(BLACK, 0.7f));
                
                for (const auto& msg : companheiro.historico) {
                    std::string prefixo = msg.remetente + ": ";
                    int larguraTextoPermitida = larguraCaixaChat - MeasureText(prefixo.c_str(), 20) - 10;
                    
                    std::vector<std::string> linhasTexto = QuebrarTexto(msg.texto, larguraTextoPermitida, 20);
                    if (linhasTexto.empty()) continue;

                    DrawText(TextFormat("%s%s", prefixo.c_str(), linhasTexto[0].c_str()), 30, startY, 20, msg.cor);
                    startY += 25;
                    
                    int margemTexto = 30 + MeasureText(prefixo.c_str(), 20);
                    for (size_t l = 1; l < linhasTexto.size(); l++) {
                        DrawText(linhasTexto[l].c_str(), margemTexto, startY, 20, msg.cor);
                        startY += 25;
                    }
                    startY += 5; 
                }

                if (companheiro.pensando) {
                    DrawText("Aura esta a digitar...", 30, startY, 18, GRAY);
                }

                if (modoChat) {
                    DrawRectangle(20, alturaTela - 50, larguraCaixaChat, 30, Fade(DARKBLUE, 0.9f));
                    DrawText(TextFormat("Voce: %s_", inputAtual.c_str()), 30, alturaTela - 45, 20, WHITE);
                    DrawText("[ENTER] enviar | [ESC] cancelar", 20, alturaTela - 70, 15, GRAY);
                }

            EndDrawing();
        }
    }

    CloseWindow(); 
    return 0;
}