#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <deque>

using namespace std;

// Macros para resoluções dinâmicas nativas (Ultrawide)
#define SW (float)GetScreenWidth()
#define SH (float)GetScreenHeight()

// =====================================================================
// SHADERS DE PÓS-PROCESSAMENTO AAA (COMPATÍVEL COM ANDROID / OPENGL ES)
// =====================================================================
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)

const char* vertexShaderCode = R"(
#version 100
precision mediump float;
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;
varying vec2 fragTexCoord;
varying vec4 fragColor;
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
#version 100
precision mediump float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float time;
uniform float pulse;
uniform float damageVignette;

float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;
    
    vec2 crtUV = uv - 0.5;
    float rsq = crtUV.x*crtUV.x + crtUV.y*crtUV.y;
    crtUV += crtUV * (rsq * 0.10); 
    crtUV += 0.5;
    
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float shiftAmount = 0.0008 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    float r = texture2D(texture0, crtUV + shift).r;
    float g = texture2D(texture0, crtUV).g;
    float b = texture2D(texture0, crtUV - shift).b;
    vec4 baseColor = vec4(r, g, b, 1.0);

    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.2; 
    
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.25) * 0.25;
    
    float scanline = sin(crtUV.y * resolution.y * 2.0) * 0.025;
    float grain = (rand(crtUV * time) - 0.5) * 0.02;
    
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.95, 0.4, dist);
    
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.5) * (1.0 - scanline) + grain;
    
    finalRGB = mix(vec3(0.5), finalRGB, 1.15); 
    float luma = dot(finalRGB, vec3(0.299, 0.587, 0.114));
    finalRGB = mix(vec3(luma), finalRGB, 1.4); 
    
    finalRGB *= vignette;
    
    if (damageVignette > 0.0) {
        finalRGB += vec3(0.8, 0.0, 0.0) * damageVignette * smoothstep(0.3, 0.8, dist);
    }

    gl_FragColor = vec4(finalRGB, 1.0) * fragColor;
}
)";

#else

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

float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;
    
    vec2 crtUV = uv - 0.5;
    float rsq = crtUV.x*crtUV.x + crtUV.y*crtUV.y;
    crtUV += crtUV * (rsq * 0.10); 
    crtUV += 0.5;
    
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float shiftAmount = 0.0008 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    float r = texture(texture0, crtUV + shift).r;
    float g = texture(texture0, crtUV).g;
    float b = texture(texture0, crtUV - shift).b;
    vec4 baseColor = vec4(r, g, b, 1.0);

    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.2; 
    
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.25) * 0.25;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.25) * 0.25;
    
    float scanline = sin(crtUV.y * resolution.y * 2.0) * 0.025;
    float grain = (rand(crtUV * time) - 0.5) * 0.02;
    
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.95, 0.4, dist);
    
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.5) * (1.0 - scanline) + grain;
    
    finalRGB = mix(vec3(0.5), finalRGB, 1.15); 
    float luma = dot(finalRGB, vec3(0.299, 0.587, 0.114));
    finalRGB = mix(vec3(luma), finalRGB, 1.4); 
    
    finalRGB *= vignette;
    
    if (damageVignette > 0.0) {
        finalRGB += vec3(0.8, 0.0, 0.0) * damageVignette * smoothstep(0.3, 0.8, dist);
    }

    finalColor = vec4(finalRGB, 1.0) * fragColor;
}
)";

#endif

// =====================================================================
// CONSTANTES E CONFIGURAÇÕES DO MUNDO 3D E GRÁFICOS
// =====================================================================
const int MAX_BOARD_WIDTH = 50;  
const int BOARD_HEIGHT = 22;     
const float CUBE_SIZE = 0.95f;

// 0: LOW, 1: MED, 2: HIGH, 3: ULTRA
int graphicsQuality = 3; 

// Cores levemente ajustadas para evitar que o Bloom sature e apague o núcleo transparente
const Color C_CYAN   = { 0, 220, 255, 255 };
const Color C_BLUE   = { 0, 100, 255, 255 }; 
const Color C_ORANGE = { 255, 120, 0, 255 };
const Color C_YELLOW = { 240, 240, 0, 255 };
const Color C_GREEN  = { 0, 230, 0, 255 }; 
const Color C_PURPLE = { 180, 0, 255, 255 };
const Color C_RED    = { 255, 10, 50, 255 };
const Color C_MAGENTA= { 255, 0, 255, 255 };
const Color C_BG     = { 5, 8, 15, 255 }; 

// Cores corrigidas para maior estabilidade visual (Removido BLANK problemático)
Color pieceColors[15] = { {0,0,0,0}, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, C_MAGENTA, {255, 255, 255, 255}, {50, 255, 50, 255}, {255, 105, 180, 255}, {255, 215, 0, 255}, {238, 130, 238, 255}, {100, 100, 100, 255} };

// =====================================================================
// SISTEMAS AAA: Easing, Springs e Áudio
// =====================================================================
enum GameState { INTRO, MENU, SETTINGS, CREDITS, PLAYING }; 
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

float GetRandomFloat(float min, float max) { 
    return min + (max - min) * ((float)GetRandomValue(0, 10000) / 10000.0f); 
}

// =====================================================================
// SISTEMA DE PARTÍCULAS AVANÇADO (OTIMIZADO COM GRAPHICS QUALITY)
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

void SpawnFloatingText(Vector3 pos, string text, Color color, float scale = 1.0f) {
    floatingTexts.push_back({pos, text, 1.5f, color, scale});
}

void SpawnParticles3D(Vector3 pos, Color color, int amount, float force) {
    // Aplica o filtro de qualidade gráfica nas partículas gerais
    if (graphicsQuality == 0) amount = amount / 4;
    else if (graphicsQuality == 1) amount = amount / 2;
    else if (graphicsQuality == 2) amount = (int)(amount * 0.8f);
    
    if (amount <= 0 && graphicsQuality > 0) amount = 1;
    if (amount <= 0) return;

    for (int i = 0; i < amount; i++) {
        Particle3D p;
        p.position = pos;
        p.velocity = { GetRandomFloat(-force, force), GetRandomFloat(force * 0.2f, force * 1.5f), GetRandomFloat(-force, force) };
        p.color = color;
        p.maxLife = GetRandomFloat(0.5f, 1.5f);
        p.life = p.maxLife;
        p.isSpark = (GetRandomValue(0, 10) > 4);
        p.size = p.isSpark ? GetRandomFloat(0.1f, 0.4f) : GetRandomFloat(0.3f, 0.6f);
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

        if (particles[i].life <= 0 || particles[i].position.y < -5.0f) {
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
                DrawCube({0,0,0}, s, s, s, ColorAlpha(fadeColor, 0.8f));
                rlPopMatrix();
            }
        }
    }
}

// =====================================================================
// LÓGICA DAS PEÇAS E MATRIZ (ANTI-PLÁGIO)
// =====================================================================
struct Tetromino { vector<vector<int>> shape; int colorID; };

vector<Tetromino> pieces = {
    // 4 BLOCOS CLÁSSICOS (Apenas as peças padrão)
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
    if (n == 0) return mat;
    int m = mat[0].size();
    
    vector<vector<int>> res(m, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) { res[j][n - 1 - i] = mat[i][j]; }
    }
    return res;
}

// =====================================================================
// ENGINE PRINCIPAL DO JOGO AAA
// =====================================================================
class JogoTetris3D {
private:
    GameState currentState = INTRO; 
    int menuSelection = 0;
    int settingsSelection = 0;

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int continues = 3; 
    int bombs = 2; 
    int stars = 0; 
    int currentGridWidth = 14; 
    int linesClearedTotal = 0;
    int comboCount = 0; 
    
    bool gameOver = false;
    bool isPaused = false; 
    bool isClassicMode = false; 
    bool isExpansiveMode = true; 
    bool isBossMode = false;   
    
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
    
    const float currentGridElevation = 2.0f; 
    Camera3D camera = { 0 };
    
    // CÂMARA LEVANTADA E A OLHAR MAIS PARA BAIXO PARA NÃO COBRIR O TEXTO
    Vector3 defaultCamPos = { 0.0f, 5.5f, 34.0f }; 
    Vector3 defaultCamTarget = { 0.0f, 13.5f, 0.0f };
    
