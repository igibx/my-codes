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
// SHADERS DE PÓS-PROCESSAMENTO AAA (POST-PROCESSING STACK) + FXAA LITE
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
uniform float hitStop; 

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    vec3 pos = vertexPosition;
    
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
uniform float goldTint;

float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;
    
    vec2 crtUV = uv - 0.5;
    crtUV += 0.5;
    
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Aberração Cromática Sutil (Aumenta com o Dano)
    float shiftAmount = 0.0008 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    // ANTI-ALIASING LITE (FXAA de 5 Taps)
    vec2 off = 1.0 / resolution;
    vec3 colBlur = texture(texture0, crtUV + shift).rgb * 0.5;
    colBlur += texture(texture0, crtUV + shift + vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift - vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift + vec2(0.0, off.y)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift - vec2(0.0, off.y)).rgb * 0.125;
    vec4 baseColor = vec4(colBlur, 1.0);

    // Bloom Otimizado
    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.8; 
    
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    
    // Scanlines e Film Grain
    float scanline = sin(crtUV.y * resolution.y * 2.0) * 0.025;
    float grain = (rand(crtUV * time) - 0.5) * 0.02;
    
    // Vignette
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.95, 0.4, dist);
    
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.5) * (1.0 - scanline) + grain;
    
    // AUMENTO DE QUALIDADE GRÁFICA AAA
    finalRGB = mix(vec3(0.5), finalRGB, 1.15); // Contraste
    float luma = dot(finalRGB, vec3(0.299, 0.587, 0.114));
    finalRGB = mix(vec3(luma), finalRGB, 1.4); // Saturação
    
    // AURA GOLD EDITION
    if (goldTint > 0.0) {
        finalRGB += vec3(0.3, 0.2, 0.0) * goldTint * smoothstep(0.8, 0.2, dist);
    }
    
    finalRGB *= vignette;
    
    if (damageVignette > 0.0) {
        finalRGB += vec3(0.8, 0.0, 0.0) * damageVignette * smoothstep(0.3, 0.8, dist);
    }

    finalColor = vec4(finalRGB, 1.0) * fragColor;
}
)";

// =====================================================================
// CONSTANTES E CONFIGURAÇÕES DO MUNDO 3D
// =====================================================================
const int MAX_BOARD_WIDTH = 1000; 
const int BOARD_HEIGHT = 20;    
const int SCREEN_WIDTH = 1920;  
const int SCREEN_HEIGHT = 1080; 
const float CUBE_SIZE = 0.85f; 

// Variável global de qualidade gráfica para acesso global às partículas (0=LOW, 1=MED, 2=HIGH, 3=ULTRA)
int globalGraphicsQuality = 3; 

// Paleta Cyberpunk Estendida + GOLD
const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 0, 100, 255, 255 }; 
const Color C_ORANGE = { 255, 120, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 0, 255 }; 
const Color C_PURPLE = { 180, 0, 255, 255 };
const Color C_RED    = { 255, 10, 50, 255 };
const Color C_GOLD   = { 255, 215, 0, 255 }; // NOVO: GOLD EDITION
const Color C_BG     = { 8, 6, 12, 255 };    // Fundo levemente mais rico/quente

Color pieceColors[10] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, DARKGRAY, C_GOLD };

// =====================================================================
// SISTEMAS AAA: Easing, Springs e Áudio
// =====================================================================
enum GameState { INTRO, AUTH, MENU, SETTINGS, CREDITS, PLAYING }; 
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

float ElasticEaseOut(float t) {
    return sin(-13.0f * (t + 1.0f) * PI / 2.0f) * pow(2.0f, -10.0f * t) + 1.0f;
}

// =====================================================================
// SISTEMA DE PARTÍCULAS AVANÇADO (FUNÇÕES GLOBAIS)
// =====================================================================
struct Particle3D {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    bool isSpark; 
    bool isRing; // NOVO: Onda de choque
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
        p.isRing = false;
        p.size = p.isSpark ? GetRandomFloat(0.1f, 0.4f) : GetRandomFloat(0.3f, 0.6f);
        p.shapeType = shapeType;
        p.rotation = { GetRandomFloat(0, 360), GetRandomFloat(0, 360), GetRandomFloat(0, 360) };
        p.rotVelocity = { GetRandomFloat(-400, 400), GetRandomFloat(-400, 400), GetRandomFloat(-400, 400) };
        particles.push_back(p);
    }
}

// NOVA FUNÇÃO: ONDA DE CHOQUE (HARD DROP IMPACT)
void SpawnShockwave(Vector3 pos, Color color) {
    Particle3D p;
    p.position = pos;
    p.velocity = {0,0,0};
    p.color = color;
    p.maxLife = 0.6f;
    p.life = p.maxLife;
    p.isSpark = false;
    p.isRing = true;
    p.size = 0.1f; // Tamanho inicial
    p.rotation = {90, 0, 0}; // Deitado no chão
    p.rotVelocity = {0,0,0};
    particles.push_back(p);
}

void UpdateAndDrawParticles3D(float dt) {
    for (int i = particles.size() - 1; i >= 0; i--) {
        if (!particles[i].isRing) {
            particles[i].trail.push_front(particles[i].position);
            if (particles[i].trail.size() > 8) particles[i].trail.pop_back();

            particles[i].position.x += particles[i].velocity.x * dt;
            particles[i].position.y += particles[i].velocity.y * dt;
            particles[i].position.z += particles[i].velocity.z * dt;
            
            particles[i].velocity.y -= 25.0f * dt;
            
            particles[i].velocity.x *= 0.95f; 
            particles[i].velocity.z *= 0.95f;
            particles[i].rotation.x += particles[i].rotVelocity.x * dt;
            particles[i].rotation.y += particles[i].rotVelocity.y * dt;
            particles[i].rotation.z += particles[i].rotVelocity.z * dt;
        } else {
            // Expansão do Anel de Choque
            particles[i].size += dt * 30.0f;
        }

        particles[i].life -= dt;

        if (particles[i].life <= 0 || particles[i].position.y < -5.0f) {
            particles.erase(particles.begin() + i);
        } else {
            float alpha = particles[i].life / particles[i].maxLife;
            Color fadeColor = particles[i].color;
            fadeColor.a = (unsigned char)(255 * (alpha * alpha));
            
            if (particles[i].isRing) {
                rlPushMatrix();
                rlTranslatef(particles[i].position.x, particles[i].position.y, particles[i].position.z);
                rlRotatef(particles[i].rotation.x, 1, 0, 0);
                
                // Desenha um anel 3D rudimentar usando linhas circulares
                rlBegin(RL_LINES);
                rlColor4ub(fadeColor.r, fadeColor.g, fadeColor.b, fadeColor.a);
                int segments = 36;
                for(int s = 0; s < segments; s++) {
                    float a1 = (float)s / segments * PI * 2.0f;
                    float a2 = (float)(s+1) / segments * PI * 2.0f;
                    rlVertex3f(cos(a1)*particles[i].size, sin(a1)*particles[i].size, 0);
                    rlVertex3f(cos(a2)*particles[i].size, sin(a2)*particles[i].size, 0);
                    
                    // Anel duplo para mais volume
                    rlVertex3f(cos(a1)*(particles[i].size*0.9f), sin(a1)*(particles[i].size*0.9f), 0);
                    rlVertex3f(cos(a2)*(particles[i].size*0.9f), sin(a2)*(particles[i].size*0.9f), 0);
                }
                rlEnd();
                rlPopMatrix();
            } else if (particles[i].isSpark) {
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
                DrawCubeV({0,0,0}, {s*0.9f, s*0.9f, s*0.9f}, ColorAlpha(fadeColor, alpha * 0.6f));
                rlPopMatrix();
            }
        }
    }
}

// =====================================================================
// SISTEMA DE PEÇAS E FUNDO WIREFRAME ANIMADO!
// =====================================================================
struct Tetromino { vector<vector<int>> shape; int colorID; };

vector<Tetromino> pieces = {
    { {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, 1 }, // Barra Longa (Cyan)
    { {{0,0,0}, {1,1,1}, {0,0,0}}, 2 },                // Barra Curta (Blue)
    { {{0,1,0}, {1,1,1}, {0,0,0}}, 3 },                // T (Orange)
    { {{1,1}, {1,1}}, 4 },                             // Quadrado (Yellow)
    { {{1,0}, {1,1}}, 5 },                             // Mini L (Green)
    { {{0,1}, {1,1}}, 6 },                             // Mini J (Purple)
    { {{0,0,1}, {1,1,1}, {0,0,0}}, 2 },                // L Inteiro (Blue)
    { {{1,1,0}, {0,1,1}, {0,0,0}}, 1 },                // Z Inteiro (Cyan)
    { {{1,1,1}, {1,1,1}, {0,0,0}}, 9 },                // Bloco com 6 cubos (GOLD!)
    { {{1,1,1}, {1,1,0}, {0,0,0}}, 6 }                 // Bloco com 5 cubos 
};

vector<vector<int>> RotateMatrix(const vector<vector<int>>& mat) {
    int n = mat.size();
    vector<vector<int>> res(n, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) { res[j][n - 1 - i] = mat[i][j]; }
    }
    return res;
}

struct BgPiece {
    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    Vector3 rotVelocity;
    int pieceType;
    Color color;
    float scale;
    deque<Vector3> trail;
};
vector<BgPiece> bgPieces;

Vector3 GetSafeBgPos() {
    float x, y, z;
    do {
        x = GetRandomFloat(-150, 150);
        y = GetRandomFloat(-80, 150);
    } while (x > -40.0f && x < 40.0f && y > -20.0f && y < 60.0f);
    z = GetRandomFloat(-500, 50);
    return {x, y, z};
}

void InitBackgroundPieces() {
    bgPieces.clear();
    for (int i = 0; i < 35; i++) { // Reduzido drasticamente para evitar poluição visual
        BgPiece p;
        p.position = GetSafeBgPos();
        p.velocity = { 0, 0, GetRandomFloat(40, 100) };
        p.rotation = { GetRandomFloat(0, 360), GetRandomFloat(0, 360), GetRandomFloat(0, 360) };
        p.rotVelocity = { GetRandomFloat(-60, 60), GetRandomFloat(-60, 60), GetRandomFloat(-60, 60) };
        p.pieceType = GetRandomValue(0, pieces.size() - 1);
        p.color = pieceColors[pieces[p.pieceType].colorID];
        p.scale = GetRandomFloat(1.5f, 4.5f);
        bgPieces.push_back(p);
    }
}

// =====================================================================
// ENGINE PRINCIPAL DO JOGO AAA
// =====================================================================
class JogoTetris3D {
private:
    GameState currentState = INTRO; 
    int menuSelection = 0;
    int settingsSelection = 0;

    // Resoluções solicitadas
    struct Resolution { int w; int h; };
    vector<Resolution> resList = {
        {800,600}, {1024,768}, {1128,634}, {1280,960}, {1280,1024},
        {1366,768}, {1680,1050}, {1760,990}, {1920,1080},
        {2560,1440}, {3072,1728}, {3200,1800}, {3840,2160}
    };
    int currentResIdx = 8; // Começa na 1920x1080
    
    // Qualidade Gráfica
    vector<string> qualities = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
    int currentQualityIdx = 3; // ULTRA default

    string currentInputKey = "";
    bool checkingOnline = false;
    bool hasLocalLicense = false; 
    float checkTimer = 0.0f;
    string authStatusMsg = "SISTEMA PROTEGIDO. INSIRA A CHAVE ONLINE:";
    int charCount = 0;

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int continues = 3; 
    int bombs = 2; 
    int stars = 0; 
    int currentGridWidth = 14; 
    int linesClearedTotal = 0;
    
    // SISTEMA DE COMBO EXPANDIDO E EMOCIONANTE (JOGADOR)
    int comboCount = 0; 
    float comboTimer = 0.0f; 
    
    // SISTEMA DE CONTINUE
    bool isContinuing = false;
    float continueTimer = 0.0f;

    bool gameOver = false;
    bool isPaused = false; 
    
    bool isClassicMode = false;   
    bool isExpansiveMode = true;  
    bool isBossMode = false;      
    bool isTimeAttackMode = false; 
    bool isHardcoreMode = false;   
    bool isDuelMode = false;       

    float timeAttackTimer = 180.0f; 
    float hardcoreJunkTimer = 0.0f; 
    
    // =====================================================================
    // AI DUEL MODE VARIABLES (Independência total)
    // =====================================================================
    int aiBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int aiScore = 0;
    int aiBombs = 2;     // A I.A. tem o seu próprio estoque de bombas (Purge)!
    bool aiDead = false; // Controla se a IA morreu sufocada
    vector<vector<int>> aiCurrentPiece;
    vector<vector<int>> aiNextPiece;
    int aiCurrentX, aiCurrentY, aiCurrentColor, aiNextColor;
    float aiRenderFallY, aiRenderX, aiCurrentRotAngle;
    float aiMoveTimer = 0.0f;
    int aiTargetX = 0, aiTargetRot = 0;
    bool aiIsBrilliant = false;
    
    // MENSAGENS E COMBOS INDEPENDENTES DA IA
    int aiComboCount = 0; 
    float aiComboTimer = 0.0f; 
    string aiMensagemEspecial = "";
    float aiTimerMensagem = 0.0f;

    struct PieceTrail {
        vector<vector<int>> shape;
        int colorID;
        float x, y, rot;
        float life, maxLife;
    };
    vector<PieceTrail> pieceTrails;
    vector<PieceTrail> aiPieceTrails; // Trilhas isoladas para a AI
    float trailSpawnTimer = 0.0f;

    float manualZoomOffset = 0.0f; 
    float manualCamAngleX = 0.0f;  
    float manualCamAngleY = 0.0f;  
    Vector3 manualCamPan = {0.0f, 0.0f, 0.0f}; 
    
    vector<vector<int>> currentPiece;
    int currentX, currentY, currentColor;
    float renderFallY; 
    float renderX; 
    float currentRotAngle = 0.0f; 
    bool currentIsBrilliant = false; 
    
    vector<vector<int>> nextPiece;
    int nextColor;
    bool nextIsBrilliant = false; 

    // SISTEMA DE HOLD E RASTRO (TRAIL) E ANIMAÇÃO DE SPAWN MAGICA
    vector<vector<int>> holdPiece;
    int holdColor = 0;
    bool canHold = true;
    float pieceSpawnAnimTimer = 0.0f; 
    float aiPieceSpawnAnimTimer = 0.0f; 

    float fallTimer = 0.0f;
    float gridExpansionTimer = 0.0f; 
    float currentGridElevation = 2.0f; 
    string mensagemEspecial = "";
    float timerMensagem = 0.0f;

    float hitStopTimer = 0.0f;
    float damageVignette = 0.0f;
    float goldTint = 0.0f; 
    float musicPulse = 0.0f;
    
