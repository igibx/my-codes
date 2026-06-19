#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <random>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>

using namespace std;

const char* vertexShaderCode = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
out vec2 fragTexCoord;
out vec4 fragColor;
uniform mat4 mvp;
uniform float time;
uniform float pulse;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    vec3 pos = vertexPosition;
    // Efeito de Vertex Shader: Distorção sutil baseada no pulso da música
    float wave = sin(pos.y * 15.0 + time * 8.0) * 0.005 * pulse;
    pos.x += wave;
    
    gl_Position = mvp * vec4(pos, 1.0);
}
)";

const char* fragmentShaderCode = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float pulse;

void main() {
    vec2 texel = 1.0 / resolution;
    vec4 baseColor = texture(texture0, fragTexCoord);

    // --- ANTIALIASING (FXAA Simplificado / Blur Direcional) ---
    vec4 aaColor = baseColor * 0.5;
    aaColor += texture(texture0, fragTexCoord + vec2(-texel.x, -texel.y)) * 0.125;
    aaColor += texture(texture0, fragTexCoord + vec2( texel.x, -texel.y)) * 0.125;
    aaColor += texture(texture0, fragTexCoord + vec2(-texel.x,  texel.y)) * 0.125;
    aaColor += texture(texture0, fragTexCoord + vec2( texel.x,  texel.y)) * 0.125;

    // --- BLOOM LEVE OTIMIZADO (Apenas para o Cenário) ---
    vec2 offset = texel * (4.0 + pulse * 6.0);
    vec4 bloom = vec4(0.0);
    
    // Coleta amostras espaçadas e subtrai 0.4 para isolar os neons, economizando performance
    bloom += max(vec4(0.0), texture(texture0, fragTexCoord + vec2(offset.x, 0.0)) - 0.4);
    bloom += max(vec4(0.0), texture(texture0, fragTexCoord - vec2(offset.x, 0.0)) - 0.4);
    bloom += max(vec4(0.0), texture(texture0, fragTexCoord + vec2(0.0, offset.y)) - 0.4);
    bloom += max(vec4(0.0), texture(texture0, fragTexCoord - vec2(0.0, offset.y)) - 0.4);

    // Combina o Antialiasing com o efeito de Bloom impulsionado
    finalColor = (aaColor + bloom * (1.2 + pulse)) * fragColor;
}
)";

const int MAX_BOARD_WIDTH = 22; 
const int BOARD_HEIGHT = 20;    
const int SCREEN_WIDTH = 1920;  
const int SCREEN_HEIGHT = 1080; 
const float CUBE_SIZE = 0.9f;

const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 30, 144, 255, 255 }; 
const Color C_ORANGE = { 255, 140, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 100, 255 };
const Color C_PURPLE = { 200, 0, 255, 255 };
const Color C_RED    = { 255, 20, 60, 255 };
const Color C_BG     = { 2, 4, 10, 255 }; 

Color pieceColors[9] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, DARKGRAY };

enum GameState { AUTH, MENU, SETTINGS, CREDITS, PLAYING };

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

struct Star3D {
    Vector3 pos;
    float speed;
    float size;
    Color color;
};
vector<Star3D> starfield;

struct Particle3D {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    bool isSpark; 
    int shapeType;
    Vector3 rotation;
    Vector3 rotVelocity;
};

vector<Particle3D> particles;

float GetRandomFloat(float min, float max) {
    return min + (max - min) * ((float)rand() / RAND_MAX);
}

void SpawnParticles3D(Vector3 pos, Color color, int amount, float force, int shapeType = 0) {
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
        p.size = p.isSpark ? GetRandomFloat(0.2f, 0.8f) : GetRandomFloat(0.3f, 0.7f);
        p.shapeType = shapeType;
        p.rotation = { GetRandomFloat(0, 360), GetRandomFloat(0, 360), GetRandomFloat(0, 360) };
        p.rotVelocity = { GetRandomFloat(-400, 400), GetRandomFloat(-400, 400), GetRandomFloat(-400, 400) };
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
        p.shapeType = 0;
        p.rotation = { GetRandomFloat(0, 360), GetRandomFloat(0, 360), GetRandomFloat(0, 360) };
        p.rotVelocity = { GetRandomFloat(-200, 200), GetRandomFloat(-200, 200), GetRandomFloat(-200, 200) };
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

        particles[i].rotation.x += particles[i].rotVelocity.x * dt;
        particles[i].rotation.y += particles[i].rotVelocity.y * dt;
        particles[i].rotation.z += particles[i].rotVelocity.z * dt;

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
                rlPushMatrix();
                rlTranslatef(particles[i].position.x, particles[i].position.y, particles[i].position.z);
                rlRotatef(particles[i].rotation.x, 1, 0, 0);
                rlRotatef(particles[i].rotation.y, 0, 1, 0);
                rlRotatef(particles[i].rotation.z, 0, 0, 1);
                
                float s = particles[i].size;
                switch (particles[i].shapeType % 5) {
                    case 0: DrawCubeWiresV({0,0,0}, {s, s, s}, fadeColor); break;
                    case 1: DrawSphereWires({0,0,0}, s*0.6f, 6, 6, fadeColor); break;
                    case 2: DrawCubeWiresV({0,0,0}, {s, s, s}, fadeColor); break;
                    case 3: 
                        DrawCubeWiresV({0,0,0}, {s, s*0.2f, s*0.2f}, fadeColor);
                        DrawCubeWiresV({0,0,0}, {s*0.2f, s, s*0.2f}, fadeColor);
                        DrawCubeWiresV({0,0,0}, {s*0.2f, s*0.2f, s}, fadeColor);
                        break;
                    case 4: DrawCylinderWires({0, -s*0.4f, 0}, s*0.6f, s*0.6f, s*0.8f, 8, fadeColor); break;
                }
                rlPopMatrix();
            }
        }
    }
}


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