    float cameraShakeTimer = 0.0f;
    float cameraShakeIntensity = 0.0f;

    string mensagemEspecial = "";
    float timerMensagem = 0.0f;
    float hitStopTimer = 0.0f;
    float damageVignette = 0.0f;
    float musicPulse = 0.0f;
    
    // Novas variáveis da Pirâmide Boss
    float bossOrbitAngle = 0.0f; 
    Vector3 currentBossPos = {0,0,0}; 
    bool isBossStrobing = false;
    float bossStrobeTimer = 0.0f;
    float bossRushCooldown = 15.0f;
    
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
    
    RenderTexture2D bgRenderTarget; 
    RenderTexture2D nextPieceRT; 

    Sound sndMove, sndRotate, sndDrop, sndClear1, sndClear2, sndClear3, sndClear4, sndGameOver;
    Music sndMusic = { 0 };
    int currentMusicTrack = 1; 

    Shader postProcessShader;
    int resLoc, timeLoc, pulseLoc, hitStopLoc, dmgVignetteLoc;

    // Variáveis Touch
    bool btnLeftDown = false, btnRightDown = false, btnDownDown = false;
    bool btnLeftPressed = false, btnRightPressed = false, btnDownPressed = false;
    bool btnMenuUpPressed = false, btnRotatePressed = false, btnDropPressed = false, btnPausePressed = false;
    bool btnNukePressed = false, btnYesPressed = false, btnNoPressed = false;
    bool btnMusicPressed = false, btnMenuPressed = false, btnShufflePressed = false;
    bool prevTouch[10] = {false}; 

    // Retângulos Dinâmicos para o UI
    Rectangle recBtnLeft, recBtnRight, recBtnDown, recBtnMenuUp, recBtnMenu, recBtnShuffle;
    Rectangle recBtnDrop, recBtnRotate, recBtnNuke, recBtnPause, recBtnMusic;
    Rectangle recBtnYes, recBtnNo;

    void UpdateRectangles() {
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();

        // Cruz do lado Esquerdo (Centro em X=240, Y=sh-360)
        float cx = 240.0f;
        float cy = sh - 340.0f;
        
        recBtnLeft   = { cx - 180, cy, 160, 160 }; // Esquerda
        recBtnRight  = { cx + 180, cy, 160, 160 }; // Direita
        recBtnDown   = { cx, cy + 180, 160, 160 }; // Baixo (V)
        recBtnMenuUp = { cx, cy - 180, 160, 160 }; // Cima (^)

        // Menu abaixo do Score
        recBtnMenu   = { 40, 360, 200, 100 };
        recBtnShuffle= { 260, 360, 200, 100 };

        // Ações Lado Direito Perfeitas!
        recBtnDrop   = { sw - 560, sh - 340, 200, 200 }; // DROP na esquerda da ação
        recBtnRotate = { sw - 280, sh - 260, 200, 200 }; // ROTATE encostado à direita inferior
        recBtnNuke   = { sw - 360, sh - 600, 160, 160 }; // BMB (Acima do Rotate)
        
        // Pause e Música (Abaixo da caixa Next Piece - Fora do caminho!)
        recBtnPause  = { sw - 180, 400, 140, 140 }; 
        recBtnMusic  = { sw - 180, 560, 140, 140 };

        recBtnYes    = { sw/2.0f - 350, sh/2.0f + 100, 300, 150 };
        recBtnNo     = { sw/2.0f + 50,  sh/2.0f + 100, 300, 150 };
    }

    // =====================================================================
    // SISTEMA DE FONTES EM BLOCOS DE TETRIS
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
            if (c == ' ') { currentX += 3.0f * blockSize; continue; }

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
                case '<': letter = {"001", "010", "100", "010", "001"}; break;
                case '^': letter = {"010", "101", "000", "000", "000"}; break; 
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

            Color baseColor = useRainbow ? pieceColors[(k % 14) + 1] : flatColor; 
            
