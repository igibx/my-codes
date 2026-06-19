#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <random>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

// =====================================================================
// CONSTANTES E CONFIGURAÇÕES DO MUNDO 3D
// =====================================================================
const int BOARD_WIDTH = 10;
const int BOARD_HEIGHT = 20;
const int SCREEN_WIDTH = 1920;  // FULL HD
const int SCREEN_HEIGHT = 1080; // FULL HD
const float CUBE_SIZE = 1.0f;

// Cores Neon Aprimoradas (Mais saturação para o efeito Bloom)
const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 30, 144, 255, 255 }; // Dodger Blue
const Color C_ORANGE = { 255, 140, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 100, 255 };
const Color C_PURPLE = { 200, 0, 255, 255 };
const Color C_RED    = { 255, 20, 60, 255 };
const Color C_BG     = { 2, 4, 10, 255 }; // Espaço profundo mais escuro para o Neon brilhar

Color pieceColors[8] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED };

// =====================================================================
// SISTEMA DE PARTÍCULAS 3D VOLUMÉTRICAS
// =====================================================================
struct Particle3D {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    bool isSpark; // Diferencia cubos de faíscas longas
};

vector<Particle3D> particles;

float GetRandomFloat(float min, float max) {
    return min + (max - min) * ((float)rand() / RAND_MAX);
}

void SpawnParticles3D(Vector3 pos, Color color, int amount, float force) {
    // Spawna Cubos e Faíscas (Usado na explosão de linhas)
    for (int i = 0; i < amount; i++) {
        Particle3D p;
        p.position = pos;
        p.velocity = {
            GetRandomFloat(-force, force),
            GetRandomFloat(-force * 0.2f, force * 2.0f), // Mais força pra cima
            GetRandomFloat(-force, force)
        };
        p.color = color;
        p.maxLife = GetRandomFloat(0.5f, 1.5f);
        p.life = p.maxLife;
        p.isSpark = (GetRandomFloat(0, 1) > 0.6f); // 40% chance de ser uma faísca de luz
        p.size = p.isSpark ? GetRandomFloat(0.2f, 0.8f) : GetRandomFloat(0.1f, 0.3f);
        particles.push_back(p);
    }
}

// NOVA FUNÇÃO: Partículas de poeira rasteira para o impacto no chão
void SpawnDustParticles3D(Vector3 pos, Color color, int amount) {
    for (int i = 0; i < amount; i++) {
        Particle3D p;
        // Posição base levemente abaixo do centro do bloco
        p.position = { pos.x + GetRandomFloat(-0.5f, 0.5f), pos.y - 0.4f, pos.z + GetRandomFloat(-0.5f, 0.5f) };
        p.velocity = {
            GetRandomFloat(-8.0f, 8.0f), // Espalha muito para os lados (X)
            GetRandomFloat(0.5f, 3.0f),  // Pulo bem baixo (Y)
            GetRandomFloat(-8.0f, 8.0f)  // Espalha muito para frente/trás (Z)
        };
        p.color = color;
        p.maxLife = GetRandomFloat(0.3f, 0.7f); // Vida curta (poeira some rápido)
        p.life = p.maxLife;
        p.isSpark = false; 
        p.size = GetRandomFloat(0.05f, 0.15f); // Partículas minúsculas
        particles.push_back(p);
    }
}

void UpdateAndDrawParticles3D(float dt) {
    for (int i = particles.size() - 1; i >= 0; i--) {
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].position.z += particles[i].velocity.z * dt;
        particles[i].velocity.y -= 20.0f * dt; // Gravidade forte
        
        // Atrito no ar
        particles[i].velocity.x *= 0.98f;
        particles[i].velocity.z *= 0.98f;

        particles[i].life -= dt;

        if (particles[i].life <= 0 || particles[i].position.y < -2.0f) {
            particles.erase(particles.begin() + i);
        } else {
            float alpha = particles[i].life / particles[i].maxLife;
            Color fadeColor = particles[i].color;
            fadeColor.a = (unsigned char)(255 * alpha);
            
            if (particles[i].isSpark) {
                // Desenha uma linha de luz simulando motion blur da faísca
                Vector3 tail = {
                    particles[i].position.x - particles[i].velocity.x * 0.05f,
                    particles[i].position.y - particles[i].velocity.y * 0.05f,
                    particles[i].position.z - particles[i].velocity.z * 0.05f
                };
                DrawLine3D(particles[i].position, tail, fadeColor);
            } else {
                // Desenha pedaços de vidro/poeira
                DrawCube(particles[i].position, particles[i].size, particles[i].size, particles[i].size, ColorAlpha(fadeColor, 0.6f));
                DrawCubeWires(particles[i].position, particles[i].size, particles[i].size, particles[i].size, fadeColor);
            }
        }
    }
}