class JogoTetris3D {
private:
    GameState currentState = AUTH; 
    int menuSelection = 0;
    int settingsSelection = 0;

    string currentInputKey = "";
    bool checkingOnline = false;
    float checkTimer = 0.0f;
    string authStatusMsg = "SISTEMA PROTEGIDO. INSIRA A CHAVE ONLINE:";
    int charCount = 0;

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int continues = 3; 
    int bombs = 2; 
    int stars = 0; 
    int currentGridWidth = 10; 
    int linesClearedTotal = 0;
    
    bool gameOver = false;
    bool isPaused = false; 
    bool isClassicMode = false; 
    
    vector<vector<int>> currentPiece;
    int currentX, currentY, currentColor;
    float renderFallY; 
    bool currentIsBrilliant = false; 
    
    vector<vector<int>> nextPiece;
    int nextColor;
    bool nextIsBrilliant = false; 

    float fallTimer = 0.0f;
    float gridExpansionTimer = 0.0f; 
    float currentGridElevation = 0.15f; 
    string mensagemEspecial = "";
    float timerMensagem = 0.0f;

    float musicPulse = 0.0f;
    Color themeCyan = C_CYAN;
    Color themeBlue = C_BLUE;
    Color themeBg = C_BG;

    Camera3D camera = { 0 };
    Vector3 defaultCamPos = { 0.0f, 14.0f, 31.2f }; 
    Vector3 defaultCamTarget = { 0.0f, 13.0f, 0.0f };
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;
    float cameraZoomOffset = 0.0f;
    float targetZoomOffset = 0.0f; 
    float cameraZoomHoldTimer = 0.0f; 
    float lastClearedY = 0.0f; 
    float motionBlurIntensity = 0.0f; 
    float nukeSpinAngle = 0.0f; 
    float bossOrbitAngle = 0.0f; 
    
    float moveLeftTimer = 0.0f;
    float moveRightTimer = 0.0f;
    const float DAS_DELAY = 0.15f; 
    const float ARR_RATE = 0.03f;  

    bool bossActive = false;
    int bossHp = 0;
    int linesUntilBoss = 15; 
    float bossAttackTimer = 0.0f;
    float bossEntryAnim = 0.0f;
    int bossEncounterCount = 0; 
    float currentBossAttackDelay = 16.0f; 

    bool showExitPrompt = false;
    bool confirmExit = false;

    bool sfxEnabled = true;
    bool musicEnabled = true;
    bool isFullscreen = true; 
    
    RenderTexture2D bgRenderTarget; 

    Sound sndMove;
    Sound sndRotate;
    Sound sndDrop;
    Sound sndClear1;
    Sound sndClear2;
    Sound sndClear3;
    Sound sndClear4;
    Sound sndGameOver;
    Music sndMusic = { 0 };
    int currentMusicTrack = 1; 

    mt19937 rng;

    Shader postProcessShader;
    int resLoc;
    int timeLoc;
    int pulseLoc;

    bool ValidateKey(string key) {
        if (key.length() != 16) return false;
        if (key == "OMEGARED2026PRO1") return true; 
        
        int sum = 0;
        for(char c : key) sum += c;
        if (sum % 10 == 7) return true; 
        
        return false;
    }

    void CheckLocalLicense() {
        if (FileExists("omegared_sys.key")) {
            char *data = LoadFileText("omegared_sys.key");
            if (data != NULL) {
                string key = "";
                for (int i = 0; data[i] != '\0'; i++) {
                    if (isalnum(data[i])) key += toupper(data[i]);
                }
                UnloadFileText(data);
                if (ValidateKey(key)) {
                    currentState = MENU; 
                }
            }
        }
    }

    void SaveLicense(string key) {
        SaveFileText("omegared_sys.key", (char*)key.c_str());
    }

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