            float jumpY = (useRainbow || c == '>') ? sin(time * 8.0f + k * 0.5f) * (pulse * 3.0f) : 0.0f; 
            float glitchX = (useRainbow && GetRandomValue(0, 100) > 95) ? GetRandomFloat(-pulse, pulse) * 3.0f : 0.0f;

            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 3; j++) {
                    if (letter[i][j] == '1') {
                        float bx = currentX + j * blockSize + glitchX;
                        float by = startY + i * blockSize + jumpY;
                        
                        float innerSize = blockSize * 0.85f; 
                        float beatScale = 1.0f + (pulse * 0.06f); 
                        
                        DrawRectangle(bx + (blockSize*0.25f), by + (blockSize*0.25f), innerSize * beatScale, innerSize * beatScale, ColorAlpha(BLACK, 0.6f));
                        DrawRectangle(bx, by, innerSize * beatScale, innerSize * beatScale, baseColor);
                        DrawRectangle(bx + (blockSize*0.1f), by + (blockSize*0.1f), innerSize * beatScale - (blockSize*0.25f), innerSize * beatScale - (blockSize*0.25f), ColorAlpha(WHITE, 0.3f));
                        DrawRectangleLines(bx, by, innerSize * beatScale, innerSize * beatScale, ColorAlpha(WHITE, 0.7f));
                    }
                }
            }
            currentX += 4.0f * blockSize; 
        }
    }

    void DrawCenteredTextInRect(string text, Rectangle rec, float size, Color c) {
        float tw = MeasureTetrisText(text, size);
        float th = 5.0f * size;
        float px = rec.x + (rec.width - tw) / 2.0f;
        float py = rec.y + (rec.height - th) / 2.0f;
        DrawTetrisText(text, px, py, size, 0.0f, false, c);
    }

    void DrawCube2D(Vector2 pos, float size, Color col) {
        float shadowOff = size * 0.15f;
        DrawRectangle(pos.x + shadowOff, pos.y + shadowOff, size, size, ColorAlpha(BLACK, 0.6f));
        DrawRectangle(pos.x, pos.y, size, size, col);
        DrawRectangle(pos.x + size*0.1f, pos.y + size*0.1f, size*0.8f, size*0.8f, ColorAlpha(WHITE, 0.2f));
        DrawRectangleLines(pos.x, pos.y, size, size, ColorAlpha(WHITE, 0.5f));
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
        
        Color mainCol = ColorAlpha(C_CYAN, alpha);
        Color secCol = ColorAlpha(C_ORANGE, alpha);
        Color whiteCol = ColorAlpha(WHITE, alpha);
        Color glitchCol = ColorAlpha(C_RED, alpha * 0.5f);
        
        float cx = (float)GetScreenWidth() / 2.0f;
        float cy = (float)GetScreenHeight() / 2.0f;
        
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
        DrawRectangle(cx - barWidth/2, cy, barWidth, 2, ColorAlpha(C_CYAN, alpha * 0.5f));
        DrawRectangle(cx - barWidth/2, cy + 140, barWidth, 2, ColorAlpha(C_CYAN, alpha * 0.5f));
        
        // Mantivemos o logo original apenas na INTRO!
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        float copySize = 2.5f;
        string copyStr = "(C) BETTARELLO CODE.";
        DrawTetrisText(copyStr, sw/2 - MeasureTetrisText(copyStr, copySize)/2, sh - 30, copySize, 0.0f, false, ColorAlpha(GRAY, alpha * 0.7f));
    }

    int GetRandomPiece() { 
        return GetRandomValue(0, (int)pieces.size() - 1); 
    }

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
        
        sndMusic = LoadMusicStream(fileName.c_str());
        
        if (sndMusic.stream.buffer != NULL) {
            PlayMusicStream(sndMusic);
            AttachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
            musicEnabled = true;
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
        currentRotAngle = 0.0f;

        currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
        currentY = -5; 
        renderFallY = -5.0f; 
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
                currentY = -5; renderFallY = -5.0f; renderX = currentX;
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
        cameraShakeTimer = 0.15f; cameraShakeIntensity = 0.3f; 

        bool blockPlacedOut = false;

        for (int i = 0; i < currentPiece.size(); ++i) {
            for (int j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j] != 0) {
                    if ((currentY + i) < 0) {
                        blockPlacedOut = true; 
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
            return; 
        }
        
        if (currentIsBrilliant) {
            if (currentGridWidth < 36) {
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
                SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_CYAN, 150, 25.0f);
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
                    SpawnParticles3D(blockPos, pieceColors[board[i][j]], 25, 20.0f);
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
            avgClearPos.y /= linesClearedNow;
            linesClearedTotal += linesClearedNow;
            
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
            hitStopTimer = linesClearedNow * 0.05f; 

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
            else if (comboCount >= 5) { mensagemEspecial = "GOD MODE!"; timerMensagem = 3.0f; }
            else if (linesClearedNow >= 4) { mensagemEspecial = "MARVELOUS"; timerMensagem = 3.0f; }
            else if (linesClearedNow == 3) { mensagemEspecial = "IMPRESSIVE"; timerMensagem = 2.0f; }
            else if (linesClearedNow == 2) { mensagemEspecial = "GOOD"; timerMensagem = 2.0f; }

            SpawnFloatingText(avgClearPos, "+" + to_string(ptsGained), C_CYAN, 1.5f + (linesClearedNow * 0.5f));
            
            if (comboCount > 1) {
                SpawnFloatingText({avgClearPos.x, avgClearPos.y + 2.0f, avgClearPos.z}, "COMBO x" + to_string(comboCount), C_YELLOW, 2.0f);
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
                    SpawnParticles3D({0, currentGridElevation + 20.0f, -5.0f}, C_RED, 400, 60.0f); 
                    cameraShakeTimer = 2.0f; cameraShakeIntensity = 8.0f;
                    hitStopTimer = 0.5f; 
                    mensagemEspecial = "VIRUS DELETED!"; timerMensagem = 4.0f;
                    TocarSom(sndClear4);
                }
            }
            level = (linesClearedTotal / 10) + 1;
        } else {
            comboCount = 0; 
        }
    }

    void NukeBoard() {
        int blocksDestroyed = 0;
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < currentGridWidth; j++) {
                if (board[i][j] != 0) {
                    blocksDestroyed++;
                    SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 25.0f);
                    board[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            score += blocksDestroyed * 50 * level;
            mensagemEspecial = "SYSTEM PURGE!!!"; timerMensagem = 3.0f;
            cameraShakeTimer = 1.5f; cameraShakeIntensity = 6.0f;
            hitStopTimer = 0.3f;
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
                cameraShakeTimer = 2.0f; cameraShakeIntensity = 8.0f;
                hitStopTimer = 0.5f; 
                mensagemEspecial = "BOSS PURGED!"; timerMensagem = 4.0f;
            }
        }
    }

    // =====================================================================
    // O FIM DO Z-FIGHTING NO ANDROID E FANTASMA LISO (INTEGRADO COM GRAPHICS)
    // =====================================================================
    void DrawSciFiBlock3D(Vector3 pos, Color baseCol, bool isReflection, bool isGhost = false, bool isBrilliant = false, float scale = 1.0f) {
        // Redução cirúrgica do tamanho do cubo de 0.85f para 0.78f para dar leve espaçamento
        float s = CUBE_SIZE * 0.78f * scale; 

        if (isGhost) {
            rlDisableDepthMask();
            // Apenas renderiza o fantasma sólido em qualidades mais altas (poupa a placa gráfica no Low/Med)
            if (graphicsQuality > 0) DrawCube(pos, s, s, s, ColorAlpha(WHITE, 0.15f));
            DrawCubeWires(pos, s, s, s, ColorAlpha(WHITE, 0.4f));
            rlEnableDepthMask();
        } else {
            Color coreCol = baseCol; 
            coreCol.a = 255; 

            if (isBrilliant) {
                float t = (float)GetTime() * 10.0f;
                coreCol = ColorFromHSV(fmod(t * 60.0f, 360.0f), 1.0f, 1.0f);
                coreCol.a = 255; 
            }
            
            // Ultra & High tem o núcleo branco interno super bonito
            if (graphicsQuality >= 2) {
                DrawCube(pos, s * 0.5f, s * 0.5f, s * 0.5f, WHITE);
            }
            
            rlDisableDepthMask();
            // No Low a casca externa vira o bloco sólido para poupar geometria
            float glassAlpha = (graphicsQuality == 0) ? 0.8f : 0.35f;
            DrawCube(pos, s, s, s, ColorAlpha(coreCol, glassAlpha));
            rlEnableDepthMask();
            
            // Med, High e Ultra tem os fios brilhantes contornando o vidro
            if (graphicsQuality >= 1) {
                DrawCubeWires(pos, s, s, s, coreCol); 
            }
        }
    }

    void DrawProceduralEnvironment() {
        rlBegin(RL_LINES);
        
        int limit = starfield.size();
        if (graphicsQuality == 0) limit /= 4;
        else if (graphicsQuality == 1) limit /= 2;
        else if (graphicsQuality == 2) limit = (int)(limit * 0.8f);
        
        for(int i = 0; i < limit; i++) {
            const auto& s = starfield[i];
            Color tailCol = ColorAlpha(s.color, 0.0f);
            Vector3 tail = { s.pos.x, s.pos.y, s.pos.z - (s.speed * 0.6f) }; 
            
            rlColor4ub(tailCol.r, tailCol.g, tailCol.b, tailCol.a);
            rlVertex3f(tail.x, tail.y, tail.z);
            
            rlColor4ub(s.color.r, s.color.g, s.color.b, s.color.a);
            rlVertex3f(s.pos.x, s.pos.y, s.pos.z);
        }
        rlEnd();
    }

    void DrawPlayfieldAndPieces(bool isReflection) {
        float trueBottomY = currentGridElevation;
        float trueTopY = currentGridElevation + BOARD_HEIGHT + 3.0f;
        float halfW = currentGridWidth / 2.0f;

        Color frameC = bossActive ? C_RED : DARKGRAY;
        frameC.a = 255;

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
            Color glowC = ColorAlpha(C_CYAN, blinkAlpha * 0.7f);

            float flashLeftX = -halfW + 0.5f;
            float flashRightX = halfW - 0.5f;

            for (float y = 0; y < BOARD_HEIGHT + 3.0f; y += 1.0f) {
                DrawSciFiBlock3D({flashLeftX, trueBottomY + y + 0.5f, 0}, glowC, isReflection, false, true, 1.0f);
                DrawSciFiBlock3D({flashRightX, trueBottomY + y + 0.5f, 0}, glowC, isReflection, false, true, 1.0f);
            }
        }

        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < currentGridWidth; ++j) {
                if (board[i][j] != 0) {
                    DrawSciFiBlock3D(GetWorldPos(j, i), pieceColors[board[i][j]], isReflection);
                }
            }
        }

        if (currentState == PLAYING) {
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
                            DrawSciFiBlock3D(ghostPos, BLANK, isReflection, true); 
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
                        DrawSciFiBlock3D(dropPos, pieceColors[currentColor], isReflection, false, currentIsBrilliant);
                    }
                }
            }
            rlPopMatrix();
        }
    }

    // =====================================================================
    // O NOVO BOSS AAA: A PIRÂMIDE MUTANTE ESTROBOSCÓPICA
    // =====================================================================
    void DrawBossEncounter() {
        if (bossEntryAnim < 0.01f || !isBossMode) return;
        
        float time = (float)GetTime();
        float hpRatio = (float)bossHp / (10.0f + (bossEncounterCount * 5.0f));
        float instability = 1.0f - hpRatio; 

        rlPushMatrix();
            // Jumpscare Shake Intenso
            float shakeX = isBossStrobing ? GetRandomFloat(-0.8f, 0.8f) : 0.0f;
            float shakeY = isBossStrobing ? GetRandomFloat(-0.8f, 0.8f) : 0.0f;
            float shakeZ = isBossStrobing ? GetRandomFloat(-0.8f, 0.8f) : 0.0f;

            rlTranslatef(currentBossPos.x + shakeX, currentBossPos.y + sin(time * 2.0f) * 2.0f + shakeY, currentBossPos.z + shakeZ);
            
            float heartbeat = pow(sin(time * 8.0f), 8.0f);
            
            // Tamanho base da pirâmide super reduzido (0.35f) para não poluir
            float mutationScale = 0.35f * (1.0f + (musicPulse * 0.08f) + (heartbeat * 0.3f));
            rlScalef(mutationScale, mutationScale, mutationScale);

            // Rotação do Núcleo
            rlRotatef(time * 50.0f, 0.0f, 1.0f, 0.0f);
            if (isBossStrobing) rlRotatef(time * 300.0f, 1.0f, 1.0f, 1.0f); // Giro enlouquecido durante o ataque

            // Geometria da Pirâmide Principal
            float b = 7.5f + (instability * 2.0f); // Base
            float h = 10.0f + (instability * 3.0f); // Altura
            
            // Núcleo com Cores Estroboscópicas se estiver atacando
            Color coreColor = (GetRandomValue(0, 100) > (90 - instability * 50)) ? WHITE : C_RED;
            if (isBossStrobing && (int)(time * 30) % 2 == 0) coreColor = C_CYAN;

            rlDisableDepthMask();
            
            // Superfícies Sólidas com brilho interno
            rlBegin(RL_TRIANGLES);
            rlColor4ub(coreColor.r, coreColor.g, coreColor.b, 200);
            
            // Triângulos Frontais, Laterais e Traseiros da Pirâmide Superior
            rlVertex3f(0, h, 0); rlVertex3f(-b, -h, b); rlVertex3f(b, -h, b);
            rlVertex3f(0, h, 0); rlVertex3f(b, -h, b); rlVertex3f(b, -h, -b);
            rlVertex3f(0, h, 0); rlVertex3f(b, -h, -b); rlVertex3f(-b, -h, -b);
            rlVertex3f(0, h, 0); rlVertex3f(-b, -h, -b); rlVertex3f(-b, -h, b);
            
            // Pirâmide Invertida (Brilho e Magia Interna)
            rlColor4ub(C_ORANGE.r, C_ORANGE.g, C_ORANGE.b, 150);
            rlVertex3f(0, -h, 0); rlVertex3f(-b, h, b); rlVertex3f(b, h, b);
            rlVertex3f(0, -h, 0); rlVertex3f(b, h, b); rlVertex3f(b, h, -b);
            rlVertex3f(0, -h, 0); rlVertex3f(b, h, -b); rlVertex3f(-b, h, -b);
            rlVertex3f(0, -h, 0); rlVertex3f(-b, h, -b); rlVertex3f(-b, h, b);
            rlEnd();

            rlEnableDepthMask();

            // Wireframes de Contorno AAA
            rlBegin(RL_LINES);
            rlColor4ub(WHITE.r, WHITE.g, WHITE.b, 255);
            // Base
            rlVertex3f(-b, -h, b); rlVertex3f(b, -h, b);
            rlVertex3f(b, -h, b); rlVertex3f(b, -h, -b);
            rlVertex3f(b, -h, -b); rlVertex3f(-b, -h, -b);
            rlVertex3f(-b, -h, -b); rlVertex3f(-b, -h, b);
            // Lados subindo ao topo
            rlVertex3f(0, h, 0); rlVertex3f(-b, -h, b);
            rlVertex3f(0, h, 0); rlVertex3f(b, -h, b);
            rlVertex3f(0, h, 0); rlVertex3f(b, -h, -b);
            rlVertex3f(0, h, 0); rlVertex3f(-b, -h, -b);
            rlEnd();
            
            // Luzes Estroboscópicas Orbitantes
            int numFragments = 8 + (bossEncounterCount * 2);
            if (graphicsQuality == 0) numFragments /= 3;
            else if (graphicsQuality == 1) numFragments /= 2;

            for (int i = 0; i < numFragments; i++) {
                rlPushMatrix();
                    float speed = time * (2.0f + instability * 5.0f);
                    if (isBossStrobing) speed *= 4.0f; // Aceleração mortal do estroboscópio

                    float angle1 = speed * (1.2f + i*0.1f) + (i * PI * 2.0f / numFragments);
                    float angle2 = speed * 0.7f + (i * PI / numFragments);
                    
                    rlRotatef(angle1 * RAD2DEG, 1.0f, 0.5f, 0.0f);
                    rlRotatef(angle2 * RAD2DEG, 0.0f, 1.0f, 0.5f);
                    
                    float fragDist = b * 1.5f + sin(time * 4.0f + i) * 2.0f + (instability * 4.0f);
                    if (isBossStrobing) fragDist += GetRandomFloat(0.0f, 10.0f); // Luzes espalham descontroladamente

                    rlTranslatef(0.0f, 0.0f, fragDist);
                    rlRotatef(time * 200.0f, 1, 1, 1);
                    
                    Color fragColor = (i % 2 == 0) ? C_CYAN : WHITE;
                    if (isBossStrobing && (int)(time * 20) % 2 == 0) fragColor = C_RED; // Pisca em vermelho no Jumpscare
                    
                    float fragSize = 1.0f;
                    DrawCube({0,0,0}, fragSize, fragSize, fragSize, ColorAlpha(fragColor, 0.9f));
                    DrawCubeWires({0,0,0}, fragSize + 0.2f, fragSize + 0.2f, fragSize + 0.2f, WHITE);
                rlPopMatrix();
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
                
                DrawCenteredTextInRect(floatingTexts[i].text, {screenPos.x, screenPos.y, 0, 0}, floatingTexts[i].scale, c);
            }
        }
        rlEnableDepthMask();
    }

    // =====================================================================
    // SISTEMA DE TOQUE DINÂMICO PARA QUALQUER TELA
    // =====================================================================
    void ProcessTouchInputs() {
        UpdateRectangles(); 

        btnLeftDown = btnRightDown = btnDownDown = false;
        btnLeftPressed = btnRightPressed = btnDownPressed = false;
        btnMenuUpPressed = btnRotatePressed = btnDropPressed = btnPausePressed = false;
        btnNukePressed = btnYesPressed = btnNoPressed = false;
        btnMusicPressed = btnMenuPressed = btnShufflePressed = false;

        bool currentTouch[10] = {false};

        for (int i = 0; i < GetTouchPointCount() && i < 10; i++) {
            Vector2 tPos = GetTouchPosition(i); 
            currentTouch[i] = true;

            if (showExitPrompt) {
                if (CheckCollisionPointRec(tPos, recBtnYes) && !prevTouch[i]) btnYesPressed = true;
                if (CheckCollisionPointRec(tPos, recBtnNo) && !prevTouch[i]) btnNoPressed = true;
            } else {
                if (CheckCollisionPointRec(tPos, recBtnLeft)) {
                    btnLeftDown = true;
                    if (!prevTouch[i]) btnLeftPressed = true;
                }
                if (CheckCollisionPointRec(tPos, recBtnRight)) {
                    btnRightDown = true;
                    if (!prevTouch[i]) btnRightPressed = true;
                }
                if (CheckCollisionPointRec(tPos, recBtnDown)) {
                    btnDownDown = true;
                    if (!prevTouch[i]) btnDownPressed = true;
                }
                if (CheckCollisionPointRec(tPos, recBtnMenuUp)) {
                    if (!prevTouch[i]) btnMenuUpPressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnRotate)) {
                    if (!prevTouch[i]) btnRotatePressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnDrop)) {
                    if (!prevTouch[i]) btnDropPressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnNuke)) {
                    if (!prevTouch[i]) btnNukePressed = true;
                }
                if (CheckCollisionPointRec(tPos, recBtnPause)) {
                    if (!prevTouch[i]) btnPausePressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnMusic)) {
                    if (!prevTouch[i]) btnMusicPressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnMenu)) {
                    if (!prevTouch[i]) btnMenuPressed = true; 
                }
                if (CheckCollisionPointRec(tPos, recBtnShuffle)) {
                    if (!prevTouch[i]) btnShufflePressed = true; 
                }
            }
        }
        for (int i = 0; i < 10; i++) prevTouch[i] = currentTouch[i];
    }

