#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <random>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <deque>

using namespace std;

// =====================================================================
// SHADERS DE PÓS-PROCESSAMENTO AAA (POST-PROCESSING STACK)
// =====================================================================
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
uniform float hitStop; // Efeito de deformação no impacto

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    vec3 pos = vertexPosition;
    
    // Totalmente desvinculado do ritmo da música.
    // Agora apenas o HitStop (quando você faz ponto ou solta a peça) causa uma leve trepidação.
    pos.x += (hitStop * sin(time * 50.0) * 0.02);
    
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
uniform float time;
uniform float pulse;
uniform float damageVignette;

// Função pseudo-randômica para Film Grain
float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;
    
    // 1. Curvatura de Lente / Fisheye (CRT Effect)
    vec2 crtUV = uv - 0.5;
    float rsq = crtUV.x*crtUV.x + crtUV.y*crtUV.y;
    crtUV += crtUV * (rsq * 0.15); // Força da distorção
    crtUV += 0.5;
    
    // Se saiu da tela por causa da curvatura, escurece
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 2. Aberração Cromática (Fixa, sem seguir a música para melhorar a nitidez)
    float shiftAmount = 0.001 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    float r = texture(texture0, crtUV + shift).r;
    float g = texture(texture0, crtUV).g;
    float b = texture(texture0, crtUV - shift).b;
    vec4 baseColor = vec4(r, g, b, 1.0);

    // 3. Bloom (Glow fixo e reduzido para as peças ficarem totalmente legíveis)
    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.5; // Spread fixo e baixo para não borrar as peças
    
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.3) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.3) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.3) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.3) * 0.25;
    
    // 4. Scanlines
    float scanline = sin(crtUV.y * resolution.y * 1.5) * 0.04;
    
    // 5. Film Grain dinâmico
    float grain = (rand(crtUV * time) - 0.5) * 0.06;
    
    // 6. Vignette Dinâmico (Escurece as bordas, pisca vermelho se tomar dano)
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.8, 0.4, dist);
    
    // Intensidade de brilho fixa (removido o pulse)
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.2) * (1.0 - scanline) + grain;
    finalRGB *= vignette;
    
    // Efeito de Dano no Boss
    if (damageVignette > 0.0) {
        finalRGB += vec3(0.8, 0.0, 0.0) * damageVignette * smoothstep(0.3, 0.8, dist);
    }

    finalColor = vec4(finalRGB, 1.0) * fragColor;
}
)";

// =====================================================================
// CONSTANTES E CONFIGURAÇÕES DO MUNDO 3D
// =====================================================================
const int MAX_BOARD_WIDTH = 22; 
const int BOARD_HEIGHT = 20;    
const int SCREEN_WIDTH = 1920;  
const int SCREEN_HEIGHT = 1080; 
const float CUBE_SIZE = 0.95f;

// Paleta Cyberpunk Estendida
const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 0, 100, 255, 255 }; 
const Color C_ORANGE = { 255, 120, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 150, 255 };
const Color C_PURPLE = { 180, 0, 255, 255 };
const Color C_RED    = { 255, 10, 50, 255 };
const Color C_BG     = { 5, 8, 15, 255 }; 
const Color C_HOLO   = { 100, 255, 255, 100 };

Color pieceColors[9] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, DARKGRAY };

// =====================================================================
// SISTEMAS AAA: Easing, Springs e Áudio
// =====================================================================
enum GameState { AUTH, MENU, SETTINGS, CREDITS, PLAYING };
float globalMusicAmplitude = 0.0f;

void AudioInputCallback(void *bufferData, unsigned int frames) {
    float *samples = (float *)bufferData;
    float sum = 0.0f;
    for (unsigned int i = 0; i < frames; i++) sum += fabs(samples[i]);
    globalMusicAmplitude = sum / (float)frames;
}

Color LerpColor(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

// Interpolação elástica
float ElasticEaseOut(float t) {
    return sin(-13.0f * (t + 1.0f) * PI / 2.0f) * pow(2.0f, -10.0f * t) + 1.0f;
}

// =====================================================================
// SISTEMA DE PARTÍCULAS
// =====================================================================
struct Star3D { Vector3 pos; float speed; float size; Color color; };
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
    deque<Vector3> trail;
};
vector<Particle3D> particles;

struct FloatingText {
    Vector3 pos;
    string text;
    float life;
    Color color;
    float scale;
};
vector<FloatingText> floatingTexts;

float GetRandomFloat(float min, float max) { return min + (max - min) * ((float)rand() / RAND_MAX); }

void SpawnFloatingText(Vector3 pos, string text, Color color, float scale = 1.0f) {
    floatingTexts.push_back({pos, text, 1.5f, color, scale});
}

void SpawnParticles3D(Vector3 pos, Color color, int amount, float force, int shapeType = 0) {
    for (int i = 0; i < amount; i++) {
        Particle3D p;
        p.position = pos;
        p.velocity = { GetRandomFloat(-force, force), GetRandomFloat(force * 0.2f, force * 1.5f), GetRandomFloat(-force, force) };
        p.color = color;
        p.maxLife = GetRandomFloat(0.5f, 1.5f);
        p.life = p.maxLife;
        p.isSpark = (GetRandomFloat(0, 1) > 0.4f); 
        p.size = p.isSpark ? GetRandomFloat(0.1f, 0.4f) : GetRandomFloat(0.3f, 0.6f);
        p.shapeType = shapeType;
        p.rotation = { GetRandomFloat(0, 360), GetRandomFloat(0, 360), GetRandomFloat(0, 360) };
        p.rotVelocity = { GetRandomFloat(-400, 400), GetRandomFloat(-400, 400), GetRandomFloat(-400, 400) };
        particles.push_back(p);
    }
}

void UpdateAndDrawParticles3D(float dt) {
    for (int i = particles.size() - 1; i >= 0; i--) {
        particles[i].trail.push_front(particles[i].position);
        if (particles[i].trail.size() > 5) particles[i].trail.pop_back();

        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].position.z += particles[i].velocity.z * dt;
        particles[i].velocity.y -= 25.0f * dt; 
        particles[i].velocity.x *= 0.95f; 
        particles[i].velocity.z *= 0.95f;
        particles[i].rotation.x += particles[i].rotVelocity.x * dt;
        particles[i].rotation.y += particles[i].rotVelocity.y * dt;
        particles[i].rotation.z += particles[i].rotVelocity.z * dt;
        particles[i].life -= dt;

        if (particles[i].life <= 0 || particles[i].position.y < -15.0f) {
            particles.erase(particles.begin() + i);
        } else {
            float alpha = particles[i].life / particles[i].maxLife;
            Color fadeColor = particles[i].color;
            fadeColor.a = (unsigned char)(255 * alpha);
            
            if (particles[i].isSpark) {
                rlBegin(RL_LINES);
                for(size_t j = 0; j < particles[i].trail.size() - 1; j++) {
                    float trailAlpha = fadeColor.a * (1.0f - ((float)j / particles[i].trail.size()));
                    rlColor4ub(fadeColor.r, fadeColor.g, fadeColor.b, (unsigned char)trailAlpha);
                    rlVertex3f(particles[i].trail[j].x, particles[i].trail[j].y, particles[i].trail[j].z);
                    rlVertex3f(particles[i].trail[j+1].x, particles[i].trail[j+1].y, particles[i].trail[j+1].z);
                }
                rlEnd();
            } else {
                rlPushMatrix();
                rlTranslatef(particles[i].position.x, particles[i].position.y, particles[i].position.z);
                rlRotatef(particles[i].rotation.x, 1, 0, 0);
                rlRotatef(particles[i].rotation.y, 0, 1, 0);
                rlRotatef(particles[i].rotation.z, 0, 0, 1);
                
                float s = particles[i].size * ElasticEaseOut(1.0f - alpha);
                DrawCubeWiresV({0,0,0}, {s, s, s}, fadeColor);
                DrawCubeV({0,0,0}, {s*0.8f, s*0.8f, s*0.8f}, ColorAlpha(WHITE, alpha * 0.5f)); 
                rlPopMatrix();
            }
        }
    }
}