    void LoadNextMusic() {
        if (sndMusic.stream.buffer != NULL) {
            DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
            UnloadMusicStream(sndMusic);
            sndMusic.stream.buffer = NULL;
        }
        string fileName = "music" + to_string(currentMusicTrack) + ".mp3";
        
        if (FileExists(fileName.c_str())) {
            sndMusic = LoadMusicStream(fileName.c_str());
            if (sndMusic.stream.buffer != NULL) {
                PlayMusicStream(sndMusic);
                AttachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
            }
        } else {
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
        
        if (currentIsBrilliant) {
            if (currentGridWidth < MAX_BOARD_WIDTH) {
                int expansion = 2; 
                int shift = expansion / 2;
                
                for (int i = 0; i < BOARD_HEIGHT; i++) {
                    for (int j = currentGridWidth - 1; j >= 0; j--) {
                        board[i][j + shift] = board[i][j];
                    }
                    for (int j = 0; j < shift; j++) board[i][j] = 0; 
                }
                
                currentGridWidth += expansion;
                gridExpansionTimer = 1.0f; 
                mensagemEspecial = "ARENA EXPANDIDA!";
                timerMensagem = 3.0f;
                cameraShakeTimer = 1.0f;
                cameraShakeIntensity = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_YELLOW, 100, 20.0f, 4);
                TocarSom(sndClear4); 
            } else {
                mensagemEspecial = "PODER MÁXIMO!";
                score += 2000 * level;
                timerMensagem = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_CYAN, 100, 20.0f, 3);
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
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 40, 18.0f, board[i][j]);
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
            
            targetZoomOffset = -2.5f - (linesClearedNow * 0.3f); 
            cameraZoomHoldTimer = 1.0f; 

            bool canShowCombo = (mensagemEspecial == "" || 
                                 mensagemEspecial == "GOOD !" || 
                                 mensagemEspecial == "VERY GOOD !!!" || 
                                 mensagemEspecial == "IMPRESSIVE!!!" || 
                                 mensagemEspecial == "MARVELOUS!!!!!!");

            if (linesClearedNow == 1) {
                score += 100 * level;
                if (canShowCombo) { mensagemEspecial = "GOOD !"; timerMensagem = 2.0f; }
                TocarSom(sndClear1);
            } 
            else if (linesClearedNow == 2) {
                score += 300 * level;
                if (canShowCombo) { mensagemEspecial = "VERY GOOD !!!"; timerMensagem = 2.0f; }
                TocarSom(sndClear2);
            } 
            else if (linesClearedNow == 3) {
                score += 500 * level;
                if (canShowCombo) { mensagemEspecial = "IMPRESSIVE!!!"; timerMensagem = 2.0f; }
                TocarSom(sndClear3);
            } 
            else if (linesClearedNow >= 4) {
                score += 800 * level;
                if (canShowCombo) { mensagemEspecial = "MARVELOUS!!!!!!"; timerMensagem = 3.0f; }
                cameraShakeIntensity = 4.0f; 
                TocarSom(sndClear4);
            }

            if (bossActive) {
                bossHp -= linesClearedNow; 
                if (bossHp <= 0) {
                    bossActive = false;
                    score += 5000 * level;
                    SpawnParticles3D({0, 20.0f, -5.0f}, C_RED, 200, 40.0f, 8); 
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
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 15, 25.0f, board[i][j]);
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
            targetZoomOffset = -3.0f; 
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

    void DrawSciFiBlock3D(Vector3 pos, Color c, bool isReflection, bool isGhost = false, bool isBrilliant = false, int shapeType = 0) {
        float s = CUBE_SIZE * 0.98f; 
        
        Color wireColor = isReflection ? ColorAlpha(c, 0.3f) : (isGhost ? ColorAlpha(c, 0.2f) : c);
        Color glowColor = isReflection ? ColorAlpha(WHITE, 0.1f) : (isGhost ? BLANK : ColorAlpha(WHITE, 0.8f));

        if (isBrilliant && !isGhost) {
            float t = (float)GetTime() * 5.0f;
            wireColor = ColorFromHSV(fmod(t * 60.0f, 360.0f), 1.0f, 1.0f);
            glowColor = ColorAlpha(WHITE, 0.9f);
        }

        DrawCubeWires(pos, s, s, s, isGhost ? ColorAlpha(wireColor, 0.4f) : ColorAlpha(wireColor, 1.0f));

        if (!isGhost) {
            float coreAlpha = isReflection ? 0.15f : 0.5f; 
            DrawCube(pos, s * 0.95f, s * 0.95f, s * 0.95f, ColorAlpha(c, coreAlpha));

            if (!isReflection) {
                rlPushMatrix();
                rlTranslatef(pos.x, pos.y, pos.z);
                
                float rot = (float)GetTime() * 60.0f;
                rlRotatef(rot * ((shapeType % 2 == 0) ? 1.0f : -1.0f), 0, 1, 0);
                rlRotatef(rot * 0.5f, 1, 0, 0);

                switch (shapeType % 5) {
                    case 0: 
                        DrawCubeWiresV({0,0,0}, {s*0.6f, s*0.6f, s*0.6f}, wireColor);
                        if(glowColor.a > 0) DrawCubeWiresV({0,0,0}, {s*0.7f, s*0.7f, s*0.7f}, glowColor);
                        break;
                    case 1: 
                        DrawSphereWires({0,0,0}, s*0.35f, 6, 6, wireColor);
                        if(glowColor.a > 0) DrawSphereWires({0,0,0}, s*0.45f, 6, 6, glowColor);
                        break;
                    case 2:
                        rlRotatef(45.0f, 1, 1, 0);
                        DrawCubeWiresV({0,0,0}, {s*0.5f, s*0.5f, s*0.5f}, wireColor);
                        if(glowColor.a > 0) DrawCubeWiresV({0,0,0}, {s*0.6f, s*0.6f, s*0.6f}, glowColor);
                        break;
                    case 3: 
                        DrawCubeWiresV({0,0,0}, {s*0.8f, s*0.15f, s*0.15f}, wireColor);
                        DrawCubeWiresV({0,0,0}, {s*0.15f, s*0.8f, s*0.15f}, wireColor);
                        DrawCubeWiresV({0,0,0}, {s*0.15f, s*0.15f, s*0.8f}, wireColor);
                        if(glowColor.a > 0) DrawCubeWiresV({0,0,0}, {s*0.3f, s*0.3f, s*0.3f}, glowColor); 
                        break;
                    case 4:
                        DrawCylinderWires({0, -s*0.2f, 0}, s*0.4f, s*0.4f, s*0.4f, 8, wireColor);
                        if(glowColor.a > 0) DrawCylinderWires({0, -s*0.25f, 0}, s*0.5f, s*0.5f, s*0.5f, 8, glowColor);
                        break;
                }
                rlPopMatrix();
            }
        }
    }

    void DrawSciFiBackground() {
        float time = (float)GetTime();
        
        for(const auto& s : starfield) {
            Vector3 tail = { s.pos.x, s.pos.y, s.pos.z - (s.speed * 0.3f * (3.0f + musicPulse * 8.0f)) };
            
            float intensity = Clamp(musicPulse, 0.0f, 1.5f) / 1.5f; 
            float variance = fmod(s.speed, 40.0f) - 20.0f; 
            
            float hue = Lerp(220.0f, 0.0f, intensity) + variance; 
            if (hue < 0.0f) hue += 360.0f;
            else if (hue > 360.0f) hue -= 360.0f;
            
            float sat = Lerp(0.2f, 1.0f, intensity); 
            float brightness = Lerp(0.4f, 0.9f, intensity);
            
            Color headCol = ColorFromHSV(hue, sat, brightness);
            Color tailCol = ColorFromHSV(hue + 30.0f, sat, 0.0f); 
            headCol.a = s.color.a; 
            
            if (bossActive) {
                headCol = ColorAlpha(C_RED, s.color.a);
                tailCol = ColorAlpha(C_ORANGE, 0.0f);
            }
            
            rlBegin(RL_LINES);
                rlColor4ub(tailCol.r, tailCol.g, tailCol.b, tailCol.a);
                rlVertex3f(tail.x, tail.y, tail.z);
                
                rlColor4ub(headCol.r, headCol.g, headCol.b, headCol.a);
                rlVertex3f(s.pos.x, s.pos.y, s.pos.z);
            rlEnd();
        }

        auto DrawRing3D = [](Vector3 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color) {
            rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            float angleStep = (endAngle - startAngle) / segments;
            for (int i = 0; i < segments; i++) {
                float a1 = (startAngle + i * angleStep) * DEG2RAD;
                float a2 = (startAngle + (i + 1) * angleStep) * DEG2RAD;
                
                rlVertex3f(center.x + cos(a1)*innerRadius, center.y, center.z + sin(a1)*innerRadius);
                rlVertex3f(center.x + cos(a2)*innerRadius, center.y, center.z + sin(a2)*innerRadius);
                rlVertex3f(center.x + cos(a2)*outerRadius, center.y, center.z + sin(a2)*outerRadius);
                rlVertex3f(center.x + cos(a1)*outerRadius, center.y, center.z + sin(a1)*outerRadius);
            }
            rlEnd();
        };

        float pScale = 1.4f + musicPulse * 0.1f;
        float groundY = currentGridElevation - 0.1f; 
        int segments = 180; 

        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * PI * 2.0f;
            float angle2 = (float)(i + 1) / segments * PI * 2.0f;

            float wave1 = sin(angle1 * 8.0f + time * 15.0f) * musicPulse * 2.5f;
            float wave2 = sin(angle2 * 8.0f + time * 15.0f) * musicPulse * 2.5f;

            float r1 = 11.0f * pScale + wave1;
            float r2 = 11.0f * pScale + wave2;

            Vector3 p1 = { cos(angle1) * r1, groundY, sin(angle1) * r1 };
            Vector3 p2 = { cos(angle2) * r2, groundY, sin(angle2) * r2 };

            float mix1 = (sin(angle1 * 3.0f + time * 8.0f) + 1.0f) / 2.0f;
            Color waveCol1;
            if (mix1 < 0.5f) waveCol1 = LerpColor(C_YELLOW, WHITE, mix1 * 2.0f);
            else waveCol1 = LerpColor(WHITE, C_BLUE, (mix1 - 0.5f) * 2.0f);

            if (bossActive) waveCol1 = LerpColor(C_RED, C_ORANGE, mix1);

            DrawLine3D(p1, p2, waveCol1);
        }

        Color inRingBase = bossActive ? ColorAlpha(C_RED, 0.6f) : ColorAlpha(WHITE, 0.6f);
        Color inRingDetail = bossActive ? ColorAlpha(C_ORANGE, 0.9f) : ColorAlpha(C_YELLOW, 0.9f);
        
        DrawRing3D({0, groundY, 0}, 9.8f * pScale, 10.2f * pScale, time * 60.0f, time * 60.0f + 360.0f, 64, inRingBase);
        for(int i=0; i<360; i+=30) {
            DrawRing3D({0, groundY + 0.01f, 0}, 10.2f * pScale, 11.0f * pScale, time * 60.0f + i, time * 60.0f + i + 15.0f, 8, inRingDetail);
        }

        Color outRingBase = bossActive ? ColorAlpha(C_ORANGE, 0.5f) : ColorAlpha(C_BLUE, 0.6f);
        Color outRingDetail = bossActive ? ColorAlpha(WHITE, 0.8f) : ColorAlpha(WHITE, 0.9f);

        DrawRing3D({0, groundY, 0}, 14.5f * pScale, 14.8f * pScale, -time * 40.0f, -time * 40.0f + 360.0f, 64, outRingBase);
        for(int i=0; i<360; i+=10) {
            DrawRing3D({0, groundY + 0.01f, 0}, 14.8f * pScale, 15.2f * pScale, -time * 40.0f + i, -time * 40.0f + i + 5.0f, 8, outRingDetail);
        }
        
        float startY = currentGridElevation;
        float extraTop = 3.0f; 
        float worldMidY = startY + (BOARD_HEIGHT + extraTop) / 2.0f;
        float alphaPanel = 0.95f;
        if (bossEntryAnim > 0.01f) alphaPanel = Lerp(0.95f, 0.0f, bossEntryAnim); 
        
        Color backPanelColor = ColorAlpha({5, 15, 35, 255}, alphaPanel);
        if (bossActive || bossEntryAnim > 0.01f) {
            backPanelColor = ColorAlpha({35, 5, 5, 255}, alphaPanel);
        }
        
        if (alphaPanel > 0.05f) { 
          
            DrawCube({0.0f, worldMidY, -0.55f}, (float)currentGridWidth - 0.3f, (float)(BOARD_HEIGHT + extraTop) - 0.3f, 0.05f, backPanelColor);
        }
    }

    void DrawSciFiGrid() {
        float worldTopY = BOARD_HEIGHT;
        float worldMidY = BOARD_HEIGHT / 2.0f;
        float startX = -(currentGridWidth / 2.0f);
        float endX = (currentGridWidth / 2.0f);

        for (int i = 0; i <= currentGridWidth; i++) {
            float x = startX + i;
            DrawLine3D({x, 0, -0.5f}, {x, worldTopY, -0.5f}, ColorAlpha(themeCyan, 0.3f));
        }
        for (int i = 0; i <= BOARD_HEIGHT; i++) {
            float y = (float)i;
            DrawLine3D({startX, y, -0.5f}, {endX, y, -0.5f}, ColorAlpha(themeCyan, 0.3f));
        }

        DrawCubeWires({startX - 0.15f, worldMidY, 0}, 0.35f, BOARD_HEIGHT, 0.35f, themeCyan);
        DrawCubeWires({endX + 0.15f, worldMidY, 0}, 0.35f, BOARD_HEIGHT, 0.35f, themeCyan);
        DrawCubeWires({0, worldTopY + 0.15f, 0}, currentGridWidth + 0.6f, 0.35f, 0.35f, themeCyan);

        if (gridExpansionTimer > 0.0f) {
            float expAlpha = gridExpansionTimer; // Vai de 1.0 a 0.0
            Color expColor = ColorAlpha(C_YELLOW, expAlpha * 0.4f);
            Color expWire = ColorAlpha(WHITE, expAlpha * 0.8f);
            
            DrawCube({startX + 0.5f, worldMidY, 0}, 1.0f, BOARD_HEIGHT, 0.5f, expColor);
            DrawCubeWires({startX + 0.5f, worldMidY, 0}, 1.0f, BOARD_HEIGHT, 0.5f, expWire);
            
            DrawCube({endX - 0.5f, worldMidY, 0}, 1.0f, BOARD_HEIGHT, 0.5f, expColor);
            DrawCubeWires({endX - 0.5f, worldMidY, 0}, 1.0f, BOARD_HEIGHT, 0.5f, expWire);
        }
    }

    void DrawAllPieces(bool isReflection) {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] != 0) {
                    DrawSciFiBlock3D(GetWorldPos(j, i), pieceColors[board[i][j]], isReflection, false, false, board[i][j]);
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
                            DrawSciFiBlock3D(ghostPos, pieceColors[currentColor], isReflection, true, false, currentColor);
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
                        DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection, false, currentIsBrilliant, currentColor);
                    }
                }
            }
        }
    }

    void DrawBoss() {
        if (bossEntryAnim < 0.01f) return;

        Vector3 bossPos = { 0.0f, Lerp(40.0f, 26.0f, bossEntryAnim), 2.5f }; 
        float pulse = 1.0f + musicPulse * 0.5f;
        float time = (float)GetTime();

        rlPushMatrix();
            rlTranslatef(bossPos.x, bossPos.y, bossPos.z);
            rlRotatef(time * 100.0f, 0, 1, 0); 
            rlRotatef((float)sin(time*3.0f) * 20.0f, 1, 0, 1);

            Vector3 blocks[4] = {
                {0, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}
            };

            for (int i = 0; i < 4; i++) {
                float glitchX = sin(time * 20.0f + i) * 0.5f * pulse;
                float glitchY = cos(time * 25.0f - i) * 0.5f * pulse;
                float glitchZ = sin(time * 30.0f + i * 2.0f) * 0.5f * pulse;
                
                Vector3 blockPos = {
                    blocks[i].x * 3.0f + glitchX,
                    blocks[i].y * 3.0f + glitchY,
                    blocks[i].z * 3.0f + glitchZ
                };

                Color glitchColor = (sin(time * 50.0f + i * 10.0f) > 0.8f) ? WHITE : C_RED;
                
                DrawCubeWires(blockPos, 2.8f * pulse, 2.8f * pulse, 2.8f * pulse, C_ORANGE);
                DrawCubeWires(blockPos, 2.5f * pulse, 2.5f * pulse, 2.5f * pulse, ColorAlpha(glitchColor, 0.8f));
            }

            DrawCubeWires({0,0,0}, 12.0f * pulse, 12.0f * pulse, 12.0f * pulse, ColorAlpha(C_RED, 0.4f));
            DrawCubeWires({0,0,0}, 14.0f * pulse, 2.0f * pulse, 14.0f * pulse, ColorAlpha(C_ORANGE, 0.2f));

        rlPopMatrix();
    }