public:
    JogoTetris3D() : currentState(INTRO) { 
        camera.position = defaultCamPos;
        camera.target = defaultCamTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        
        bgRenderTarget = LoadRenderTexture(GetScreenWidth(), GetScreenHeight()); 
        nextPieceRT = LoadRenderTexture(350, 350); 

        SetTextureFilter(bgRenderTarget.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(nextPieceRT.texture, TEXTURE_FILTER_BILINEAR);

        for(int i = 0; i < 400; i++) {
            starfield.push_back({
                { GetRandomFloat(-300.0f, 300.0f), GetRandomFloat(-100.0f, 200.0f), GetRandomFloat(-400.0f, 0.0f) },
                GetRandomFloat(80.0f, 250.0f), 1.0f, ColorAlpha(WHITE, GetRandomFloat(0.3f, 1.0f)) 
            });
        }

        sndMove = LoadSound("move.mp3"); sndRotate = LoadSound("rotate.mp3"); sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3"); sndClear2 = LoadSound("clear2.mp3"); sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3"); sndGameOver = LoadSound("gameover.mp3");
        
        LoadNextMusic();
        SpawnPiece();

        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        hitStopLoc = GetShaderLocation(postProcessShader, "hitStop");
        dmgVignetteLoc = GetShaderLocation(postProcessShader, "damageVignette");
        
        float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        UnloadRenderTexture(bgRenderTarget); 
        UnloadRenderTexture(nextPieceRT);
        UnloadShader(postProcessShader);
        UnloadSound(sndMove); UnloadSound(sndRotate); UnloadSound(sndDrop); UnloadSound(sndClear1);
        UnloadSound(sndClear2); UnloadSound(sndClear3); UnloadSound(sndClear4); UnloadSound(sndGameOver);
        if (sndMusic.stream.buffer != NULL) UnloadMusicStream(sndMusic);
    }

    void Update(float dt) {
        ProcessTouchInputs();

        float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);

        if (hitStopTimer > 0.0f) {
            hitStopTimer -= GetFrameTime(); 
            dt *= 0.1f; 
        }

        if (musicEnabled && sndMusic.stream.buffer != NULL) {
            UpdateMusicStream(sndMusic);
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                currentMusicTrack++; if (currentMusicTrack > 15) currentMusicTrack = 1; 
                LoadNextMusic();
            }
            musicPulse = Lerp(musicPulse, globalMusicAmplitude * 15.0f, dt * 20.0f);
        } else musicPulse = Lerp(musicPulse, 0.0f, dt * 5.0f);

        if (damageVignette > 0.0f) damageVignette = Lerp(damageVignette, 0.0f, dt * 2.0f);
        if (currentRotAngle != 0.0f) currentRotAngle = Lerp(currentRotAngle, 0.0f, dt * 15.0f); 
        
        if (btnMusicPressed) {
            musicEnabled = !musicEnabled;
            if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic);
            TocarSom(sndMove);
        }

        if (btnShufflePressed) {
            currentMusicTrack = GetRandomValue(1, 15);
            LoadNextMusic();
            TocarSom(sndMove);
        }

        if (currentState == INTRO) {
            introTimer -= dt;
            if (introTimer <= 0.0f) {
                currentState = MENU; 
                introTimer = 0.0f;
                damageVignette = 0.0f;
                musicPulse = 0.0f;
            } else {
                if (GetRandomValue(0, 100) > 90) damageVignette = GetRandomFloat(0.1f, 0.3f);
                else damageVignette = Lerp(damageVignette, 0.0f, dt * 5.0f);
                musicPulse = abs(sin(introTimer * 10.0f)) * 2.0f;
            }
        }
        else if (currentState == MENU || currentState == SETTINGS) {
            int* sel = (currentState == MENU) ? &menuSelection : &settingsSelection;
            int maxSel = (currentState == MENU) ? 5 : 3; 

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || btnMenuUpPressed || btnLeftPressed) { (*sel)--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || btnDownPressed || btnRightPressed) { (*sel)++; TocarSom(sndMove); }
            if (*sel < 0) *sel = maxSel;
            if (*sel > maxSel) *sel = 0;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || btnDropPressed) {
                TocarSom(sndDrop);
                if (currentState == MENU) {
                    if (*sel == 0) { isClassicMode = true; isExpansiveMode = false; isBossMode = false; Restart(); currentState = PLAYING; }
                    else if (*sel == 1) { isClassicMode = false; isExpansiveMode = true; isBossMode = false; Restart(); currentState = PLAYING; }
                    else if (*sel == 2) { isClassicMode = false; isExpansiveMode = false; isBossMode = true; Restart(); currentState = PLAYING; }
                    else if (*sel == 3) currentState = SETTINGS;
                    else if (*sel == 4) currentState = CREDITS;
                    else if (*sel == 5) confirmExit = true; 
                } else {
                    if (*sel == 0) { graphicsQuality = (graphicsQuality + 1) % 4; TocarSom(sndMove); }
                    else if (*sel == 1) sfxEnabled = !sfxEnabled;
                    else if (*sel == 2) { musicEnabled = !musicEnabled; if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic); }
                    else if (*sel == 3) currentState = MENU;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) || btnPausePressed) if(currentState == SETTINGS) currentState = MENU;
        }
        else if (currentState == CREDITS) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || btnDropPressed || btnPausePressed) { TocarSom(sndDrop); currentState = MENU; }
        }
        else if (currentState == PLAYING) {
            if (IsKeyPressed(KEY_P) || btnPausePressed) { isPaused = !isPaused; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_ESCAPE) || btnMenuPressed) showExitPrompt = !showExitPrompt; 

            if (showExitPrompt) {
                if (IsKeyPressed(KEY_Y) || btnYesPressed) { showExitPrompt = false; currentState = MENU; }
                if (IsKeyPressed(KEY_N) || btnNoPressed) showExitPrompt = false;
                return; 
            }

            if (gameOver) {
                if (IsKeyPressed(KEY_ENTER) || btnDropPressed) { Restart(); currentState = MENU; }
                return;
            }

            if (isBossMode && !bossActive && linesClearedTotal >= linesUntilBoss) {
                bossActive = true; bossEncounterCount++;
                bossHp = 10 + (bossEncounterCount * 5); 
                currentBossAttackDelay = fmax(3.0f, 15.0f - (bossEncounterCount * 2.0f));
                bossAttackTimer = currentBossAttackDelay; 
                TocarSom(sndGameOver);
                cameraShakeTimer = 2.5f; cameraShakeIntensity = 5.0f;
                damageVignette = 1.0f; hitStopTimer = 0.5f;
            }

            if (bossActive && !isPaused) {
                bossEntryAnim = Lerp(bossEntryAnim, 1.0f, dt * 2.0f); 
                bossAttackTimer -= dt;
                
                bossCinematicCooldown -= dt;
                if (bossCinematicCooldown <= 0.0f && bossCinematicSpinTimer <= 0.0f) {
                    bossCinematicSpinTimer = 2.0f; 
                    bossCinematicCooldown = GetRandomFloat(20.0f, 30.0f); 
                    mensagemEspecial = "WARNING!"; timerMensagem = 2.0f;
                }
                if (bossCinematicSpinTimer > 0.0f) bossCinematicSpinTimer -= dt;

                if (GetRandomValue(0, 100) > 92) {
                    cameraShakeTimer = 0.1f;
                    cameraShakeIntensity = 0.4f; // Reduzido tremor de câmera do Boss
                    damageVignette = GetRandomFloat(0.05f, 0.2f); 
                }

                // Chance de partículas de fogo da Pirâmide baseada na qualidade gráfica
                int bossFireChance = 90;
                if (graphicsQuality == 0) bossFireChance = 98;
                else if (graphicsQuality == 1) bossFireChance = 95;
                else if (graphicsQuality == 2) bossFireChance = 92;

                if (GetRandomValue(0, 100) > bossFireChance) { 
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
                float speed = fmax(0.08f, 0.8f - (level * 0.03f)); 
                fallTimer += dt;

                if ((IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL) || btnNukePressed) && bombs > 0) { bombs--; NukeBoard(); }

                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || btnLeftPressed) {
                    if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveLeftTimer = 0.0f; 
                } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A) || btnLeftDown) {
                    moveLeftTimer += dt;
                    if (moveLeftTimer >= DAS_DELAY) {
                        moveLeftTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); }
                    }
                } else { moveLeftTimer = 0.0f; }

                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || btnRightPressed) {
                    if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveRightTimer = 0.0f; 
                } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || btnRightDown) {
                    moveRightTimer += dt;
                    if (moveRightTimer >= DAS_DELAY) {
                        moveRightTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); }
                    }
                } else { moveRightTimer = 0.0f; }

                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || btnRotatePressed) {
                    auto rotated = RotateMatrix(currentPiece);
                    bool rotatedSuccess = false;
                    
                    // Sistema de WALL KICK (Tenta girar na mesma posição, depois empurra lados e por último cima)
                    vector<pair<int, int>> kicks = { {0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1} };
                    
                    for (auto kick : kicks) {
                        if (IsValidMove(rotated, currentX + kick.first, currentY + kick.second)) {
                            currentPiece = rotated;
                            currentX += kick.first;
                            currentY += kick.second;
                            rotatedSuccess = true;
                            break;
                        }
                    }
                    
                    if (rotatedSuccess) { 
                        TocarSom(sndRotate); 
                        currentRotAngle = 90.0f; 
                        SpawnParticles3D(GetWorldPos(currentX, currentY), pieceColors[currentColor], 5, 5.0f);
                    }
                }
                
                if (IsKeyPressed(KEY_SPACE) || btnDropPressed) { 
                    int dropDist = 0;
                    while (IsValidMove(currentPiece, currentX, currentY + 1)) { currentY++; dropDist++; }
                    score += dropDist * 2;
                    LockPiece(); fallTimer = 0;
                    cameraShakeTimer = 0.2f; cameraShakeIntensity = dropDist * 0.15f; 
                    SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 15, dropDist * 2.0f);
                    damageVignette = dropDist * 0.05f;
                } else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S) || btnDownDown) { 
                    fallTimer += dt * 12.0f; // Velocidade da descida suave controlada (Reduzido de 20 para 12)
                }

                if (fallTimer >= speed) {
                    fallTimer = 0;
                    if (IsValidMove(currentPiece, currentX, currentY + 1)) currentY++;
                    else LockPiece();
                }

                // Velocidade visual do bloco alcançando a física re-ajustada de 30 para 20
                renderFallY = Lerp(renderFallY, (float)currentY, dt * 20.0f);
                renderX = Lerp(renderX, (float)currentX, dt * 20.0f);

                // ====================================================================
                // RASTRO (Muito mais partículas na cauda)
                // ====================================================================
                for (int i = 0; i < currentPiece.size(); ++i) {
                    for (int j = 0; j < currentPiece[i].size(); ++j) {
                        if (currentPiece[i][j] != 0) {
                            int trailChance = 50; // Super frequente
                            if (graphicsQuality == 0) trailChance = 80;
                            else if (graphicsQuality == 1) trailChance = 65;
                            else if (graphicsQuality == 2) trailChance = 50;

                            if (GetRandomValue(0, 100) > trailChance) { 
                                // Em qualidades Altas e Ultra a gente duplica o número de partículas geradas!
                                int pCount = (graphicsQuality >= 2) ? 2 : 1;
                                
                                for (int pc = 0; pc < pCount; pc++) {
                                    Particle3D trail;
                                    float halfW = currentGridWidth / 2.0f;
                                    Vector3 dropPos = { renderX + j - halfW + 0.5f, (float)BOARD_HEIGHT - (renderFallY + i) - 0.5f + currentGridElevation, 0.0f };

                                    trail.position = dropPos;
                                    trail.position.x += GetRandomFloat(-0.4f, 0.4f);
                                    trail.position.y += GetRandomFloat(-0.4f, 0.4f);
                                    trail.position.z += GetRandomFloat(-0.4f, 0.4f);

                                    trail.velocity = { GetRandomFloat(-1.5f, 1.5f), GetRandomFloat(5.0f, 15.0f), GetRandomFloat(-1.5f, 1.5f) };
                                    trail.color = currentIsBrilliant ? ColorFromHSV(fmod((float)GetTime() * 60.0f, 360.0f), 1.0f, 1.0f) : pieceColors[currentColor];
                                    trail.maxLife = GetRandomFloat(0.3f, 0.7f);
                                    trail.life = trail.maxLife;
                                    trail.size = GetRandomFloat(0.1f, 0.3f);
                                    trail.isSpark = true;
                                    trail.rotation = {0,0,0};
                                    trail.rotVelocity = {0,0,0};
                                    particles.push_back(trail);
                                }
                            }
                        }
                    }
                }
                // ====================================================================
            } 
        }

        if (gridExpansionTimer > 0.0f) gridExpansionTimer -= dt;

        if (!isPaused) {
            for(auto& s : starfield) {
                s.pos.z += s.speed * dt * 2.2f; 
                if(s.pos.z > camera.position.z + 5.0f) { 
                    s.pos.z = camera.position.z + GetRandomFloat(-400.0f, -200.0f); 
                    s.pos.x = camera.position.x + GetRandomFloat(-300.0f, 300.0f);
                    s.pos.y = camera.position.y + GetRandomFloat(-150.0f, 150.0f);
                }
            }
        }

        // ==========================================================
        // CÂMARA E LÓGICA DO BOSS AAA (Paradinha do Jumpscare + Orbit)
        // ==========================================================
        float zoomOut = (currentGridWidth > 14) ? (currentGridWidth - 14) * 2.5f : 0.0f;
        float dynamicZ = 34.0f + zoomOut; 
        
        Vector3 finalPosTarget = { 0.0f, 5.5f, dynamicZ };
        Vector3 finalLookTarget = defaultCamTarget;

        if (bossCinematicSpinTimer > 0.0f && !isPaused) {
            float progress = 1.0f - (bossCinematicSpinTimer / 2.0f); 
            float easedProgress = -(cos(PI * progress) - 1.0f) / 2.0f; 
            float spinAngle = easedProgress * PI * 2.0f; 
            
            finalPosTarget.x = sin(spinAngle) * dynamicZ;
            finalPosTarget.z = cos(spinAngle) * dynamicZ;
        }

        if (bossActive) {
            // Logica do Jumpscare Estroboscópico
            if (bossRushCooldown > 0.0f) bossRushCooldown -= dt;

            if (bossRushCooldown <= 0.0f && !isBossStrobing && !isPaused) {
                isBossStrobing = true;
                bossStrobeTimer = 3.0f; // Boss prende o jogador por 3 segundos de agonia
                bossRushCooldown = GetRandomFloat(15.0f, 25.0f); // Recarrega o Jumpscare
            }

            if (isBossStrobing) {
                bossStrobeTimer -= dt;
                
                // Boss vai voando e freia diretamente na frente da câmera / na cara do jogador!
                Vector3 screenPos = camera.position;
                Vector3 lookDir = { 
                    camera.target.x - camera.position.x, 
                    camera.target.y - camera.position.y, 
                    camera.target.z - camera.position.z 
                };
                float length = sqrt(lookDir.x*lookDir.x + lookDir.y*lookDir.y + lookDir.z*lookDir.z);
                lookDir.x /= length; lookDir.y /= length; lookDir.z /= length;

                Vector3 targetPos = {
                    screenPos.x + lookDir.x * 12.0f,
                    screenPos.y + lookDir.y * 12.0f - 2.0f,
                    screenPos.z + lookDir.z * 12.0f
                };
                
                currentBossPos.x = Lerp(currentBossPos.x, targetPos.x, dt * 6.0f);
                currentBossPos.y = Lerp(currentBossPos.y, targetPos.y, dt * 6.0f);
                currentBossPos.z = Lerp(currentBossPos.z, targetPos.z, dt * 6.0f);

                if (bossStrobeTimer <= 0.0f) isBossStrobing = false;

            } else {
                bossOrbitAngle += dt * 1.5f; 
                float bossDist = 20.0f + sin(bossOrbitAngle * 0.45f) * 15.0f; 
                float targetX = (float)sin(bossOrbitAngle) * bossDist;
                float targetY = 14.0f + sin(bossOrbitAngle * 0.7f) * 14.0f;
                float targetZ = (float)cos(bossOrbitAngle) * bossDist;
                
                currentBossPos.x = Lerp(currentBossPos.x, targetX, dt * 3.0f);
                currentBossPos.y = Lerp(currentBossPos.y, targetY, dt * 3.0f);
                currentBossPos.z = Lerp(currentBossPos.z, targetZ, dt * 3.0f);
            }
        }

        if (cameraShakeTimer > 0 && !isPaused) {
            float rx = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            float ry = GetRandomFloat(-cameraShakeIntensity, cameraShakeIntensity);
            finalPosTarget.x += rx; finalPosTarget.y += ry;
            finalLookTarget.x += rx*0.5f; finalLookTarget.y += ry*0.5f;
            cameraShakeTimer -= dt;
        }

        camera.position = finalPosTarget;
        camera.target = finalLookTarget;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;

        if (timerMensagem > 0 && !isPaused) { timerMensagem -= dt; if (timerMensagem <= 0.0f) mensagemEspecial = ""; }

        float timeVal = (float)GetTime();
        SetShaderValue(postProcessShader, timeLoc, &timeVal, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, pulseLoc, &musicPulse, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, hitStopLoc, &hitStopTimer, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, dmgVignetteLoc, &damageVignette, SHADER_UNIFORM_FLOAT);
    }

    void DrawHUDTouch() {
        if(currentState == INTRO) return;
        
        Color cDpad = ColorAlpha(WHITE, 0.15f);
        Color cAction = ColorAlpha(C_CYAN, 0.15f);
        Color cAlert = ColorAlpha(C_ORANGE, 0.15f);
        Color cPressed = ColorAlpha(WHITE, 0.4f);

        if (showExitPrompt) {
            DrawRectangleRounded(recBtnYes, 0.5f, 10, btnYesPressed ? cPressed : ColorAlpha(C_GREEN, 0.2f));
            DrawCenteredTextInRect("YES", recBtnYes, 6.0f, WHITE);

            DrawRectangleRounded(recBtnNo, 0.5f, 10, btnNoPressed ? cPressed : ColorAlpha(C_RED, 0.2f));
            DrawCenteredTextInRect("NO", recBtnNo, 6.0f, WHITE);
            return;
        }

        DrawRectangleRounded(recBtnLeft, 0.5f, 10, btnLeftDown ? cPressed : cDpad);
        DrawCenteredTextInRect("<", recBtnLeft, 8.0f, WHITE);

        DrawRectangleRounded(recBtnRight, 0.5f, 10, btnRightDown ? cPressed : cDpad);
        DrawCenteredTextInRect(">", recBtnRight, 8.0f, WHITE);
        
        DrawRectangleRounded(recBtnDown, 0.5f, 10, btnDownDown ? cPressed : cDpad);
        DrawCenteredTextInRect("V", recBtnDown, 8.0f, WHITE);

        if (currentState != PLAYING) {
            DrawRectangleRounded(recBtnMenuUp, 0.5f, 10, btnMenuUpPressed ? cPressed : cDpad);
            DrawCenteredTextInRect("^", recBtnMenuUp, 8.0f, WHITE);
        }

        DrawRectangleRounded(recBtnDrop, 0.5f, 10, btnDropPressed ? cPressed : cAction);
        DrawCenteredTextInRect(currentState == PLAYING ? "DROP" : "OK", recBtnDrop, 6.0f, WHITE);

        if (currentState == PLAYING) {
            DrawRectangleRounded(recBtnRotate, 0.5f, 10, btnRotatePressed ? cPressed : cAction);
            DrawCenteredTextInRect("ROT", recBtnRotate, 6.0f, WHITE);
        }
        
        if(currentState == PLAYING && bombs > 0) {
            DrawRectangleRounded(recBtnNuke, 0.5f, 10, btnNukePressed ? cPressed : cAlert);
            DrawCenteredTextInRect("BMB", recBtnNuke, 5.0f, WHITE);
        }

        if (currentState == PLAYING) {
            DrawRectangleRounded(recBtnPause, 0.5f, 10, btnPausePressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredTextInRect("II", recBtnPause, 5.0f, WHITE);
            
            DrawRectangleRounded(recBtnMusic, 0.5f, 10, btnMusicPressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredTextInRect("MUS", recBtnMusic, 4.0f, WHITE);

            DrawRectangleRounded(recBtnMenu, 0.5f, 10, btnMenuPressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredTextInRect("MENU", recBtnMenu, 4.0f, WHITE);

            DrawRectangleRounded(recBtnShuffle, 0.5f, 10, btnShufflePressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredTextInRect("SHUF", recBtnShuffle, 4.0f, WHITE);
        } else if (currentState != MENU) {
            DrawRectangleRounded(recBtnPause, 0.5f, 10, btnPausePressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredTextInRect("ESC", recBtnPause, 5.0f, WHITE);
        }
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

        BeginTextureMode(bgRenderTarget);
        
        if (currentState == INTRO) {
            ClearBackground(BLACK); 
            DrawIntro2D(GetFrameTime());
        } else {
            ClearBackground(C_BG); 
            BeginMode3D(camera);
                DrawProceduralEnvironment(); 
                DrawPlayfieldAndPieces(false);
                DrawBossEncounter();
                UpdateAndDrawParticles3D(GetFrameTime());
            EndMode3D();
            
            DrawFloatingTexts(GetFrameTime());
        }
        
        EndTextureMode();

        ClearBackground(BLACK);
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();

        Rectangle source = { 0, 0, (float)bgRenderTarget.texture.width, -(float)bgRenderTarget.texture.height };
        Rectangle dest = { 0, 0, sw, sh };
        
        if (graphicsQuality > 0) {
            BeginShaderMode(postProcessShader);
                DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
            EndShaderMode();
        } else {
            DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
        }

        DrawUI();
        DrawHUDTouch();

        // Camada do Flash Estroboscópico de Tela Inteira na UI! (Jumpscare)
        if (currentState == PLAYING && bossActive && isBossStrobing) {
            if ((int)(GetTime() * 20.0f) % 2 == 0) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(WHITE, 0.4f)); // Flash Brutal Branco
                Color strobeColor = (GetRandomValue(0, 1) == 0) ? C_RED : C_CYAN;
                DrawRectangle(0, 0, sw, sh, ColorAlpha(strobeColor, 0.3f)); // Subtom frenético
            }
        }
    }

    void DrawUI() {
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();

        if (currentState == INTRO) return; 

        if (currentState == MENU) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.7f));
            
            float titleBlockSize = 16.0f;
            string title = "TETRABETTA";
            float titleWidth = MeasureTetrisText(title, titleBlockSize);
            float logoStartX = (sw / 2.0f) - (titleWidth / 2.0f);
            
            DrawTetrisText(title, logoStartX, 150.0f, titleBlockSize, musicPulse, true, WHITE);

            const char* menuItems[] = { "CLASSIC RUN", "EXPANSIVE RUN", "BOSS RUSH", "SYSTEM CONFIG", "CREDITS", "LOGOUT" };
            for (int i = 0; i < 6; i++) {
                Color c = (i == menuSelection) ? C_YELLOW : GRAY;
                float itemBlockSize = 8.0f;
                
                string text = menuItems[i];
                float baseWidth = MeasureTetrisText(text, itemBlockSize);
                float itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);
                float yPos = 370.0f + i * 70.0f;
                
                if (i == menuSelection) {
                    float beatScale = 1.0f + (musicPulse * 0.05f);
                    float activeSize = itemBlockSize * beatScale;
                    baseWidth = MeasureTetrisText(text, activeSize);
                    itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);
                    
                    DrawTetrisText("> ", itemStartX - 8 * activeSize, yPos, activeSize, musicPulse, false, C_YELLOW);
                    DrawTetrisText(text, itemStartX, yPos, activeSize, musicPulse * 0.5f, false, C_YELLOW);
                } else {
                    DrawTetrisText(text, itemStartX, yPos, itemBlockSize, 0.0f, false, c);
                }
            }
        } 
        else if (currentState == SETTINGS) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.8f));
            
            string title = "SYSTEM CONFIG";
            float titleSize = 10.0f;
            float titleWidth = MeasureTetrisText(title, titleSize);
            DrawTetrisText(title, (sw/2.0f) - (titleWidth/2.0f), 150.0f, titleSize, musicPulse * 0.2f, false, C_CYAN);

            string qualStr = "ULTRA";
            if (graphicsQuality == 0) qualStr = "LOW";
            else if (graphicsQuality == 1) qualStr = "MED";
            else if (graphicsQuality == 2) qualStr = "HIGH";

            string opt1 = string("GRAPHICS: ") + qualStr;
            string opt2 = string("HAPTIC SFX: ") + (sfxEnabled ? "ON" : "OFF");
            string opt3 = string("SYNTHWAVE: ") + (musicEnabled ? "ON" : "OFF");
            string opt4 = "RETURN";

            const char* setItems[] = { opt1.c_str(), opt2.c_str(), opt3.c_str(), opt4.c_str() };
            for (int i = 0; i < 4; i++) {
                Color c = (i == settingsSelection) ? C_ORANGE : GRAY;
                float itemSize = 7.0f;
                
                float baseWidth = MeasureTetrisText(setItems[i], itemSize);
                float itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);
                float yPos = 350.0f + i * 80.0f;

                if (i == settingsSelection) {
                    float beatScale = 1.0f + (musicPulse * 0.05f);
                    float activeSize = itemSize * beatScale;
                    baseWidth = MeasureTetrisText(setItems[i], activeSize);
                    itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);

                    DrawTetrisText("> ", itemStartX - 8 * activeSize, yPos, activeSize, musicPulse, false, C_ORANGE);
                    DrawTetrisText(setItems[i], itemStartX, yPos, activeSize, musicPulse * 0.3f, false, C_ORANGE);
                } else {
                    DrawTetrisText(setItems[i], itemStartX, yPos, itemSize, 0.0f, false, c);
                }
            }
        }
        else if (currentState == CREDITS) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.9f));
            
            float size1 = 5.0f;
            float size2 = 8.0f;
            
            string t1 = "EXECUTIVE PRODUCER";
            DrawTetrisText(t1, sw/2 - MeasureTetrisText(t1, size1)/2, 300, size1, 0, false, C_ORANGE);
            
            string t2 = "IGOR BETTARELLO - OMEGARED";
            DrawTetrisText(t2, sw/2 - MeasureTetrisText(t2, size2)/2, 350, size2, musicPulse*0.2f, true, WHITE); 
            
            string t3 = "AAA LEAD ENGINEER";
            DrawTetrisText(t3, sw/2 - MeasureTetrisText(t3, size1)/2, 500, size1, 0, false, C_ORANGE);
            
            string t4 = "AI ASSISTANT - GEMINI GOD MODE";
            DrawTetrisText(t4, sw/2 - MeasureTetrisText(t4, size2)/2, 550, size2, 0, false, C_CYAN);
        }
        else if (currentState == PLAYING) {
            DrawFrameWithCubes2D({20, 20, 520, 320}, 8.0f, ColorAlpha(C_CYAN, 0.5f), ColorAlpha(C_BG, 0.6f));
            
            DrawTetrisText("SYS.VER: AAA.2026", 40, 40, 2.5f, 0, false, C_GREEN);
            DrawTetrisText(TextFormat("SCORE: %08d", score), 40, 80, 6.0f, 0, false, WHITE);
            DrawTetrisText(TextFormat("LEVEL: %02d", level), 40, 140, 5.0f, 0, false, C_ORANGE);
            DrawTetrisText(TextFormat("LIVES: %d", continues), 40, 190, 4.0f, 0, false, C_RED); 
            
            if (isExpansiveMode) {
                DrawTetrisText("MAGIC CHARGE:", 40, 240, 3.0f, 0, false, WHITE);
                DrawRectangleLines(40, 270, 300, 20, C_YELLOW);
                DrawRectangle(40, 270, 300 * (stars/10.0f), 20, ColorAlpha(C_YELLOW, 0.7f + sin(GetTime()*10)*0.3f));
            }

            if (bossActive && isBossMode) {
                float glitchX = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;
                float glitchY = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;

                // Frame do Boss movido para o topo e consideravelmente menor (Largura de 800 para 600, Altura para 60)
                DrawFrameWithCubes2D({sw/2 - 300 + glitchX, 10 + glitchY, 600, 60}, 5.0f, C_RED, ColorAlpha(C_BG, 0.7f));
                
                string bName = TextFormat("ANOMALY: OMEGARED V.%d", bossEncounterCount);
                float bSize = 4.0f;
                DrawTetrisText(bName, sw/2 - MeasureTetrisText(bName, bSize)/2 + glitchX, 18 + glitchY, bSize, 0, false, C_RED);
                
                float hpRatio = (float)bossHp / (10.0f + (bossEncounterCount * 5.0f)); 
                DrawRectangleRounded({sw/2 - 285 + glitchX, 40 + glitchY, 570 * hpRatio, 12}, 0.5f, 8, C_ORANGE);
            }

            DrawFrameWithCubes2D({sw - 540.0f, 20.0f, 520.0f, 320.0f}, 8.0f, ColorAlpha(C_CYAN, 0.5f), ColorAlpha(C_BG, 0.6f));
            
            string qText = "NEXT PIECE";
            DrawTetrisText(qText, sw - 280.0f - MeasureTetrisText(qText, 4.5f)/2, 50.0f, 4.5f, 0, false, WHITE);

            Rectangle sourceRT = { 0, 0, (float)nextPieceRT.texture.width, -(float)nextPieceRT.texture.height };
            Rectangle destRT = { sw - 410.0f, 60.0f, 260.0f, 260.0f };
            if (!gameOver && !isPaused) {
                DrawTexturePro(nextPieceRT.texture, sourceRT, destRT, {0, 0}, 0.0f, WHITE);
            }

            if (timerMensagem > 0) {
                float popScale = ElasticEaseOut(1.0f - (timerMensagem / 3.0f));
                float fontSize = 12.0f * popScale; 
                float textWidth = MeasureTetrisText(mensagemEspecial, fontSize);
                DrawTetrisText(mensagemEspecial, sw/2 - textWidth/2, sh/3, fontSize, 0, false, WHITE);
            }

            if (isPaused) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.8f));
                string pMsg = "SYSTEM PAUSED";
                DrawTetrisText(pMsg, sw/2 - MeasureTetrisText(pMsg, 10.0f)/2, sh/2 - 30, 10.0f, 0, false, C_CYAN);
            }

            if (gameOver) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(C_RED, 0.4f)); 
                DrawRectangle(0, sh/2 - 100, sw, 200, ColorAlpha(BLACK, 0.9f));
                string failMsg = "CRITICAL FAILURE";
                DrawTetrisText(failMsg, sw/2 - MeasureTetrisText(failMsg, 12.0f)/2, sh/2 - 80, 12.0f, 0, false, C_RED);
                string rebMsg = "PRESS [OK] TO REBOOT";
                DrawTetrisText(rebMsg, sw/2 - MeasureTetrisText(rebMsg, 4.0f)/2, sh/2 + 40, 4.0f, 0, false, WHITE);
            }

            if (showExitPrompt) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.95f));
                string extMsg = "ABORT SIMULATION?";
                DrawTetrisText(extMsg, sw/2 - MeasureTetrisText(extMsg, 10.0f)/2, sh/2 - 60, 10.0f, 0, false, C_ORANGE);
            }
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0; level = 1; continues = 3; bombs = 2; stars = 0; 
        
        // Define modos diferentes
        if (isClassicMode) currentGridWidth = 14; 
        else if (isBossMode) currentGridWidth = 12;
        else currentGridWidth = 10; 

        nextIsBrilliant = false; currentIsBrilliant = false; linesClearedTotal = 0;
        comboCount = 0; 
        
        gameOver = false; isPaused = false; timerMensagem = 0; mensagemEspecial = ""; 
        gridExpansionTimer = 0.0f; 
        moveLeftTimer = 0.0f; moveRightTimer = 0.0f;
        hitStopTimer = 0.0f; damageVignette = 0.0f;
        
        bossActive = false; bossEntryAnim = 0.0f; linesUntilBoss = 15; bossEncounterCount = 0; 
        currentBossAttackDelay = 15.0f; bossOrbitAngle = 0.0f; 
        bossCinematicSpinTimer = 0.0f; bossCinematicCooldown = 15.0f;
        
        particles.clear(); floatingTexts.clear();
        SpawnPiece();
        
        currentMusicTrack = 1; LoadNextMusic();
    }

    bool ShouldExit() { return confirmExit; }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(0, 0, "TeTRABeTTA - AAA OVERDRIVE EDITION");
    SetExitKey(KEY_NULL); 
    InitAudioDevice(); 
    SetTargetFPS(60);

    JogoTetris3D* game = new JogoTetris3D();

    while (!WindowShouldClose() && !game->ShouldExit()) {
        game->Update(GetFrameTime());
        BeginDrawing();
        game->Draw();
        EndDrawing();
    }

    delete game;
    CloseAudioDevice(); 
    CloseWindow();
    return 0;
}