// =====================================================================
// LÓGICA DAS PEÇAS E MATRIZ
// =====================================================================
struct Tetromino { vector<vector<int>> shape; int colorID; };

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
        for (int j = 0; j < n; ++j) { res[j][n - 1 - i] = mat[i][j]; }
    }
    return res;
}

// =====================================================================
// ENGINE PRINCIPAL DO JOGO AAA
// =====================================================================
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
    float renderX; 
    float currentRotAngle = 0.0f; 
    bool currentIsBrilliant = false; 
    
    vector<vector<int>> nextPiece;
    int nextColor;
    bool nextIsBrilliant = false; 

    float fallTimer = 0.0f;
    float gridExpansionTimer = 0.0f; 
    float currentGridElevation = -4.0f; // Grid subiu um pouco (era -8.0f)
    string mensagemEspecial = "";
    float timerMensagem = 0.0f;

    float hitStopTimer = 0.0f;
    float damageVignette = 0.0f;
    float musicPulse = 0.0f;
    
    Color themeCyan = C_CYAN;
    Color themeBlue = C_BLUE;
    Color themeBg = C_BG;

    Camera3D camera = { 0 };
    Vector3 defaultCamPos = { 0.0f, 2.0f, 40.0f }; // Câmera um pouco mais alta e distanciada (era 30.0f)
    Vector3 defaultCamTarget = { 0.0f, 12.0f, 0.0f }; // Olhando ligeiramente mais pra cima
    Vector3 currentCamPosTarget = defaultCamPos; 
    Vector3 currentCamTargetTarget = defaultCamTarget;
    float cameraFovTarget = 45.0f; // FOV Menor aumenta os elementos (zoom in)
    float cameraBankAngle = 0.0f; 
    
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;
    float nukeSpinAngle = 0.0f; 
    float bossOrbitAngle = 0.0f; 
    Vector3 currentBossPos = {0,0,0}; // Guardar posição do boss para rajadas
    
    float moveLeftTimer = 0.0f;
    float moveRightTimer = 0.0f;
    const float DAS_DELAY = 0.12f; 
    const float ARR_RATE = 0.02f;  

    // Boss System
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
    RenderTexture2D nextPieceRT; // Textura estrita para consertar a renderização da próxima peça

    Sound sndMove, sndRotate, sndDrop, sndClear1, sndClear2, sndClear3, sndClear4, sndGameOver;
    Music sndMusic = { 0 };
    int currentMusicTrack = 1; 

    mt19937 rng;
    Shader postProcessShader;
    int resLoc, timeLoc, pulseLoc, hitStopLoc, dmgVignetteLoc;

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
                for (int i = 0; data[i] != '\0'; i++) if (isalnum(data[i])) key += toupper(data[i]);
                UnloadFileText(data);
                if (ValidateKey(key)) currentState = MENU; 
            }
        }
    }
    void SaveLicense(string key) { SaveFileText("omegared_sys.key", (char*)key.c_str()); }
    int GetRandomPiece() { uniform_int_distribution<int> dist(0, 6); return dist(rng); }

    Vector3 GetWorldPos(int logicalX, int logicalY) {
        return {
            (float)logicalX - (currentGridWidth / 2.0f) + 0.5f,
            (float)(BOARD_HEIGHT - logicalY) - 0.5f + currentGridElevation,
            0.0f
        };
    }

    void TocarSom(Sound snd) { if (sfxEnabled) PlaySound(snd); }

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
            if (currentMusicTrack > 1) { currentMusicTrack = 1; LoadNextMusic(); }
            else musicEnabled = false; 
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
        currentRotAngle = 0.0f;

        currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
        currentY = -4; 
        renderFallY = -4.0f; 
        renderX = currentX;

        int p2 = GetRandomPiece();
        nextPiece = pieces[p2].shape;
        nextColor = pieces[p2].colorID;

        if (!IsValidMove(currentPiece, currentX, currentY)) {
            if (continues > 0) {
                continues--; 
                for(int i=0; i<BOARD_HEIGHT; i++) {
                    for(int j=0; j<currentGridWidth; j++) {
                        if(board[i][j] != 0) {
                            SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 10.0f);
                            board[i][j] = 0;
                        }
                    }
                }
                TocarSom(sndGameOver); 
                cameraShakeTimer = 0.5f; cameraShakeIntensity = 2.5f; damageVignette = 1.0f;
                currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
                currentY = -4; renderFallY = -4.0f; renderX = currentX;
            } else {
                gameOver = true; TocarSom(sndGameOver);
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
        cameraShakeTimer = 0.2f; cameraShakeIntensity = 1.0f; 

        bool blockPlacedOut = false;

        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0) {
                    if ((currentY + i) < 0) {
                        blockPlacedOut = true; // Se a peça travar fora do topo, jogador perde!
                    } else if ((currentY + i) >= 0 && (currentY + i) < BOARD_HEIGHT) {
                        board[currentY + i][currentX + j] = currentColor;
                        SpawnParticles3D(GetWorldPos(currentX + j, currentY + i), pieceColors[currentColor], 8, 5.0f);
                    }
                }
            }
        }

        if (blockPlacedOut) {
            if (continues > 0) {
                continues--; 
                for(int i=0; i<BOARD_HEIGHT; i++) {
                    for(int j=0; j<currentGridWidth; j++) {
                        if(board[i][j] != 0) {
                            SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 10.0f);
                            board[i][j] = 0;
                        }
                    }
                }
                TocarSom(sndGameOver); 
                cameraShakeTimer = 0.5f; cameraShakeIntensity = 2.5f; damageVignette = 1.0f;
                SpawnPiece();
            } else {
                gameOver = true; TocarSom(sndGameOver);
            }
            return; // Impede que o jogo continue verificando linhas se o jogador perdeu
        }
        
        if (currentIsBrilliant) {
            if (currentGridWidth < MAX_BOARD_WIDTH) {
                int expansion = 2; int shift = expansion / 2;
                for (int i = 0; i < BOARD_HEIGHT; i++) {
                    for (int j = currentGridWidth - 1; j >= 0; j--) board[i][j + shift] = board[i][j];
                    for (int j = 0; j < shift; j++) board[i][j] = 0; 
                }
                currentGridWidth += expansion;
                gridExpansionTimer = 1.5f; 
                mensagemEspecial = "SYSTEM EXPANDED"; timerMensagem = 3.0f;
                cameraShakeTimer = 1.0f; cameraShakeIntensity = 3.5f;
                hitStopTimer = 0.2f; 
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_YELLOW, 200, 30.0f, 4);
                TocarSom(sndClear4); 
            } else {
                mensagemEspecial = "MAX POWER OVERDRIVE!"; score += 2000 * level; timerMensagem = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_CYAN, 150, 25.0f, 3);
                TocarSom(sndClear3);
            }
        }

        ClearLines();
        SpawnPiece();
    }

    void BossAddJunkLine() {
        for (int i = 0; i < BOARD_HEIGHT - 1; i++) {
            for (int j = 0; j < currentGridWidth; j++) board[i][j] = board[i+1][j];
        }
        int hole = GetRandomValue(0, currentGridWidth - 1);
        for (int j = 0; j < currentGridWidth; j++) {
            if (j == hole) board[BOARD_HEIGHT - 1][j] = 0;
            else board[BOARD_HEIGHT - 1][j] = 8; 
        }
        cameraShakeTimer = 0.6f; cameraShakeIntensity = 3.0f; damageVignette = 1.0f;
        TocarSom(sndDrop);
    }

    void ClearLines() {
        int linesClearedNow = 0;
        Vector3 avgClearPos = {0,0,0};

        for (int i = BOARD_HEIGHT - 1; i >= 0; --i) {
            bool isFull = true;
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] == 0) { isFull = false; break; }
            }

            if (isFull) {
                linesClearedNow++;
                avgClearPos.y += GetWorldPos(0, i).y; 
                
                for(int j = 0; j < currentGridWidth; j++) {
                    Vector3 blockPos = GetWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 25, 20.0f, board[i][j]);
                }

                for (int k = i; k > 0; --k) {
                    for (int j = 0; j < currentGridWidth; ++j) board[k][j] = board[k - 1][j];
                }
                for (int j = 0; j < currentGridWidth; ++j) board[0][j] = 0;
                i++; 
            }
        }

        if (linesClearedNow > 0) {
            avgClearPos.y /= linesClearedNow;
            linesClearedTotal += linesClearedNow;
            stars += linesClearedNow;
            if (stars >= 10) {
                stars -= 10; nextIsBrilliant = true; 
                mensagemEspecial = "MAGIC PIECE READY"; timerMensagem = 3.0f;
                TocarSom(sndClear2);
            }

            cameraShakeTimer = 0.5f + (linesClearedNow * 0.15f);
            cameraShakeIntensity = linesClearedNow * 2.5f; 
            cameraFovTarget = 45.0f - (linesClearedNow * 3.0f); 
            hitStopTimer = linesClearedNow * 0.05f; 

            int ptsGained = 0;
            if (linesClearedNow == 1) { ptsGained = 100 * level; TocarSom(sndClear1); } 
            else if (linesClearedNow == 2) { ptsGained = 300 * level; mensagemEspecial = "GOOD"; timerMensagem = 2.0f; TocarSom(sndClear2); } 
            else if (linesClearedNow == 3) { ptsGained = 500 * level; mensagemEspecial = "IMPRESSIVE"; timerMensagem = 2.0f; TocarSom(sndClear3); } 
            else if (linesClearedNow >= 4) { ptsGained = 800 * level; mensagemEspecial = "MARVELOUS"; timerMensagem = 3.0f; cameraShakeIntensity = 6.0f; TocarSom(sndClear4); }
            
            score += ptsGained;
            SpawnFloatingText(avgClearPos, "+" + to_string(ptsGained), C_CYAN, 1.5f + (linesClearedNow * 0.5f));

            if (bossActive) {
                bossHp -= linesClearedNow; 
                SpawnFloatingText({0, avgClearPos.y + 4.0f, 0}, "BOSS DMG -" + to_string(linesClearedNow), C_RED, 2.5f);
                if (bossHp <= 0) {
                    bossActive = false; score += 5000 * level;
                    SpawnParticles3D({0, 20.0f, -5.0f}, C_RED, 400, 60.0f, 8); 
                    cameraShakeTimer = 2.0f; cameraShakeIntensity = 8.0f;
                    hitStopTimer = 0.5f; 
                    mensagemEspecial = "VIRUS DELETED!"; timerMensagem = 4.0f;
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
                    SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 25.0f, board[i][j]);
                    board[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            score += blocksDestroyed * 50 * level;
            mensagemEspecial = "SYSTEM PURGE!!!"; timerMensagem = 3.0f;
            cameraShakeTimer = 1.5f; cameraShakeIntensity = 6.0f;
            cameraFovTarget = 35.0f; 
            nukeSpinAngle = PI * 4.0f; 
            hitStopTimer = 0.3f;
            TocarSom(sndClear4); 

            if (bossActive) {
                bossHp -= 10;
                if (bossHp <= 0) { bossActive = false; score += 10000; SpawnParticles3D({0, 20.0f, -5.0f}, C_RED, 500, 70.0f); }
            }
        }
    }

    // Desenho Geométrico Estático sem seguir a música
    void DrawSciFiBlock3D(Vector3 pos, Color baseCol, bool isReflection, bool isGhost = false, bool isBrilliant = false) {
        float s = CUBE_SIZE * 0.98f; 
        
        Color coreCol = isReflection ? ColorAlpha(baseCol, 0.2f) : baseCol;
        Color shellCol = isReflection ? ColorAlpha(WHITE, 0.02f) : ColorAlpha(WHITE, 0.15f);

        if (isBrilliant && !isGhost) {
            float t = (float)GetTime() * 10.0f;
            coreCol = ColorFromHSV(fmod(t * 60.0f, 360.0f), 1.0f, 1.0f);
            shellCol = WHITE;
        }

        // Fantasma (Branco estático, sem animações para evitar bugs de profundidade)
        if (isGhost) {
            DrawCubeWires(pos, s, s, s, ColorAlpha(WHITE, 0.6f)); // Arestas brancas mais fortes
            DrawCube(pos, s * 0.85f, s * 0.85f, s * 0.85f, ColorAlpha(WHITE, 0.15f)); // Miolo branco leve e fixo
        } else {
            // Peças Normais
            DrawCubeWires(pos, s, s, s, shellCol);
            DrawCube(pos, s * 0.8f, s * 0.8f, s * 0.8f, coreCol);
        }
    }

    void DrawProceduralEnvironment() {
        // Estrelas em Velocidade da Luz (Profundidade Warp)
        rlBegin(RL_LINES);
        for(const auto& s : starfield) {
            Color tailCol = ColorAlpha(s.color, 0.0f);
            // Desenha um longo rastro de luz para a estrela vindo pra frente
            Vector3 tail = { s.pos.x, s.pos.y, s.pos.z - (s.speed * 0.6f) }; 
            
            rlColor4ub(tailCol.r, tailCol.g, tailCol.b, tailCol.a);
            rlVertex3f(tail.x, tail.y, tail.z);
            
            rlColor4ub(s.color.r, s.color.g, s.color.b, s.color.a);
            rlVertex3f(s.pos.x, s.pos.y, s.pos.z);
        }
        rlEnd();

        // Painel Traseiro escurecido para contraste (Grid Removido do Fundo)
        float panelWidth = currentGridWidth - 0.15f;
        float panelHeight = BOARD_HEIGHT + 3.0f;
        Color backPanelColor = bossActive ? Color{40, 0, 0, 200} : Color{5, 10, 20, 200};
        DrawCube({0.0f, currentGridElevation + panelHeight/2.0f, -0.6f}, panelWidth, panelHeight, 0.1f, backPanelColor);
    }

    void DrawPlayfieldAndPieces(bool isReflection) {
        // Grid Frame Neon Estático
        float trueBottomY = currentGridElevation;
        float trueTopY = currentGridElevation + BOARD_HEIGHT + 3.0f;
        float trueMidY = trueBottomY + (BOARD_HEIGHT + 3.0f) / 2.0f;
        float halfW = currentGridWidth / 2.0f;
        
        float sideWidth = 0.1f;
        float baseHeight = 0.2f;
        
        Color frameC = bossActive ? C_RED : C_CYAN;
        frameC.a = 153; 

        // Bordas Principais
        DrawCubeWires({-halfW, trueMidY, 0}, sideWidth, BOARD_HEIGHT+3, sideWidth, frameC);
        DrawCubeWires({ halfW, trueMidY, 0}, sideWidth, BOARD_HEIGHT+3, sideWidth, frameC);
        DrawCube({0, trueBottomY - 0.1f, 0}, currentGridWidth + 0.4f, baseHeight, 0.4f, frameC); 

        // ======== GRID INTERNO DO JOGO ========
        rlBegin(RL_LINES);
        Color gridCellCol = ColorAlpha(WHITE, 0.1f); // Grade interna suave branca/cinza
        
        // Linhas Verticais
        for (int x = 0; x <= currentGridWidth; x++) {
            float lineX = -halfW + x;
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(lineX, trueBottomY, 0);
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(lineX, trueTopY, 0);
        }
        // Linhas Horizontais
        for (int y = 0; y <= BOARD_HEIGHT + 3; y++) {
            float lineY = trueBottomY + y;
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(-halfW, lineY, 0);
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(halfW, lineY, 0);
        }
        rlEnd();
        // ======================================

        // Desenha as peças no tabuleiro
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] != 0) {
                    DrawSciFiBlock3D(GetWorldPos(j, i), pieceColors[board[i][j]], isReflection);
                }
            }
        }

        if (currentState == PLAYING) {
            // Ghost Piece (Fantasma)
            int ghostY = currentY;
            while (IsValidMove(currentPiece, currentX, ghostY + 1)) ghostY++;

            if (ghostY > currentY) {
                for (int i = 0; i < currentPiece.size(); ++i) {
                    for (int j = 0; j < currentPiece[i].size(); ++j) {
                        if (currentPiece[i][j] != 0) {
                            Vector3 ghostPos = {
                                renderX + j - halfW + 0.5f,
                                (float)BOARD_HEIGHT - (ghostY + i) - 0.5f + currentGridElevation, 0.0f
                            };
                            DrawSciFiBlock3D(ghostPos, BLANK, isReflection, true); // Forçado como fantasma transparente branco
                        }
                    }
                }
            }

            // Peça Atual Caindo (com rotação interpolada para suavidade)
            rlPushMatrix();
            float cx = renderX + currentPiece[0].size()/2.0f - halfW;
            float cy = (float)BOARD_HEIGHT - renderFallY - currentPiece.size()/2.0f + currentGridElevation;
            
            rlTranslatef(cx, cy, 0.0f);
            rlRotatef(currentRotAngle, 0, 0, 1);
            rlTranslatef(-cx, -cy, 0.0f);

            for (int i = 0; i < currentPiece.size(); ++i) {
                for (int j = 0; j < currentPiece[i].size(); ++j) {
                    if (currentPiece[i][j] != 0) {
                        Vector3 dropPos = { renderX + j - halfW + 0.5f, (float)BOARD_HEIGHT - (renderFallY + i) - 0.5f + currentGridElevation, 0.0f };
                        DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection, false, currentIsBrilliant);
                    }
                }
            }
            rlPopMatrix();
        }
    }

    void DrawBossEncounter() {
        if (bossEntryAnim < 0.01f) return;
        
        float time = (float)GetTime();

        rlPushMatrix();
            // Boss Menor e Orbitando o Grid
            rlTranslatef(currentBossPos.x, currentBossPos.y, currentBossPos.z);
            rlRotatef(time * 150.0f, 0, 1, 0); 
            rlRotatef(sin(time*5.0f) * 30.0f, 1, 0, 1);

            // Reduzido raio da esfera do chefe
            DrawSphereWires({0,0,0}, 1.8f, 16, 16, ColorAlpha(C_RED, 0.8f));
            DrawCubeWires({0,0,0}, 4.0f, 4.0f, 4.0f, C_ORANGE);
            
            for (int i = 0; i < 4; i++) {
                float angle = time * 5.0f + (i * PI/2.0f);
                Vector3 sat = { cos(angle)*3.5f, sin(time*10.0f)*1.5f, sin(angle)*3.5f };
                DrawCube(sat, 0.8f, 0.8f, 0.8f, WHITE);
                DrawCubeWires(sat, 1.0f, 1.0f, 1.0f, C_RED);
            }
        rlPopMatrix();
    }

    void DrawFloatingTexts(float dt) {
        rlDisableDepthMask(); 
        for(int i = floatingTexts.size()-1; i>=0; i--) {
            floatingTexts[i].pos.y += dt * 3.0f; 
            floatingTexts[i].life -= dt;
            
            if(floatingTexts[i].life <= 0.0f) {
                floatingTexts.erase(floatingTexts.begin() + i);
            } else {
                Vector2 screenPos = GetWorldToScreen(floatingTexts[i].pos, camera);
                float alpha = Clamp(floatingTexts[i].life, 0.0f, 1.0f);
                Color c = floatingTexts[i].color; c.a = (unsigned char)(255 * alpha);
                
                int fontSize = 30 * floatingTexts[i].scale;
                DrawText(floatingTexts[i].text.c_str(), screenPos.x - MeasureText(floatingTexts[i].text.c_str(), fontSize)/2, screenPos.y, fontSize, c);
            }
        }
        rlEnableDepthMask();
    }