public:
    JogoTetris3D() : currentState(AUTH), rng(random_device{}()) {
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 55.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        
        bgRenderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

        for(int i = 0; i < 150; i++) {
            Star3D s;
            s.pos = { GetRandomFloat(-100.0f, 100.0f), GetRandomFloat(-80.0f, 80.0f), GetRandomFloat(-150.0f, 150.0f) };
            s.speed = GetRandomFloat(20.0f, 80.0f);
            s.size = 1.0f;
            s.color = ColorAlpha(WHITE, GetRandomFloat(0.2f, 0.9f));
            starfield.push_back(s);
        }

        sndMove = LoadSound("move.mp3");
        sndRotate = LoadSound("rotate.mp3");
        sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3");
        sndClear2 = LoadSound("clear2.mp3");
        sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3");
        sndGameOver = LoadSound("gameover.mp3");
        
        currentMusicTrack = 1;
        LoadNextMusic();

        SpawnPiece();
        
        CheckLocalLicense(); 

        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        
        float res[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) {
            DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        }
        UnloadRenderTexture(bgRenderTarget);
        UnloadShader(postProcessShader); 
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
            
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                currentMusicTrack++;
                if (currentMusicTrack > 30) currentMusicTrack = 1; 
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

        if (IsKeyPressed(KEY_K) && musicEnabled && currentState != AUTH) {
            currentMusicTrack++;
            if (currentMusicTrack > 30) currentMusicTrack = 1;
            LoadNextMusic();
            mensagemEspecial = "TRACK " + to_string(currentMusicTrack);
            timerMensagem = 2.0f;
        }
        
        
        if (currentState == AUTH) {
            if (checkingOnline) {
                checkTimer -= dt;
                if (checkTimer <= 0.0f) {
                    checkingOnline = false;
                    if (ValidateKey(currentInputKey)) {
                        SaveLicense(currentInputKey);
                        currentState = MENU; 
                        TocarSom(sndClear4); 
                    } else {
                        authStatusMsg = "FALHA: CHAVE INVÁLIDA OU BLOQUEADA PELO SERVIDOR!";
                        currentInputKey = "";
                        charCount = 0;
                        TocarSom(sndGameOver);
                        cameraShakeTimer = 1.0f;
                        cameraShakeIntensity = 2.0f;
                    }
                }
            } else {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 125) && charCount < 16) {
                        char c = (char)toupper(key);
                        if (isalnum(c)) {
                            currentInputKey += c;
                            charCount++;
                            TocarSom(sndMove);
                        }
                    }
                    key = GetCharPressed(); 
                }

                if (IsKeyPressed(KEY_BACKSPACE) && charCount > 0) {
                    currentInputKey.pop_back();
                    charCount--;
                    TocarSom(sndMove);
                }

                if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && charCount == 16) {
                    checkingOnline = true;
                    checkTimer = 2.5f; 
                    authStatusMsg = "CONECTANDO AO SERVIDOR OMEGA RED...";
                    TocarSom(sndDrop);
                }
            }
        }
        else if (currentState == MENU) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { menuSelection--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { menuSelection++; TocarSom(sndMove); }
            
            if (menuSelection < 0) menuSelection = 4;
            if (menuSelection > 4) menuSelection = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                if (menuSelection == 0) { isClassicMode = true; Restart(); currentState = PLAYING; }
                else if (menuSelection == 1) { isClassicMode = false; Restart(); currentState = PLAYING; }
                else if (menuSelection == 2) currentState = SETTINGS;
                else if (menuSelection == 3) currentState = CREDITS;
                else if (menuSelection == 4) confirmExit = true;
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

            if (!isClassicMode && !bossActive && linesClearedTotal >= linesUntilBoss) {
                bossActive = true;
                bossEncounterCount++;
                
                bossHp = 4 + (bossEncounterCount * 2); 
                
                currentBossAttackDelay = fmax(3.0f, 18.0f - (bossEncounterCount * 3.0f));
                bossAttackTimer = currentBossAttackDelay; 
                
                linesUntilBoss += 25; 
                TocarSom(sndGameOver);
                cameraShakeTimer = 2.0f;
                cameraShakeIntensity = 3.0f;
            }

            if (bossActive && !isPaused) {
                bossEntryAnim = Lerp(bossEntryAnim, 1.0f, dt * 1.0f); 
                bossAttackTimer -= dt;
                
                float orbitSpeed = 0.01f + abs(sin(bossOrbitAngle)) * 1.2f; 
                bossOrbitAngle += dt * orbitSpeed;
                
                if (bossAttackTimer <= 0.0f) {
                    BossAddJunkLine();
                    bossAttackTimer = currentBossAttackDelay; 
                }

                themeCyan = LerpColor(themeCyan, C_RED, dt * 3.0f);
                themeBlue = LerpColor(themeBlue, C_ORANGE, dt * 3.0f);
                themeBg = LerpColor(themeBg, {30, 0, 0, 255}, dt * 3.0f);
                
                if (musicEnabled && sndMusic.stream.buffer != NULL) SetMusicPitch(sndMusic, Lerp(0.6f, 0.9f, musicPulse));
            } else {
                bossEntryAnim = Lerp(bossEntryAnim, 0.0f, dt * 1.0f); 
                
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

        if (gridExpansionTimer > 0) gridExpansionTimer -= dt;

        if (!isPaused) {
            float halfWidth = currentGridWidth / 2.0f; // Metade da largura da arena
            
            for(auto& s : starfield) {
                float prevZ = s.pos.z; 
                
                s.pos.z += s.speed * (5.0f + musicPulse * 25.0f) * dt; 

                bool inGridX = (s.pos.x >= -halfWidth - 0.7f && s.pos.x <= halfWidth + 0.7f);
                bool inGridY = (s.pos.y >= -0.5f && s.pos.y <= BOARD_HEIGHT + 0.5f);
                
                if (inGridX && inGridY && prevZ < -1.0f && s.pos.z >= -1.0f) {
                    s.pos.z = 151.0f; 

                if(s.pos.z > 150.0f) {
                    s.pos.z = GetRandomFloat(-180.0f, -150.0f);
                    s.pos.x = GetRandomFloat(-100.0f, 100.0f);
                    s.pos.y = GetRandomFloat(-80.0f, 80.0f);
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

       
        float baseDist = 30.0f + ((currentGridWidth - 10) * 0.8f);

        float baseOrbitAngle = (float)GetTime() * 0.3f + nukeSpinAngle; 
        Vector3 normalCamPos = {
            -1.5f + (float)sin(baseOrbitAngle) * 5.5f, 
            defaultCamPos.y + (float)sin(GetTime() * 0.5f) * 1.0f, 
            baseDist + cameraZoomOffset 
        };
        Vector3 normalTarget = defaultCamTarget;

        float bossDist = baseDist + 8.0f; 
        Vector3 bossCamPos = {
            (float)sin(bossOrbitAngle) * bossDist,
            defaultCamPos.y + 10.0f, 
            (float)cos(bossOrbitAngle) * bossDist
        };
        Vector3 bossTarget = { 0.0f, defaultCamTarget.y + 6.0f, 0.0f };

        Vector3 targetCamPos = Vector3Lerp(normalCamPos, bossCamPos, bossEntryAnim);
        Vector3 currentTarget = Vector3Lerp(normalTarget, bossTarget, bossEntryAnim);

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

        if (timerMensagem > 0 && !isPaused) {
            timerMensagem -= dt;
            if (timerMensagem <= 0.0f) {
                mensagemEspecial = ""; 
            }
        }

        float timeVal = (float)GetTime();
        SetShaderValue(postProcessShader, timeLoc, &timeVal, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, pulseLoc, &musicPulse, SHADER_UNIFORM_FLOAT);
    }

    void Draw() {
        BeginTextureMode(bgRenderTarget);
        ClearBackground(C_BG);
        
        BeginMode3D(camera);
            DrawSciFiBackground(); 
            
            rlPushMatrix();
                rlScalef(1.0f, -1.0f, 1.0f);
                DrawAllPieces(true);
            rlPopMatrix();

            DrawCylinder({0.0f, -0.1f, 0.0f}, 25.0f, 25.0f, 0.2f, 64, ColorAlpha(themeBg, 0.85f));
        EndMode3D();
        EndTextureMode();

        ClearBackground(BLACK);
        
        Rectangle source = { 0, 0, (float)bgRenderTarget.texture.width, -(float)bgRenderTarget.texture.height };
        Rectangle dest = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        
        BeginShaderMode(postProcessShader);
            DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
        EndShaderMode();

        BeginBlendMode(BLEND_ADDITIVE);
        Rectangle bloomDest = { -5, -5, SCREEN_WIDTH + 10.0f, SCREEN_HEIGHT + 10.0f }; 
        DrawTexturePro(bgRenderTarget.texture, source, bloomDest, {0,0}, 0.0f, ColorAlpha(WHITE, 0.4f));
        
        if (motionBlurIntensity > 0.01f) {
            for (int i = 1; i <= 3; i++) {
                float scale = 1.0f + (0.015f * i * motionBlurIntensity);
                float alpha = 0.15f * motionBlurIntensity;
                Rectangle mbDest = {
                    -(SCREEN_WIDTH * (scale - 1.0f)) / 2.0f,
                    -(SCREEN_HEIGHT * (scale - 1.0f)) / 2.0f,
                    SCREEN_WIDTH * scale,
                    SCREEN_HEIGHT * scale
                };
                DrawTexturePro(bgRenderTarget.texture, source, mbDest, {0,0}, 0.0f, ColorAlpha(WHITE, alpha));
            }
        }
        EndBlendMode();

        BeginMode3D(camera);
            DrawSciFiGrid(); 
            DrawBoss();
            DrawAllPieces(false);
            UpdateAndDrawParticles3D(GetFrameTime());
        EndMode3D();

        if (currentState == AUTH) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            
            DrawText("SISTEMA DE SEGURANÇA MÁXIMA", SCREEN_WIDTH/2 - MeasureText("SISTEMA DE SEGURANÇA MÁXIMA", 60)/2, 200, 60, themeCyan);
            DrawText("VERIFICAÇÃO DE LICENÇA ONLINE", SCREEN_WIDTH/2 - MeasureText("VERIFICAÇÃO DE LICENÇA ONLINE", 30)/2, 280, 30, C_RED);
            
            DrawText(authStatusMsg.c_str(), SCREEN_WIDTH/2 - MeasureText(authStatusMsg.c_str(), 24)/2, 400, 24, (checkingOnline || authStatusMsg[0] == 'F') ? C_ORANGE : WHITE);

            string displayKey = "";
            for(int i=0; i<16; i++) {
                if (i < charCount) displayKey += currentInputKey[i];
                else displayKey += "_";
                
                if (i == 3 || i == 7 || i == 11) displayKey += "-";
            }

            Color boxColor = checkingOnline ? C_ORANGE : themeCyan;
            DrawRectangle(SCREEN_WIDTH/2 - 300, 450, 600, 80, ColorAlpha(themeBg, 0.8f));
            DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 300, 450, 600, 80}, 3, boxColor);
            DrawText(displayKey.c_str(), SCREEN_WIDTH/2 - MeasureText(displayKey.c_str(), 50)/2, 465, 50, WHITE);

            if (!checkingOnline) {
                if (charCount < 16) DrawText("DIGITE SUA CHAVE DE 16 CARACTERES (SEM TRAÇOS)", SCREEN_WIDTH/2 - MeasureText("DIGITE SUA CHAVE DE 16 CARACTERES (SEM TRAÇOS)", 20)/2, 560, 20, GRAY);
                else DrawText("PRESSIONE [ENTER] PARA VALIDAR NO SERVIDOR", SCREEN_WIDTH/2 - MeasureText("PRESSIONE [ENTER] PARA VALIDAR NO SERVIDOR", 20)/2, 560, 20, C_GREEN);
            } else {
                DrawText("AUTENTICANDO NO SERVIDOR...", SCREEN_WIDTH/2 - MeasureText("AUTENTICANDO NO SERVIDOR...", 20)/2, 560, 20, C_ORANGE);
                DrawRectangle(SCREEN_WIDTH/2 - 200, 600, 400 * (1.0f - checkTimer/2.5f), 10, C_ORANGE);
                DrawRectangleLines(SCREEN_WIDTH/2 - 200, 600, 400, 10, WHITE);
            }
            
            DrawText("[ OMEGA RED ANTI-PIRACY v2.0 - MAXIMUM SECURITY ]", 40, SCREEN_HEIGHT - 40, 15, ColorAlpha(C_RED, 0.5f));
        }
        else if (currentState == MENU) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.6f));
            DrawText("TeTRABeTTA", SCREEN_WIDTH/2 - MeasureText("TeTRABeTTA", 80)/2, 200, 80, themeCyan);
            DrawText("3D EDITION", SCREEN_WIDTH/2 - MeasureText("3D EDITION", 30)/2, 290, 30, themeBlue);

            const char* menuItems[] = { "MODO CLÁSSICO", "MODO CHEFE", "CONFIGURAÇÕES", "CRÉDITOS", "SAIR" };
            for (int i = 0; i < 5; i++) {
                Color c = (i == menuSelection) ? C_ORANGE : WHITE;
                if (i == menuSelection) DrawText("> ", SCREEN_WIDTH/2 - MeasureText(menuItems[i], 40)/2 - 40, 420 + i * 70, 40, C_ORANGE);
                DrawText(menuItems[i], SCREEN_WIDTH/2 - MeasureText(menuItems[i], 40)/2, 420 + i * 70, 40, c);
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
                                    DrawSciFiBlock3D(npPos, pieceColors[nextColor], false, false, nextIsBrilliant, nextColor);
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
        currentGridWidth = 10; 
        nextIsBrilliant = false;
        currentIsBrilliant = false;
        linesClearedTotal = 0;
        gameOver = false;
        isPaused = false;
        timerMensagem = 0;
        mensagemEspecial = ""; 
        gridExpansionTimer = 0.0f; 
        currentGridElevation = 0.15f; 
        moveLeftTimer = 0.0f;
        moveRightTimer = 0.0f;
        nukeSpinAngle = 0.0f;
        
        bossActive = false;
        bossEntryAnim = 0.0f;
        linesUntilBoss = 15;
        bossEncounterCount = 0;
        currentBossAttackDelay = 16.0f; 
        bossOrbitAngle = 0.0f; 
        
        particles.clear();
        SpawnPiece();
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        cameraZoomOffset = 0.0f;
        lastClearedY = 0.0f; 
        
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