// =====================================================================
// LÓGICA DAS PEÇAS
// =====================================================================
struct Tetromino {
    vector<vector<int>> shape;
    int colorID;
};

vector<Tetromino> pieces = {
    { {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, 1 }, // I
    { {{1,0,0}, {1,1,1}, {0,0,0}}, 2 },                // J
    { {{0,0,1}, {1,1,1}, {0,0,0}}, 3 },                // L
    { {{1,1}, {1,1}}, 4 },                             // O
    { {{0,1,1}, {1,1,0}, {0,0,0}}, 5 },                // S
    { {{0,1,0}, {1,1,1}, {0,0,0}}, 6 },                // T
    { {{1,1,0}, {0,1,1}, {0,0,0}}, 7 }                 // Z
};

vector<vector<int>> RotateMatrix(const vector<vector<int>>& mat) {
    int n = mat.size();
    vector<vector<int>> res(n, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            res[j][n - 1 - i] = mat[i][j];
        }
    }
    return res;
}

// =====================================================================
// ENGINE PRINCIPAL DO JOGO
// =====================================================================
class JogoTetris3D {
private:
    int board[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int linesClearedTotal = 0;
    bool gameOver = false;
    
    vector<vector<int>> currentPiece;
    int currentX, currentY, currentColor;
    float renderFallY; 
    
    vector<vector<int>> nextPiece;
    int nextColor;

    float fallTimer = 0.0f;
    string mensagemEspecial = "";
    float timerMensagem = 0.0f;

    // Câmera Dinâmica Cinematográfica
    Camera3D camera = { 0 };
    Vector3 defaultCamPos = { 0.0f, 8.0f, 26.0f }; 
    Vector3 defaultCamTarget = { 0.0f, 8.0f, 0.0f };
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;
    float cameraZoomOffset = 0.0f;
    float cameraZoomHoldTimer = 0.0f; 
    float lastClearedY = 0.0f; // Salva o eixo Y da última explosão para a câmera focar
    
    // Controle do Menu de Saída (ESC)
    bool showExitPrompt = false;
    bool confirmExit = false;
    
    // Post-Processing Fake (Textura para Bloom)
    RenderTexture2D renderTarget;

    // Efeitos Sonoros Separados por intensidade
    Sound sndMove;
    Sound sndRotate;
    Sound sndDrop;
    Sound sndClear1;
    Sound sndClear2;
    Sound sndClear3;
    Sound sndClear4;
    Sound sndGameOver;

    mt19937 rng;

    int GetRandomPiece() {
        uniform_int_distribution<int> dist(0, 6);
        return dist(rng);
    }

    Vector3 GetWorldPos(int logicalX, int logicalY) {
        // Y=0 é o chão da plataforma. 
        return {
            (float)logicalX - (BOARD_WIDTH / 2.0f) + 0.5f,
            (float)(BOARD_HEIGHT - logicalY) - 0.5f,
            0.0f
        };
    }

    void SpawnPiece() {
        if (nextPiece.empty()) {
            int p1 = GetRandomPiece();
            nextPiece = pieces[p1].shape;
            nextColor = pieces[p1].colorID;
        }

        currentPiece = nextPiece;
        currentColor = nextColor;
        currentX = BOARD_WIDTH / 2 - currentPiece.size() / 2;
        currentY = 0;
        renderFallY = 0.0f;

        int p2 = GetRandomPiece();
        nextPiece = pieces[p2].shape;
        nextColor = pieces[p2].colorID;

        if (!IsValidMove(currentPiece, currentX, currentY)) {
            gameOver = true;
            PlaySound(sndGameOver);
        }
    }

    bool IsValidMove(const vector<vector<int>>& piece, int x, int y) {
        for (int i = 0; i < piece.size(); ++i) {
            for (int j = 0; j < piece[i].size(); ++j) {
                if (piece[i][j] != 0) {
                    int boardX = x + j;
                    int boardY = y + i;
                    if (boardX < 0 || boardX >= BOARD_WIDTH || boardY >= BOARD_HEIGHT) return false;
                    if (boardY >= 0 && board[boardY][boardX] != 0) return false;
                }
            }
        }
        return true;
    }

    void LockPiece() {
        PlaySound(sndDrop);

        // Impacto e partículas de POEIRA
        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0 && (currentY + i) >= 0) {
                    board[currentY + i][currentX + j] = currentColor;
                    Vector3 pos = GetWorldPos(currentX + j, currentY + i);
                    // Dispara a poeira rasteira de impacto
                    SpawnDustParticles3D(pos, pieceColors[currentColor], 8);
                }
            }
        }
        
        cameraShakeTimer = 0.15f;
        cameraShakeIntensity = 0.15f;
        ClearLines();
        SpawnPiece();
    }

    void ClearLines() {
        int linesClearedNow = 0;
        float sumClearedY = 0.0f; // Para descobrir onde a câmera deve focar

        for (int i = BOARD_HEIGHT - 1; i >= 0; --i) {
            bool isFull = true;
            for (int j = 0; j < BOARD_WIDTH; ++j) {
                if (board[i][j] == 0) { isFull = false; break; }
            }

            if (isFull) {
                linesClearedNow++;
                sumClearedY += GetWorldPos(0, i).y; // Soma a altura dessa linha
                
                // SUPER EXPLOSÃO AAA (Mantida intacta para quebra de linhas)
                for(int j = 0; j < BOARD_WIDTH; j++) {
                    Vector3 blockPos = GetWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 40, 18.0f);
                }

                for (int k = i; k > 0; --k) {
                    for (int j = 0; j < BOARD_WIDTH; ++j) board[k][j] = board[k - 1][j];
                }
                for (int j = 0; j < BOARD_WIDTH; ++j) board[0][j] = 0;
                i++; 
            }
        }

        if (linesClearedNow > 0) {
            linesClearedTotal += linesClearedNow;
            
            // Foco de altura da explosão
            lastClearedY = sumClearedY / (float)linesClearedNow; 
            
            // Reações Cinematográficas - CORRIGIDAS para não cortar o visual
            cameraShakeTimer = 0.5f + (linesClearedNow * 0.1f);
            cameraShakeIntensity = linesClearedNow * 0.6f;
            
            // Zoom menos agressivo para evitar o corte dos modelos (-10 base)
            cameraZoomOffset = -10.0f - (linesClearedNow * 1.5f); 
            cameraZoomHoldTimer = 0.4f; 

            // Sistema de Pontuação, Mensagens Dinâmicas e Áudio Modular
            if (linesClearedNow == 1) {
                score += 100 * level;
                mensagemEspecial = "GOOD !";
                timerMensagem = 2.0f;
                PlaySound(sndClear1);
            } 
            else if (linesClearedNow == 2) {
                score += 300 * level;
                mensagemEspecial = "VERY GOOD !!!";
                timerMensagem = 2.0f;
                PlaySound(sndClear2);
            } 
            else if (linesClearedNow == 3) {
                score += 500 * level;
                mensagemEspecial = "IMPRESSIVE!!!";
                timerMensagem = 2.0f;
                PlaySound(sndClear3);
            } 
            else if (linesClearedNow >= 4) {
                score += 800 * level;
                mensagemEspecial = "MARVELOUS!!!!!!";
                timerMensagem = 3.0f;
                cameraShakeIntensity = 2.5f; // Terremoto
                PlaySound(sndClear4);
            }

            level = (linesClearedTotal / 10) + 1;
        }
    }

    // =========================================================
    // RENDERIZADOR DE BLOCOS AAA (Vidro / Energia Volumétrica)
    // =========================================================
    void DrawSciFiBlock3D(Vector3 pos, Color c, bool isReflection, bool isGhost = false) {
        float s = CUBE_SIZE * 0.96f; 
        
        // Ajusta as cores baseado se é reflexo no chão ou o cubo real
        Color coreColor = isReflection ? ColorAlpha(WHITE, 0.05f) : ColorAlpha(WHITE, 0.6f);
        Color glassColor = isReflection ? ColorAlpha(c, 0.1f) : ColorAlpha(c, 0.35f);
        Color edgeColor = isReflection ? ColorAlpha(c, 0.3f) : c;

        // Modifica drasticamente as cores se for a Sombra (Ghost Piece)
        if (isGhost) {
            coreColor = BLANK; // Sem núcleo
            glassColor = ColorAlpha(c, 0.05f); // Vidro quase invisível
            edgeColor = ColorAlpha(c, 0.15f); // Borda fraca para demarcar
        }

        // 1. Núcleo Interno Sólido (A fonte de luz do neon) - Omite no Ghost Piece
        if (!isGhost) {
            DrawCube(pos, s * 0.4f, s * 0.4f, s * 0.4f, coreColor);
        }
        
        // 2. Casca Externa de Vidro
        DrawCube(pos, s, s, s, glassColor);
        
        // 3. Arestas Brilhantes
        DrawCubeWires(pos, s, s, s, edgeColor);
        if (!isGhost) {
            DrawCubeWires(pos, s * 1.02f, s * 1.02f, s * 1.02f, ColorAlpha(edgeColor, 0.5f));
        }
    }

    // Renderiza todo o cenário holográfico em volta
    void DrawSciFiArena() {
        float time = (float)GetTime();

        // ==========================================
        // ANÉIS GIRATÓRIOS (Deitados no chão)
        // ==========================================
        rlPushMatrix();
        rlRotatef(90, 1, 0, 0); // Deita SOMENTE OS ANÉIS no eixo XZ
            rlPushMatrix();
                rlRotatef(time * 10.0f, 0, 0, 1);
                DrawRing({0,0}, 10.0f, 10.2f, 0, 360, 64, ColorAlpha(C_CYAN, 0.4f));
            rlPopMatrix();
            
            rlPushMatrix();
                rlRotatef(-time * 5.0f, 0, 0, 1);
                DrawRing({0,0}, 13.0f, 13.5f, 0, 360, 64, ColorAlpha(C_BLUE, 0.2f));
                // Traços no anel externo
                for(int i=0; i<360; i+=20) {
                    DrawRing({0,0}, 13.5f, 14.5f, i, i+10, 8, ColorAlpha(C_CYAN, 0.3f));
                }
            rlPopMatrix();
        rlPopMatrix();

        // Malha customizada do chão para substituir o DrawGrid (Mais sutil e sci-fi)
        for (int i = -15; i <= 15; i++) {
            DrawLine3D({(float)i * 2.0f, 0, -30.0f}, {(float)i * 2.0f, 0, 30.0f}, ColorAlpha(C_BLUE, 0.1f));
            DrawLine3D({-30.0f, 0, (float)i * 2.0f}, {30.0f, 0, (float)i * 2.0f}, ColorAlpha(C_BLUE, 0.1f));
        }
        
        // ==========================================
        // GRADE DO JOGO E FUNDO FUMÊ (Playfield 10x20)
        // ==========================================
        float startX = -(BOARD_WIDTH / 2.0f);
        float endX = (BOARD_WIDTH / 2.0f);

        // Placa de vidro escuro atrás do tabuleiro (Impede que o cenário misture com as peças)
        DrawCube({0.0f, BOARD_HEIGHT / 2.0f - 0.5f, -0.6f}, (float)BOARD_WIDTH, (float)BOARD_HEIGHT, 0.1f, ColorAlpha(BLACK, 0.8f));
        DrawCubeWires({0.0f, BOARD_HEIGHT / 2.0f - 0.5f, -0.6f}, (float)BOARD_WIDTH, (float)BOARD_HEIGHT, 0.1f, ColorAlpha(C_CYAN, 0.3f));
        
        // Linhas Verticais do Tabuleiro
        for (int i = 0; i <= BOARD_WIDTH; i++) {
            float x = startX + i;
            DrawLine3D({x, 0, 0}, {x, (float)BOARD_HEIGHT, 0}, ColorAlpha(C_CYAN, 0.15f));
        }
        // Linhas Horizontais do Tabuleiro
        for (int i = 0; i <= BOARD_HEIGHT; i++) {
            float y = (float)i;
            DrawLine3D({startX, y, 0}, {endX, y, 0}, ColorAlpha(C_CYAN, 0.15f));
        }

        // Paredes Limites Holográficas Fortes (As quinas da arena)
        DrawLine3D({startX, 0, 0}, {startX, (float)BOARD_HEIGHT, 0}, C_CYAN);
        DrawLine3D({endX, 0, 0}, {endX, (float)BOARD_HEIGHT, 0}, C_CYAN);
        DrawLine3D({startX, (float)BOARD_HEIGHT, 0}, {endX, (float)BOARD_HEIGHT, 0}, C_CYAN); // Topo
    }

    // Lógica para desenhar todas as peças (reutilizada para os reflexos)
    void DrawAllPieces(bool isReflection) {
        // Tabuleiro Fixo
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < BOARD_WIDTH; ++j) {
                if (board[i][j] != 0) {
                    DrawSciFiBlock3D(GetWorldPos(j, i), pieceColors[board[i][j]], isReflection);
                }
            }
        }

        // ==========================================
        // CÁLCULO E RENDERIZAÇÃO DA GHOST PIECE (Sombra)
        // ==========================================
        int ghostY = currentY;
        while (IsValidMove(currentPiece, currentX, ghostY + 1)) {
            ghostY++;
        }

        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0) {
                    Vector3 ghostPos = {
                        (float)(currentX + j) - (BOARD_WIDTH / 2.0f) + 0.5f,
                        (float)BOARD_HEIGHT - (ghostY + i) - 0.5f,
                        0.0f
                    };
                    // Desenha a sombra apenas se não estiver sobrepondo a peça original no mesmo lugar
                    if (ghostY > currentY) {
                        DrawSciFiBlock3D(ghostPos, pieceColors[currentColor], isReflection, true);
                    }
                }
            }
        }

        // ==========================================
        // Peça Caindo (Original)
        // ==========================================
        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0) {
                    Vector3 dropPos = {
                        (float)(currentX + j) - (BOARD_WIDTH / 2.0f) + 0.5f,
                        (float)BOARD_HEIGHT - (renderFallY + i) - 0.5f,
                        0.0f
                    };
                    DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection);
                }
            }
        }
    }