    Camera3D camera = { 0 };
    Vector3 defaultCamPos = { 0.0f, 2.0f, 22.0f }; 
    Vector3 defaultCamTarget = { 0.0f, 10.0f, 0.0f };
    float cameraFovTarget = 45.0f;
    float cameraBankAngle = 0.0f; 
    
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;
    float nukeSpinAngle = 0.0f; 
    float bossOrbitAngle = 0.0f; 
    Vector3 currentBossPos = {0,0,0}; 
    
    float moveLeftTimer = 0.0f;
    float moveRightTimer = 0.0f;
    const float DAS_DELAY = 0.12f; 
    const float ARR_RATE = 0.02f;  

    bool bossActive = false;
    int bossHp = 0;
    int linesUntilBoss = 15; 
    float bossAttackTimer = 0.0f;
    float bossEntryAnim = 0.0f;
    int bossEncounterCount = 0; 
    float currentBossAttackDelay = 16.0f; 
    float bossCinematicSpinTimer = 0.0f;
    float bossCinematicCooldown = 15.0f;

    bool showExitPrompt = false;
    bool confirmExit = false;
    bool sfxEnabled = true;
    bool musicEnabled = true;
    bool isFullscreen = true; 
    
    RenderTexture2D bgRenderTarget; 
    RenderTexture2D nextPieceRT; 
    RenderTexture2D holdPieceRT; 
    RenderTexture2D finalRenderTarget; 

    Sound sndMove, sndRotate, sndDrop, sndClear1, sndClear2, sndClear3, sndClear4, sndGameOver;

    Music sndMusic = { 0 };
    int currentMusicTrack = 1; 

    mt19937 rng;
    Shader postProcessShader;
    int resLoc, timeLoc, pulseLoc, hitStopLoc, dmgVignetteLoc, goldTintLoc;

    // =====================================================================
    // O NOVO SISTEMA UNIVERSAL DE FONTES EM BLOCOS DE TETRIS (AAA ENGINE)
    // =====================================================================
    float MeasureTetrisText(string text, float blockSize) {
        float width = 0.0f;
        for (char c : text) {
            if (c == ' ') width += 3.0f * blockSize; 
            else width += 4.0f * blockSize; 
        }
        return width > 0 ? width - blockSize : 0; 
    }

