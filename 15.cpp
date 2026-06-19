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
const int MAX_BOARD_WIDTH = 18; // Limite máximo da expansão horizontal
const int BOARD_HEIGHT = 20;    // Altura fixa restaurada
const int SCREEN_WIDTH = 1920;  
const int SCREEN_HEIGHT = 1080; 
const float CUBE_SIZE = 1.0f;

// Cores Neon Aprimoradas
const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 30, 144, 255, 255 }; 
const Color C_ORANGE = { 255, 140, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 100, 255 };
const Color C_PURPLE = { 200, 0, 255, 255 };
const Color C_RED    = { 255, 20, 60, 255 };
const Color C_BG     = { 2, 4, 10, 255 }; 

// Tabela de Cores das Peças (Cor 8 = Lixo do Boss)
Color pieceColors[9] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, DARKGRAY };

// =====================================================================
// ESTADOS DO JOGO E MENUS
// =====================================================================
enum GameState { MENU, SETTINGS, CREDITS, PLAYING };

// =====================================================================
// PROCESSADOR DE ÁUDIO EM TEMPO REAL E TRANSIÇÃO DE CORES
// =====================================================================
float globalMusicAmplitude = 0.0f;

void AudioInputCallback(void *bufferData, unsigned int frames) {
    float *samples = (float *)bufferData;
    float sum = 0.0f;
    for (unsigned int i = 0; i < frames; i++) {
        sum += fabs(samples[i]);
    }
    globalMusicAmplitude = sum / (float)frames;
}