public:
    JogoTetris3D() : rng(random_device{}()) {
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 55.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        
        renderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

        // Carregando todos os Sons individualizados
        sndMove = LoadSound("move.mp3");
        sndRotate = LoadSound("rotate.mp3");
        sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3");
        sndClear2 = LoadSound("clear2.mp3");
        sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3");
        sndGameOver = LoadSound("gameover.mp3");

        SpawnPiece();
    }

    ~JogoTetris3D() {
        UnloadRenderTexture(renderTarget);
        
        // Descarregando os Sons
        UnloadSound(sndMove);
        UnloadSound(sndRotate);
        UnloadSound(sndDrop);
        UnloadSound(sndClear1);
        UnloadSound(sndClear2);
        UnloadSound(sndClear3);
        UnloadSound(sndClear4);
        UnloadSound(sndGameOver);
    }

    void Update(float dt) {
        // Toggle Fullscreen / Windowed (Atalhos U ou ALT+ENTER)
        if (IsKeyPressed(KEY_U) || (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))) {
            ToggleFullscreen();
        }

        // Sistema de Confirmação de Saída (ESC)
        if (IsKeyPressed(KEY_ESCAPE)) {
            showExitPrompt = !showExitPrompt; // Mostra ou esconde a janela
        }

        if (showExitPrompt) {
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_Y)) confirmExit = true;
            if (IsKeyPressed(KEY_N)) showExitPrompt = false;
            return; // Interrompe as atualizações do jogo enquanto a pergunta está na tela
        }

        if (gameOver) return;

        float speed = fmax(0.05f, 0.6f - (level * 0.05f)); 
        fallTimer += dt;

        // Inputs com sons vinculados
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            if (IsValidMove(currentPiece, currentX - 1, currentY)) {
                currentX--;
                PlaySound(sndMove);
            }
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            if (IsValidMove(currentPiece, currentX + 1, currentY)) {
                currentX++;
                PlaySound(sndMove);
            }
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            auto rotated = RotateMatrix(currentPiece);
            if (IsValidMove(rotated, currentX, currentY)) {
                currentPiece = rotated;
                PlaySound(sndRotate);
            }
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            fallTimer += dt * 15.0f;
        }
        if (IsKeyPressed(KEY_SPACE)) { 
            while (IsValidMove(currentPiece, currentX, currentY + 1)) {
                currentY++;
                score += 2;
            }
            LockPiece(); // O som sndDrop toca dentro da função LockPiece()
            fallTimer = 0;
        }

        if (fallTimer >= speed) {
            fallTimer = 0;
            if (IsValidMove(currentPiece, currentX, currentY + 1)) {
                currentY++;
            } else {
                LockPiece();
            }
        }

        renderFallY = Lerp(renderFallY, (float)currentY, dt * 25.0f);

        // --- CÂMERA DINÂMICA COM FOCO CORRIGIDO ---
        float orbitAngle = (float)GetTime() * 0.15f;
        Vector3 targetCamPos = {
            defaultCamPos.x + (float)sin(orbitAngle) * 4.0f,
            defaultCamPos.y + (float)sin(GetTime() * 0.5f) * 1.0f, 
            defaultCamPos.z + (float)cos(orbitAngle) * 2.0f + cameraZoomOffset
        };
        Vector3 currentTarget = defaultCamTarget;

        // Quando o Zoom engaja na explosão, ela foca e olha pra baixo na linha explodida
        if (cameraZoomHoldTimer > 0 || cameraZoomOffset < -0.1f) {
            targetCamPos.y = lastClearedY + 8.0f; // Sobe a câmera pra "olhar de cima" e não cortar blocos
            currentTarget.y = lastClearedY;       // Mira exatamente na altura da explosão
        }

        // Controle do recuo em Alta Velocidade
        if (cameraZoomHoldTimer > 0) {
            cameraZoomHoldTimer -= dt; // Segura focado na explosão
        } else if (cameraZoomOffset < 0) {
            cameraZoomOffset += dt * 40.0f; // Volta muito rápido!
            if (cameraZoomOffset > 0) cameraZoomOffset = 0;
        }

        if (cameraShakeTimer > 0) {
            targetCamPos.x += GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            targetCamPos.y += GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            currentTarget.x += GetRandomFloat(-cameraShakeIntensity*0.5f, cameraShakeIntensity*0.5f);
            currentTarget.y += GetRandomFloat(-cameraShakeIntensity*0.5f, cameraShakeIntensity*0.5f);
            cameraShakeTimer -= dt;
        }

        // Acelera muito a câmera quando está mergulhando no Zoom
        float camLerpSpeed = (cameraZoomOffset < -5.0f) ? 18.0f : 6.0f; 
        camera.position = Vector3Lerp(camera.position, targetCamPos, dt * camLerpSpeed);
        camera.target = Vector3Lerp(camera.target, currentTarget, dt * 10.0f);

        if (timerMensagem > 0) timerMensagem -= dt;
    }

    void Draw() {
        // ==========================================
        // PASSO 1: RENDERIZAR CENA PARA TEXTURA (Para Bloom)
        // ==========================================
        BeginTextureMode(renderTarget);
        ClearBackground(C_BG);
        
        BeginMode3D(camera);

            DrawSciFiArena();

            // ----------------------------------------
            // SISTEMA DE REFLEXOS (SSR FAKE)
            // ----------------------------------------
            rlPushMatrix();
                // Espelha o mundo no eixo Y a partir do chão (y=0)
                rlScalef(1.0f, -1.0f, 1.0f);
                DrawAllPieces(true); // Renderiza blocos versão reflexo
            rlPopMatrix();

            // O Chão de Vidro Escuro que cobre o reflexo
            DrawPlane({0, 0, 0}, {30.0f, 30.0f}, ColorAlpha(C_BG, 0.85f));

            // ----------------------------------------
            // RENDERIZAÇÃO REAL DOS BLOCOS E GHOST
            // ----------------------------------------
            DrawAllPieces(false);

            // Próxima Peça Holográfica (Ajustada matematicamente para 1080p)
            Vector3 nextPieceOrigin = { 11.5f, 9.5f, -2.0f };
            for (int i = 0; i < nextPiece.size(); ++i) {
                for (int j = 0; j < nextPiece[i].size(); ++j) {
                    if (nextPiece[i][j] != 0) {
                        Vector3 npPos = { nextPieceOrigin.x + j, nextPieceOrigin.y - i, nextPieceOrigin.z };
                        rlPushMatrix();
                            rlTranslatef(npPos.x, npPos.y, npPos.z);
                            rlRotatef((float)GetTime() * 50.0f, 0, 1, 0); 
                            rlRotatef((float)sin(GetTime()*2)*15.0f, 1, 0, 0); // Balanço
                            DrawSciFiBlock3D({0,0,0}, pieceColors[nextColor], false);
                        rlPopMatrix();
                    }
                }
            }

            UpdateAndDrawParticles3D(GetFrameTime());

        EndMode3D();
        EndTextureMode();

        // ==========================================
        // PASSO 2: RENDERIZAR NA TELA COM FAKE BLOOM
        // ==========================================
        ClearBackground(BLACK);
        
        // Desenha a cena original normal
        Rectangle source = { 0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
        Rectangle dest = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        DrawTexturePro(renderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);

        // Aplica o Bloom Fake (Adiciona a cena por cima de si mesma escalada e transparente)
        BeginBlendMode(BLEND_ADDITIVE);
        Rectangle bloomDest = { -5, -5, SCREEN_WIDTH + 10.0f, SCREEN_HEIGHT + 10.0f }; // Levemente maior
        DrawTexturePro(renderTarget.texture, source, bloomDest, {0,0}, 0.0f, ColorAlpha(WHITE, 0.4f));
        EndBlendMode();

        // ==========================================
        // PASSO 3: RENDERIZAÇÃO DO HUD DIEGÉTICO (Alinhado 1920x1080)
        // ==========================================
        // Retângulo Sci-fi de fundo esquerdo
        DrawRectangle(40, 40, 380, 240, ColorAlpha(C_BG, 0.8f));
        DrawRectangleLinesEx({40, 40, 380, 240}, 2, ColorAlpha(C_CYAN, 0.6f));
        DrawLine(40, 90, 420, 90, ColorAlpha(C_CYAN, 0.6f));
        DrawRectangle(40, 40, 20, 20, C_CYAN); // Detalhe de canto

        DrawText("SCORPIO_SYS_V4", 75, 55, 24, C_CYAN);
        DrawText("STATUS: ONLINE", 260, 60, 14, C_GREEN);

        DrawText("PONTUACAO:", 60, 110, 24, WHITE);
        DrawText(TextFormat("%08d", score), 60, 145, 45, C_CYAN);

        DrawText(TextFormat("NIVEL: %02d", level), 60, 210, 26, WHITE);

        // Retângulo Sci-fi Próxima peça (Direita) - Feito mais alto para encaixar a peça 3D
        DrawRectangle(SCREEN_WIDTH - 380, 40, 340, 300, ColorAlpha(C_BG, 0.8f));
        DrawRectangleLinesEx({SCREEN_WIDTH - 380, 40, 340, 300}, 2, ColorAlpha(C_CYAN, 0.6f));
        DrawText("PROXIMA PECA:", SCREEN_WIDTH - 350, 60, 24, WHITE);
        DrawLine(SCREEN_WIDTH - 380, 100, SCREEN_WIDTH - 40, 100, ColorAlpha(C_CYAN, 0.6f));

        // Letreiros de Combos com Glitch
        if (timerMensagem > 0) {
            float scale = 1.0f + (float)sin(GetTime() * 20.0f) * 0.1f;
            int fontSize = 55 * scale; // Ajustado pra caber as palavras maiores
            int textWidth = MeasureText(mensagemEspecial.c_str(), fontSize);
            
            // Efeito de separação RGB (Glitch)
            BeginBlendMode(BLEND_ADDITIVE);
            DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2 - 4, SCREEN_HEIGHT/3, fontSize, C_RED);
            DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2 + 4, SCREEN_HEIGHT/3, fontSize, C_CYAN);
            EndBlendMode();
            
            DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/3, fontSize, WHITE);
        }

        if (gameOver) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.9f));
            DrawText("SISTEMA COMPROMETIDO", SCREEN_WIDTH/2 - MeasureText("SISTEMA COMPROMETIDO", 60)/2, SCREEN_HEIGHT/2 - 80, 60, C_RED);
            DrawText("PRESSIONE [ENTER] PARA REBOOT", SCREEN_WIDTH/2 - MeasureText("PRESSIONE [ENTER] PARA REBOOT", 30)/2, SCREEN_HEIGHT/2 + 30, 30, C_CYAN);
        }

        // TELA DE CONFIRMAÇÃO DE SAÍDA (ESC)
        if (showExitPrompt) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.95f));
            
            // Caixa do prompt holográfica
            DrawRectangle(SCREEN_WIDTH/2 - 400, SCREEN_HEIGHT/2 - 150, 800, 300, ColorAlpha(C_BG, 0.9f));
            DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 400, (float)SCREEN_HEIGHT/2 - 150, 800, 300}, 2, C_CYAN);
            
            DrawText("DESEJA REALMENTE SAIR?", SCREEN_WIDTH/2 - MeasureText("DESEJA REALMENTE SAIR?", 50)/2, SCREEN_HEIGHT/2 - 60, 50, C_ORANGE);
            DrawText("[S] SIM   -   [N] NAO", SCREEN_WIDTH/2 - MeasureText("[S] SIM   -   [N] NAO", 30)/2, SCREEN_HEIGHT/2 + 40, 30, WHITE);
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0;
        level = 1;
        linesClearedTotal = 0;
        gameOver = false;
        timerMensagem = 0;
        particles.clear();
        SpawnPiece();
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
    }

    // Retorna verdadeiro se o jogador confirmou a saída do jogo
    bool ShouldExit() { return confirmExit; }
    bool IsGameOver() { return gameOver; }
};

int main() {
    // Liga os sinalizadores para Gráficos HD em Alta Definição
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    
    // Inicia a janela na resolução FULL HD exata (1920x1080)
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Scorpio Tetris AAA");
    
    // DESABILITA O ESC FECHAR O JOGO (Permite nosso prompt customizado assumir o controle)
    SetExitKey(KEY_NULL); 

    // Força o modo TELA CHEIA (Fullscreen Imersivo)
    ToggleFullscreen();
    
    // Inicia a Placa de Som do computador
    InitAudioDevice(); 
    
    SetTargetFPS(60);

    JogoTetris3D game;

    // O loop agora só fecha se fechar a janela (X) OU se nossa variável interna disser que o jogador confirmou
    while (!WindowShouldClose() && !game.ShouldExit()) {
        float dt = GetFrameTime();
        game.Update(dt);
        
        if (game.IsGameOver() && IsKeyPressed(KEY_ENTER)) {
            game.Restart();
        }

        BeginDrawing();
        game.Draw();
        EndDrawing();
    }

    // Desliga a Placa de Som antes de fechar o jogo
    CloseAudioDevice(); 
    CloseWindow();
    
    return 0;
}