    void DrawTetrisText(string text, float startX, float startY, float blockSize, float pulse, bool useRainbow, Color flatColor) {
        float currentX = startX;
        float time = (float)GetTime();

        for (int k = 0; k < text.length(); k++) {
            char c = toupper(text[k]); 
            if (c == ' ') {
                currentX += 3.0f * blockSize; 
                continue;
            }

            vector<string> letter;
            switch(c) {
                case 'A': letter = {"010", "101", "111", "101", "101"}; break;
                case 'B': letter = {"110", "101", "110", "101", "110"}; break;
                case 'C': letter = {"011", "100", "100", "100", "011"}; break;
                case 'D': letter = {"110", "101", "101", "101", "110"}; break;
                case 'E': letter = {"111", "100", "111", "100", "111"}; break;
                case 'F': letter = {"111", "100", "110", "100", "100"}; break;
                case 'G': letter = {"011", "100", "101", "101", "011"}; break;
                case 'H': letter = {"101", "101", "111", "101", "101"}; break;
                case 'I': letter = {"111", "010", "010", "010", "111"}; break;
                case 'J': letter = {"001", "001", "001", "101", "111"}; break;
                case 'K': letter = {"101", "110", "100", "110", "101"}; break;
                case 'L': letter = {"100", "100", "100", "100", "111"}; break;
                case 'M': letter = {"101", "111", "101", "101", "101"}; break;
                case 'N': letter = {"111", "101", "101", "101", "101"}; break;
                case 'O': letter = {"010", "101", "101", "101", "010"}; break;
                case 'P': letter = {"110", "101", "110", "100", "100"}; break;
                case 'Q': letter = {"010", "101", "101", "010", "001"}; break;
                case 'R': letter = {"110", "101", "110", "101", "101"}; break;
                case 'S': letter = {"011", "100", "010", "001", "110"}; break;
                case 'T': letter = {"111", "010", "010", "010", "010"}; break;
                case 'U': letter = {"101", "101", "101", "101", "011"}; break;
                case 'V': letter = {"101", "101", "101", "101", "010"}; break;
                case 'W': letter = {"101", "101", "101", "111", "101"}; break;
                case 'X': letter = {"101", "101", "010", "101", "101"}; break;
                case 'Y': letter = {"101", "101", "010", "010", "010"}; break;
                case 'Z': letter = {"111", "001", "010", "100", "111"}; break;
                case '0': letter = {"111", "101", "101", "101", "111"}; break;
                case '1': letter = {"010", "110", "010", "010", "111"}; break;
                case '2': letter = {"111", "001", "111", "100", "111"}; break;
                case '3': letter = {"111", "001", "111", "001", "111"}; break;
                case '4': letter = {"101", "101", "111", "001", "001"}; break;
                case '5': letter = {"111", "100", "111", "001", "111"}; break;
                case '6': letter = {"111", "100", "111", "101", "111"}; break;
                case '7': letter = {"111", "001", "001", "010", "010"}; break;
                case '8': letter = {"111", "101", "111", "101", "111"}; break;
                case '9': letter = {"111", "101", "111", "001", "111"}; break;
                case ':': letter = {"000", "010", "000", "010", "000"}; break;
                case '-': letter = {"000", "000", "111", "000", "000"}; break;
                case '>': letter = {"100", "010", "001", "010", "100"}; break;
                case '/': letter = {"001", "001", "010", "100", "100"}; break;
                case '[': letter = {"110", "100", "100", "100", "110"}; break;
                case ']': letter = {"011", "001", "001", "001", "011"}; break;
                case '.': letter = {"000", "000", "000", "000", "010"}; break;
                case '+': letter = {"000", "010", "111", "010", "000"}; break;
                case '?': letter = {"010", "101", "001", "000", "010"}; break;
                case '(': letter = {"010", "100", "100", "100", "010"}; break;
                case ')': letter = {"010", "001", "001", "001", "010"}; break;
                case '!': letter = {"010", "010", "010", "000", "010"}; break;
                case '=': letter = {"000", "111", "000", "111", "000"}; break;
                default:  letter = {"111", "111", "111", "111", "111"}; break; 
            }

            Color baseColor = useRainbow ? pieceColors[(k % 8) + 1] : flatColor; 
            
            float jumpY = (useRainbow || c == '>') ? sin(time * 8.0f + k * 0.5f) * (pulse * 3.0f) : 0.0f; 
            float glitchX = (useRainbow && GetRandomValue(0, 100) > 95) ? GetRandomFloat(-pulse, pulse) * 3.0f : 0.0f;

            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 3; j++) {
                    if (letter[i][j] == '1') {
                        float bx = currentX + j * blockSize + glitchX;
                        float by = startY + i * blockSize + jumpY;
                        
                        float innerSize = blockSize * 0.85f; 
                        float beatScale = 1.0f + (pulse * 0.06f); 
                        
                        DrawRectangle(bx, by, innerSize * beatScale, innerSize * beatScale, baseColor);
                        DrawRectangle(bx + (blockSize*0.1f), by + (blockSize*0.1f), innerSize * beatScale - (blockSize*0.25f), innerSize * beatScale - (blockSize*0.25f), ColorAlpha(WHITE, 0.4f));
                        DrawRectangleLines(bx, by, innerSize * beatScale, innerSize * beatScale, ColorAlpha(WHITE, 0.8f));
                    }
                }
            }
            currentX += 4.0f * blockSize; 
        }
    }
    
    // FUNÇÃO: TEXTO BRANCO COM CONTORNO BRILHANTE DEGRADE PULSANTE
    void DrawTetrisTextGlowing(string text, float startX, float startY, float blockSize, float pulse) {
        float t = (float)GetTime() * 150.0f;
        Color glow1 = ColorFromHSV(fmod(t, 360.0f), 1.0f, 1.0f);
        Color glow2 = ColorFromHSV(fmod(t + 180.0f, 360.0f), 1.0f, 1.0f);
        glow1.a = 200; glow2.a = 200;
        
        float off = blockSize * 0.25f;
        
        DrawTetrisText(text, startX - off, startY, blockSize, pulse, false, glow1);
        DrawTetrisText(text, startX + off, startY, blockSize, pulse, false, glow2);
        DrawTetrisText(text, startX, startY - off, blockSize, pulse, false, glow2);
        DrawTetrisText(text, startX, startY + off, blockSize, pulse, false, glow1);
        
        DrawTetrisText(text, startX, startY, blockSize, pulse, false, WHITE);
    }

    // =====================================================================
    // MOLDURAS DA UI COM MINI CUBOS DE TETRIS 2D (AAA FRAMES)
    // =====================================================================
    void DrawCube2D(Vector2 pos, float size, Color col) {
        DrawRectangle(pos.x, pos.y, size, size, col);
        DrawRectangle(pos.x + size*0.1f, pos.y + size*0.1f, size*0.8f, size*0.8f, ColorAlpha(WHITE, 0.3f));
        DrawRectangleLines(pos.x, pos.y, size, size, ColorAlpha(WHITE, 0.6f));
    }

    void DrawFrameWithCubes2D(Rectangle rect, float cubeSize, Color color, Color bgColor) {
        DrawRectangle(rect.x + cubeSize, rect.y + cubeSize, rect.width - cubeSize*2, rect.height - cubeSize*2, bgColor);
        
        for (float x = rect.x; x < rect.x + rect.width; x += cubeSize) {
            DrawCube2D({x, rect.y}, cubeSize, color);
            DrawCube2D({x, rect.y + rect.height - cubeSize}, cubeSize, color);
        }
        for (float y = rect.y + cubeSize; y < rect.y + rect.height - cubeSize; y += cubeSize) {
            DrawCube2D({rect.x, y}, cubeSize, color);
            DrawCube2D({rect.x + rect.width - cubeSize, y}, cubeSize, color);
        }
    }

    // =====================================================================
    // MÓDULO DE INTRODUÇÃO: BETTARELLO CODE
    // =====================================================================
    float introTimer = 5.0f; 

    void DrawHexagonWire(Vector2 center, float radius, float rotation, float thick, Color color) {
        for(int i=0; i<6; i++) {
            float a1 = rotation + (i * PI / 3.0f);
            float a2 = rotation + ((i+1) * PI / 3.0f);
            Vector2 p1 = { center.x + cos(a1)*radius, center.y + sin(a1)*radius };
            Vector2 p2 = { center.x + cos(a2)*radius, center.y + sin(a2)*radius };
            DrawLineEx(p1, p2, thick, color);
        }
    }

    void DrawIntro2D(float dt) {
        float progress = 5.0f - introTimer; 
        
        string targetText1 = "BETTARELLO";
        string targetText2 = "CODE";
        
        string renderText1 = "";
        string renderText2 = "";
        
        int charsToReveal1 = (int)((progress / 2.0f) * targetText1.length());
        int charsToReveal2 = (int)(((progress - 1.5f) / 1.5f) * targetText2.length());
        
        for(int i=0; i<targetText1.length(); i++) {
            if (i < charsToReveal1) renderText1 += targetText1[i];
            else renderText1 += (char)GetRandomValue(33, 126); 
        }
        for(int i=0; i<targetText2.length(); i++) {
            if (charsToReveal2 < 0) { renderText2 = ""; break; }
            if (i < charsToReveal2) renderText2 += targetText2[i];
            else renderText2 += (char)GetRandomValue(33, 126);
        }
        
        float alpha = 1.0f;
        if (introTimer < 0.5f) alpha = introTimer / 0.5f; 
        if (progress < 0.5f) alpha = progress / 0.5f; 
        
        Color mainCol = ColorAlpha(C_GOLD, alpha);
        Color secCol = ColorAlpha(C_ORANGE, alpha);
        Color whiteCol = ColorAlpha(WHITE, alpha);
        Color glitchCol = ColorAlpha(C_RED, alpha * 0.5f);
        
        float cx = SCREEN_WIDTH / 2.0f;
        float cy = SCREEN_HEIGHT / 2.0f;
        
        float logoY = cy - 120.0f;
        float rotation = progress * 2.0f; 
        float radius = Lerp(0.0f, 80.0f, Clamp(progress * 2.0f, 0.0f, 1.0f)); 
        
        DrawHexagonWire({cx, logoY}, radius, rotation, 4.0f, mainCol);
        DrawHexagonWire({cx, logoY}, radius * 0.8f, -rotation * 1.5f, 2.0f, secCol);
        
        if (progress > 1.0f) {
            float bAlpha = Clamp((progress - 1.0f) * 2.0f, 0.0f, 1.0f) * alpha;
            float bSize = 12.0f;
            DrawTetrisText("B", cx - MeasureTetrisText("B", bSize)/2, logoY - (bSize*2.5f), bSize, 0.0f, false, ColorAlpha(WHITE, bAlpha));
        }
        
        float t1Size = 10.0f;
        float t2Size = 6.0f;
        
        if (GetRandomValue(0, 100) > 90) { 
            DrawTetrisText(renderText1, cx - MeasureTetrisText(renderText1, t1Size)/2 + GetRandomValue(-8, 8), cy + 10, t1Size, 0, false, glitchCol);
        }
        
        DrawTetrisText(renderText1, cx - MeasureTetrisText(renderText1, t1Size)/2, cy + 10, t1Size, 0, false, whiteCol);
        
        if (progress > 1.5f) {
            string spacedCode = "";
            for(char c : renderText2) { spacedCode += c; spacedCode += " "; }
            DrawTetrisText(spacedCode, cx - MeasureTetrisText(spacedCode, t2Size)/2, cy + 80, t2Size, 0, false, secCol);
        }
        
        float barWidth = Lerp(0.0f, 400.0f, Clamp(progress, 0.0f, 1.0f));
        DrawRectangle(cx - barWidth/2, cy, barWidth, 2, ColorAlpha(C_GOLD, alpha * 0.5f));
        DrawRectangle(cx - barWidth/2, cy + 140, barWidth, 2, ColorAlpha(C_GOLD, alpha * 0.5f));
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
                
                float blockSize = 4.0f * floatingTexts[i].scale;
                DrawTetrisText(floatingTexts[i].text, screenPos.x - MeasureTetrisText(floatingTexts[i].text, blockSize)/2, screenPos.y, blockSize, 0.0f, false, c);
            }
        }
        rlEnableDepthMask();
    }

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
                if (ValidateKey(key)) hasLocalLicense = true; 
            }
        }
    }
    void SaveLicense(string key) { SaveFileText("omegared_sys.key", (char*)key.c_str()); }
    
    int GetRandomPiece() { uniform_int_distribution<int> dist(0, pieces.size() - 1); return dist(rng); }

    // Grid do Player ainda MAIS focado pro centro!
    Vector3 GetWorldPos(int logicalX, int logicalY) {
        float offsetX = isDuelMode ? -5.2f : 0.0f; 
        return {
            (float)logicalX - (currentGridWidth / 2.0f) + 0.5f + offsetX,
            (float)(BOARD_HEIGHT - logicalY) - 0.5f + currentGridElevation,
            0.0f
        };
    }

    // Grid da AI ainda MAIS focado pro centro!
    Vector3 GetAIWorldPos(int logicalX, int logicalY) {
        return {
            (float)logicalX - (currentGridWidth / 2.0f) + 0.5f + 5.2f, 
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
        string fileName = "src/music" + to_string(currentMusicTrack) + ".mp3";
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

    void ShuffleMusic() {
        int prev = currentMusicTrack;
        for(int i = 0; i < 30; i++) {
            currentMusicTrack = GetRandomValue(1, 30);
            if (currentMusicTrack != prev) {
                string fn = "src/music" + to_string(currentMusicTrack) + ".mp3";
                if (FileExists(fn.c_str())) break;
            }
        }
        LoadNextMusic();
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
        currentY = -5; 
        renderFallY = -5.0f; 
        renderX = currentX;
        
        pieceSpawnAnimTimer = 1.0f; 

        float spawnWorldX = currentX + (currentPiece[0].size() / 2.0f) - (currentGridWidth / 2.0f) + 0.5f;
        if (isDuelMode) spawnWorldX -= 5.2f; 
        
        float spawnWorldY = (float)BOARD_HEIGHT - (currentY + currentPiece.size() / 2.0f) - 0.5f + currentGridElevation;
        SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, WHITE, 8, 15.0f);
        SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[currentColor], 15, 25.0f);
        SpawnShockwave({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[currentColor]);

        int p2 = GetRandomPiece();
        nextPiece = pieces[p2].shape;
        nextColor = pieces[p2].colorID;

        canHold = true; 

        if (!IsValidMove(currentPiece, currentX, currentY)) {
            if (continues > 0 && !isDuelMode) {
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
                currentY = -5; renderFallY = -5.0f; renderX = currentX;
            } else {
                if (isDuelMode) {
                    gameOver = true; 
                    TocarSom(sndGameOver);
                } else {
                    isContinuing = true; continueTimer = 9.99f; TocarSom(sndGameOver);
                }
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
        cameraShakeTimer = 0.15f; cameraShakeIntensity = 0.3f; 

        bool blockPlacedOut = false;

        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0) {
                    if ((currentY + i) < 0) {
                        blockPlacedOut = true; 
                    } else if ((currentY + i) >= 0 && (currentY + i) < BOARD_HEIGHT) {
                        board[currentY + i][currentX + j] = currentColor;
                        Vector3 worldLockPos = GetWorldPos(currentX + j, currentY + i);
                        SpawnParticles3D(worldLockPos, pieceColors[currentColor], 8, 5.0f);
                    }
                }
            }
        }

        if (blockPlacedOut) {
            if (continues > 0 && !isDuelMode) {
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
                if (isDuelMode) {
                    gameOver = true;
                    TocarSom(sndGameOver);
                } else {
                    isContinuing = true; continueTimer = 9.99f; TocarSom(sndGameOver);
                }
            }
            return; 
        }
        
        if (currentIsBrilliant && isExpansiveMode) {
            if (currentGridWidth < MAX_BOARD_WIDTH) {
                int expansion = 2; int shift = expansion / 2;
                for (int i = 0; i < BOARD_HEIGHT; i++) {
                    for (int j = currentGridWidth - 1; j >= 0; j--) board[i][j + shift] = board[i][j];
                    for (int j = 0; j < shift; j++) board[i][j] = 0; 
                }
                currentGridWidth += expansion;
                gridExpansionTimer = 2.0f; 
                mensagemEspecial = "SYSTEM EXPANDED"; timerMensagem = 3.0f;
                cameraShakeTimer = 1.0f; cameraShakeIntensity = 3.5f;
                hitStopTimer = 0.2f; 
                TocarSom(sndClear4); 
            } else {
                mensagemEspecial = "MAX POWER OVERDRIVE!"; score += 2000 * level; timerMensagem = 3.0f;
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_GOLD, 150, 25.0f, 3);
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
            else board[BOARD_HEIGHT - 1][j] = 8; // Cor 8 é o Bloco Fantasma/Cinza
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
            comboCount++; 
            comboTimer = 6.0f; 
            
            avgClearPos.y /= linesClearedNow;
            linesClearedTotal += linesClearedNow;
            if (isDuelMode) avgClearPos.x -= 5.2f;
            
            if (isExpansiveMode) {
                stars += linesClearedNow;
                if (stars >= 10) {
                    stars -= 10; nextIsBrilliant = true; 
                    mensagemEspecial = "MAGIC PIECE READY"; timerMensagem = 3.0f;
                    TocarSom(sndClear2);
                }
            }

            cameraShakeTimer = 0.5f + (linesClearedNow * 0.15f);
            cameraShakeIntensity = linesClearedNow * 2.5f; 
            cameraFovTarget = 45.0f - (linesClearedNow * 3.0f); 
            hitStopTimer = linesClearedNow * 0.05f; 
            
            if(linesClearedNow >= 4) {
                goldTint = 1.0f; // Efeito Gold flash!
                SpawnShockwave({isDuelMode ? -5.2f : 0.0f, avgClearPos.y, 0}, C_GOLD);
            }

            int ptsGained = 0;
            if (linesClearedNow == 1) { ptsGained = 100 * level; TocarSom(sndClear1); } 
            else if (linesClearedNow == 2) { ptsGained = 300 * level; TocarSom(sndClear2); } 
            else if (linesClearedNow == 3) { ptsGained = 500 * level; TocarSom(sndClear3); } 
            else if (linesClearedNow >= 4) { ptsGained = 800 * level; cameraShakeIntensity = 6.0f; TocarSom(sndClear4); }
            
            ptsGained *= comboCount;
            score += ptsGained;
            
            if (comboCount == 2) { mensagemEspecial = "NICE!"; timerMensagem = 2.0f; }
            else if (comboCount == 3) { mensagemEspecial = "VERY NICE!"; timerMensagem = 2.0f; }
            else if (comboCount == 4) { mensagemEspecial = "INCREDIBLE!"; timerMensagem = 3.0f; }
            else if (comboCount >= 5) { mensagemEspecial = "GOD MODE!"; timerMensagem = 3.0f; goldTint = 1.0f; }
            else if (linesClearedNow >= 4) { mensagemEspecial = "TETRABETTA!"; timerMensagem = 3.0f; }
            else if (linesClearedNow == 3) { mensagemEspecial = "IMPRESSIVE"; timerMensagem = 2.0f; }
            else if (linesClearedNow == 2) { mensagemEspecial = "GOOD"; timerMensagem = 2.0f; }

            SpawnFloatingText(avgClearPos, "+" + to_string(ptsGained), C_GOLD, 1.5f + (linesClearedNow * 0.5f));
            
            if (comboCount > 1) {
                SpawnFloatingText({avgClearPos.x, avgClearPos.y + 2.0f, avgClearPos.z}, "COMBO X" + to_string(comboCount), C_GOLD, 2.0f);
            }
            
            // Bônus de Tempo para o Modo TIME ATTACK
            if (isTimeAttackMode) {
                int bonusTime = 0;
                if (linesClearedNow == 1) bonusTime = 5;
                else if (linesClearedNow == 2) bonusTime = 8;
                else if (linesClearedNow == 3) bonusTime = 11;
                else if (linesClearedNow >= 4) bonusTime = 15;

                timeAttackTimer += bonusTime;
                
                FloatingText ft;
                ft.pos = {0.0f, currentGridElevation + (BOARD_HEIGHT / 2.0f), 0.0f}; 
                ft.text = "+" + to_string(bonusTime) + " SEC";
                ft.color = C_CYAN;
                ft.scale = 3.5f; 
                ft.life = 0.8f;  
                floatingTexts.push_back(ft);
            }

            if (bossActive && isBossMode) {
                bossHp -= linesClearedNow; 
                SpawnFloatingText({0, avgClearPos.y + 4.0f, 0}, "BOSS DMG -" + to_string(linesClearedNow), C_RED, 2.5f);
                if (bossHp <= 0) {
                    bossActive = false; 
                    bossCinematicSpinTimer = 0.0f; 
                    
                    if (bossEncounterCount == 1) linesUntilBoss = 45;
                    else if (bossEncounterCount == 2) linesUntilBoss = 80;
                    else if (bossEncounterCount == 3) linesUntilBoss = 110;
                    else if (bossEncounterCount == 4) linesUntilBoss = 150;
                    else linesUntilBoss = 150 + (bossEncounterCount - 4) * 50; 
                    
                    score += 5000 * level;
                    SpawnParticles3D({0, currentGridElevation + 20.0f, -5.0f}, C_GOLD, 400, 60.0f, 8); 
                    cameraShakeTimer = 2.0f; cameraShakeIntensity = 3.0f;
                    hitStopTimer = 0.5f; 
                    mensagemEspecial = "VIRUS DELETED!"; timerMensagem = 4.0f;
                    TocarSom(sndClear4);
                }
            }
            if (!isHardcoreMode) { 
                level = (linesClearedTotal / 10) + 1;
            }
        } 
    }

    // INDEPENDENTE: Somente o Jogador aciona o próprio tabuleiro!
    void NukeBoard() {
        int blocksDestroyed = 0;
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < currentGridWidth; j++) {
                if (board[i][j] != 0) {
                    blocksDestroyed++;
                    Vector3 blockPos = GetWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 10, 25.0f, board[i][j]);
                    board[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            score += blocksDestroyed * 50 * level;
            mensagemEspecial = "SYSTEM PURGE!!!"; timerMensagem = 3.0f;
            cameraShakeTimer = 1.5f; cameraShakeIntensity = 4.0f;
            cameraFovTarget = 35.0f; 
            nukeSpinAngle = PI * 4.0f; 
            hitStopTimer = 0.3f;
            goldTint = 1.0f;
            SpawnShockwave({isDuelMode ? -5.2f : 0.0f, currentGridElevation, 0}, C_GOLD);
            TocarSom(sndClear4); 

            if (bossActive && isBossMode) {
                bossActive = false; 
                bossCinematicSpinTimer = 0.0f;
                
                if (bossEncounterCount == 1) linesUntilBoss = 45;
                else if (bossEncounterCount == 2) linesUntilBoss = 80;
                else if (bossEncounterCount == 3) linesUntilBoss = 110;
                else if (bossEncounterCount == 4) linesUntilBoss = 150;
                else linesUntilBoss = 150 + (bossEncounterCount - 4) * 50;
                
                score += 10000; 
                SpawnParticles3D({0, currentGridElevation + 20.0f, -5.0f}, C_RED, 800, 80.0f); 
                cameraShakeTimer = 2.0f; cameraShakeIntensity = 3.0f;
                hitStopTimer = 0.5f; 
                mensagemEspecial = "BOSS PURGED!"; timerMensagem = 4.0f;
            }
        }
    }

    // =====================================================================
    // AI DUEL MODE FUNCTIONS (Engrenagens Analíticas do Monstro)
    // =====================================================================
    bool IsValidAIMove(const vector<vector<int>>& piece, int x, int y) {
        for (int i = 0; i < piece.size(); ++i) {
            for (int j = 0; j < piece[i].size(); ++j) {
                if (piece[i][j] != 0) {
                    int bX = x + j;
                    int bY = y + i;
                    if (bX < 0 || bX >= currentGridWidth || bY >= BOARD_HEIGHT) return false;
                    if (bY >= 0 && aiBoard[bY][bX] != 0) return false;
                }
            }
        }
        return true;
    }

    int GetAIHoles(int tempBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH]) {
        int holes = 0;
        for (int x = 0; x < currentGridWidth; x++) {
            bool blockFound = false;
            for (int y = 0; y < BOARD_HEIGHT; y++) {
                if (tempBoard[y][x] != 0) blockFound = true;
                else if (blockFound) holes++;
            }
        }
        return holes;
    }

    int GetAIAggregateHeight(int tempBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH]) {
        int height = 0;
        for (int x = 0; x < currentGridWidth; x++) {
            for (int y = 0; y < BOARD_HEIGHT; y++) {
                if (tempBoard[y][x] != 0) {
                    height += (BOARD_HEIGHT - y);
                    break;
                }
            }
        }
        return height;
    }

    int GetAIBumpiness(int tempBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH]) {
        int bumpiness = 0;
        int heights[MAX_BOARD_WIDTH] = {0};
        for (int x = 0; x < currentGridWidth; x++) {
            for (int y = 0; y < BOARD_HEIGHT; y++) {
                if (tempBoard[y][x] != 0) {
                    heights[x] = BOARD_HEIGHT - y;
                    break;
                }
            }
        }
        for (int x = 0; x < currentGridWidth - 1; x++) {
            bumpiness += abs(heights[x] - heights[x+1]);
        }
        return bumpiness;
    }

    void EvaluateBestAIMove() {
        int bestScore = -2000000000; 
        aiTargetX = aiCurrentX;
        aiTargetRot = 0;
        
        vector<vector<int>> testPiece = aiCurrentPiece;
        for (int rot = 0; rot < 4; rot++) {
            for (int x = -4; x < currentGridWidth + 4; x++) {
                if (IsValidAIMove(testPiece, x, aiCurrentY)) {
                    int dropY = aiCurrentY;
                    while (IsValidAIMove(testPiece, x, dropY + 1)) dropY++;
                    
                    int tempBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH];
                    for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) tempBoard[i][j] = aiBoard[i][j];
                    
                    for (int i = 0; i < testPiece.size(); ++i) {
                        for (int j = 0; j < testPiece[i].size(); ++j) {
                            if (testPiece[i][j] != 0) {
                                if (dropY + i >= 0 && dropY + i < BOARD_HEIGHT && x + j >= 0 && x + j < currentGridWidth)
                                    tempBoard[dropY + i][x + j] = 1;
                            }
                        }
                    }
                    
                    int lines = 0;
                    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
                        bool full = true;
                        for (int cx = 0; cx < currentGridWidth; cx++) {
                            if (tempBoard[y][cx] == 0) { full = false; break; }
                        }
                        if (full) {
                            lines++;
                            for (int k = y; k > 0; --k) {
                                for (int cx = 0; cx < currentGridWidth; cx++) tempBoard[k][cx] = tempBoard[k - 1][cx];
                            }
                            for (int cx = 0; cx < currentGridWidth; cx++) tempBoard[0][cx] = 0;
                            y++; 
                        }
                    }
                    
                    int holes = GetAIHoles(tempBoard);
                    int aggHeight = GetAIAggregateHeight(tempBoard);
                    int bumpiness = GetAIBumpiness(tempBoard);
                    
                    int score = (lines * 3400) - (aggHeight * 510) - (holes * 3560) - (bumpiness * 180);
                    
                    if (score > bestScore) {
                        bestScore = score;
                        aiTargetX = x;
                        aiTargetRot = rot;
                    }
                }
            }
            testPiece = RotateMatrix(testPiece);
        }
    }

    void SpawnAIPiece() {
        if (aiNextPiece.empty()) {
            int p1 = GetRandomPiece();
            aiNextPiece = pieces[p1].shape;
            aiNextColor = pieces[p1].colorID;
        }
        aiCurrentPiece = aiNextPiece;
        aiCurrentColor = aiNextColor;
        aiCurrentRotAngle = 0.0f;
        aiCurrentX = currentGridWidth / 2 - aiCurrentPiece[0].size() / 2;
        aiCurrentY = -5; 
        aiRenderFallY = -5.0f; 
        aiRenderX = aiCurrentX;

        aiPieceSpawnAnimTimer = 1.0f; 

        float spawnWorldX = aiCurrentX + (aiCurrentPiece[0].size() / 2.0f) - (currentGridWidth / 2.0f) + 0.5f + 5.2f;
        float spawnWorldY = (float)BOARD_HEIGHT - (aiCurrentY + aiCurrentPiece.size() / 2.0f) - 0.5f + currentGridElevation;
        SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, WHITE, 8, 15.0f);
        SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[aiCurrentColor], 15, 25.0f);
        SpawnShockwave({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[aiCurrentColor]);

        int p2 = GetRandomPiece();
        aiNextPiece = pieces[p2].shape;
        aiNextColor = pieces[p2].colorID;

        EvaluateBestAIMove(); 
    }

    // INDEPENDENTE: Sistema Purge isolado exclusivo da IA (Mensagem alterada para diferenciar!)
    void AINukeBoard() {
        int blocksDestroyed = 0;
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < currentGridWidth; j++) {
                if (aiBoard[i][j] != 0) {
                    blocksDestroyed++;
                    SpawnParticles3D(GetAIWorldPos(j, i), pieceColors[aiBoard[i][j]], 10, 25.0f, aiBoard[i][j]);
                    aiBoard[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            aiScore += blocksDestroyed * 50 * level;
            aiMensagemEspecial = "AI SYSTEM PURGE!!!"; aiTimerMensagem = 3.0f; // Diferenciado!
            cameraShakeTimer = 1.5f; cameraShakeIntensity = 4.0f;
            hitStopTimer = 0.3f;
            SpawnShockwave({5.2f, currentGridElevation, 0}, C_RED);
            TocarSom(sndClear4); 
        }
    }

    void ClearAILines() {
        int linesClearedNow = 0;
        Vector3 avgClearPos = {0,0,0};

        for (int i = BOARD_HEIGHT - 1; i >= 0; --i) {
            bool isFull = true;
            for (int j = 0; j < currentGridWidth; ++j) {
                if (aiBoard[i][j] == 0) { isFull = false; break; }
            }
            if (isFull) {
                linesClearedNow++;
                avgClearPos.y += GetAIWorldPos(0, i).y;

                for(int j = 0; j < currentGridWidth; j++) {
                    Vector3 blockPos = GetAIWorldPos(j, i);
                    SpawnParticles3D(blockPos, pieceColors[aiBoard[i][j]], 25, 20.0f, aiBoard[i][j]);
                }
                for (int k = i; k > 0; --k) {
                    for (int j = 0; j < currentGridWidth; ++j) aiBoard[k][j] = aiBoard[k - 1][j];
                }
                for (int j = 0; j < currentGridWidth; ++j) aiBoard[0][j] = 0;
                i++; 
            }
        }
        if (linesClearedNow > 0) {
            aiComboCount++; 
            aiComboTimer = 6.0f; 
            
            avgClearPos.y /= linesClearedNow;
            avgClearPos.x += 5.2f; // AI Offset na Posição Media!
            
            cameraShakeTimer = 0.5f + (linesClearedNow * 0.15f);
            cameraShakeIntensity = linesClearedNow * 2.5f; 
            hitStopTimer = linesClearedNow * 0.05f; 

            if(linesClearedNow >= 4) {
                SpawnShockwave({5.2f, avgClearPos.y, 0}, C_RED);
            }

            int ptsGained = 0;
            if (linesClearedNow == 1) { ptsGained = 100 * level; TocarSom(sndClear1); } 
            else if (linesClearedNow == 2) { ptsGained = 300 * level; TocarSom(sndClear2); } 
            else if (linesClearedNow == 3) { ptsGained = 500 * level; TocarSom(sndClear3); } 
            else if (linesClearedNow >= 4) { ptsGained = 800 * level; cameraShakeIntensity = 6.0f; TocarSom(sndClear4); } 
            
            ptsGained *= aiComboCount;
            aiScore += ptsGained;

            // Elogios Isolados da Inteligência Artificial!
            if (aiComboCount == 2) { aiMensagemEspecial = "NICE!"; aiTimerMensagem = 2.0f; }
            else if (aiComboCount == 3) { aiMensagemEspecial = "VERY NICE!"; aiTimerMensagem = 2.0f; }
            else if (aiComboCount == 4) { aiMensagemEspecial = "INCREDIBLE!"; aiTimerMensagem = 3.0f; }
            else if (aiComboCount >= 5) { aiMensagemEspecial = "GOD MODE!"; aiTimerMensagem = 3.0f; }
            else if (linesClearedNow >= 4) { aiMensagemEspecial = "TETRABETTA!"; aiTimerMensagem = 3.0f; }
            else if (linesClearedNow == 3) { aiMensagemEspecial = "IMPRESSIVE"; aiTimerMensagem = 2.0f; }
            else if (linesClearedNow == 2) { aiMensagemEspecial = "GOOD"; aiTimerMensagem = 2.0f; }

            SpawnFloatingText(avgClearPos, "+" + to_string(ptsGained), C_RED, 1.5f + (linesClearedNow * 0.5f));
            if (aiComboCount > 1) {
                SpawnFloatingText({avgClearPos.x, avgClearPos.y + 2.0f, avgClearPos.z}, "COMBO X" + to_string(aiComboCount), C_RED, 2.0f);
            }
        }
    }

    void LockAIPiece() {
        TocarSom(sndDrop);
        for (int i = 0; i < aiCurrentPiece.size(); ++i) {
            for (int j = 0; j < aiCurrentPiece[i].size(); ++j) {
                if (aiCurrentPiece[i][j] != 0) {
                    if ((aiCurrentY + i) >= 0 && (aiCurrentY + i) < BOARD_HEIGHT) {
                        aiBoard[aiCurrentY + i][aiCurrentX + j] = aiCurrentColor;
                        SpawnParticles3D(GetAIWorldPos(aiCurrentX + j, aiCurrentY + i), pieceColors[aiCurrentColor], 8, 5.0f);
                    } else if ((aiCurrentY + i) < 0) {
                        // A Inteligencia Artificial sobrevive soltando o seu proprio Purge Nuke se puder!
                        if (aiBombs > 0) {
                            aiBombs--;
                            AINukeBoard();
                            SpawnAIPiece();
                            return; 
                        } else {
                            aiDead = true;   
                            gameOver = true; 
                            TocarSom(sndGameOver);
                            return;
                        }
                    }
                }
            }
        }
        ClearAILines();
        SpawnAIPiece();
    }

    void DrawSciFiBlock3D(Vector3 pos, Color baseCol, bool isReflection, bool isGhost = false, bool isBrilliant = false, float scale = 1.0f) {
        float s = CUBE_SIZE * scale; 
        
        Color coreCol = isReflection ? ColorAlpha(baseCol, 0.2f) : baseCol;
        Color shellCol = isReflection ? ColorAlpha(WHITE, 0.02f) : ColorAlpha(WHITE, 0.5f); 
        
        if (isBrilliant && !isGhost) {
            float t = (float)GetTime() * 10.0f;
            coreCol = ColorFromHSV(fmod(t * 20.0f + 40.0f, 60.0f), 1.0f, 1.0f); 
            shellCol = C_GOLD;
        }

        float coreSize = s * 0.70f; 
        float highY = coreSize * 0.42f;
        float highZ = 0.05f * scale;

        if (isGhost) {
            float pulse = (sin((float)GetTime() * 10.0f) + 1.0f) * 0.5f;
            DrawCube(pos, coreSize, coreSize, coreSize, ColorAlpha(baseCol, 0.1f + pulse*0.1f)); 
            DrawCubeWires(pos, s * 0.95f, s * 0.95f, s * 0.95f, ColorAlpha(baseCol, 0.4f + pulse*0.4f)); 
        } else {
            DrawCube(pos, coreSize, coreSize, coreSize, coreCol);
            if (!isReflection) {
                DrawCube({pos.x, pos.y + highY, pos.z + highZ}, coreSize*0.9f, s*0.05f, coreSize*0.9f, ColorAlpha(WHITE, 0.7f));
            }
            DrawCubeWires(pos, s * 0.95f, s * 0.95f, s * 0.95f, shellCol);
        }
    }

    void DrawProceduralEnvironment() {
        rlDisableDepthMask();
        
        for(auto& bp : bgPieces) {
            if (bp.trail.size() > 1) {
                rlBegin(RL_LINES);
                for(size_t j = 0; j < bp.trail.size() - 1; j++) {
                    float beatTrailAlpha = 0.08f + (musicPulse * 0.04f); 
                    float trailAlpha = (1.0f - ((float)j / bp.trail.size())) * beatTrailAlpha; 
                    rlColor4ub(bp.color.r, bp.color.g, bp.color.b, (unsigned char)(bp.color.a * fmin(1.0f, fmax(0.0f, trailAlpha))));
                    rlVertex3f(bp.trail[j].x, bp.trail[j].y, bp.trail[j].z);
                    rlVertex3f(bp.trail[j+1].x, bp.trail[j+1].y, bp.trail[j+1].z);
                }
                rlEnd();
            }

            rlPushMatrix();
            rlTranslatef(bp.position.x, bp.position.y, bp.position.z);
            rlRotatef(bp.rotation.x, 1, 0, 0);
            rlRotatef(bp.rotation.y, 0, 1, 0);
            rlRotatef(bp.rotation.z, 0, 0, 1);
            
            float beatScale = bp.scale * (1.0f + (musicPulse * 0.25f)); 
            rlScalef(beatScale, beatScale, beatScale);

            auto shape = pieces[bp.pieceType].shape;
            float offX = shape[0].size() / 2.0f;
            float offY = shape.size() / 2.0f;

            for (int i = 0; i < shape.size(); ++i) {
                for (int j = 0; j < shape[i].size(); ++j) {
                    if (shape[i][j] != 0) {
                        Vector3 bPos = { (j - offX + 0.5f) * CUBE_SIZE, (-i + offY - 0.5f) * CUBE_SIZE, 0.0f };
                        
                        float beatAlphaWire = fmin(1.0f, fmax(0.0f, 0.06f + (musicPulse * 0.08f))); 
                        float beatAlphaCore = fmin(1.0f, fmax(0.0f, 0.005f + (musicPulse * 0.03f))); 

                        DrawCubeWires(bPos, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, ColorAlpha(bp.color, beatAlphaWire));
                        DrawCube(bPos, CUBE_SIZE*0.9f, CUBE_SIZE*0.9f, CUBE_SIZE*0.9f, ColorAlpha(bp.color, beatAlphaCore));
                    }
                }
            }
            rlPopMatrix();
        }
        
        rlEnableDepthMask();

        float panelWidth = currentGridWidth;
        float panelHeight = BOARD_HEIGHT + 3.0f;
        Color backPanelColor = bossActive ? Color{40, 0, 0, 200} : Color{15, 10, 5, 180}; 
        
        if (bossCinematicSpinTimer > 0.0f) {
            float spinProgress = 1.0f - (bossCinematicSpinTimer / 2.0f); 
            float fade = 1.0f - sin(spinProgress * PI); 
            backPanelColor.a = (unsigned char)(backPanelColor.a * fade);
        }

        DrawCube({0.0f, currentGridElevation + panelHeight/2.0f, -0.2f}, panelWidth, panelHeight, 0.05f, backPanelColor);
    }

    void DrawPlayfieldAndPieces(bool isReflection) {
        float trueBottomY = currentGridElevation;
        float trueTopY = currentGridElevation + BOARD_HEIGHT + 3.0f;
        float halfW = currentGridWidth / 2.0f;

        Color frameC = bossActive ? C_RED : C_GOLD; 

        float wallScale = 0.25f; 
        float leftWallX = -halfW - (wallScale / 2.0f);
        float rightWallX = halfW + (wallScale / 2.0f);
        float bottomWallY = trueBottomY - (wallScale / 2.0f);

        for (float y = trueBottomY; y < trueTopY; y += wallScale) {
            DrawSciFiBlock3D({leftWallX, y + (wallScale/2.0f), 0}, frameC, isReflection, false, bossActive, wallScale);
            DrawSciFiBlock3D({rightWallX, y + (wallScale/2.0f), 0}, frameC, isReflection, false, bossActive, wallScale);
        }
        for (float x = -halfW - wallScale; x < halfW + wallScale; x += wallScale) {
            DrawSciFiBlock3D({x + (wallScale/2.0f), bottomWallY, 0}, frameC, isReflection, false, bossActive, wallScale);
        }

        if (gridExpansionTimer > 0.0f && !isReflection) {
            float blinkAlpha = abs(sin((2.0f - gridExpansionTimer) * 2.0f * PI)); 
            Color glowC = ColorAlpha(C_GOLD, blinkAlpha * 0.7f);

            float flashLeftX = -halfW + 0.5f;
            float flashRightX = halfW - 0.5f;

            for (float y = 0; y < BOARD_HEIGHT + 3.0f; y += 1.0f) {
                DrawSciFiBlock3D({flashLeftX, trueBottomY + y + 0.5f, 0}, glowC, isReflection, false, true, 1.0f);
                DrawSciFiBlock3D({flashRightX, trueBottomY + y + 0.5f, 0}, glowC, isReflection, false, true, 1.0f);
            }
        }

        rlBegin(RL_LINES);
        Color gridCellCol = ColorAlpha(WHITE, 0.1f); 
        Color backGridCol = ColorAlpha(C_GOLD, 0.25f); 
        float backZ = -0.25f; 

        for (int x = 0; x <= currentGridWidth; x++) {
            float lineX = -halfW + x;
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(lineX, trueBottomY, 0);
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(lineX, trueTopY, 0);
            
            rlColor4ub(backGridCol.r, backGridCol.g, backGridCol.b, backGridCol.a); rlVertex3f(lineX, trueBottomY, backZ);
            rlColor4ub(backGridCol.r, backGridCol.g, backGridCol.b, backGridCol.a); rlVertex3f(lineX, trueTopY, backZ);
        }
        for (int y = 0; y <= BOARD_HEIGHT + 3; y++) {
            float lineY = trueBottomY + y;
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(-halfW, lineY, 0);
            rlColor4ub(gridCellCol.r, gridCellCol.g, gridCellCol.b, gridCellCol.a); rlVertex3f(halfW, lineY, 0);
            
            rlColor4ub(backGridCol.r, backGridCol.g, backGridCol.b, backGridCol.a); rlVertex3f(-halfW, lineY, backZ);
            rlColor4ub(backGridCol.r, backGridCol.g, backGridCol.b, backGridCol.a); rlVertex3f(halfW, lineY, backZ);
        }
        rlEnd();

        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] != 0) {
                    Vector3 localPos = {
                        (float)j - halfW + 0.5f,
                        (float)(BOARD_HEIGHT - i) - 0.5f + currentGridElevation,
                        0.0f
                    };
                    DrawSciFiBlock3D(localPos, pieceColors[board[i][j]], isReflection);
                }
            }
        }

        if (currentState == PLAYING && !isContinuing) {
            int trailIndex = 0;
            for (auto& pt : pieceTrails) {
                float alpha = pt.life / pt.maxLife;
                Color tCol = pieceColors[pt.colorID];
                
                float zOffset = -0.05f - (trailIndex * 0.005f); 
                
                rlPushMatrix();
                float cx = pt.x + pt.shape[0].size()/2.0f - halfW;
                float cy = (float)BOARD_HEIGHT - pt.y - pt.shape.size()/2.0f + currentGridElevation;
                
                rlTranslatef(cx, cy, zOffset);
                rlRotatef(pt.rot, 0, 0, 1);
                rlTranslatef(-cx, -cy, -zOffset);

                for (int i = 0; i < pt.shape.size(); ++i) {
                    for (int j = 0; j < pt.shape[i].size(); ++j) {
                        if (pt.shape[i][j] != 0) {
                            Vector3 tPos = { pt.x + j - halfW + 0.5f, (float)BOARD_HEIGHT - (pt.y + i) - 0.5f + currentGridElevation, zOffset };
                            float s = CUBE_SIZE * (0.6f + 0.4f * alpha); 
                            DrawCube(tPos, s*0.8f, s*0.8f, s*0.8f, ColorAlpha(tCol, 0.15f * alpha));
                            DrawCubeWires(tPos, s, s, s, ColorAlpha(tCol, 0.5f * alpha));
                        }
                    }
                }
                rlPopMatrix();
                trailIndex++;
            }

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
                            DrawSciFiBlock3D(ghostPos, pieceColors[currentColor], isReflection, true); 
                        }
                    }
                }
            }

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
                        bool showBrilliant = currentIsBrilliant || (pieceSpawnAnimTimer > 0.6f);
                        DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection, false, showBrilliant);
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
            rlTranslatef(currentBossPos.x, currentBossPos.y, currentBossPos.z);
            rlRotatef(time * 150.0f, 0, 1, 0); 
            rlRotatef(sin(time*5.0f) * 30.0f, 1, 0, 1);

            int pieceIdx = (bossEncounterCount - 1) % pieces.size();
            auto shape = pieces[pieceIdx].shape;
            
            float offX = shape[0].size() / 2.0f;
            float offY = shape.size() / 2.0f;
            
            for (int i = 0; i < shape.size(); ++i) {
                for (int j = 0; j < shape[i].size(); ++j) {
                    if (shape[i][j] != 0) {
                        float glitchX = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-0.4f, 0.4f) : 0.0f;
                        float glitchY = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-0.4f, 0.4f) : 0.0f;
                        float glitchZ = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-0.4f, 0.4f) : 0.0f;
                        
                        Color virusColor = (GetRandomValue(0, 10) > 8) ? WHITE : C_RED; 
                        
                        Vector3 bPos = {
                            ((float)j - offX + 0.5f) * 2.5f + glitchX, 
                            ((float)-i + offY - 0.5f) * 2.5f + glitchY, 
                            glitchZ
                        };
                        
                        DrawCube(bPos, 2.2f, 2.2f, 2.2f, ColorAlpha(virusColor, 0.8f));
                        DrawCubeWires(bPos, 2.3f, 2.3f, 2.3f, C_ORANGE);
                    }
                }
            }
            
            for (int i = 0; i < 4; i++) {
                float angle = time * 5.0f + (i * PI/2.0f);
                Vector3 sat = { cos(angle)*5.0f, sin(time*10.0f)*3.0f, sin(angle)*5.0f };
                DrawCube(sat, 1.0f, 1.0f, 1.0f, WHITE);
                DrawCubeWires(sat, 1.1f, 1.1f, 1.1f, C_RED);
            }
        rlPopMatrix();
    }