Color LerpColor(Color a, Color b, float t) {
    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

// =====================================================================
// SISTEMA DE PARTÍCULAS E PARALLAX
// =====================================================================
struct Particle3D {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    bool isSpark; 
};

struct ParallaxElement {
    Vector3 pos;
    float size;
    Color color;
    float speed;
    bool isWire;
};

vector<Particle3D> particles;
vector<ParallaxElement> parallaxElements; 

float GetRandomFloat(float min, float max) {
    return min + (max - min) * ((float)rand() / RAND_MAX);
}

void SpawnParticles3D(Vector3 pos, Color color, int amount, float force) {
    for (int i = 0; i < amount; i++) {
        Particle3D p;
        p.position = pos;
        p.velocity = {
            GetRandomFloat(-force, force),
            GetRandomFloat(-force * 0.2f, force * 2.0f), 
            GetRandomFloat(-force, force)
        };
        p.color = color;
        p.maxLife = GetRandomFloat(0.5f, 1.5f);
        p.life = p.maxLife;
        p.isSpark = (GetRandomFloat(0, 1) > 0.6f); 
        p.size = p.isSpark ? GetRandomFloat(0.2f, 0.8f) : GetRandomFloat(0.1f, 0.3f);
        particles.push_back(p);
    }
}

void SpawnDustParticles3D(Vector3 pos, Color color, int amount) {
    for (int i = 0; i < amount; i++) {
        Particle3D p;
        p.position = { pos.x + GetRandomFloat(-0.5f, 0.5f), pos.y - 0.4f, pos.z + GetRandomFloat(-0.5f, 0.5f) };
        p.velocity = {
            GetRandomFloat(-8.0f, 8.0f), 
            GetRandomFloat(0.5f, 3.0f),  
            GetRandomFloat(-8.0f, 8.0f)  
        };
        p.color = color;
        p.maxLife = GetRandomFloat(0.3f, 0.7f); 
        p.life = p.maxLife;
        p.isSpark = false; 
        p.size = GetRandomFloat(0.05f, 0.15f); 
        particles.push_back(p);
    }
}

void UpdateAndDrawParticles3D(float dt) {
    for (int i = particles.size() - 1; i >= 0; i--) {
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].position.z += particles[i].velocity.z * dt;
        particles[i].velocity.y -= 20.0f * dt; 
        
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
                Vector3 tail = {
                    particles[i].position.x - particles[i].velocity.x * 0.05f,
                    particles[i].position.y - particles[i].velocity.y * 0.05f,
                    particles[i].position.z - particles[i].velocity.z * 0.05f
                };
                DrawLine3D(particles[i].position, tail, fadeColor);
            } else {
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
    GameState currentState = MENU;
    int menuSelection = 0;
    int settingsSelection = 0;

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int continues = 3; 
    int bombs = 2; 
    int stars = 0; 
    int currentGridWidth = 10; // NOVO: Largura Dinâmica da Arena (Começa em 10)
    int linesClearedTotal = 0;
    
    bool gameOver = false;
    bool isPaused = false; 
    
    vector<vector<int>> currentPiece;
    int currentX, currentY, currentColor;
    float renderFallY; 
    bool currentIsBrilliant = false; 
    
    vector<vector<int>> nextPiece;
    int nextColor;
    bool nextIsBrilliant = false; 

    float fallTimer = 0.0f;
    string mensagemEspecial = "";
    float timerMensagem = 0.0f;

    float musicPulse = 0.0f;
    Color themeCyan = C_CYAN;
    Color themeBlue = C_BLUE;
    Color themeBg = C_BG;

    Camera3D camera = { 0 };
    Vector3 defaultCamPos = { 0.0f, 8.5f, 26.0f }; 
    Vector3 defaultCamTarget = { 0.0f, 8.0f, 0.0f };
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;
    float cameraZoomOffset = 0.0f;
    float targetZoomOffset = 0.0f; 
    float cameraZoomHoldTimer = 0.0f; 
    float lastClearedY = 0.0f; 
    float motionBlurIntensity = 0.0f; 
    float nukeSpinAngle = 0.0f; 
    
    float moveLeftTimer = 0.0f;
    float moveRightTimer = 0.0f;
    const float DAS_DELAY = 0.15f; 
    const float ARR_RATE = 0.03f;  

    // BOSS BATTLE
    bool bossActive = false;
    int bossHp = 0;
    int linesUntilBoss = 15; 
    float bossAttackTimer = 0.0f;
    float bossEntryAnim = 0.0f;
    int bossEncounterCount = 0; 
    float currentBossAttackDelay = 8.0f; 

    bool showExitPrompt = false;
    bool confirmExit = false;

    bool sfxEnabled = true;
    bool musicEnabled = true;
    bool isFullscreen = true; 
    
    RenderTexture2D renderTarget;

    Sound sndMove;
    Sound sndRotate;
    Sound sndDrop;
    Sound sndClear1;
    Sound sndClear2;
    Sound sndClear3;
    Sound sndClear4;
    Sound sndGameOver;
    Music sndMusic = { 0 };
    int currentMusicTrack = 1; // Para gerenciar a lista de faixas de 1 a 30

    mt19937 rng;

    int GetRandomPiece() {
        uniform_int_distribution<int> dist(0, 6);
        return dist(rng);
    }

    Vector3 GetWorldPos(int logicalX, int logicalY) {
        return {
            (float)logicalX - (currentGridWidth / 2.0f) + 0.5f,
            (float)(BOARD_HEIGHT - logicalY) - 0.5f,
            0.0f
        };
    }

    void TocarSom(Sound snd) {
        if (sfxEnabled) PlaySound(snd);
    }

    // Função para carregar dinamicamente a próxima faixa
    void LoadNextMusic() {
        if (sndMusic.stream.buffer != NULL) {
            DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
            UnloadMusicStream(sndMusic);
            sndMusic.stream.buffer = NULL;
        }
        string fileName = "music" + to_string(currentMusicTrack) + ".mp3";
        
        // Verifica se o arquivo existe ANTES de carregar para evitar crash
        if (FileExists(fileName.c_str())) {
            sndMusic = LoadMusicStream(fileName.c_str());
            if (sndMusic.stream.buffer != NULL) {
                PlayMusicStream(sndMusic);
                AttachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
            }
        } else {
            // Se não achar a música, volta para a faixa 1. 
            // Se nem a 1 existir, desativa a música automaticamente.
            if (currentMusicTrack > 1) {
                currentMusicTrack = 1;
                LoadNextMusic(); 
            } else {
                musicEnabled = false; 
            }
        }
    }

    void SpawnPiece() {
        if (nextPiece.empty()) {
            int p1 = GetRandomPiece();
            nextPiece = pieces[p1].shape;
            nextColor = pieces[p1].colorID;
        }

        currentPiece = nextPiece;
        currentColor = nextColor;
        currentIsBrilliant = nextIsBrilliant;
        nextIsBrilliant = false; 

        currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
        currentY = 0; 
        renderFallY = 0.0f;

        int p2 = GetRandomPiece();
        nextPiece = pieces[p2].shape;
        nextColor = pieces[p2].colorID;

        // SISTEMA DE CONTINUES
        if (!IsValidMove(currentPiece, currentX, currentY)) {
            if (continues > 0) {
                continues--; 
                for(int i=0; i<BOARD_HEIGHT; i++) {
                    for(int j=0; j<currentGridWidth; j++) {
                        if(board[i][j] != 0) {
                            Vector3 pos = GetWorldPos(j, i);
                            SpawnDustParticles3D(pos, pieceColors[board[i][j]], 4);
                            board[i][j] = 0;
                        }
                    }
                }
                TocarSom(sndGameOver); 
                cameraShakeTimer = 0.5f;
                cameraShakeIntensity = 1.5f;
                
                currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
                currentY = 0; 
                renderFallY = 0.0f;
            } else {
                gameOver = true;
                TocarSom(sndGameOver);
            }
        }
    }

    bool IsValidMove(const vector<vector<int>>& piece, int x, int y) {
        for (int i = 0; i < piece.size(); ++i) {
            for (int j = 0; j < piece[i].size(); ++j) {
                if (piece[i][j] != 0) {
                    int boardX = x + j;
                    int boardY = y + i;
                    if (boardX < 0 || boardX >= currentGridWidth || boardY >= BOARD_HEIGHT) return false;
                    if (boardY >= 0 && board[boardY][boardX] != 0) return false;
                }
            }
        }
        return true;
    }

    void LockPiece() {
        TocarSom(sndDrop);

        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0 && (currentY + i) >= 0) {
                    board[currentY + i][currentX + j] = currentColor;
                    Vector3 pos = GetWorldPos(currentX + j, currentY + i);
                    SpawnDustParticles3D(pos, pieceColors[currentColor], 8);
                }
            }
        }
        
        // EXPANSÃO HORIZONTAL DO GRID (PODER DA PEÇA BRILHANTE)
        if (currentIsBrilliant) {
            if (currentGridWidth < MAX_BOARD_WIDTH) {
                int expansion = 4; // Expande 4 blocos de uma vez (2 para cada lado)
                int shift = expansion / 2;
                
                // Desloca as peças existentes para que continuem no centro do novo grid
                for (int i = 0; i < BOARD_HEIGHT; i++) {
                    for (int j = currentGridWidth - 1; j >= 0; j--) {
                        board[i][j + shift] = board[i][j];
                    }
                    for (int j = 0; j < shift; j++) board[i][j] = 0; 
                }
                
                currentGridWidth += expansion;
                mensagemEspecial = "ARENA EXPANDIDA!";
                timerMensagem = 3.0f;
                cameraShakeTimer = 1.0f;
                cameraShakeIntensity = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_YELLOW, 100, 20.0f);
                TocarSom(sndClear4); 
            } else {
                mensagemEspecial = "PODER MÁXIMO!";
                score += 2000 * level;
                timerMensagem = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_CYAN, 100, 20.0f);
                TocarSom(sndClear3);
            }
        }

        cameraShakeTimer = 0.3f;
        cameraShakeIntensity = 1.5f; 
        
        ClearLines();
        SpawnPiece();
    }

    void BossAddJunkLine() {
        for (int i = 0; i < BOARD_HEIGHT - 1; i++) {
            for (int j = 0; j < currentGridWidth; j++) {
                board[i][j] = board[i+1][j];
            }
        }
        int hole = GetRandomValue(0, currentGridWidth - 1);
        for (int j = 0; j < currentGridWidth; j++) {
            if (j == hole) board[BOARD_HEIGHT - 1][j] = 0;
            else board[BOARD_HEIGHT - 1][j] = 8; 
        }
        cameraShakeTimer = 0.5f;
        cameraShakeIntensity = 2.0f;
        TocarSom(sndDrop);
    }

    // NOVO ATAQUE: Joga blocos corrompidos vindos do topo!
    void BossThrowJunk() {
        int blocksToThrow = currentGridWidth / 2; 
        Vector3 bossPos = { 0.0f, 22.0f, 2.5f }; // Origem visual do ataque
        
        for (int b = 0; b < blocksToThrow; b++) {
            int col = GetRandomValue(0, currentGridWidth - 1);
            
            int targetRow = BOARD_HEIGHT - 1;
            for (int row = 0; row < BOARD_HEIGHT; row++) {
                if (board[row][col] != 0) {
                    targetRow = row - 1;
                    break;
                }
            }
            
            if (targetRow >= 0) {
                board[targetRow][col] = 8; // DARKGRAY/JUNK
                Vector3 pos = GetWorldPos(col, targetRow);
                
                // Cria um rastro de partículas do boss até o bloco que ele "jogou"
                for (int p = 0; p < 8; p++) {
                    Vector3 trailPos = Vector3Lerp(bossPos, pos, (float)p / 8.0f);
                    SpawnParticles3D(trailPos, C_RED, 4, 3.0f);
                }
                
                SpawnParticles3D(pos, C_RED, 25, 15.0f);
            }
        }
        
        cameraShakeTimer = 0.8f;
        cameraShakeIntensity = 3.0f;
        TocarSom(sndDrop);
        mensagemEspecial = "ATAQUE AÉREO!";
        timerMensagem = 2.0f;
    }

    void ClearLines() {
        int linesClearedNow = 0;
        float sumClearedY = 0.0f;

        for (int i = BOARD_HEIGHT - 1; i >= 0; --i) {
            bool isFull = true;
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] == 0) { isFull = false; break; }
            }

            if (isFull) {
                linesClearedNow++;
                sumClearedY += GetWorldPos(0, i).y; 
                
                for(int j = 0; j < currentGridWidth; j++) {
                    Vector3 blockPos = GetWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 40, 18.0f);
                }

                for (int k = i; k > 0; --k) {
                    for (int j = 0; j < currentGridWidth; ++j) board[k][j] = board[k - 1][j];
                }
                for (int j = 0; j < currentGridWidth; ++j) board[0][j] = 0;
                i++; 
            }
        }

        if (linesClearedNow > 0) {
            linesClearedTotal += linesClearedNow;
            
            // SISTEMA DE ESTRELAS
            stars += linesClearedNow;
            if (stars >= 10) {
                stars -= 10;
                nextIsBrilliant = true; 
                mensagemEspecial = "PEÇA MÁGICA PRONTA!";
                timerMensagem = 3.0f;
                TocarSom(sndClear2);
            }

            lastClearedY = sumClearedY / (float)linesClearedNow; 
            
            cameraShakeTimer = 0.6f + (linesClearedNow * 0.1f);
            cameraShakeIntensity = linesClearedNow * 1.5f; 
            
            targetZoomOffset = -4.0f - (linesClearedNow * 0.5f); 
            cameraZoomHoldTimer = 1.0f; 

            if (linesClearedNow == 1) {
                score += 100 * level;
                if (mensagemEspecial == "") { mensagemEspecial = "GOOD !"; timerMensagem = 2.0f; }
                TocarSom(sndClear1);
            } 
            else if (linesClearedNow == 2) {
                score += 300 * level;
                if (mensagemEspecial == "") { mensagemEspecial = "VERY GOOD !!!"; timerMensagem = 2.0f; }
                TocarSom(sndClear2);
            } 
            else if (linesClearedNow == 3) {
                score += 500 * level;
                if (mensagemEspecial == "") { mensagemEspecial = "IMPRESSIVE!!!"; timerMensagem = 2.0f; }
                TocarSom(sndClear3);
            } 
            else if (linesClearedNow >= 4) {
                score += 800 * level;
                if (mensagemEspecial == "") { mensagemEspecial = "MARVELOUS!!!!!!"; timerMensagem = 3.0f; }
                cameraShakeIntensity = 4.0f; 
                TocarSom(sndClear4);
            }

            if (bossActive) {
                bossHp -= linesClearedNow; 
                if (bossHp <= 0) {
                    bossActive = false;
                    score += 5000 * level;
                    SpawnParticles3D({0, 20.0f, -5.0f}, C_RED, 200, 40.0f); 
                    cameraShakeTimer = 1.5f;
                    cameraShakeIntensity = 4.0f;
                    mensagemEspecial = "VIRUS DELETED!";
                    timerMensagem = 4.0f;
                    TocarSom(sndClear4);
                }
            }

            level = (linesClearedTotal / 10) + 1;
        }
    }

    void NukeBoard() {
        int blocksDestroyed = 0;
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < currentGridWidth; j++) {
                if (board[i][j] != 0) {
                    blocksDestroyed++;
                    Vector3 blockPos = GetWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 15, 25.0f);
                    board[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            score += blocksDestroyed * 50 * level;
            mensagemEspecial = "SYSTEM PURGE!!!";
            timerMensagem = 3.0f;
            cameraShakeTimer = 1.0f;
            cameraShakeIntensity = 3.0f;
            targetZoomOffset = -4.0f; 
            cameraZoomHoldTimer = 1.5f; 
            nukeSpinAngle = PI * 4.0f; 
            lastClearedY = BOARD_HEIGHT / 2.0f; 
            TocarSom(sndClear4); 

            if (bossActive) {
                bossHp -= 10;
                if (bossHp <= 0) {
                    bossActive = false;
                    score += 10000;
                    SpawnParticles3D({0, 20.0f, -5.0f}, C_RED, 300, 50.0f); 
                }
            }
        }
    }

    void DrawSciFiBlock3D(Vector3 pos, Color c, bool isReflection, bool isGhost = false, bool isBrilliant = false) {
        float s = CUBE_SIZE * 0.98f; 
        
        Color coreColor = isReflection ? ColorAlpha(WHITE, 0.05f) : ColorAlpha(WHITE, 0.85f);
        Color glassColor = isReflection ? ColorAlpha(c, 0.2f) : ColorAlpha(c, 0.5f); 
        Color edgeColor = isReflection ? ColorAlpha(WHITE, 0.1f) : ColorAlpha(WHITE, 0.7f); 
        Color glowColor = isReflection ? ColorAlpha(c, 0.3f) : c;

        if (isBrilliant && !isGhost) {
            float t = (float)GetTime() * 5.0f;
            glowColor = ColorFromHSV(fmod(t * 60.0f, 360.0f), 1.0f, 1.0f);
            coreColor = WHITE;
            glassColor = ColorAlpha(glowColor, 0.8f);
            edgeColor = WHITE;
        }

        if (isGhost) {
            coreColor = BLANK; 
            glassColor = ColorAlpha(c, 0.12f); 
            edgeColor = ColorAlpha(c, 0.35f);  
            glowColor = BLANK;
        }

        if (!isGhost) {
            DrawSphere(pos, s * 0.35f, coreColor);
            DrawCubeWires(pos, s * 0.7f, s * 0.7f, s * 0.7f, ColorAlpha(c, 0.2f));
        }
        
        DrawCube(pos, s, s, s, glassColor);
        DrawCubeWires(pos, s, s, s, edgeColor);
        if (!isGhost) {
            DrawCubeWires(pos, s * 1.05f, s * 1.05f, s * 1.05f, ColorAlpha(glowColor, 0.6f));
        }
    }

    void DrawSciFiArena() {
        float time = (float)GetTime();
        
        // OSCILOSCÓPIO PROFISSIONAL
        rlPushMatrix();
        rlRotatef(90, 1, 0, 0); 
            float pScale = 1.0f + musicPulse * 0.1f;
            
            int segments = 120;
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * PI * 2.0f;
                float angle2 = (float)(i + 1) / segments * PI * 2.0f;

                float wave1 = sin(angle1 * 12.0f + time * 5.0f) * cos(angle1 * 8.0f - time * 3.0f);
                float wave2 = sin(angle2 * 12.0f + time * 5.0f) * cos(angle2 * 8.0f - time * 3.0f);

                float r1 = 11.0f * pScale + (wave1 * musicPulse * 2.5f);
                float r2 = 11.0f * pScale + (wave2 * musicPulse * 2.5f);

                Vector3 p1 = { cos(angle1) * r1, sin(angle1) * r1, 0.0f };
                Vector3 p2 = { cos(angle2) * r2, sin(angle2) * r2, 0.0f };

                DrawLine3D(p1, p2, ColorAlpha(themeCyan, 0.8f + musicPulse * 0.2f));

                if (i % 3 == 0) {
                    float barHeight = (fabs(wave1) + 0.1f) * musicPulse * 5.0f;
                    Vector3 b1 = { cos(angle1) * 12.0f * pScale, sin(angle1) * 12.0f * pScale, 0.0f };
                    Vector3 b2 = { cos(angle1) * (12.0f * pScale + barHeight), sin(angle1) * (12.0f * pScale + barHeight), 0.0f };
                    DrawLine3D(b1, b2, ColorAlpha(themeBlue, 0.6f + musicPulse * 0.4f));
                }
            }

            rlPushMatrix();
                rlRotatef(time * 30.0f, 0, 0, 1);
                DrawRing({0,0}, 9.8f * pScale, 10.2f * pScale, 0, 360, 64, ColorAlpha(themeCyan, 0.3f));
                for(int i=0; i<360; i+=30) {
                    DrawRing({0,0}, 10.2f * pScale, 11.0f * pScale, i, i+15, 8, ColorAlpha(themeCyan, 0.5f));
                }
            rlPopMatrix();

            rlPushMatrix();
                rlRotatef(-time * 20.0f, 0, 0, 1);
                DrawRing({0,0}, 14.5f * pScale, 14.8f * pScale, 0, 360, 64, ColorAlpha(themeBlue, 0.2f));
                for(int i=0; i<360; i+=10) {
                    DrawRing({0,0}, 14.8f * pScale, 15.2f * pScale, i, i+5, 8, ColorAlpha(themeBlue, 0.4f));
                }
            rlPopMatrix();
        rlPopMatrix();

        // PARALLAX 3D PULSANDO
        for(const auto& p : parallaxElements) {
            float currentSize = p.size * (1.0f + musicPulse * 1.5f); 
            Color pCol = bossActive ? LerpColor(p.color, C_RED, 0.6f) : p.color;
            Color glow = ColorAlpha(pCol, pCol.a + musicPulse * 0.5f); 
            if(p.isWire) DrawCubeWires(p.pos, currentSize, currentSize, currentSize, glow);
            else DrawCube(p.pos, currentSize, currentSize, currentSize, glow);
        }
        
        // CONSTRUÇÃO DO GRID HORIZONTAL DINÂMICO!
        float worldTopY = BOARD_HEIGHT;
        float worldMidY = BOARD_HEIGHT / 2.0f;
        float startX = -(currentGridWidth / 2.0f);
        float endX = (currentGridWidth / 2.0f);

        // Fundo Azul Opaco Profundo (95% visibilidade para focar as peças)
        Color backPanelColor = ColorAlpha({5, 15, 35, 255}, 0.95f);
        if (bossActive) backPanelColor = ColorAlpha({35, 5, 5, 255}, 0.95f);
        DrawCube({0.0f, worldMidY, -0.6f}, (float)currentGridWidth, (float)BOARD_HEIGHT, 0.1f, backPanelColor);
        
        // Linhas de Grade
        for (int i = 0; i <= currentGridWidth; i++) {
            float x = startX + i;
            DrawLine3D({x, 0, -0.5f}, {x, worldTopY, -0.5f}, ColorAlpha(themeCyan, 0.2f));
        }
        for (int i = 0; i <= BOARD_HEIGHT; i++) {
            float y = (float)i;
            DrawLine3D({startX, y, -0.5f}, {endX, y, -0.5f}, ColorAlpha(themeCyan, 0.2f));
        }

        // Pilares Neon Limitadores (Acompanham a largura atual)
        DrawCube({startX - 0.15f, worldMidY, 0}, 0.3f, BOARD_HEIGHT, 0.3f, ColorAlpha(themeCyan, 0.8f)); 
        DrawCube({endX + 0.15f, worldMidY, 0}, 0.3f, BOARD_HEIGHT, 0.3f, ColorAlpha(themeCyan, 0.8f));   
        DrawCube({0, worldTopY + 0.15f, 0}, currentGridWidth + 0.6f, 0.3f, 0.3f, ColorAlpha(themeCyan, 0.8f));              
        
        DrawCubeWires({startX - 0.15f, worldMidY, 0}, 0.35f, BOARD_HEIGHT, 0.35f, themeCyan);
        DrawCubeWires({endX + 0.15f, worldMidY, 0}, 0.35f, BOARD_HEIGHT, 0.35f, themeCyan);
        DrawCubeWires({0, worldTopY + 0.15f, 0}, currentGridWidth + 0.6f, 0.35f, 0.35f, themeCyan);
    }

    void DrawAllPieces(bool isReflection) {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] != 0) {
                    DrawSciFiBlock3D(GetWorldPos(j, i), pieceColors[board[i][j]], isReflection);
                }
            }
        }

        if (currentState == PLAYING) {
            int ghostY = currentY;
            while (IsValidMove(currentPiece, currentX, ghostY + 1)) {
                ghostY++;
            }

            for (int i = 0; i < currentPiece.size(); ++i) {
                for (int j = 0; j < currentPiece[i].size(); ++j) {
                    if (currentPiece[i][j] != 0) {
                        Vector3 ghostPos = {
                            (float)(currentX + j) - (currentGridWidth / 2.0f) + 0.5f,
                            (float)BOARD_HEIGHT - (ghostY + i) - 0.5f,
                            0.0f
                        };
                        if (ghostY > currentY) {
                            DrawSciFiBlock3D(ghostPos, pieceColors[currentColor], isReflection, true);
                        }
                    }
                }
            }

            for (int i = 0; i < currentPiece.size(); ++i) {
                for (int j = 0; j < currentPiece[i].size(); ++j) {
                    if (currentPiece[i][j] != 0) {
                        Vector3 dropPos = {
                            (float)(currentX + j) - (currentGridWidth / 2.0f) + 0.5f,
                            (float)BOARD_HEIGHT - (renderFallY + i) - 0.5f,
                            0.0f
                        };
                        DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection, false, currentIsBrilliant);
                    }
                }
            }
        }
    }

    // Função para desenhar raios gerados proceduralmente
    void DrawLightning(Vector3 start, Vector3 end, Color c) {
        Vector3 current = start;
        int segments = 8;
        for(int i=1; i<=segments; i++) {
            float t = (float)i / segments;
            Vector3 next = Vector3Lerp(start, end, t);
            
            if (i < segments) {
                // Caos matemático para o raio (Random procedural sem estado)
                float seed = start.x * i + (float)GetTime() * 50.0f;
                next.x += sin(seed) * 4.0f;
                next.y += cos(seed * 1.2f) * 4.0f;
                next.z += sin(seed * 0.8f) * 4.0f;
            }
            
            DrawLine3D(current, next, c);
            DrawLine3D({current.x + 0.2f, current.y, current.z + 0.2f}, next, WHITE);
            current = next;
        }
    }

    void DrawBoss() {
        if (bossEntryAnim < 0.01f) return;

        // O Boss agora está na FRENTE do grid
        Vector3 bossPos = { 0.0f, Lerp(40.0f, 22.0f, bossEntryAnim), 2.5f }; 
        float pulse = 1.0f + musicPulse * 0.5f;
        float time = (float)GetTime();

        // =========================================================
        // CAOS DA MATRIX: Ideogramas (Chuva Digital) e Raios
        // =========================================================
        
        // 1. Raios malucos pelo cenário cortando o céu até o boss
        if ((int)(time * 15.0f) % 7 == 0) { 
            for(int i=0; i<4; i++) {
                Vector3 p1 = { GetRandomFloat(-40.0f, 40.0f), GetRandomFloat(0.0f, 50.0f), GetRandomFloat(-30.0f, 10.0f) };
                Vector3 p2 = { bossPos.x, bossPos.y, bossPos.z }; 
                DrawLightning(p1, p2, (i%2==0) ? C_RED : C_PURPLE);
            }
        }

        // 2. Chuva da Matrix em volta da fase
        for (int i = 0; i < 200; i++) {
            float seed = i * 13.37f;
            float x = fmod(seed * 123.0f, 100.0f) - 50.0f;
            float z = fmod(seed * 321.0f, 60.0f) - 30.0f;
            
            // Para não cair no meio exato da arena atrapalhando a visão das peças
            if (x > -12.0f && x < 12.0f) x += (x > 0) ? 15.0f : -15.0f; 

            float speed = 20.0f + fmod(seed, 30.0f);
            float y = 60.0f - fmod(time * speed + seed, 80.0f);
            
            // Matrix verde tradicional + vermelho do Omega Red corrompendo a simulação
            Color rainCol = (i % 6 == 0) ? C_RED : C_GREEN; 
            float length = 1.0f + fmod(seed, 5.0f);
            
            // Renderiza "Ideogramas" como traços holográficos brilhantes caindo
            DrawCube({x, y, z}, 0.2f, length, 0.2f, ColorAlpha(rainCol, 0.6f));
            DrawCubeWires({x, y, z}, 0.3f, length + 0.2f, 0.3f, WHITE);
        }
        // =========================================================

        rlPushMatrix();
            rlTranslatef(bossPos.x, bossPos.y, bossPos.z);
            rlRotatef(time * 100.0f, 0, 1, 0); // Rotação caótica base
            rlRotatef((float)sin(time*3.0f) * 20.0f, 1, 0, 1);

            // Boss OMEGA RED - Peça de Tetris "T" distorcida e corrompida
            Vector3 blocks[4] = {
                {0, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}
            };

            for (int i = 0; i < 4; i++) {
                // Efeito visual de glitch (offsets instáveis e matemáticos)
                float glitchX = sin(time * 20.0f + i) * 0.5f * pulse;
                float glitchY = cos(time * 25.0f - i) * 0.5f * pulse;
                float glitchZ = sin(time * 30.0f + i * 2.0f) * 0.5f * pulse;
                
                Vector3 blockPos = {
                    blocks[i].x * 3.0f + glitchX,
                    blocks[i].y * 3.0f + glitchY,
                    blocks[i].z * 3.0f + glitchZ
                };

                // Cor distorcida pulsante alternando entre Branco (Raio) e Vermelho Sangue
                Color glitchColor = (sin(time * 50.0f + i * 10.0f) > 0.8f) ? WHITE : C_RED;
                
                DrawCubeWires(blockPos, 2.8f * pulse, 2.8f * pulse, 2.8f * pulse, C_ORANGE);
                DrawCube(blockPos, 2.5f * pulse, 2.5f * pulse, 2.5f * pulse, ColorAlpha(glitchColor, 0.8f));
            }

            // Gaiola caótica ao redor do vírus
            DrawCubeWires({0,0,0}, 12.0f * pulse, 12.0f * pulse, 12.0f * pulse, ColorAlpha(C_RED, 0.4f));
            DrawCubeWires({0,0,0}, 14.0f * pulse, 2.0f * pulse, 14.0f * pulse, ColorAlpha(C_ORANGE, 0.2f));

        rlPopMatrix();

        // Laser centralizador enquanto prepara ataque
        if (bossAttackTimer < 3.0f && bossAttackTimer > 0.0f) {
            float laserWidth = (3.0f - bossAttackTimer) * 0.8f;
            DrawCylinder({bossPos.x, bossPos.y - 12.0f, bossPos.z}, laserWidth, laserWidth, 24.0f, 16, ColorAlpha(C_RED, 0.4f + (float)sin(time*30)*0.2f));
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

        for(int i = 0; i < 200; i++) {
            ParallaxElement p;
            p.pos = { GetRandomFloat(-100.0f, 100.0f), GetRandomFloat(-50.0f, 80.0f), GetRandomFloat(-20.0f, -120.0f) };
            p.size = GetRandomFloat(0.2f, 1.2f);
            p.speed = GetRandomFloat(0.5f, 4.0f);
            p.isWire = (GetRandomFloat(0.0f, 1.0f) > 0.4f); 
            int c = rand() % 4;
            if(c == 0) p.color = ColorAlpha(C_CYAN, 0.2f);
            else if(c == 1) p.color = ColorAlpha(C_BLUE, 0.2f);
            else if(c == 2) p.color = ColorAlpha(C_PURPLE, 0.2f);
            else p.color = ColorAlpha(WHITE, 0.1f);
            parallaxElements.push_back(p);
        }

        sndMove = LoadSound("move.mp3");
        sndRotate = LoadSound("rotate.mp3");
        sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3");
        sndClear2 = LoadSound("clear2.mp3");
        sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3");
        sndGameOver = LoadSound("gameover.mp3");
        
        // Inicializa a playlist com a música 1
        currentMusicTrack = 1;
        LoadNextMusic();

        SpawnPiece();
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) {
            DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        }
        UnloadRenderTexture(renderTarget);
        UnloadSound(sndMove);
        UnloadSound(sndRotate);
        UnloadSound(sndDrop);
        UnloadSound(sndClear1);
        UnloadSound(sndClear2);
        UnloadSound(sndClear3);
        UnloadSound(sndClear4);
        UnloadSound(sndGameOver);
        if (sndMusic.stream.buffer != NULL) UnloadMusicStream(sndMusic);
    }

    void Update(float dt) {
        if (musicEnabled && sndMusic.stream.buffer != NULL) {
            UpdateMusicStream(sndMusic);
            
            // Verifica se a música atual acabou (com uma margem segura) para carregar a próxima
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                currentMusicTrack++;
                if (currentMusicTrack > 30) currentMusicTrack = 1; // Volta para 1 se passar de 30
                LoadNextMusic();
            }

            musicPulse = Lerp(musicPulse, globalMusicAmplitude * 10.0f, dt * 15.0f);
            if(musicPulse > 1.5f) musicPulse = 1.5f; 
        } else {
            musicPulse = Lerp(musicPulse, 0.0f, dt * 5.0f);
        }

        if (IsKeyPressed(KEY_U) || (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))) {
            ToggleFullscreen();
            isFullscreen = !isFullscreen;
        }

        // NOVO: Atalho global para pular para a próxima música!
        if (IsKeyPressed(KEY_K) && musicEnabled) {
            currentMusicTrack++;
            if (currentMusicTrack > 30) currentMusicTrack = 1;
            LoadNextMusic();
            mensagemEspecial = "TRACK " + to_string(currentMusicTrack);
            timerMensagem = 2.0f;
        }

        if (currentState == MENU) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { menuSelection--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { menuSelection++; TocarSom(sndMove); }
            
            if (menuSelection < 0) menuSelection = 3;
            if (menuSelection > 3) menuSelection = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                if (menuSelection == 0) { Restart(); currentState = PLAYING; }
                else if (menuSelection == 1) currentState = SETTINGS;
                else if (menuSelection == 2) currentState = CREDITS;
                else if (menuSelection == 3) confirmExit = true;
            }
        } 
        else if (currentState == SETTINGS) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { settingsSelection--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { settingsSelection++; TocarSom(sndMove); }
            
            if (settingsSelection < 0) settingsSelection = 3;
            if (settingsSelection > 3) settingsSelection = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                if (settingsSelection == 0) { ToggleFullscreen(); isFullscreen = !isFullscreen; }
                else if (settingsSelection == 1) { sfxEnabled = !sfxEnabled; }
                else if (settingsSelection == 2) { 
                    musicEnabled = !musicEnabled; 
                    if (!musicEnabled) PauseMusicStream(sndMusic);
                    else ResumeMusicStream(sndMusic);
                }
                else if (settingsSelection == 3) currentState = MENU;
            }
            if (IsKeyPressed(KEY_ESCAPE)) currentState = MENU;
        }
        else if (currentState == CREDITS) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                currentState = MENU;
            }
        }
        else if (currentState == PLAYING) {
            if (IsKeyPressed(KEY_P)) {
                isPaused = !isPaused;
                TocarSom(sndMove);
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                showExitPrompt = !showExitPrompt; 
            }

            if (showExitPrompt) {
                if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_Y)) {
                    showExitPrompt = false;
                    currentState = MENU; 
                }
                if (IsKeyPressed(KEY_N)) showExitPrompt = false;
                return; 
            }

            if (gameOver) {
                if (IsKeyPressed(KEY_ENTER)) {
                    Restart();
                    currentState = MENU; 
                }
                return;
            }

            if (!bossActive && linesClearedTotal >= linesUntilBoss) {
                bossActive = true;
                bossEncounterCount++;
                
                // ESCRITA DINÂMICA: Inicia com 6 HP (Encontro 1), depois 8 HP (Encontro 2), etc...
                bossHp = 4 + (bossEncounterCount * 2); 
                
                // ESCRITA DINÂMICA: Timer inicia em 10.0s (lento), cai 2.0s por encontro, até o limite turbo de 3.0s!
                currentBossAttackDelay = fmax(3.0f, 12.0f - (bossEncounterCount * 2.0f));
                bossAttackTimer = currentBossAttackDelay; 
                
                linesUntilBoss += 25; 
                TocarSom(sndGameOver);
                cameraShakeTimer = 2.0f;
                cameraShakeIntensity = 3.0f;
            }

            if (bossActive && !isPaused) {
                bossEntryAnim = Lerp(bossEntryAnim, 1.0f, dt * 2.0f);
                bossAttackTimer -= dt;
                
                if (bossAttackTimer <= 0.0f) {
                    // Decide aleatoriamente qual golpe sujo usar
                    if (GetRandomValue(0, 1) == 0) {
                        BossAddJunkLine();
                    } else {
                        BossThrowJunk();
                    }
                    bossAttackTimer = currentBossAttackDelay; // Reseta o timer baseado na dificuldade atual
                }

                themeCyan = LerpColor(themeCyan, C_RED, dt * 3.0f);
                themeBlue = LerpColor(themeBlue, C_ORANGE, dt * 3.0f);
                themeBg = LerpColor(themeBg, {30, 0, 0, 255}, dt * 3.0f);
                
                if (musicEnabled && sndMusic.stream.buffer != NULL) SetMusicPitch(sndMusic, Lerp(0.6f, 0.9f, musicPulse));
            } else {
                bossEntryAnim = Lerp(bossEntryAnim, 0.0f, dt * 2.0f);
                themeCyan = LerpColor(themeCyan, C_CYAN, dt * 5.0f);
                themeBlue = LerpColor(themeBlue, C_BLUE, dt * 5.0f);
                themeBg = LerpColor(themeBg, C_BG, dt * 5.0f);
                if (musicEnabled && sndMusic.stream.buffer != NULL) SetMusicPitch(sndMusic, 1.0f); 
            }

            if (!isPaused) {
                if (cameraZoomHoldTimer <= 0.0f) {
                    float speed = fmax(0.05f, 0.6f - (level * 0.05f)); 
                    fallTimer += dt;

                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && bombs > 0) {
                        bombs--;
                        NukeBoard();
                    }

                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                        if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); }
                        moveLeftTimer = 0.0f;
                    } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
                        moveLeftTimer += dt;
                        if (moveLeftTimer >= DAS_DELAY) {
                            moveLeftTimer -= ARR_RATE;
                            if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); }
                        }
                    } else {
                        moveLeftTimer = 0.0f;
                    }

                    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                        if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); }
                        moveRightTimer = 0.0f;
                    } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
                        moveRightTimer += dt;
                        if (moveRightTimer >= DAS_DELAY) {
                            moveRightTimer -= ARR_RATE;
                            if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); }
                        }
                    } else {
                        moveRightTimer = 0.0f;
                    }

                    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                        auto rotated = RotateMatrix(currentPiece);
                        if (IsValidMove(rotated, currentX, currentY)) { currentPiece = rotated; TocarSom(sndRotate); }
                    }
                    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) { fallTimer += dt * 15.0f; }
                    if (IsKeyPressed(KEY_SPACE)) { 
                        while (IsValidMove(currentPiece, currentX, currentY + 1)) {
                            currentY++; score += 2;
                        }
                        LockPiece(); 
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
                }
            } 
        }

        if (!isPaused) {
            for(auto& p : parallaxElements) {
                p.pos.z += p.speed * (1.0f + musicPulse * 6.0f) * dt; 
                if(p.pos.z > -10.0f) {
                    p.pos.z = GetRandomFloat(-120.0f, -150.0f);
                    p.pos.x = GetRandomFloat(-100.0f, 100.0f);
                    p.pos.y = GetRandomFloat(-50.0f, 80.0f);
                }
            }
        }

        if (cameraZoomHoldTimer > 0) {
            cameraZoomHoldTimer -= dt;
            cameraZoomOffset = Lerp(cameraZoomOffset, targetZoomOffset, dt * 10.0f); 
        } else {
            cameraZoomOffset = Lerp(cameraZoomOffset, 0.0f, dt * 3.0f); 
        }

        if (nukeSpinAngle > 0.0f) {
            nukeSpinAngle = Lerp(nukeSpinAngle, 0.0f, dt * 1.5f);
        }

        float orbitAngle = (float)GetTime() * 0.3f + nukeSpinAngle; 
        Vector3 targetCamPos = {
            -1.5f + (float)sin(orbitAngle) * 5.5f, 
            defaultCamPos.y + (float)sin(GetTime() * 0.5f) * 1.0f, 
            defaultCamPos.z + cameraZoomOffset
        };
        Vector3 currentTarget = defaultCamTarget;

        targetCamPos.y = Lerp(targetCamPos.y, defaultCamPos.y, 1.0f);
        currentTarget.y = Lerp(currentTarget.y, defaultCamTarget.y, 1.0f);

        // =========================================================
        // CINEMÁTICA DO BOSS: Câmera afasta (dobro) e foca no alto
        // =========================================================
        if (bossActive) {
            targetCamPos.y += 12.0f; 
            targetCamPos.z += 30.0f; // Afasta pra caramba pra ver toda a anomalia (era 26.0f, vai pra ~56.0f)
            currentTarget.y += 10.0f;
        }

        bool isZoomingIn = (cameraZoomHoldTimer > 0.0f && abs(cameraZoomOffset - targetZoomOffset) > 0.5f);
        bool isZoomingOut = (cameraZoomHoldTimer <= 0.0f && cameraZoomOffset < -0.5f);

        if ((isZoomingIn || isZoomingOut) && !isPaused) {
            motionBlurIntensity = Lerp(motionBlurIntensity, 1.0f, dt * 25.0f);
        } else {
            motionBlurIntensity = Lerp(motionBlurIntensity, 0.0f, dt * 15.0f);
        }

        if (cameraShakeTimer > 0 && !isPaused) {
            targetCamPos.x += GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            targetCamPos.y += GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            currentTarget.x += GetRandomFloat(-cameraShakeIntensity*0.5f, cameraShakeIntensity*0.5f);
            currentTarget.y += GetRandomFloat(-cameraShakeIntensity*0.5f, cameraShakeIntensity*0.5f);
            cameraShakeTimer -= dt;
        }

        camera.position = Vector3Lerp(camera.position, targetCamPos, dt * 10.0f);
        camera.target = Vector3Lerp(camera.target, currentTarget, dt * 10.0f);

        if (timerMensagem > 0 && !isPaused) timerMensagem -= dt;
    }

    void Draw() {
        BeginTextureMode(renderTarget);
        ClearBackground(C_BG);
        
        BeginMode3D(camera);
            DrawSciFiArena(); 
            DrawBoss(); 
            
            rlPushMatrix();
                rlScalef(1.0f, -1.0f, 1.0f);
                DrawAllPieces(true); 
            rlPopMatrix();

            // Chão refletivo opaco 
            DrawPlane({0, 0, 0}, {30.0f, 30.0f}, ColorAlpha(themeBg, 0.85f));

            DrawAllPieces(false);
            UpdateAndDrawParticles3D(GetFrameTime());
        EndMode3D();
        EndTextureMode();

        ClearBackground(BLACK);
        
        Rectangle source = { 0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
        Rectangle dest = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        DrawTexturePro(renderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);

        BeginBlendMode(BLEND_ADDITIVE);
        Rectangle bloomDest = { -5, -5, SCREEN_WIDTH + 10.0f, SCREEN_HEIGHT + 10.0f }; 
        DrawTexturePro(renderTarget.texture, source, bloomDest, {0,0}, 0.0f, ColorAlpha(WHITE, 0.4f));
        
        if (motionBlurIntensity > 0.01f) {
            for (int i = 1; i <= 6; i++) {
                float scale = 1.0f + (0.015f * i * motionBlurIntensity);
                float alpha = 0.15f * motionBlurIntensity;
                Rectangle mbDest = {
                    -(SCREEN_WIDTH * (scale - 1.0f)) / 2.0f,
                    -(SCREEN_HEIGHT * (scale - 1.0f)) / 2.0f,
                    SCREEN_WIDTH * scale,
                    SCREEN_HEIGHT * scale
                };
                DrawTexturePro(renderTarget.texture, source, mbDest, {0,0}, 0.0f, ColorAlpha(WHITE, alpha));
            }
        }
        EndBlendMode();

        if (currentState == MENU) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.6f));
            DrawText("TeTRABeTTA", SCREEN_WIDTH/2 - MeasureText("TeTRABeTTA", 80)/2, 200, 80, themeCyan);
            DrawText("3D EDITION", SCREEN_WIDTH/2 - MeasureText("3D EDITION", 30)/2, 290, 30, themeBlue);

            const char* menuItems[] = { "NOVO JOGO", "CONFIGURAÇÕES", "CRÉDITOS", "SAIR" };
            for (int i = 0; i < 4; i++) {
                Color c = (i == menuSelection) ? C_ORANGE : WHITE;
                if (i == menuSelection) DrawText("> ", SCREEN_WIDTH/2 - MeasureText(menuItems[i], 40)/2 - 40, 450 + i * 80, 40, C_ORANGE);
                DrawText(menuItems[i], SCREEN_WIDTH/2 - MeasureText(menuItems[i], 40)/2, 450 + i * 80, 40, c);
            }
        } 
        else if (currentState == SETTINGS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("OPÇÕES", SCREEN_WIDTH/2 - MeasureText("OPÇÕES", 60)/2, 150, 60, themeCyan);

            string opt1 = string("TELA CHEIA: ") + (isFullscreen ? "SIM" : "NÃO");
            string opt2 = string("EFEITOS (SFX): ") + (sfxEnabled ? "SIM" : "NÃO");
            string opt3 = string("MÚSICA: ") + (musicEnabled ? "SIM" : "NÃO");
            string opt4 = "VOLTAR AO MENU";

            const char* setItems[] = { opt1.c_str(), opt2.c_str(), opt3.c_str(), opt4.c_str() };
            for (int i = 0; i < 4; i++) {
                Color c = (i == settingsSelection) ? C_ORANGE : WHITE;
                if (i == settingsSelection) DrawText("> ", SCREEN_WIDTH/2 - MeasureText(setItems[i], 40)/2 - 40, 350 + i * 80, 40, C_ORANGE);
                DrawText(setItems[i], SCREEN_WIDTH/2 - MeasureText(setItems[i], 40)/2, 350 + i * 80, 40, c);
            }
        }
        else if (currentState == CREDITS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("CRÉDITOS", SCREEN_WIDTH/2 - MeasureText("CRÉDITOS", 60)/2, 150, 60, themeCyan);
            
            DrawText("PRODUTOR EXECUTIVO E DIRETOR", SCREEN_WIDTH/2 - MeasureText("PRODUTOR EXECUTIVO E DIRETOR", 30)/2, 350, 30, C_ORANGE);
            DrawText("Igor Bettarello - OMEGARED", SCREEN_WIDTH/2 - MeasureText("Igor Bettarello - OMEGARED", 50)/2, 400, 50, WHITE);
            
            DrawText("PROGRAMADOR LÍDER", SCREEN_WIDTH/2 - MeasureText("PROGRAMADOR LÍDER", 30)/2, 550, 30, C_ORANGE);
            DrawText("AI Assistant - Gemini God Mode", SCREEN_WIDTH/2 - MeasureText("AI Assistant - Gemini God Mode", 50)/2, 600, 50, WHITE);

            DrawText("[ PRESSIONE ESC PARA VOLTAR ]", SCREEN_WIDTH/2 - MeasureText("[ PRESSIONE ESC PARA VOLTAR ]", 20)/2, 900, 20, GRAY);
        }
        else if (currentState == PLAYING) {
            // Aumentado a HUD Esquerda para caber Estrelas, Bombas e Atalhos
            DrawRectangle(40, 40, 380, 380, ColorAlpha(themeBg, 0.8f));
            DrawRectangleLinesEx({40, 40, 380, 380}, 2, ColorAlpha(themeCyan, 0.6f));
            DrawLine(40, 90, 420, 90, ColorAlpha(themeCyan, 0.6f));
            DrawRectangle(40, 40, 20, 20, themeCyan); 

            DrawText("TeTRABeTTA", 60, 60, 20, themeCyan);
            DrawText("STATUS: ONLINE", 260, 65, 12, bossActive ? C_RED : C_GREEN);

            DrawText("PONTUAÇÃO:", 60, 105, 20, WHITE);
            DrawText(TextFormat("%08d", score), 60, 135, 45, themeCyan);

            DrawText(TextFormat("NÍVEL: %02d", level), 60, 210, 26, WHITE);
            DrawText(TextFormat("CONTINUES: %d", continues), 60, 245, 20, C_RED); 
            DrawText(TextFormat("ESTRELAS: %d/10", stars), 60, 275, 20, C_YELLOW); 
            DrawText(TextFormat("[DIR] SYSTEM PURGE (BOMBAS): %d", bombs), 60, 310, 15, C_ORANGE); 
            DrawText("[K] TROCAR MÚSICA", 60, 345, 18, C_GREEN); 

            DrawRectangle(SCREEN_WIDTH - 380, 40, 340, 300, ColorAlpha(themeBg, 0.8f));
            DrawRectangleLinesEx({SCREEN_WIDTH - 380, 40, 340, 300}, 2, ColorAlpha(themeCyan, 0.6f));
            DrawText("PRÓXIMA PEÇA:", SCREEN_WIDTH - 350, 60, 20, WHITE);
            DrawLine(SCREEN_WIDTH - 380, 100, SCREEN_WIDTH - 40, 100, ColorAlpha(themeCyan, 0.6f));

            if (bossActive) {
                DrawRectangle(SCREEN_WIDTH/2 - 300, 40, 600, 60, ColorAlpha(themeBg, 0.8f));
                DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 300, 40, 600, 60}, 2, C_RED);
                
                // HUD indica o Encontro
                string avisoBoss = "ALERTA: VÍRUS OMEGARED - NIVEL " + to_string(bossEncounterCount);
                DrawText(avisoBoss.c_str(), SCREEN_WIDTH/2 - MeasureText(avisoBoss.c_str(), 24)/2, 45, 24, C_RED);
                
                float hpRatio = (float)bossHp / (4.0f + (bossEncounterCount * 2.0f)); 
                DrawRectangle(SCREEN_WIDTH/2 - 280, 75, 560 * hpRatio, 15, C_ORANGE);
                DrawRectangleLines(SCREEN_WIDTH/2 - 280, 75, 560, 15, WHITE);
            }

            if (!gameOver && !isPaused) {
                Camera3D staticCamera = { 0 };
                staticCamera.position = { 0.0f, 0.0f, 10.0f }; 
                staticCamera.target = { 0.0f, 0.0f, 0.0f };
                staticCamera.up = { 0.0f, 1.0f, 0.0f };
                staticCamera.fovy = 55.0f;
                staticCamera.projection = CAMERA_PERSPECTIVE;

                BeginMode3D(staticCamera);
                    Vector3 nextPieceOrigin = { 7.2f, 3.2f, 0.0f }; 
                    rlPushMatrix();
                        rlTranslatef(nextPieceOrigin.x, nextPieceOrigin.y, nextPieceOrigin.z);
                        rlScalef(0.4f, 0.4f, 0.4f); 
                        rlRotatef((float)GetTime() * 50.0f, 0, 1, 0); 
                        rlRotatef((float)sin(GetTime()*2)*15.0f, 1, 0, 0); 
                        
                        float offsetX = nextPiece[0].size() / 2.0f;
                        float offsetY = nextPiece.size() / 2.0f;

                        for (int i = 0; i < nextPiece.size(); ++i) {
                            for (int j = 0; j < nextPiece[i].size(); ++j) {
                                if (nextPiece[i][j] != 0) {
                                    Vector3 npPos = { (float)j - offsetX + 0.5f, (float)-i + offsetY - 0.5f, 0.0f };
                                    DrawSciFiBlock3D(npPos, pieceColors[nextColor], false, false, nextIsBrilliant);
                                }
                            }
                        }
                    rlPopMatrix();
                EndMode3D();
            }

            if (timerMensagem > 0) {
                float scale = 1.0f + (float)sin(GetTime() * 20.0f) * 0.1f;
                int fontSize = 55 * scale; 
                int textWidth = MeasureText(mensagemEspecial.c_str(), fontSize);
                
                BeginBlendMode(BLEND_ADDITIVE);
                DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2 - 4, SCREEN_HEIGHT/3, fontSize, C_RED);
                DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2 + 4, SCREEN_HEIGHT/3, fontSize, themeCyan);
                EndBlendMode();
                
                DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/3, fontSize, WHITE);
            }

            if (isPaused) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.6f));
                DrawText("SISTEMA EM PAUSA", SCREEN_WIDTH/2 - MeasureText("SISTEMA EM PAUSA", 60)/2, SCREEN_HEIGHT/2 - 30, 60, themeCyan);
            }

            if (gameOver) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.9f));
                DrawText("FALHA CRÍTICA - SEM CONTINUES", SCREEN_WIDTH/2 - MeasureText("FALHA CRÍTICA - SEM CONTINUES", 60)/2, SCREEN_HEIGHT/2 - 80, 60, C_RED);
                DrawText("PRESSIONE [ENTER] PARA MENU", SCREEN_WIDTH/2 - MeasureText("PRESSIONE [ENTER] PARA MENU", 30)/2, SCREEN_HEIGHT/2 + 30, 30, themeCyan);
            }

            if (showExitPrompt) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.95f));
                
                DrawRectangle(SCREEN_WIDTH/2 - 400, SCREEN_HEIGHT/2 - 150, 800, 300, ColorAlpha(themeBg, 0.9f));
                DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 400, (float)SCREEN_HEIGHT/2 - 150, 800, 300}, 2, themeCyan);
                
                DrawText("ABORTAR SIMULAÇÃO?", SCREEN_WIDTH/2 - MeasureText("ABORTAR SIMULAÇÃO?", 50)/2, SCREEN_HEIGHT/2 - 60, 50, C_ORANGE);
                DrawText("[S] SIM   -   [N] NÃO", SCREEN_WIDTH/2 - MeasureText("[S] SIM   -   [N] NÃO", 30)/2, SCREEN_HEIGHT/2 + 40, 30, WHITE);
            }
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0;
        level = 1;
        continues = 3; 
        bombs = 2; 
        stars = 0; 
        currentGridWidth = 10; // Arena espremida inicialmente
        nextIsBrilliant = false;
        currentIsBrilliant = false;
        linesClearedTotal = 0;
        gameOver = false;
        isPaused = false;
        timerMensagem = 0;
        moveLeftTimer = 0.0f;
        moveRightTimer = 0.0f;
        nukeSpinAngle = 0.0f;
        
        // Zera os atributos do chefão
        bossActive = false;
        bossEntryAnim = 0.0f;
        linesUntilBoss = 15;
        bossEncounterCount = 0;
        currentBossAttackDelay = 8.0f;
        
        particles.clear();
        SpawnPiece();
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        cameraZoomOffset = 0.0f;
        lastClearedY = 0.0f; 
        
        // Reinicia a musica para a faixa 1 ao começar novo jogo
        currentMusicTrack = 1;
        LoadNextMusic();
    }

    bool ShouldExit() { return confirmExit; }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TeTRABeTTA - 3D Edition");
    SetExitKey(KEY_NULL); 
    ToggleFullscreen();
    InitAudioDevice(); 
    SetTargetFPS(60);

    JogoTetris3D game;

    while (!WindowShouldClose() && !game.ShouldExit()) {
        float dt = GetFrameTime();
        game.Update(dt);
        
        BeginDrawing();
        game.Draw();
        EndDrawing();
    }

    CloseAudioDevice(); 
    CloseWindow();
    
    return 0;
}