public:
    JogoTetris3D() : currentState(AUTH), rng(random_device{}()) {
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        
        bgRenderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
        nextPieceRT = LoadRenderTexture(260, 260); // Textura para consertar a Next Piece

        // Estrelas geradas bem longe para dar profundidade de "velocidade da luz"
        for(int i = 0; i < 400; i++) {
            starfield.push_back({
                { GetRandomFloat(-150.0f, 150.0f), GetRandomFloat(-80.0f, 150.0f), GetRandomFloat(-300.0f, 0.0f) },
                GetRandomFloat(80.0f, 250.0f), 1.0f, ColorAlpha(WHITE, GetRandomFloat(0.3f, 1.0f)) // Muito mais rápidas
            });
        }

        sndMove = LoadSound("move.mp3"); sndRotate = LoadSound("rotate.mp3"); sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3"); sndClear2 = LoadSound("clear2.mp3"); sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3"); sndGameOver = LoadSound("gameover.mp3");
        
        LoadNextMusic();
        SpawnPiece();
        CheckLocalLicense(); 

        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        hitStopLoc = GetShaderLocation(postProcessShader, "hitStop");
        dmgVignetteLoc = GetShaderLocation(postProcessShader, "damageVignette");
        
        float res[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        UnloadRenderTexture(bgRenderTarget); 
        UnloadRenderTexture(nextPieceRT); // Liberar a textura extra
        UnloadShader(postProcessShader);
        UnloadSound(sndMove); UnloadSound(sndRotate); UnloadSound(sndDrop); UnloadSound(sndClear1);
        UnloadSound(sndClear2); UnloadSound(sndClear3); UnloadSound(sndClear4); UnloadSound(sndGameOver);
        if (sndMusic.stream.buffer != NULL) UnloadMusicStream(sndMusic);
    }

    void Update(float dt) {
        if (hitStopTimer > 0.0f) {
            hitStopTimer -= GetFrameTime(); 
            dt *= 0.1f; 
        }

        if (musicEnabled && sndMusic.stream.buffer != NULL) {
            UpdateMusicStream(sndMusic);
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                currentMusicTrack++; if (currentMusicTrack > 30) currentMusicTrack = 1; 
                LoadNextMusic();
            }
            musicPulse = Lerp(musicPulse, globalMusicAmplitude * 15.0f, dt * 20.0f);
        } else musicPulse = Lerp(musicPulse, 0.0f, dt * 5.0f);

        if (IsKeyPressed(KEY_U) || (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))) {
            ToggleFullscreen(); isFullscreen = !isFullscreen;
        }

        if (damageVignette > 0.0f) damageVignette = Lerp(damageVignette, 0.0f, dt * 2.0f);
        if (currentRotAngle != 0.0f) currentRotAngle = Lerp(currentRotAngle, 0.0f, dt * 15.0f); 
        
        if (currentState == AUTH) {
            if (checkingOnline) {
                checkTimer -= dt;
                if (checkTimer <= 0.0f) {
                    checkingOnline = false;
                    if (ValidateKey(currentInputKey)) {
                        SaveLicense(currentInputKey); currentState = MENU; TocarSom(sndClear4); 
                    } else {
                        authStatusMsg = "ACESSO NEGADO - SEGURANÇA ATIVADA";
                        currentInputKey = ""; charCount = 0; TocarSom(sndGameOver);
                        cameraShakeTimer = 1.0f; cameraShakeIntensity = 4.0f; damageVignette = 1.0f;
                    }
                }
            } else {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 125) && charCount < 16) {
                        char c = (char)toupper(key);
                        if (isalnum(c)) { currentInputKey += c; charCount++; TocarSom(sndMove); }
                    }
                    key = GetCharPressed(); 
                }
                if (IsKeyPressed(KEY_BACKSPACE) && charCount > 0) { currentInputKey.pop_back(); charCount--; TocarSom(sndMove); }
                if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && charCount == 16) {
                    checkingOnline = true; checkTimer = 2.0f; 
                    authStatusMsg = "DECRIPTANDO..."; TocarSom(sndDrop);
                }
            }
        }
        else if (currentState == MENU || currentState == SETTINGS) {
            int* sel = (currentState == MENU) ? &menuSelection : &settingsSelection;
            int maxSel = (currentState == MENU) ? 4 : 3;

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { (*sel)--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { (*sel)++; TocarSom(sndMove); }
            if (*sel < 0) *sel = maxSel;
            if (*sel > maxSel) *sel = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                if (currentState == MENU) {
                    if (*sel == 0) { isClassicMode = true; Restart(); currentState = PLAYING; }
                    else if (*sel == 1) { isClassicMode = false; Restart(); currentState = PLAYING; }
                    else if (*sel == 2) currentState = SETTINGS;
                    else if (*sel == 3) currentState = CREDITS;
                    else if (*sel == 4) confirmExit = true;
                } else {
                    if (*sel == 0) { ToggleFullscreen(); isFullscreen = !isFullscreen; }
                    else if (*sel == 1) sfxEnabled = !sfxEnabled;
                    else if (*sel == 2) { musicEnabled = !musicEnabled; if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic); }
                    else if (*sel == 3) currentState = MENU;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) && currentState == SETTINGS) currentState = MENU;
        }
        else if (currentState == CREDITS) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) { TocarSom(sndDrop); currentState = MENU; }
        }
        else if (currentState == PLAYING) {
            if (IsKeyPressed(KEY_P)) { isPaused = !isPaused; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_ESCAPE)) showExitPrompt = !showExitPrompt; 

            if (showExitPrompt) {
                if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_Y)) { showExitPrompt = false; currentState = MENU; }
                if (IsKeyPressed(KEY_N)) showExitPrompt = false;
                return; 
            }

            if (gameOver) {
                if (IsKeyPressed(KEY_ENTER)) { Restart(); currentState = MENU; }
                return;
            }

            if (!isClassicMode && !bossActive && linesClearedTotal >= linesUntilBoss) {
                bossActive = true; bossEncounterCount++;
                bossHp = 10 + (bossEncounterCount * 5); 
                currentBossAttackDelay = fmax(3.0f, 15.0f - (bossEncounterCount * 2.0f));
                bossAttackTimer = currentBossAttackDelay; 
                linesUntilBoss += 20; 
                TocarSom(sndGameOver);
                cameraShakeTimer = 2.5f; cameraShakeIntensity = 5.0f;
                damageVignette = 1.0f; hitStopTimer = 0.5f;
            }

            if (bossActive && !isPaused) {
                bossEntryAnim = Lerp(bossEntryAnim, 1.0f, dt * 2.0f); 
                bossAttackTimer -= dt;
                bossOrbitAngle += dt * 1.5f; // Rotação do chefe em órbita

                // Boss cuspindo rajadas de fogo (efeito visual de partículas)
                if (GetRandomValue(0, 100) > 90) { 
                    Particle3D fire;
                    fire.position = currentBossPos;
                    // Mira em uma área aleatória no grid
                    Vector3 target = { GetRandomFloat(-10, 10), currentGridElevation + GetRandomFloat(0, 15), 0 };
                    
                    Vector3 dir;
                    dir.x = target.x - currentBossPos.x;
                    dir.y = target.y - currentBossPos.y;
                    dir.z = target.z - currentBossPos.z;
                    float len = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                    if(len > 0) { dir.x/=len; dir.y/=len; dir.z/=len; } // Normalize

                    float force = GetRandomFloat(30.0f, 60.0f);
                    fire.velocity = { dir.x * force, dir.y * force, dir.z * force };
                    fire.color = C_ORANGE;
                    fire.maxLife = GetRandomFloat(0.3f, 0.8f);
                    fire.life = fire.maxLife;
                    fire.size = GetRandomFloat(0.4f, 1.0f);
                    fire.isSpark = true;
                    particles.push_back(fire);
                }

                if (bossAttackTimer <= 0.0f) { BossAddJunkLine(); bossAttackTimer = currentBossAttackDelay; }
                if (musicEnabled && sndMusic.stream.buffer != NULL) SetMusicPitch(sndMusic, 0.8f + (musicPulse*0.1f));
            } else {
                bossEntryAnim = Lerp(bossEntryAnim, 0.0f, dt * 2.0f); 
                if (musicEnabled && sndMusic.stream.buffer != NULL) SetMusicPitch(sndMusic, 1.0f); 
            }

            if (!isPaused) {
                float speed = fmax(0.08f, 0.8f - (level * 0.03f)); 
                fallTimer += dt;

                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && bombs > 0) { bombs--; NukeBoard(); }

                cameraBankAngle = Lerp(cameraBankAngle, 0.0f, dt * 10.0f); 

                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                    if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveLeftTimer = 0.0f; cameraBankAngle = -2.0f;
                } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
                    moveLeftTimer += dt;
                    if (moveLeftTimer >= DAS_DELAY) {
                        moveLeftTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); cameraBankAngle = -5.0f;}
                    }
                } else { moveLeftTimer = 0.0f; }

                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                    if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveRightTimer = 0.0f; cameraBankAngle = 2.0f;
                } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
                    moveRightTimer += dt;
                    if (moveRightTimer >= DAS_DELAY) {
                        moveRightTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); cameraBankAngle = 5.0f;}
                    }
                } else { moveRightTimer = 0.0f; }

                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                    auto rotated = RotateMatrix(currentPiece);
                    if (IsValidMove(rotated, currentX, currentY)) { 
                        currentPiece = rotated; TocarSom(sndRotate); 
                        currentRotAngle = 90.0f; 
                        SpawnParticles3D(GetWorldPos(currentX, currentY), pieceColors[currentColor], 5, 5.0f);
                    }
                }
                
                if (IsKeyPressed(KEY_SPACE)) { 
                    int dropDist = 0;
                    while (IsValidMove(currentPiece, currentX, currentY + 1)) { currentY++; dropDist++; }
                    score += dropDist * 2;
                    LockPiece(); fallTimer = 0;
                    cameraShakeTimer = 0.3f; cameraShakeIntensity = dropDist * 0.5f; 
                    SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 15, dropDist * 2.0f);
                } else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) { 
                    fallTimer += dt * 20.0f; 
                }

                if (fallTimer >= speed) {
                    fallTimer = 0;
                    if (IsValidMove(currentPiece, currentX, currentY + 1)) currentY++;
                    else LockPiece();
                }

                renderFallY = Lerp(renderFallY, (float)currentY, dt * 30.0f);
                renderX = Lerp(renderX, (float)currentX, dt * 30.0f);
            } 
        }

        if (gridExpansionTimer > 0) gridExpansionTimer -= dt;

        if (!isPaused) {
            for(auto& s : starfield) {
                // Estrelas vindo com força total na direção do eixo Z para dar efeito Warp / Velocidade da luz
                s.pos.z += s.speed * dt * 1.5f; 
                if(s.pos.z > camera.position.z + 5.0f) { 
                    s.pos.z = GetRandomFloat(-350.0f, -200.0f); // Respawna muito mais longe
                    s.pos.x = GetRandomFloat(-150.0f, 150.0f);
                    s.pos.y = GetRandomFloat(-80.0f, 150.0f);
                }
            }
        }

        cameraFovTarget = Lerp(cameraFovTarget, 45.0f, dt * 2.0f); 
        camera.fovy = Lerp(camera.fovy, cameraFovTarget, dt * 10.0f);
        
        currentGridElevation = Lerp(currentGridElevation, (bossActive || bossEntryAnim > 0.01f) ? Lerp(-4.0f, 1.0f, bossEntryAnim) : -4.0f, dt * 3.0f);

        float baseOrbitAngle = (float)GetTime() * 0.2f + nukeSpinAngle; 
        
        // Posição base (visão imponente e massiva do board)
        Vector3 targetPosNormal = {
            -1.0f + (float)sin(baseOrbitAngle) * 2.0f, 
            defaultCamPos.y + currentGridElevation + 10.0f, 
            38.0f + ((currentGridWidth - 10) * 0.6f) // Z base distanciado (era 28.0f)
        };
        Vector3 targetTargetNormal = { 0.0f, currentGridElevation + 15.0f, 0.0f }; // Olhando quase pra cima da torre

        // Atualizando a posição lógica do Boss (órbita)
        float bossDist = 18.0f; 
        currentBossPos = { 
            (float)sin(bossOrbitAngle) * bossDist, 
            currentGridElevation + Lerp(35.0f, 15.0f, bossEntryAnim), 
            (float)cos(bossOrbitAngle) * bossDist 
        };

        // A câmera treme em formato de onda ao entrar no modo boss (sem girar loucamente)
        Vector3 targetPosBoss = { 
            targetPosNormal.x + (float)sin(bossOrbitAngle * 1.5f) * 6.0f, 
            targetPosNormal.y + (float)cos(bossOrbitAngle * 1.5f) * 3.0f, 
            targetPosNormal.z + 5.0f
        };
        Vector3 targetTargetBoss = { 
            (float)sin(bossOrbitAngle * 1.2f) * 3.0f, 
            currentGridElevation + 18.0f + (float)sin(bossOrbitAngle) * 2.0f, 
            0.0f 
        };

        Vector3 finalPosTarget = Vector3Lerp(targetPosNormal, targetPosBoss, bossEntryAnim);
        Vector3 finalLookTarget = Vector3Lerp(targetTargetNormal, targetTargetBoss, bossEntryAnim);

        if (cameraShakeTimer > 0 && !isPaused) {
            float rx = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            float ry = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            finalPosTarget.x += rx; finalPosTarget.y += ry;
            finalLookTarget.x += rx*0.5f; finalLookTarget.y += ry*0.5f;
            cameraShakeTimer -= dt;
        }

        float springTension = 15.0f;
        camera.position = Vector3Lerp(camera.position, finalPosTarget, dt * springTension);
        camera.target = Vector3Lerp(camera.target, finalLookTarget, dt * springTension);
        
        camera.up = { sin(cameraBankAngle * DEG2RAD), cos(cameraBankAngle * DEG2RAD), 0.0f };

        if (timerMensagem > 0 && !isPaused) { timerMensagem -= dt; if (timerMensagem <= 0.0f) mensagemEspecial = ""; }

        float timeVal = (float)GetTime();
        SetShaderValue(postProcessShader, timeLoc, &timeVal, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, pulseLoc, &musicPulse, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, hitStopLoc, &hitStopTimer, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, dmgVignetteLoc, &damageVignette, SHADER_UNIFORM_FLOAT);
    }

    void Draw() {
        // ========== 1. RENDERIZAR PRÓXIMA PEÇA COM PRECISÃO ============
        // Feito em sua própria textura isolada para não ter bug de perspectiva.
        BeginTextureMode(nextPieceRT);
            ClearBackground({0, 0, 0, 0}); // Totalmente transparente
            Camera3D queueCam = { { 0.0f, 0.0f, 6.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
            BeginMode3D(queueCam);
                rlPushMatrix();
                    rlRotatef(25.0f, 1, 0, 0); // Inclina pra cima para visualização isométrica
                    rlRotatef((float)GetTime() * 80.0f, 0, 1, 0); // Gira
                    float offX = nextPiece[0].size() / 2.0f;
                    float offY = nextPiece.size() / 2.0f;
                    for (int i = 0; i < nextPiece.size(); ++i) {
                        for (int j = 0; j < nextPiece[i].size(); ++j) {
                            if (nextPiece[i][j] != 0) {
                                // O grid 0 fica centralizado no espaço local
                                DrawSciFiBlock3D({ (float)j - offX + 0.5f, (float)-i + offY - 0.5f, 0.0f }, pieceColors[nextColor], false, false, nextIsBrilliant);
                            }
                        }
                    }
                rlPopMatrix();
            EndMode3D();
        EndTextureMode();
        // ===============================================================

        BeginTextureMode(bgRenderTarget);
        ClearBackground(C_BG);
        BeginMode3D(camera);
            DrawProceduralEnvironment(); 
            // SEM REFLEXO NAS PEÇAS MAIS, CHAMADA REMOVIDA.
            DrawPlayfieldAndPieces(false);
            DrawBossEncounter();
            UpdateAndDrawParticles3D(GetFrameTime());
            DrawFloatingTexts(GetFrameTime());
        EndMode3D();
        EndTextureMode();

        ClearBackground(BLACK);
        Rectangle source = { 0, 0, (float)bgRenderTarget.texture.width, -(float)bgRenderTarget.texture.height };
        Rectangle dest = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        
        BeginShaderMode(postProcessShader);
            DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
        EndShaderMode();

        DrawUI();
    }

    void DrawUI() {
        if (currentState == AUTH) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            DrawText("OMEGA RED SECURITY PROTOCOL", SCREEN_WIDTH/2 - MeasureText("OMEGA RED SECURITY PROTOCOL", 50)/2, 200, 50, C_RED);
            DrawText(authStatusMsg.c_str(), SCREEN_WIDTH/2 - MeasureText(authStatusMsg.c_str(), 24)/2, 350, 24, checkingOnline ? C_YELLOW : WHITE);

            string displayKey = "";
            for(int i=0; i<16; i++) {
                if (i < charCount) displayKey += currentInputKey[i]; else displayKey += "_";
                if (i == 3 || i == 7 || i == 11) displayKey += "-";
            }

            Color boxColor = checkingOnline ? C_ORANGE : C_CYAN;
            DrawRectangle(SCREEN_WIDTH/2 - 350, 450, 700, 80, ColorAlpha(C_BG, 0.9f));
            DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 350, 450, 700, 80}, 4, boxColor);
            DrawText(displayKey.c_str(), SCREEN_WIDTH/2 - MeasureText(displayKey.c_str(), 50)/2, 465, 50, WHITE);

            if (checkingOnline) {
                DrawRectangle(SCREEN_WIDTH/2 - 250, 600, 500 * (1.0f - checkTimer/2.0f), 15, C_ORANGE);
                DrawRectangleLines(SCREEN_WIDTH/2 - 250, 600, 500, 15, WHITE);
            }
        }
        else if (currentState == MENU) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.7f));
            float glitchX = sin(GetTime()*50.0f) * (musicPulse > 1.0f ? 5.0f : 0.0f);
            DrawText("TeTRABeTTA", SCREEN_WIDTH/2 - MeasureText("TeTRABeTTA", 100)/2 + glitchX, 180, 100, C_CYAN);
            DrawText("TeTRABeTTA", SCREEN_WIDTH/2 - MeasureText("TeTRABeTTA", 100)/2 - glitchX, 180, 100, ColorAlpha(C_RED, 0.5f));
            DrawText("AAA OVERDRIVE", SCREEN_WIDTH/2 - MeasureText("AAA OVERDRIVE", 40)/2, 280, 40, C_ORANGE);

            const char* menuItems[] = { "CLASSIC RUN", "BOSS RUSH", "SYSTEM CONFIG", "CREDITS", "LOGOUT" };
            for (int i = 0; i < 5; i++) {
                float pulseTxt = (i == menuSelection) ? 1.0f + sin(GetTime()*10.0f)*0.1f : 1.0f;
                int size = 40 * pulseTxt;
                Color c = (i == menuSelection) ? C_YELLOW : GRAY;
                if (i == menuSelection) DrawText("> ", SCREEN_WIDTH/2 - MeasureText(menuItems[i], size)/2 - 40, 450 + i * 80, size, C_YELLOW);
                DrawText(menuItems[i], SCREEN_WIDTH/2 - MeasureText(menuItems[i], size)/2, 450 + i * 80, size, c);
            }
        } 
        else if (currentState == SETTINGS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("SYSTEM CONFIG", SCREEN_WIDTH/2 - MeasureText("SYSTEM CONFIG", 60)/2, 150, 60, C_CYAN);

            string opt1 = string("FULLSCREEN: ") + (isFullscreen ? "ON" : "OFF");
            string opt2 = string("HAPTIC SFX: ") + (sfxEnabled ? "ON" : "OFF");
            string opt3 = string("SYNTHWAVE: ") + (musicEnabled ? "ON" : "OFF");
            string opt4 = "RETURN";

            const char* setItems[] = { opt1.c_str(), opt2.c_str(), opt3.c_str(), opt4.c_str() };
            for (int i = 0; i < 4; i++) {
                Color c = (i == settingsSelection) ? C_ORANGE : WHITE;
                DrawText(setItems[i], SCREEN_WIDTH/2 - MeasureText(setItems[i], 40)/2, 350 + i * 80, 40, c);
            }
        }
        else if (currentState == CREDITS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.9f));
            DrawText("EXECUTIVE PRODUCER", SCREEN_WIDTH/2 - MeasureText("EXECUTIVE PRODUCER", 30)/2, 350, 30, C_ORANGE);
            DrawText("Igor Bettarello - OMEGARED", SCREEN_WIDTH/2 - MeasureText("Igor Bettarello - OMEGARED", 50)/2, 400, 50, C_CYAN);
            DrawText("AAA LEAD ENGINEER", SCREEN_WIDTH/2 - MeasureText("AAA LEAD ENGINEER", 30)/2, 550, 30, C_ORANGE);
            DrawText("AI Assistant - Gemini God Mode", SCREEN_WIDTH/2 - MeasureText("AI Assistant - Gemini God Mode", 50)/2, 600, 50, C_CYAN);
        }
        else if (currentState == PLAYING) {
            DrawRectangle(40, 40, 400, 420, ColorAlpha(C_BG, 0.6f));
            DrawRectangleLinesEx({40, 40, 400, 420}, 2, ColorAlpha(C_CYAN, 0.5f));
            DrawText("SYS.VER: AAA.2026", 60, 60, 15, C_GREEN);
            DrawText(TextFormat("SCORE: %08d", score), 60, 100, 40, WHITE);
            DrawText(TextFormat("LEVEL: %02d", level), 60, 160, 30, C_ORANGE);
            DrawText(TextFormat("LIVES: %d", continues), 60, 210, 25, C_RED); 
            
            DrawText("MAGIC CHARGE:", 60, 260, 20, WHITE);
            DrawRectangleLines(60, 290, 300, 20, C_YELLOW);
            DrawRectangle(60, 290, 300 * (stars/10.0f), 20, ColorAlpha(C_YELLOW, 0.7f + sin(GetTime()*10)*0.3f));

            DrawText(TextFormat("[RMB] PURGE NUKE: %d", bombs), 60, 340, 20, C_RED); 
            DrawText("[K] SKIP TRACK", 60, 380, 20, GRAY); 

            if (bossActive) {
                DrawRectangle(SCREEN_WIDTH/2 - 350, 40, 700, 80, ColorAlpha(C_BG, 0.7f));
                DrawRectangleLinesEx({(float)SCREEN_WIDTH/2 - 350, 40, 700, 80}, 3, C_RED);
                DrawText(TextFormat("ANOMALY: OMEGARED V.%d", bossEncounterCount), SCREEN_WIDTH/2 - MeasureText("ANOMALY: OMEGARED V.X", 25)/2, 50, 25, C_RED);
                float hpRatio = (float)bossHp / (10.0f + (bossEncounterCount * 5.0f)); 
                DrawRectangle(SCREEN_WIDTH/2 - 330, 85, 660 * hpRatio, 20, C_ORANGE);
            }

            DrawRectangle(SCREEN_WIDTH - 300, 40, 260, 260, ColorAlpha(C_BG, 0.6f));
            DrawRectangleLinesEx({SCREEN_WIDTH - 300, 40, 260, 260}, 2, ColorAlpha(C_CYAN, 0.5f));
            DrawText("NEXT QUEUE", SCREEN_WIDTH - 250, 60, 20, WHITE);

            // DESENHA A PEÇA PERFEITAMENTE ENQUADRADA! (Invertendo altura para alinhar com coordenadas corretas)
            Rectangle sourceRT = { 0, 0, (float)nextPieceRT.texture.width, -(float)nextPieceRT.texture.height };
            Rectangle destRT = { SCREEN_WIDTH - 300.0f, 40.0f, 260.0f, 260.0f };
            if (!gameOver && !isPaused) {
                DrawTexturePro(nextPieceRT.texture, sourceRT, destRT, {0, 0}, 0.0f, WHITE);
            }

            if (timerMensagem > 0) {
                float popScale = ElasticEaseOut(1.0f - (timerMensagem / 3.0f));
                int fontSize = 60 * popScale; 
                int textWidth = MeasureText(mensagemEspecial.c_str(), fontSize);
                DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/3, fontSize, WHITE);
            }

            if (isPaused) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
                DrawText("SYSTEM PAUSED", SCREEN_WIDTH/2 - MeasureText("SYSTEM PAUSED", 60)/2, SCREEN_HEIGHT/2 - 30, 60, C_CYAN);
            }

            if (gameOver) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(C_RED, 0.4f)); 
                DrawRectangle(0, SCREEN_HEIGHT/2 - 100, SCREEN_WIDTH, 200, ColorAlpha(BLACK, 0.9f));
                DrawText("CRITICAL FAILURE", SCREEN_WIDTH/2 - MeasureText("CRITICAL FAILURE", 80)/2, SCREEN_HEIGHT/2 - 80, 80, C_RED);
                DrawText("PRESS [ENTER] TO REBOOT", SCREEN_WIDTH/2 - MeasureText("PRESS [ENTER] TO REBOOT", 30)/2, SCREEN_HEIGHT/2 + 30, 30, WHITE);
            }

            if (showExitPrompt) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.95f));
                DrawText("ABORT SIMULATION?", SCREEN_WIDTH/2 - MeasureText("ABORT SIMULATION?", 60)/2, SCREEN_HEIGHT/2 - 60, 60, C_ORANGE);
                DrawText("[Y] YES   -   [N] NO", SCREEN_WIDTH/2 - MeasureText("[Y] YES   -   [N] NO", 40)/2, SCREEN_HEIGHT/2 + 40, 40, WHITE);
            }
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0; level = 1; continues = 3; bombs = 2; stars = 0; currentGridWidth = 10; 
        nextIsBrilliant = false; currentIsBrilliant = false; linesClearedTotal = 0;
        gameOver = false; isPaused = false; timerMensagem = 0; mensagemEspecial = ""; 
        gridExpansionTimer = 0.0f; currentGridElevation = -4.0f; 
        moveLeftTimer = 0.0f; moveRightTimer = 0.0f; nukeSpinAngle = 0.0f;
        hitStopTimer = 0.0f; damageVignette = 0.0f;
        
        bossActive = false; bossEntryAnim = 0.0f; linesUntilBoss = 15; bossEncounterCount = 0;
        currentBossAttackDelay = 15.0f; bossOrbitAngle = 0.0f; 
        
        particles.clear(); floatingTexts.clear();
        SpawnPiece();
        camera.position = defaultCamPos; camera.target = defaultCamTarget;
        
        currentMusicTrack = 1; LoadNextMusic();
    }

    bool ShouldExit() { return confirmExit; }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TeTRABeTTA - AAA OVERDRIVE EDITION");
    SetExitKey(KEY_NULL); 
    ToggleFullscreen();
    InitAudioDevice(); 
    SetTargetFPS(60);

    JogoTetris3D game;

    while (!WindowShouldClose() && !game.ShouldExit()) {
        game.Update(GetFrameTime());
        BeginDrawing();
        game.Draw();
        EndDrawing();
    }

    CloseAudioDevice(); 
    CloseWindow();
    return 0;
}