public:
    JogoTetris3D() : currentState(INTRO), rng(random_device{}()) { 
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        
        bgRenderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
        nextPieceRT = LoadRenderTexture(260, 260); 
        holdPieceRT = LoadRenderTexture(260, 260); 
        
        finalRenderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

        SetTextureFilter(bgRenderTarget.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(nextPieceRT.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(holdPieceRT.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(finalRenderTarget.texture, TEXTURE_FILTER_BILINEAR);

        sndMove = LoadSound("src/move.mp3"); sndRotate = LoadSound("src/rotate.mp3"); sndDrop = LoadSound("src/drop.mp3");
        sndClear1 = LoadSound("src/clear1.mp3"); sndClear2 = LoadSound("src/clear2.mp3"); sndClear3 = LoadSound("src/clear3.mp3");
        sndClear4 = LoadSound("src/clear4.mp3"); sndGameOver = LoadSound("src/gameover.mp3");
        
        InitBackgroundPieces();
        ShuffleMusic(); 
        SpawnPiece();
        CheckLocalLicense(); 

        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        hitStopLoc = GetShaderLocation(postProcessShader, "hitStop");
        dmgVignetteLoc = GetShaderLocation(postProcessShader, "damageVignette");
        goldTintLoc = GetShaderLocation(postProcessShader, "goldTint");
        
        float res[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        UnloadRenderTexture(bgRenderTarget); 
        UnloadRenderTexture(nextPieceRT); 
        UnloadRenderTexture(holdPieceRT); 
        UnloadRenderTexture(finalRenderTarget); 
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

        for (int i = pieceTrails.size() - 1; i >= 0; i--) {
            pieceTrails[i].life -= dt;
            if (pieceTrails[i].life <= 0) pieceTrails.erase(pieceTrails.begin() + i);
        }

        for(auto& bp : bgPieces) {
            bp.trail.push_front(bp.position);
            if (bp.trail.size() > 15) bp.trail.pop_back();

            bp.position.x += bp.velocity.x * dt;
            bp.position.y += bp.velocity.y * dt;
            bp.position.z += bp.velocity.z * dt;
            bp.rotation.x += bp.rotVelocity.x * dt;
            bp.rotation.y += bp.rotVelocity.y * dt;
            bp.rotation.z += bp.rotVelocity.z * dt;

            if (bp.position.z > 100.0f) {
                bp.position = GetSafeBgPos();
                bp.position.z = -400.0f; 
                bp.pieceType = GetRandomValue(0, pieces.size() - 1);
                bp.color = pieceColors[pieces[bp.pieceType].colorID];
                bp.trail.clear();
            }
        }

        if (musicEnabled && sndMusic.stream.buffer != NULL) {
            UpdateMusicStream(sndMusic);
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                ShuffleMusic(); 
            }
            musicPulse = Lerp(musicPulse, globalMusicAmplitude * 15.0f, dt * 20.0f);
        } else musicPulse = Lerp(musicPulse, 0.0f, dt * 5.0f);

        if (IsKeyPressed(KEY_U) || (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))) {
            ToggleFullscreen(); isFullscreen = !isFullscreen;
        }

        if (damageVignette > 0.0f) damageVignette = Lerp(damageVignette, 0.0f, dt * 2.0f);
        if (goldTint > 0.0f) goldTint = Lerp(goldTint, 0.0f, dt * 1.5f); 
        
        if (currentRotAngle != 0.0f) currentRotAngle = Lerp(currentRotAngle, 0.0f, dt * 15.0f); 
        if (pieceSpawnAnimTimer > 0.0f) { pieceSpawnAnimTimer -= dt * 2.0f; if (pieceSpawnAnimTimer < 0.0f) pieceSpawnAnimTimer = 0.0f; }
        if (aiPieceSpawnAnimTimer > 0.0f) { aiPieceSpawnAnimTimer -= dt * 2.0f; if (aiPieceSpawnAnimTimer < 0.0f) aiPieceSpawnAnimTimer = 0.0f; }
        
        if (currentState == INTRO) {
            introTimer -= dt;
            if (introTimer <= 0.0f) {
                currentState = hasLocalLicense ? MENU : AUTH; 
                introTimer = 0.0f;
                damageVignette = 0.0f;
                musicPulse = 0.0f;
            } else {
                if (GetRandomValue(0, 100) > 90) damageVignette = GetRandomFloat(0.1f, 0.3f);
                else damageVignette = Lerp(damageVignette, 0.0f, dt * 5.0f);
                musicPulse = abs(sin(introTimer * 10.0f)) * 2.0f;
            }
        }
        else if (currentState == AUTH) {
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
            int maxSel = (currentState == MENU) ? 8 : 5; 

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { (*sel)--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { (*sel)++; TocarSom(sndMove); }
            if (*sel < 0) *sel = maxSel;
            if (*sel > maxSel) *sel = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                TocarSom(sndDrop);
                if (currentState == MENU) {
                    if (*sel == 0) { isClassicMode = true; isExpansiveMode = false; isBossMode = false; isTimeAttackMode = false; isHardcoreMode = false; isDuelMode = false; Restart(); currentState = PLAYING; } 
                    else if (*sel == 1) { isClassicMode = false; isExpansiveMode = true; isBossMode = false; isTimeAttackMode = false; isHardcoreMode = false; isDuelMode = false; Restart(); currentState = PLAYING; } 
                    else if (*sel == 2) { isClassicMode = false; isExpansiveMode = false; isBossMode = true; isTimeAttackMode = false; isHardcoreMode = false; isDuelMode = false; Restart(); currentState = PLAYING; } 
                    else if (*sel == 3) { isClassicMode = false; isExpansiveMode = false; isBossMode = false; isTimeAttackMode = true; isHardcoreMode = false; isDuelMode = false; Restart(); currentState = PLAYING; }
                    else if (*sel == 4) { isClassicMode = false; isExpansiveMode = false; isBossMode = false; isTimeAttackMode = false; isHardcoreMode = true; isDuelMode = false; Restart(); currentState = PLAYING; }
                    else if (*sel == 5) { isClassicMode = false; isExpansiveMode = false; isBossMode = false; isTimeAttackMode = false; isHardcoreMode = false; isDuelMode = true; Restart(); currentState = PLAYING; }
                    else if (*sel == 6) currentState = SETTINGS;
                    else if (*sel == 7) currentState = CREDITS;
                    else if (*sel == 8) confirmExit = true; 
                } else {
                    if (*sel == 0) { 
                        currentResIdx = (currentResIdx + 1) % resList.size();
                        SetWindowSize(resList[currentResIdx].w, resList[currentResIdx].h);
                    }
                    else if (*sel == 1) {
                        currentQualityIdx = (currentQualityIdx + 1) % 4;
                        globalGraphicsQuality = currentQualityIdx;
                        InitBackgroundPieces(); 
                    }
                    else if (*sel == 2) { ToggleFullscreen(); isFullscreen = !isFullscreen; }
                    else if (*sel == 3) sfxEnabled = !sfxEnabled;
                    else if (*sel == 4) { musicEnabled = !musicEnabled; if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic); }
                    else if (*sel == 5) currentState = MENU;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) && currentState == SETTINGS) currentState = MENU;
        }
        else if (currentState == CREDITS) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) { TocarSom(sndDrop); currentState = MENU; }
        }
        else if (currentState == PLAYING) {
            
            if (isContinuing) {
                continueTimer -= dt;
                
                if (IsKeyPressed(KEY_Y)) {
                    isContinuing = false;
                    continues = 3; 
                    for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
                    SpawnPiece();
                    TocarSom(sndClear2);
                } 
                else if (IsKeyPressed(KEY_N) || continueTimer <= 0.0f) {
                    isContinuing = false;
                    gameOver = true; 
                    TocarSom(sndGameOver);
                }
                return; 
            }

            if (IsKeyPressed(KEY_P)) { isPaused = !isPaused; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_ESCAPE)) showExitPrompt = !showExitPrompt; 

            if (IsKeyPressed(KEY_K)) {
                ShuffleMusic(); 
            }
            if (IsKeyPressed(KEY_L)) {
                musicEnabled = !musicEnabled; 
                if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic);
            }

            if (showExitPrompt) {
                if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_Y)) { showExitPrompt = false; currentState = MENU; }
                if (IsKeyPressed(KEY_N)) showExitPrompt = false;
                return; 
            }

            if (gameOver) {
                if (IsKeyPressed(KEY_ENTER)) { Restart(); currentState = MENU; }
                return;
            }

            if (bossActive && !isPaused) {
                bossEntryAnim = Lerp(bossEntryAnim, 1.0f, dt * 2.0f); 
                bossAttackTimer -= dt;
                bossOrbitAngle += dt * 1.5f; 
                
                bossCinematicCooldown -= dt;
                if (bossCinematicCooldown <= 0.0f && bossCinematicSpinTimer <= 0.0f) {
                    bossCinematicSpinTimer = 2.0f; 
                    bossCinematicCooldown = GetRandomFloat(20.0f, 30.0f); 
                    mensagemEspecial = "WARNING!"; timerMensagem = 2.0f;
                }
                if (bossCinematicSpinTimer > 0.0f) bossCinematicSpinTimer -= dt;

                if (GetRandomValue(0, 100) > 92) {
                    cameraShakeTimer = 0.1f;
                    cameraShakeIntensity = 1.5f;
                    damageVignette = GetRandomFloat(0.1f, 0.4f); 
                }

                if (GetRandomValue(0, 100) > 90) { 
                    Particle3D fire;
                    fire.position = currentBossPos;
                    Vector3 target = { GetRandomFloat(-10, 10), currentGridElevation + GetRandomFloat(0, 15), 0 };
                    
                    Vector3 dir;
                    dir.x = target.x - currentBossPos.x;
                    dir.y = target.y - currentBossPos.y;
                    dir.z = target.z - currentBossPos.z;
                    float len = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                    if(len > 0) { dir.x/=len; dir.y/=len; dir.z/=len; } 

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
                
                // LOGICA DO DUEL MODE (A I.A. PENSA AQUI)
                if (isDuelMode && !gameOver && !isContinuing) {
                    for (int i = aiPieceTrails.size() - 1; i >= 0; i--) {
                        aiPieceTrails[i].life -= dt;
                        if (aiPieceTrails[i].life <= 0) aiPieceTrails.erase(aiPieceTrails.begin() + i);
                    }

                    aiMoveTimer -= dt;
                    if (aiMoveTimer <= 0.0f) {
                        aiMoveTimer = fmax(0.05f, 0.25f - (level * 0.015f)); 
                        
                        if (aiTargetRot > 0) {
                            aiCurrentPiece = RotateMatrix(aiCurrentPiece);
                            aiTargetRot--;
                            aiCurrentRotAngle = 90.0f; 
                            TocarSom(sndRotate);
                        } else if (aiCurrentX < aiTargetX) {
                            if (IsValidAIMove(aiCurrentPiece, aiCurrentX + 1, aiCurrentY)) {
                                aiCurrentX++;
                                TocarSom(sndMove);
                            } else {
                                aiTargetX = aiCurrentX; 
                            }
                        } else if (aiCurrentX > aiTargetX) {
                            if (IsValidAIMove(aiCurrentPiece, aiCurrentX - 1, aiCurrentY)) {
                                aiCurrentX--;
                                TocarSom(sndMove);
                            } else {
                                aiTargetX = aiCurrentX; 
                            }
                        } else {
                            int dropDist = 0;
                            int startY = aiCurrentY;
                            while (IsValidAIMove(aiCurrentPiece, aiCurrentX, aiCurrentY + 1)) { aiCurrentY++; dropDist++; }
                            
                            for (int k = 1; k <= dropDist; k++) {
                                aiPieceTrails.push_back({aiCurrentPiece, aiCurrentColor, (float)aiCurrentX, (float)(startY + k), aiCurrentRotAngle, 0.4f - (k * 0.01f), 0.4f});
                            }
                            aiScore += dropDist * 2;
                            SpawnShockwave(GetAIWorldPos(aiCurrentX + aiCurrentPiece[0].size()/2.0f, aiCurrentY + aiCurrentPiece.size()/2.0f), WHITE); 
                            LockAIPiece();
                        }
                    }
                    aiCurrentRotAngle = Lerp(aiCurrentRotAngle, 0.0f, dt * 15.0f); 
                    aiRenderFallY = Lerp(aiRenderFallY, (float)aiCurrentY, dt * 30.0f);
                    aiRenderX = Lerp(aiRenderX, (float)aiCurrentX, dt * 30.0f);
                }

                // COMBO TIMER E EXPLOSÕES INDEPENDENTES DA IA
                if (aiComboTimer > 0.0f) {
                    aiComboTimer -= dt;
                    if (aiComboTimer <= 0.0f) {
                        aiComboTimer = 0.0f;
                        if (aiComboCount > 0) {
                            Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                            Vector3 camUp = Vector3CrossProduct(camRight, camForward);
                            
                            Vector3 spawnPos = camera.position;
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camForward, 25.0f)); 
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camRight, 13.0f)); 
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camUp, 1.0f)); // Acima do player

                            SpawnParticles3D(spawnPos, C_RED, 80, 50.0f, 3);
                            SpawnParticles3D(spawnPos, C_ORANGE, 60, 80.0f, 1);
                            
                            TocarSom(sndDrop); 
                            cameraShakeTimer = 0.5f; 
                            cameraShakeIntensity = 3.5f;
                        }
                        aiComboCount = 0; 
                    }
                }

                if (isTimeAttackMode && !bossActive) {
                    timeAttackTimer -= dt;
                    if (timeAttackTimer <= 0.0f) {
                        timeAttackTimer = 0.0f;
                        gameOver = true;
                        TocarSom(sndGameOver);
                    }
                }

                if (isHardcoreMode && !bossActive && !gameOver) {
                    hardcoreJunkTimer -= dt;
                    if (hardcoreJunkTimer <= 0.0f) {
                        hardcoreJunkTimer = GetRandomFloat(8.0f, 15.0f); 
                        BossAddJunkLine(); 
                        mensagemEspecial = "SYSTEM CORRUPTION!"; timerMensagem = 1.5f;
                        TocarSom(sndClear4); 
                    }
                }

                trailSpawnTimer += dt;
                if (trailSpawnTimer > 0.04f) { 
                    pieceTrails.push_back({currentPiece, currentColor, renderX, renderFallY, currentRotAngle, 0.25f, 0.25f});
                    trailSpawnTimer = 0.0f;
                }

                if (comboTimer > 0.0f) {
                    comboTimer -= dt;
                    if (comboTimer <= 0.0f) {
                        comboTimer = 0.0f;
                        if (comboCount > 0) {
                            Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                            Vector3 camUp = Vector3CrossProduct(camRight, camForward);
                            
                            Vector3 spawnPos = camera.position;
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camForward, 25.0f)); 
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camRight, 13.0f)); 
                            spawnPos = Vector3Add(spawnPos, Vector3Scale(camUp, isDuelMode ? -5.0f : -1.0f)); // Abaixo da IA

                            SpawnParticles3D(spawnPos, C_GOLD, 80, 50.0f, 3);
                            SpawnParticles3D(spawnPos, C_RED, 60, 80.0f, 1);
                            
                            TocarSom(sndDrop); 
                            cameraShakeTimer = 0.5f; 
                            cameraShakeIntensity = 3.5f;
                        }
                        comboCount = 0; 
                    }
                }

                float speed = fmax(0.08f, 0.8f - (level * 0.03f)); 
                fallTimer += dt;

                if ((IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) && bombs > 0) { bombs--; NukeBoard(); }

                if ((IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT)) && canHold) {
                    if (holdPiece.empty()) {
                        holdPiece = currentPiece;
                        holdColor = currentColor;
                        SpawnPiece(); 
                    } else {
                        auto tempP = currentPiece;
                        int tempC = currentColor;
                        currentPiece = holdPiece;
                        currentColor = holdColor;
                        holdPiece = tempP;
                        holdColor = tempC;
                        
                        currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
                        currentY = -5;
                        renderFallY = -5.0f;
                        renderX = currentX;
                    }
                    canHold = false;
                    TocarSom(sndMove);
                    SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 10, 5.0f);
                }

                cameraBankAngle = Lerp(cameraBankAngle, 0.0f, dt * 10.0f); 

                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                    if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveLeftTimer = 0.0f; 
                } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
                    moveLeftTimer += dt;
                    if (moveLeftTimer >= DAS_DELAY) {
                        moveLeftTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); }
                    }
                } else { moveLeftTimer = 0.0f; }

                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                    if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveRightTimer = 0.0f; 
                } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
                    moveRightTimer += dt;
                    if (moveRightTimer >= DAS_DELAY) {
                        moveRightTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); }
                    }
                } else { moveRightTimer = 0.0f; }

                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                    auto rotated = RotateMatrix(currentPiece);
                    if (IsValidMove(rotated, currentX, currentY)) { 
                        currentPiece = rotated; TocarSom(sndRotate); 
                        currentRotAngle = 90.0f; 
                        SpawnParticles3D(GetWorldPos(currentX, currentY), pieceColors[currentColor], 5, 5.0f);
                    } else if (IsValidMove(rotated, currentX - 1, currentY)) { 
                        currentPiece = rotated; currentX--; TocarSom(sndRotate); currentRotAngle = 90.0f;
                    } else if (IsValidMove(rotated, currentX + 1, currentY)) { 
                        currentPiece = rotated; currentX++; TocarSom(sndRotate); currentRotAngle = 90.0f;
                    }
                }
                
                if (IsKeyPressed(KEY_SPACE)) { 
                    int dropDist = 0;
                    int startY = currentY;
                    while (IsValidMove(currentPiece, currentX, currentY + 1)) { currentY++; dropDist++; }
                    
                    for (int k = 1; k <= dropDist; k++) {
                        pieceTrails.push_back({currentPiece, currentColor, (float)currentX, (float)(startY + k), currentRotAngle, 0.4f - (k * 0.01f), 0.4f});
                    }

                    score += dropDist * 2;
                    SpawnShockwave(GetWorldPos(currentX + currentPiece[0].size()/2.0f, currentY + currentPiece.size()/2.0f), WHITE);
                    LockPiece(); fallTimer = 0;
                    cameraShakeTimer = 0.3f; cameraShakeIntensity = fmax(2.0f, dropDist * 0.2f); 
                } 
                else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) { 
                    fallTimer += dt * 30.0f; 
                    if (fallTimer >= speed && IsValidMove(currentPiece, currentX, currentY + 1)) {
                        score += 1; 
                    }
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

        if (gridExpansionTimer > 0.0f) {
            gridExpansionTimer -= dt;
        }

        if (IsKeyPressed(KEY_Y)) {
            manualZoomOffset = 0.0f;
            manualCamAngleX = 0.0f;
            manualCamAngleY = 0.0f;
            manualCamPan.x = 0.0f; 
            manualCamPan.y = 0.0f; 
            manualCamPan.z = 0.0f;
            TocarSom(sndMove); 
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            manualZoomOffset -= wheel * 6.0f; 
        }
        
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            manualCamAngleX -= delta.x * 0.005f;
            manualCamAngleY += delta.y * 0.005f;
            if (manualCamAngleY > 1.4f) manualCamAngleY = 1.4f;
            if (manualCamAngleY < -1.4f) manualCamAngleY = -1.4f;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 delta = GetMouseDelta();
            manualCamPan.x -= delta.x * 0.05f;
            manualCamPan.y += delta.y * 0.05f;
        }

        float zoomMultiplier = 0.6f + ((currentGridWidth - 10) * 0.08f);
        float gridZoom = (currentGridWidth - 10) * zoomMultiplier + manualZoomOffset;
        float finalDist = 34.0f + gridZoom; 
        
        if (isDuelMode) finalDist += 1.5f; // Zoom in perfeito para os Grids maiores na tela

        cameraFovTarget = Lerp(cameraFovTarget, 45.0f, dt * 2.0f); 
        currentGridElevation = Lerp(currentGridElevation, (bossActive || bossEntryAnim > 0.01f) ? Lerp(2.0f, 7.0f, bossEntryAnim) : 2.0f, dt * 3.0f);

        float baseOrbitAngle = (float)GetTime() * 0.2f + nukeSpinAngle; 
        
        float camX = manualCamPan.x - 1.0f + (float)sin(baseOrbitAngle) * 2.0f + (float)sin(manualCamAngleX) * finalDist;
        float camZ = manualCamPan.z + (float)cos(manualCamAngleX) * finalDist;
        float camY = manualCamPan.y + currentGridElevation + 2.5f + (float)sin(manualCamAngleY) * finalDist;

        Vector3 targetPosNormal;
        targetPosNormal.x = camX; targetPosNormal.y = camY; targetPosNormal.z = camZ;
        
        Vector3 targetTargetNormal;
        targetTargetNormal.x = manualCamPan.x; 
        targetTargetNormal.y = currentGridElevation + 13.0f + manualCamPan.y; // Ajustado para descer o grid na tela
        targetTargetNormal.z = manualCamPan.z;

        float bossDist = 20.0f + sin(bossOrbitAngle * 0.45f) * 15.0f + (gridZoom * 0.5f); 
        
        currentBossPos.x = manualCamPan.x + (float)sin(bossOrbitAngle) * bossDist;
        currentBossPos.y = manualCamPan.y + currentGridElevation + Lerp(45.0f, 12.0f + sin(bossOrbitAngle * 0.7f) * 14.0f, bossEntryAnim);
        currentBossPos.z = manualCamPan.z + (float)cos(bossOrbitAngle) * bossDist;

        Vector3 targetPosBoss;
        targetPosBoss.x = targetPosNormal.x; targetPosBoss.y = targetPosNormal.y; targetPosBoss.z = targetPosNormal.z + 5.0f;
        
        Vector3 targetTargetBoss;
        targetTargetBoss.x = manualCamPan.x; 
        targetTargetBoss.y = currentGridElevation + 13.0f + manualCamPan.y;
        targetTargetBoss.z = manualCamPan.z;

        if (bossActive && bossCinematicSpinTimer > 0.0f) {
            float spinProgress = 1.0f - (bossCinematicSpinTimer / 2.0f); 
            float easedSpin = spinProgress * spinProgress * (3.0f - 2.0f * spinProgress); 
            float spinAngle = easedSpin * PI * 2.0f + manualCamAngleX; 
            
            float radius = finalDist + 5.0f; 
            
            targetPosBoss.x = manualCamPan.x + (float)sin(spinAngle) * radius;
            targetPosBoss.y = manualCamPan.y + currentGridElevation + 5.0f + sin(spinProgress*PI)*10.0f + (float)sin(manualCamAngleY) * radius;
            targetPosBoss.z = manualCamPan.z + (float)cos(spinAngle) * radius;
            
            targetTargetBoss.x = manualCamPan.x;
            targetTargetBoss.y = currentGridElevation + 13.0f + manualCamPan.y;
            targetTargetBoss.z = manualCamPan.z;
            
            cameraFovTarget = Lerp(45.0f, 65.0f, sin(spinProgress * PI)); 
        }

        Vector3 finalPosTarget = Vector3Lerp(targetPosNormal, targetPosBoss, bossEntryAnim);
        Vector3 finalLookTarget = Vector3Lerp(targetTargetNormal, targetTargetBoss, bossEntryAnim);

        if (cameraShakeTimer > 0 && !isPaused) {
            float rx = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            float ry = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            finalPosTarget.x += rx; finalPosTarget.y += ry;
            finalLookTarget.x += rx*0.5f; finalLookTarget.y += ry*0.5f;
            cameraShakeTimer -= dt;
            cameraShakeIntensity = Lerp(cameraShakeIntensity, 0.0f, dt * 5.0f); 
        }

        float springTension = 15.0f;
        camera.position = Vector3Lerp(camera.position, finalPosTarget, dt * springTension);
        camera.target = Vector3Lerp(camera.target, finalLookTarget, dt * springTension);
        
        float finalBank = cameraBankAngle;
        
        camera.fovy = Lerp(camera.fovy, cameraFovTarget, dt * 10.0f);
        
        camera.up.x = sin(finalBank * DEG2RAD);
        camera.up.y = cos(finalBank * DEG2RAD);
        camera.up.z = 0.0f;

        if (timerMensagem > 0 && !isPaused) { timerMensagem -= dt; if (timerMensagem <= 0.0f) mensagemEspecial = ""; }

        float timeVal = (float)GetTime();
        SetShaderValue(postProcessShader, timeLoc, &timeVal, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, pulseLoc, &musicPulse, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, hitStopLoc, &hitStopTimer, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, dmgVignetteLoc, &damageVignette, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, goldTintLoc, &goldTint, SHADER_UNIFORM_FLOAT);
    }

    void Draw() {
        BeginTextureMode(nextPieceRT);
            ClearBackground({0, 0, 0, 0}); 
            Camera3D queueCam = { { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE }; 
            BeginMode3D(queueCam);
                rlPushMatrix();
                    rlTranslatef(0.0f, -0.6f, 0.0f); 
                    rlRotatef(25.0f, 1, 0, 0); 
                    rlRotatef((float)GetTime() * 80.0f, 0, 1, 0); 
                    float offX = nextPiece[0].size() / 2.0f;
                    float offY = nextPiece.size() / 2.0f;
                    for (int i = 0; i < nextPiece.size(); ++i) {
                        for (int j = 0; j < nextPiece[i].size(); ++j) {
                            if (nextPiece[i][j] != 0) {
                                DrawSciFiBlock3D({ (float)j - offX + 0.5f, (float)-i + offY - 0.5f, 0.0f }, pieceColors[nextColor], false, false, nextIsBrilliant);
                            }
                        }
                    }
                rlPopMatrix();
            EndMode3D();
        EndTextureMode();

        BeginTextureMode(holdPieceRT);
            ClearBackground({0, 0, 0, 0}); 
            BeginMode3D(queueCam);
                if (!holdPiece.empty()) {
                    rlPushMatrix();
                        rlTranslatef(0.0f, -0.6f, 0.0f); 
                        rlRotatef(25.0f, 1, 0, 0); 
                        rlRotatef((float)GetTime() * 40.0f, 0, 1, 0); 
                        float hOffX = holdPiece[0].size() / 2.0f;
                        float hOffY = holdPiece.size() / 2.0f;
                        for (int i = 0; i < holdPiece.size(); ++i) {
                            for (int j = 0; j < holdPiece[i].size(); ++j) {
                                if (holdPiece[i][j] != 0) {
                                    Color hCol = canHold ? pieceColors[holdColor] : GRAY;
                                    DrawSciFiBlock3D({ (float)j - hOffX + 0.5f, (float)-i + hOffY - 0.5f, 0.0f }, hCol, false);
                                }
                            }
                        }
                    rlPopMatrix();
                }
            EndMode3D();
        EndTextureMode();

        BeginTextureMode(bgRenderTarget);
        if (currentState == INTRO) {
            ClearBackground(BLACK); 
            DrawIntro2D(GetFrameTime());
        } else {
            ClearBackground(C_BG); 
            BeginMode3D(camera);
                DrawProceduralEnvironment(); 
                
                if (isDuelMode) {
                    rlPushMatrix();
                    rlTranslatef(-5.2f, 0.0f, 0.0f);
                    DrawPlayfieldAndPieces(false); 
                    rlPopMatrix();

                    swap(board, aiBoard);
                    swap(currentPiece, aiCurrentPiece);
                    swap(currentX, aiCurrentX);
                    swap(currentY, aiCurrentY);
                    swap(currentColor, aiCurrentColor);
                    swap(renderFallY, aiRenderFallY);
                    swap(renderX, aiRenderX);
                    swap(currentRotAngle, aiCurrentRotAngle);
                    swap(pieceTrails, aiPieceTrails);
                    swap(currentIsBrilliant, aiIsBrilliant); 
                    swap(pieceSpawnAnimTimer, aiPieceSpawnAnimTimer); 

                    rlPushMatrix();
                    rlTranslatef(5.2f, 0.0f, 0.0f);
                    DrawPlayfieldAndPieces(false); 
                    rlPopMatrix();

                    swap(board, aiBoard);
                    swap(currentPiece, aiCurrentPiece);
                    swap(currentX, aiCurrentX);
                    swap(currentY, aiCurrentY);
                    swap(currentColor, aiCurrentColor);
                    swap(renderFallY, aiRenderFallY);
                    swap(renderX, aiRenderX);
                    swap(currentRotAngle, aiCurrentRotAngle);
                    swap(pieceTrails, aiPieceTrails);
                    swap(currentIsBrilliant, aiIsBrilliant); 
                    swap(pieceSpawnAnimTimer, aiPieceSpawnAnimTimer); 
                } else {
                    DrawPlayfieldAndPieces(false);
                }

                DrawBossEncounter();
                UpdateAndDrawParticles3D(GetFrameTime());
            EndMode3D();
            
            DrawFloatingTexts(GetFrameTime());
        }
        EndTextureMode();

        BeginTextureMode(finalRenderTarget);
            ClearBackground(BLACK);
            Rectangle source = { 0, 0, (float)bgRenderTarget.texture.width, -(float)bgRenderTarget.texture.height };
            Rectangle dest = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
            
            BeginShaderMode(postProcessShader);
                DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
            EndShaderMode();

            DrawUI(); 
        EndTextureMode();

        ClearBackground(BLACK);
        
        float scale = fmin((float)GetScreenWidth() / SCREEN_WIDTH, (float)GetScreenHeight() / SCREEN_HEIGHT);
        
        float offsetX = (GetScreenWidth() - ((float)SCREEN_WIDTH * scale)) * 0.5f;
        float offsetY = (GetScreenHeight() - ((float)SCREEN_HEIGHT * scale)) * 0.5f;

        Rectangle finalSource = { 0, 0, (float)finalRenderTarget.texture.width, -(float)finalRenderTarget.texture.height };
        Rectangle finalDest = { offsetX, offsetY, (float)SCREEN_WIDTH * scale, (float)SCREEN_HEIGHT * scale };
        
        DrawTexturePro(finalRenderTarget.texture, finalSource, finalDest, {0, 0}, 0.0f, WHITE);
    }

    void DrawUI() {
        if (currentState == INTRO) return; 

        if (currentState == AUTH) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            
            float t1Size = 10.0f;
            string t1 = "OMEGA RED SECURITY PROTOCOL";
            DrawTetrisText(t1, SCREEN_WIDTH/2 - MeasureTetrisText(t1, t1Size)/2, 200, t1Size, 0.0f, false, C_RED);
            
            float t2Size = 4.8f;
            string t2 = authStatusMsg;
            DrawTetrisText(t2, SCREEN_WIDTH/2 - MeasureTetrisText(t2, t2Size)/2, 350, t2Size, 0.0f, false, checkingOnline ? C_YELLOW : WHITE);

            string displayKey = "";
            for(int i=0; i<16; i++) {
                if (i < charCount) displayKey += currentInputKey[i]; else displayKey += "-";
                if (i == 3 || i == 7 || i == 11) displayKey += " ";
            }

            Color boxColor = checkingOnline ? C_ORANGE : C_GOLD;
            DrawFrameWithCubes2D({(float)SCREEN_WIDTH/2 - 400, 450, 800, 80}, 8.0f, boxColor, ColorAlpha(C_BG, 0.9f));
            
            float kSize = 10.0f;
            DrawTetrisText(displayKey, SCREEN_WIDTH/2 - MeasureTetrisText(displayKey, kSize)/2, 465, kSize, 0.0f, false, WHITE);

            if (checkingOnline) {
                DrawRectangle(SCREEN_WIDTH/2 - 250, 600, 500 * (1.0f - checkTimer/2.0f), 15, C_ORANGE);
                DrawRectangleLines(SCREEN_WIDTH/2 - 250, 600, 500, 15, WHITE);
            }
            
            float copySize = 2.5f;
            string copyStr = "(C) BETTARELLO CODE.";
            DrawTetrisText(copyStr, SCREEN_WIDTH/2 - MeasureTetrisText(copyStr, copySize)/2, SCREEN_HEIGHT - 30, copySize, 0.0f, false, ColorAlpha(GRAY, 0.7f));
        }
        else if (currentState == MENU) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.7f));
            
            float titleBlockSize = 16.0f;
            string title = "TETRABETTA";
            float titleWidth = MeasureTetrisText(title, titleBlockSize);
            float logoStartX = (SCREEN_WIDTH / 2.0f) - (titleWidth / 2.0f);
            DrawTetrisText(title, logoStartX, 120.0f, titleBlockSize, musicPulse, true, WHITE);

            string subtitle = "GOLD EDITION";
            float subWidth = MeasureTetrisText(subtitle, 6.0f);
            DrawTetrisText(subtitle, (SCREEN_WIDTH / 2.0f) - (subWidth / 2.0f), 220.0f, 6.0f, 0.0f, false, C_GOLD);

            const char* menuItems[] = { "CLASSIC RUN", "EXPANSIVE RUN", "BOSS RUSH", "TIME ATTACK", "HARDCORE RUN", "DUEL - AI", "SYSTEM CONFIG", "CREDITS", "LOGOUT" };
            for (int i = 0; i < 9; i++) {
                Color c = (i == menuSelection) ? C_GOLD : GRAY;
                float itemBlockSize = 8.0f;
                
                string text = menuItems[i];
                float baseWidth = MeasureTetrisText(text, itemBlockSize);
                float itemStartX = (SCREEN_WIDTH / 2.0f) - (baseWidth / 2.0f);
                float yPos = 340.0f + i * 50.0f; 
                
                if (i == menuSelection) {
                    float beatScale = 1.0f + (musicPulse * 0.05f);
                    float activeSize = itemBlockSize * beatScale;
                    baseWidth = MeasureTetrisText(text, activeSize);
                    itemStartX = (SCREEN_WIDTH / 2.0f) - (baseWidth / 2.0f);
                    
                    DrawTetrisText("> ", itemStartX - 8 * activeSize, yPos, activeSize, musicPulse, false, C_GOLD);
                    DrawTetrisText(text, itemStartX, yPos, activeSize, musicPulse * 0.5f, false, C_GOLD);
                } else {
                    DrawTetrisText(text, itemStartX, yPos, itemBlockSize, 0.0f, false, c);
                }
            }
            
            float copySize = 2.5f;
            string copyStr = "(C) BETTARELLO CODE.";
            DrawTetrisText(copyStr, SCREEN_WIDTH/2 - MeasureTetrisText(copyStr, copySize)/2, SCREEN_HEIGHT - 30, copySize, 0.0f, false, ColorAlpha(GRAY, 0.7f));
        } 
        else if (currentState == SETTINGS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            
            string title = "SYSTEM CONFIG";
            float titleSize = 10.0f;
            float titleWidth = MeasureTetrisText(title, titleSize);
            DrawTetrisText(title, (SCREEN_WIDTH/2.0f) - (titleWidth/2.0f), 150.0f, titleSize, musicPulse * 0.2f, false, C_CYAN);

            string opt0 = string("RESOLUTION: ") + to_string(resList[currentResIdx].w) + "X" + to_string(resList[currentResIdx].h);
            string opt1 = string("GRAPHICS: ") + qualities[currentQualityIdx];
            string opt2 = string("FULLSCREEN: ") + (isFullscreen ? "ON" : "OFF");
            string opt3 = string("HAPTIC SFX: ") + (sfxEnabled ? "ON" : "OFF");
            string opt4 = string("SYNTHWAVE: ") + (musicEnabled ? "ON" : "OFF");
            string opt5 = "RETURN";

            const char* setItems[] = { opt0.c_str(), opt1.c_str(), opt2.c_str(), opt3.c_str(), opt4.c_str(), opt5.c_str() };
            
            for (int i = 0; i < 6; i++) { 
                Color c = (i == settingsSelection) ? C_ORANGE : GRAY;
                float itemSize = 7.0f;
                
                float baseWidth = MeasureTetrisText(setItems[i], itemSize);
                float itemStartX = (SCREEN_WIDTH / 2.0f) - (baseWidth / 2.0f);
                float yPos = 350.0f + i * 70.0f;

                if (i == settingsSelection) {
                    float beatScale = 1.0f + (musicPulse * 0.05f);
                    float activeSize = itemSize * beatScale;
                    baseWidth = MeasureTetrisText(setItems[i], activeSize);
                    itemStartX = (SCREEN_WIDTH / 2.0f) - (baseWidth / 2.0f);

                    DrawTetrisText("> ", itemStartX - 8 * activeSize, yPos, activeSize, musicPulse, false, C_ORANGE);
                    DrawTetrisText(setItems[i], itemStartX, yPos, activeSize, musicPulse * 0.3f, false, C_ORANGE);
                } else {
                    DrawTetrisText(setItems[i], itemStartX, yPos, itemSize, 0.0f, false, c);
                }
            }
            float copySize = 2.5f;
            string copyStr = "(C) BETTARELLO CODE.";
            DrawTetrisText(copyStr, SCREEN_WIDTH/2 - MeasureTetrisText(copyStr, copySize)/2, SCREEN_HEIGHT - 30, copySize, 0.0f, false, ColorAlpha(GRAY, 0.7f));
        }
        else if (currentState == CREDITS) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.9f));
            
            float size1 = 5.0f;
            float size2 = 8.0f;
            
            string t1 = "PRODUCER AND CODER";
            DrawTetrisText(t1, SCREEN_WIDTH/2 - MeasureTetrisText(t1, size1)/2, 500, size1, 0, false, C_ORANGE);
            
            string t2 = "IGOR BETTARELLO XAVIER";
            DrawTetrisText(t2, SCREEN_WIDTH/2 - MeasureTetrisText(t2, size2)/2, 600, size2, musicPulse*0.2f, true, WHITE); 
            
            float copySize = 2.5f;
            string copyStr = "(C) BETTARELLO CODE.";
            DrawTetrisText(copyStr, SCREEN_WIDTH/2 - MeasureTetrisText(copyStr, copySize)/2, SCREEN_HEIGHT - 30, copySize, 0.0f, false, ColorAlpha(GRAY, 0.7f));
        }
        else if (currentState == PLAYING) {
            // UI ESQUERDA - STATS & HOLD
            DrawFrameWithCubes2D({40, 40, 480, 840}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f));
            
            DrawTetrisText("GOLD EDITION", 60, 60, 2.5f, 0, false, C_GOLD);
            DrawTetrisText(TextFormat("SCORE: %08d", score), 60, 100, 6.0f, 0, false, WHITE);
            DrawTetrisText(TextFormat("LEVEL: %02d", level), 60, 160, 5.0f, 0, false, C_ORANGE);
            
            if (!isDuelMode) {
                DrawTetrisText(TextFormat("LIVES: %d", continues), 60, 210, 4.0f, 0, false, C_RED); 
            } else {
                DrawTetrisText("MODE: DUEL", 60, 210, 4.0f, 0, false, C_CYAN);
            }
            
            int textStartY = 260; 
            
            if (isExpansiveMode) {
                DrawTetrisText("MAGIC CHARGE:", 60, 260, 3.0f, 0, false, WHITE);
                DrawRectangleLines(60, 290, 300, 20, C_YELLOW);
                DrawRectangle(60, 290, 300 * (stars/10.0f), 20, ColorAlpha(C_YELLOW, 0.7f + sin(GetTime()*10)*0.3f));
                textStartY = 340;
            } else if (isTimeAttackMode) {
                int min = (int)timeAttackTimer / 60;
                int sec = (int)timeAttackTimer % 60;
                DrawTetrisText("TIME REMAINING:", 60, 260, 3.0f, 0, false, WHITE);
                DrawTetrisText(TextFormat("%02d:%02d", min, sec), 60, 290, 6.0f, musicPulse*0.2f, false, timeAttackTimer < 30.0f ? C_RED : C_CYAN);
                textStartY = 340;
            } else if (isHardcoreMode) {
                DrawTetrisText("SPEED: MAX", 60, 260, 4.0f, musicPulse*0.2f, false, C_RED);
                textStartY = 310;
            }

            // HOLD UI
            string hText = "HOLD PIECE";
            DrawTetrisText(hText, 60, textStartY, 3.5f, 0, false, canHold ? C_GOLD : GRAY);
            DrawFrameWithCubes2D({60, (float)textStartY + 20, 200, 200}, 4.0f, canHold ? C_GOLD : GRAY, ColorAlpha(BLACK, 0.5f));
            Rectangle sourceHRT = { 0, 0, (float)holdPieceRT.texture.width, -(float)holdPieceRT.texture.height };
            Rectangle destHRT = { 30.0f, (float)textStartY - 10, 260.0f, 260.0f };
            if (!gameOver && !isPaused && !isContinuing) {
                DrawTexturePro(holdPieceRT.texture, sourceHRT, destHRT, {0, 0}, 0.0f, WHITE);
            }
            
            textStartY += 240;

            float instSize = 2.0f;
            float lineH = 30.0f;
            DrawTetrisText(TextFormat("[CTRL] PURGE NUKE: %d", bombs), 60, textStartY, instSize, 0, false, C_RED); 
            DrawTetrisText("[C/SHIFT] HOLD PIECE", 60, textStartY + lineH, instSize, 0, false, C_GOLD); 
            DrawTetrisText("[SPACE] HARD DROP", 60, textStartY + lineH*2, instSize, 0, false, WHITE); 
            DrawTetrisText("[Y] RESET CAMERA", 60, textStartY + lineH*3, instSize, 0, false, C_CYAN); 
            DrawTetrisText("[RMB] ROTATE CAM", 60, textStartY + lineH*4, instSize, 0, false, C_CYAN); 
            DrawTetrisText("[WHEEL] ZOOM CAM", 60, textStartY + lineH*5, instSize, 0, false, C_CYAN); 
            DrawTetrisText("[K] SKIP TRACK", 60, textStartY + lineH*6, instSize, 0, false, GRAY); 
            DrawTetrisText(TextFormat("[L] MUSIC: %s", musicEnabled ? "ON" : "MUTED"), 60, textStartY + lineH*7, instSize, 0, false, GRAY); 

            if (bossActive && isBossMode) {
                float glitchX = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;
                float glitchY = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;

                DrawFrameWithCubes2D({(float)SCREEN_WIDTH/2 - 400 + glitchX, 40 + glitchY, 800, 80}, 8.0f, C_RED, ColorAlpha(C_BG, 0.7f));
                
                string bName = TextFormat("ANOMALY: OMEGARED V.%d", bossEncounterCount);
                float bSize = 5.0f;
                DrawTetrisText(bName, SCREEN_WIDTH/2 - MeasureTetrisText(bName, bSize)/2 + glitchX, 50 + glitchY, bSize, 0, false, C_RED);
                
                float hpRatio = (float)bossHp / (10.0f + (bossEncounterCount * 5.0f)); 
                DrawRectangleRounded({SCREEN_WIDTH/2 - 380 + glitchX, 85 + glitchY, 760 * hpRatio, 20}, 0.5f, 8, C_ORANGE);
            }

            // UI DIREITA - NEXT QUEUE & AI SCORE
            DrawFrameWithCubes2D({SCREEN_WIDTH - 360.0f, 40.0f, 320.0f, 264.0f}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f));
            
            string qText = "NEXT QUEUE";
            DrawTetrisText(qText, SCREEN_WIDTH - 200 - MeasureTetrisText(qText, 3.5f)/2, 60, 3.5f, 0, false, C_GOLD);

            Rectangle sourceRT = { 0, 0, (float)nextPieceRT.texture.width, -(float)nextPieceRT.texture.height };
            Rectangle destRT = { SCREEN_WIDTH - 330.0f, 40.0f, 260.0f, 260.0f };
            if (!gameOver && !isPaused && !isContinuing) {
                DrawTexturePro(nextPieceRT.texture, sourceRT, destRT, {0, 0}, 0.0f, WHITE);
            }

            if (isDuelMode) {
                DrawFrameWithCubes2D({SCREEN_WIDTH - 360.0f, 310.0f, 320.0f, 120.0f}, 8.0f, ColorAlpha(C_RED, 0.5f), ColorAlpha(C_BG, 0.6f));
                string aiText = "AI SCORE";
                DrawTetrisText(aiText, SCREEN_WIDTH - 200 - MeasureTetrisText(aiText, 3.5f)/2, 330.0f, 3.5f, 0, false, C_RED);
                DrawTetrisText(TextFormat("%08d", aiScore), SCREEN_WIDTH - 200 - MeasureTetrisText("00000000", 5.0f)/2, 365.0f, 5.0f, 0, false, WHITE);
                DrawTetrisText(TextFormat("AI NUKES: %d", aiBombs), SCREEN_WIDTH - 200 - MeasureTetrisText("AI NUKES: 2", 3.0f)/2, 400.0f, 3.0f, 0, false, C_ORANGE);
            }
            
            // RENDERS DE COMBOS E MENSAGENS ISOLADOS E EMPILHADOS NO LADO DIREITO:

            // 1. MENSAGENS DO JOGADOR
            if (comboCount > 0 && comboTimer > 0.0f && !isContinuing) {
                float popScale = 1.0f + (musicPulse * 0.05f);
                float comboSize = 6.5f * popScale; 
                string cMult = TextFormat("COMBO X%d", comboCount);
                float cWidth = MeasureTetrisText(cMult, comboSize);
                
                float rightPanelCenter = isDuelMode ? (SCREEN_WIDTH - 200.0f) : (SCREEN_WIDTH - 240.0f); 
                float comboYPosition = isDuelMode ? 740.0f : 450.0f; // Bem abaixo da IA no Duel Mode

                DrawTetrisTextGlowing(cMult, rightPanelCenter - cWidth/2.0f, comboYPosition, comboSize, musicPulse * 0.2f);
                
                string tVal = TextFormat("%.2f", comboTimer);
                float tSize = 8.5f * popScale; 
                float tPulse = (comboTimer <= 2.0f) ? abs(sin((float)GetTime() * 15.0f)) * 0.8f : 0.0f;
                float tWidth = MeasureTetrisText(tVal, tSize);

                if (comboTimer <= 2.0f) {
                    DrawTetrisText(tVal, rightPanelCenter - tWidth/2.0f, comboYPosition + 65.0f, tSize, tPulse, false, C_RED);
                } else {
                    DrawTetrisTextGlowing(tVal, rightPanelCenter - tWidth/2.0f, comboYPosition + 65.0f, tSize, tPulse);
                }
            }

            if (timerMensagem > 0 && !isContinuing) {
                float popScale = ElasticEaseOut(1.0f - (timerMensagem / 3.0f));
                float fontSize = 9.5f * popScale; 
                float textWidth = MeasureTetrisText(mensagemEspecial, fontSize);
                
                float rightPanelCenter = isDuelMode ? (SCREEN_WIDTH - 200.0f) : (SCREEN_WIDTH - 240.0f); 
                float msgYPosition = isDuelMode ? 860.0f : 580.0f; 

                DrawTetrisTextGlowing(mensagemEspecial, rightPanelCenter - textWidth/2.0f, msgYPosition, fontSize, 0.0f);
            }

            // 2. MENSAGENS DA INTELIGÊNCIA ARTIFICIAL
            if (isDuelMode) {
                if (aiComboCount > 0 && aiComboTimer > 0.0f && !isContinuing) {
                    float popScale = 1.0f + (musicPulse * 0.05f);
                    float comboSize = 6.5f * popScale; 
                    string cMult = TextFormat("COMBO X%d", aiComboCount);
                    float cWidth = MeasureTetrisText(cMult, comboSize);
                    
                    float panelCenter = SCREEN_WIDTH - 200.0f; 
                    float comboYPosition = 490.0f; 

                    DrawTetrisTextGlowing(cMult, panelCenter - cWidth/2.0f, comboYPosition, comboSize, musicPulse * 0.2f);
                    
                    string tVal = TextFormat("%.2f", aiComboTimer);
                    float tSize = 8.5f * popScale; 
                    float tPulse = (aiComboTimer <= 2.0f) ? abs(sin((float)GetTime() * 15.0f)) * 0.8f : 0.0f;
                    float tWidth = MeasureTetrisText(tVal, tSize);

                    if (aiComboTimer <= 2.0f) {
                        DrawTetrisText(tVal, panelCenter - tWidth/2.0f, comboYPosition + 65.0f, tSize, tPulse, false, C_RED);
                    } else {
                        DrawTetrisTextGlowing(tVal, panelCenter - tWidth/2.0f, comboYPosition + 65.0f, tSize, tPulse);
                    }
                }

                if (aiTimerMensagem > 0 && !isContinuing) {
                    float popScale = ElasticEaseOut(1.0f - (aiTimerMensagem / 3.0f));
                    float fontSize = 9.5f * popScale; 
                    float textWidth = MeasureTetrisText(aiMensagemEspecial, fontSize);
                    
                    float panelCenter = SCREEN_WIDTH - 200.0f; 
                    float msgYPosition = 610.0f; 

                    DrawTetrisTextGlowing(aiMensagemEspecial, panelCenter - textWidth/2.0f, msgYPosition, fontSize, 0.0f);
                }
            }

            if (isPaused && !isContinuing) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
                string pMsg = "SYSTEM PAUSED";
                DrawTetrisText(pMsg, SCREEN_WIDTH/2 - MeasureTetrisText(pMsg, 10.0f)/2, SCREEN_HEIGHT/2 - 30, 10.0f, 0, false, C_GOLD);
            }

            if (isContinuing) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
                
                string cMsg = "CONTINUE?";
                DrawTetrisText(cMsg, SCREEN_WIDTH/2 - MeasureTetrisText(cMsg, 12.0f)/2, SCREEN_HEIGHT/2 - 140, 12.0f, 0, false, C_ORANGE);
                
                int timeInt = (int)ceil(continueTimer);
                if (timeInt < 0) timeInt = 0;
                string tMsg = to_string(timeInt);
                float numberSize = 25.0f;
                DrawTetrisText(tMsg, SCREEN_WIDTH/2 - MeasureTetrisText(tMsg, numberSize)/2, SCREEN_HEIGHT/2 - 20, numberSize, musicPulse*0.3f, false, C_RED);

                string optMsg = "YES [Y]   /   NO [N]";
                DrawTetrisText(optMsg, SCREEN_WIDTH/2 - MeasureTetrisText(optMsg, 6.0f)/2, SCREEN_HEIGHT/2 + 180, 6.0f, 0, false, WHITE);
            }

            if (gameOver) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(C_RED, 0.4f)); 
                DrawRectangle(0, SCREEN_HEIGHT/2 - 100, SCREEN_WIDTH, 200, ColorAlpha(BLACK, 0.9f));
                
                if (isDuelMode) {
                    string winner = "";
                    if (aiDead && continues > 0) winner = "PLAYER WINS!";
                    else if (continues <= 0 && !aiDead) winner = "AI WINS!";
                    else winner = (score >= aiScore) ? "PLAYER WINS!" : "AI WINS!"; 
                    
                    Color cColor = (winner == "PLAYER WINS!") ? C_GOLD : C_RED;
                    DrawTetrisText(winner, SCREEN_WIDTH/2 - MeasureTetrisText(winner, 12.0f)/2, SCREEN_HEIGHT/2 - 80, 12.0f, musicPulse*0.2f, false, cColor);
                } else {
                    string failMsg = "CRITICAL FAILURE";
                    DrawTetrisText(failMsg, SCREEN_WIDTH/2 - MeasureTetrisText(failMsg, 12.0f)/2, SCREEN_HEIGHT/2 - 80, 12.0f, 0, false, C_RED);
                }

                string rebMsg = "PRESS [ENTER] TO REBOOT";
                DrawTetrisText(rebMsg, SCREEN_WIDTH/2 - MeasureTetrisText(rebMsg, 4.0f)/2, SCREEN_HEIGHT/2 + 40, 4.0f, 0, false, WHITE);
            }

            if (showExitPrompt) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.95f));
                string extMsg = "ABORT SIMULATION?";
                DrawTetrisText(extMsg, SCREEN_WIDTH/2 - MeasureTetrisText(extMsg, 10.0f)/2, SCREEN_HEIGHT/2 - 60, 10.0f, 0, false, C_ORANGE);
                string yMsg = "[Y] YES - [N] NO";
                DrawTetrisText(yMsg, SCREEN_WIDTH/2 - MeasureTetrisText(yMsg, 6.0f)/2, SCREEN_HEIGHT/2 + 40, 6.0f, 0, false, WHITE);
            }
            
            float copySize = 2.5f;
            string copyStr = "(C) BETTARELLO CODE.";
            DrawTetrisText(copyStr, 60.0f, SCREEN_HEIGHT - 40, copySize, 0.0f, false, ColorAlpha(GRAY, 0.7f));
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0; level = 1; continues = 3; bombs = 2; stars = 0; 
        
        currentGridWidth = isExpansiveMode ? 10 : 14; 
        
        if (isHardcoreMode) { level = 20; hardcoreJunkTimer = 12.0f; } 
        if (isTimeAttackMode) timeAttackTimer = 180.0f; 
        
        if (isDuelMode) {
            currentGridWidth = 10; 
            for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) aiBoard[i][j] = 0;
            aiScore = 0;
            aiDead = false; 
            aiBombs = 2; // IA recebe as bombas dela
            aiComboCount = 0;
            aiComboTimer = 0.0f;
            aiTimerMensagem = 0.0f;
            aiMensagemEspecial = "";
            aiPieceTrails.clear();
            aiNextPiece.clear();
            SpawnAIPiece();
        }

        nextIsBrilliant = false; currentIsBrilliant = false; linesClearedTotal = 0;
        
        comboCount = 0; 
        comboTimer = 0.0f; 
        
        isContinuing = false;
        continueTimer = 0.0f;
        
        holdPiece.clear(); canHold = true;
        pieceTrails.clear(); 
        pieceSpawnAnimTimer = 0.0f; 
        aiPieceSpawnAnimTimer = 0.0f;

        manualZoomOffset = 0.0f; 
        manualCamAngleX = 0.0f; 
        manualCamAngleY = 0.0f; 
        manualCamPan.x = 0.0f; manualCamPan.y = 0.0f; manualCamPan.z = 0.0f;
        
        gameOver = false; isPaused = false; timerMensagem = 0; mensagemEspecial = ""; 
        gridExpansionTimer = 0.0f; currentGridElevation = 2.0f; 
        moveLeftTimer = 0.0f; moveRightTimer = 0.0f; nukeSpinAngle = 0.0f;
        hitStopTimer = 0.0f; damageVignette = 0.0f; goldTint = 0.0f;
        
        bossActive = false; bossEntryAnim = 0.0f; linesUntilBoss = 15; bossEncounterCount = 0; 
        currentBossAttackDelay = 15.0f; bossOrbitAngle = 0.0f; 
        bossCinematicSpinTimer = 0.0f; bossCinematicCooldown = 15.0f;
        
        particles.clear(); floatingTexts.clear();
        SpawnPiece();
        camera.position = defaultCamPos; camera.target = defaultCamTarget;
        
        ShuffleMusic(); 
    }

    bool ShouldExit() { return confirmExit; }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TeTRABeTTA - GOLD EDITION");
    SetExitKey(KEY_NULL); 
    ToggleFullscreen();
    InitAudioDevice(); 
    SetTargetFPS(60);
    HideCursor(); 

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