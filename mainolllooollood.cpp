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

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <jni.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

// Comunicação direta com o Android para travar a tela fisicamente
extern "C" struct android_app *GetAndroidApp(void);

void LockAndroidOrientation(int orientationMode) {
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;

    JNIEnv *env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (!env) return;

    jobject activity = app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID setRequestedOrientation = env->GetMethodID(activityClass, "setRequestedOrientation", "(I)V");
    
    // 6 = SCREEN_ORIENTATION_SENSOR_LANDSCAPE
    // 7 = SCREEN_ORIENTATION_SENSOR_PORTRAIT
    int androidOrientation = (orientationMode == 1) ? 7 : 6;
    env->CallVoidMethod(activity, setRequestedOrientation, androidOrientation);
    
    app->activity->vm->DetachCurrentThread();
}
#endif

using namespace std;

// Macros para resoluções dinâmicas nativas
#define SW (float)GetScreenWidth()
#define SH (float)GetScreenHeight()

// =====================================================================
// SHADERS DE PÓS-PROCESSAMENTO AAA (POST-PROCESSING STACK) + FXAA LITE
// ADAPTADOS AUTOMATICAMENTE PARA DESKTOP E ANDROID (MOBILE)
// =====================================================================

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
// --- SHADERS MOBILE (GLSL ES 100) ---
const char* vertexShaderCode = R"(
#version 100
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
uniform float goldTint;

float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;
    
    vec2 crtUV = uv - 0.5;
    crtUV += 0.5;
    
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float shiftAmount = 0.0008 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    vec2 off = 1.0 / resolution;
    vec3 colBlur = texture2D(texture0, crtUV + shift).rgb * 0.5;
    colBlur += texture2D(texture0, crtUV + shift + vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture2D(texture0, crtUV + shift - vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture2D(texture0, crtUV + shift + vec2(0.0, off.y)).rgb * 0.125;
    colBlur += texture2D(texture0, crtUV + shift - vec2(0.0, off.y)).rgb * 0.125;
    vec4 baseColor = vec4(colBlur, 1.0);

    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.8; 
    
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture2D(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    
    float scanline = sin(crtUV.y * resolution.y * 2.0) * 0.025;
    float grain = (rand(crtUV * time) - 0.5) * 0.02;
    
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.95, 0.4, dist);
    
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.5) * (1.0 - scanline) + grain;
    
    finalRGB = mix(vec3(0.5), finalRGB, 1.15); 
    float luma = dot(finalRGB, vec3(0.299, 0.587, 0.114));
    finalRGB = mix(vec3(luma), finalRGB, 1.4); 
    
    if (goldTint > 0.0) {
        finalRGB += vec3(0.3, 0.2, 0.0) * goldTint * smoothstep(0.8, 0.2, dist);
    }
    
    finalRGB *= vignette;
    
    if (damageVignette > 0.0) {
        finalRGB += vec3(0.8, 0.0, 0.0) * damageVignette * smoothstep(0.3, 0.8, dist);
    }

    gl_FragColor = vec4(finalRGB, 1.0) * fragColor;
}
)";
#else
// --- SHADERS DESKTOP (GLSL 330 ORIGINAL) ---
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

    float shiftAmount = 0.0008 + (damageVignette * 0.02);
    vec2 shift = vec2(shiftAmount * (uv.x - 0.5), shiftAmount * (uv.y - 0.5));
    
    vec2 off = 1.0 / resolution;
    vec3 colBlur = texture(texture0, crtUV + shift).rgb * 0.5;
    colBlur += texture(texture0, crtUV + shift + vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift - vec2(off.x, 0.0)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift + vec2(0.0, off.y)).rgb * 0.125;
    colBlur += texture(texture0, crtUV + shift - vec2(0.0, off.y)).rgb * 0.125;
    vec4 baseColor = vec4(colBlur, 1.0);

    vec2 texel = 1.0 / resolution;
    vec4 bloom = vec4(0.0);
    float glowSpread = 1.8; 
    
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    bloom += max(vec4(0.0), texture(texture0, crtUV + vec2(-texel.x, -texel.y) * glowSpread) - 0.2) * 0.3;
    
    float scanline = sin(crtUV.y * resolution.y * 2.0) * 0.025;
    float grain = (rand(crtUV * time) - 0.5) * 0.02;
    
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.95, 0.4, dist);
    
    vec3 finalRGB = (baseColor.rgb + bloom.rgb * 1.5) * (1.0 - scanline) + grain;
    
    finalRGB = mix(vec3(0.5), finalRGB, 1.15); // Contraste
    float luma = dot(finalRGB, vec3(0.299, 0.587, 0.114));
    finalRGB = mix(vec3(luma), finalRGB, 1.4); // Saturação
    
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
#endif

// =====================================================================
// CONSTANTES E CONFIGURAÇÕES DO MUNDO 3D
// =====================================================================
const int MAX_BOARD_WIDTH = 50;  
const int BOARD_HEIGHT = 22;     
const int SCREEN_WIDTH = 2400;   
const int SCREEN_HEIGHT = 1080;  
const float CUBE_SIZE = 0.95f;

// Paleta Estendida para as peças
const Color C_CYAN   = { 0, 255, 255, 255 };
const Color C_BLUE   = { 0, 100, 255, 255 }; 
const Color C_ORANGE = { 255, 120, 0, 255 };
const Color C_YELLOW = { 255, 255, 0, 255 };
const Color C_GREEN  = { 0, 255, 0, 255 }; 
const Color C_PURPLE = { 180, 0, 255, 255 };
const Color C_RED    = { 255, 10, 50, 255 };
const Color C_MAGENTA= { 255, 0, 255, 255 };
const Color C_GOLD   = { 255, 215, 0, 255 }; 
const Color C_BG     = { 5, 8, 15, 255 }; 

Color pieceColors[15] = { BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, C_MAGENTA, WHITE, LIME, PINK, GOLD, VIOLET, DARKGRAY };

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
// SISTEMA DE PARTÍCULAS AVANÇADO (FUNÇÕES GLOBAIS)
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
    bool isRing; 
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

void SpawnShockwave(Vector3 pos, Color color) {
    Particle3D p;
    p.position = pos;
    p.velocity = {0,0,0};
    p.color = color;
    p.maxLife = 0.6f;
    p.life = p.maxLife;
    p.isSpark = false;
    p.isRing = true;
    p.size = 0.1f; 
    p.rotation = {90, 0, 0}; 
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
                
                rlBegin(RL_LINES);
                rlColor4ub(fadeColor.r, fadeColor.g, fadeColor.b, fadeColor.a);
                int segments = 36;
                for(int s = 0; s < segments; s++) {
                    float a1 = (float)s / segments * PI * 2.0f;
                    float a2 = (float)(s+1) / segments * PI * 2.0f;
                    rlVertex3f(cos(a1)*particles[i].size, sin(a1)*particles[i].size, 0);
                    rlVertex3f(cos(a2)*particles[i].size, sin(a2)*particles[i].size, 0);
                    
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
// SISTEMA DE PEÇAS E FUNDO WIREFRAME ANIMADO
// =====================================================================
struct Tetromino { vector<vector<int>> shape; int colorID; };

vector<Tetromino> pieces = {
    // 4 BLOCOS
    { {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, 1 }, // I
    { {{1,0,0}, {1,1,1}, {0,0,0}}, 2 },                // J
    { {{0,0,1}, {1,1,1}, {0,0,0}}, 3 },                // L
    { {{1,1}, {1,1}}, 4 },                             // O
    { {{0,1,1}, {1,1,0}, {0,0,0}}, 5 },                // S
    { {{0,1,0}, {1,1,1}, {0,0,0}}, 6 },                // T
    { {{1,1,0}, {0,1,1}, {0,0,0}}, 7 },                // Z
    
    // 5 BLOCOS (PENTOMINÓS)
    { {{1,0,1}, {1,1,1}, {0,0,0}}, 8 },                // U 
    { {{1,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, 3},// L Longo
    { {{1,1,1}, {1,1,0}, {0,0,0}}, 5 },                // P 
    { {{1,0,0}, {1,0,0}, {1,1,1}}, 6 },                // L Gigante 
    { {{0,0,0,0,0}, {1,1,1,1,1}, {0,0,0,0,0}}, 7 }     // Linha 5
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
    int currentLanguage = 0; // 0 = PT-BR, 1 = EN

    int graphicsQuality = 2; // Qualidade Grafica (0=LOW, 1=MED, 2=HIGH)

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score = 0;
    int level = 1;
    int lives = 3; 
    int totalContinues = 3; 
    int bombs = 2; 
    int stars = 0; 
    int currentGridWidth = 14; 
    int linesClearedTotal = 0;
    
    int comboCount = 0; 
    float comboTimer = 0.0f; 
    
    bool isContinuing = false;
    float continueTimer = 0.0f;

    bool gameOver = false;
    bool isPaused = false; 
    
    bool isClassicMode = false;   
    bool isExpansiveMode = true;  
    bool isBossMode = false;      
    
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

    vector<vector<int>> holdPiece;
    int holdColor = 0;
    bool canHold = true;

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
    
    Vector3 defaultCamPos = { 0.0f, 3.5f, 34.0f }; 
    Vector3 defaultCamTarget = { 0.0f, 11.5f, 0.0f };
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

    Sound sndMove, sndRotate, sndDrop, sndClear1, sndClear2, sndClear3, sndClear4, sndGameOver;
    Music sndMusic = { 0 };
    int currentMusicTrack = 1; 

    mt19937 rng;
    Shader postProcessShader;
    int resLoc, timeLoc, pulseLoc, hitStopLoc, dmgVignetteLoc, goldTintLoc;
    
    Font arialFont;

    // Variáveis Touch HUD Visível
    bool btnLeftDown = false, btnRightDown = false, btnDownDown = false;
    bool btnLeftPressed = false, btnRightPressed = false, btnDownPressed = false;
    bool btnMenuUpPressed = false, btnRotatePressed = false, btnDropPressed = false, btnPausePressed = false;
    bool btnNukePressed = false, btnYesPressed = false, btnNoPressed = false;
    bool btnMusicPressed = false, btnMenuPressed = false, btnHoldPressed = false;
    bool btnEscPressed = false, btnShufflePressed = false, btnNextPressed = false; 
    bool prevTouch[10] = {false}; 

    Rectangle recBtnLeft, recBtnRight, recBtnDown, recBtnMenuUp, recBtnMenu;
    Rectangle recBtnDrop, recBtnRotate, recBtnNuke, recBtnPause, recBtnMusic, recBtnHold;
    Rectangle recBtnYes, recBtnNo, recBtnEsc, recBtnShuffle, recBtnNext;

    void UpdateRectangles() {
        float sw = SW;
        float sh = SH;

        // ==========================================
        // LAYOUT MODO LANDSCAPE (HORIZONTAL)
        // ==========================================
        float dpadX = sw * 0.08f;
        float cy = sh - (sh * 0.35f);
        float btnS = sh * 0.16f; 
        
        recBtnLeft   = { dpadX - btnS - 10, cy, btnS, btnS }; 
        recBtnRight  = { dpadX + btnS + 10, cy, btnS, btnS }; 
        recBtnDown   = { dpadX, cy + btnS + 10, btnS, btnS }; 
        recBtnMenuUp = { dpadX, cy - btnS - 10, btnS, btnS }; 
        
        recBtnShuffle = { 40.0f, 560.0f, 160.0f, 80.0f };
        recBtnNext    = { 220.0f, 560.0f, 160.0f, 80.0f };
        
        recBtnHold    = { recBtnNext.x + recBtnNext.width + 20.0f, recBtnNext.y, 160.0f, 80.0f };
        
        float actionY = cy + btnS * 0.5f; 
        recBtnRotate = { sw - dpadX - 100.0f, actionY, btnS*1.2f, btnS*1.2f }; 
        recBtnDrop   = { recBtnRotate.x - btnS*1.5f - 60.0f, actionY, btnS*1.2f, btnS*1.2f }; 
        
        recBtnNuke   = { recBtnDrop.x, recBtnDrop.y - btnS - 100.0f, btnS, btnS }; 
        
        recBtnEsc    = { sw - 560.0f, 40.0f, 160.0f, 80.0f };
        recBtnMenu   = { sw - 560.0f, 140.0f, 160.0f, 80.0f };
        
        recBtnPause  = { sw - 560.0f, 240.0f, 160.0f, 80.0f }; 
        
        recBtnMusic  = { sw - 330.0f + (260.0f/2.0f) - (btnS/2.0f), 80.0f + 260.0f + 20.0f, btnS, btnS };

        recBtnYes    = { sw/2.0f - 350, sh/2.0f + 100, 300, 150 };
        recBtnNo     = { sw/2.0f + 50,  sh/2.0f + 100, 300, 150 };
    }

    void ProcessTouchInputs() {
        UpdateRectangles(); 

        btnLeftDown = btnRightDown = btnDownDown = false;
        btnLeftPressed = btnRightPressed = btnDownPressed = false;
        btnMenuUpPressed = btnRotatePressed = btnDropPressed = btnPausePressed = false;
        btnNukePressed = btnYesPressed = btnNoPressed = false;
        btnMusicPressed = btnMenuPressed = btnHoldPressed = btnEscPressed = false;
        btnShufflePressed = btnNextPressed = false;

        float sw = SW;
        float sh = SH;
        bool currentTouch[10] = {false};
        bool optionTapped = false; 

        for (int i = 0; i < GetTouchPointCount() && i < 10; i++) {
            Vector2 tPos = GetTouchPosition(i); 
            currentTouch[i] = true;

            // =========================================================
            // MENUS: CLICANDO DIRETO NAS OPÇÕES
            // =========================================================
            if ((currentState == MENU || currentState == SETTINGS) && !optionTapped) {
                if (currentState == MENU) {
                    for (int m = 0; m < 6; m++) {
                        float yPos = 300.0f + m * 100.0f; 
                        Rectangle btnRec = { sw/2.0f - (sw*0.4f), yPos - 30.0f, sw*0.8f, 90.0f };
                        if (CheckCollisionPointRec(tPos, btnRec) && !prevTouch[i]) {
                            menuSelection = m;
                            btnDropPressed = true; 
                            optionTapped = true; 
                        }
                    }
                } else if (currentState == SETTINGS) {
                    for (int m = 0; m < 6; m++) {
                        float yPos = 300.0f + m * 100.0f;
                        Rectangle btnRec = { sw/2.0f - (sw*0.4f), yPos - 30.0f, sw*0.8f, 90.0f };
                        if (CheckCollisionPointRec(tPos, btnRec) && !prevTouch[i]) {
                            settingsSelection = m;
                            btnDropPressed = true; 
                            optionTapped = true;
                        }
                    }
                }
            }

            // Tratamento Botão Return nos Créditos
            if (currentState == CREDITS && !optionTapped) {
                Rectangle recRet = { sw/2.0f - 150.0f, sh - 150.0f, 300.0f, 80.0f };
                if (CheckCollisionPointRec(tPos, recRet) && !prevTouch[i]) {
                    btnEscPressed = true; 
                    optionTapped = true;
                }
            }

            // Tratamento UI Padrão para os Botões Visíveis
            if (showExitPrompt || isContinuing) {
                if (CheckCollisionPointRec(tPos, recBtnYes) && !prevTouch[i]) btnYesPressed = true;
                if (CheckCollisionPointRec(tPos, recBtnNo) && !prevTouch[i]) btnNoPressed = true;
            } else {
                if (CheckCollisionPointRec(tPos, recBtnLeft)) { btnLeftDown = true; if (!prevTouch[i]) btnLeftPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnRight)) { btnRightDown = true; if (!prevTouch[i]) btnRightPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnDown)) { btnDownDown = true; if (!prevTouch[i]) btnDownPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnMenuUp)) { if (!prevTouch[i]) btnMenuUpPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnHold)) { if (!prevTouch[i]) btnHoldPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnRotate)) { if (!prevTouch[i]) btnRotatePressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnDrop)) { if (!prevTouch[i]) btnDropPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnNuke)) { if (!prevTouch[i]) btnNukePressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnPause)) { if (!prevTouch[i]) btnPausePressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnMusic)) { if (!prevTouch[i]) btnMusicPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnMenu)) { if (!prevTouch[i]) btnMenuPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnEsc)) { if (!prevTouch[i]) btnEscPressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnShuffle)) { if (!prevTouch[i]) btnShufflePressed = true; }
                if (CheckCollisionPointRec(tPos, recBtnNext)) { if (!prevTouch[i]) btnNextPressed = true; }
            }
        }

        for (int i = 0; i < 10; i++) prevTouch[i] = currentTouch[i];
    }

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
                        
                        DrawRectangle(bx, by, innerSize * beatScale, innerSize * beatScale, baseColor);
                        DrawRectangle(bx + (blockSize*0.1f), by + (blockSize*0.1f), innerSize * beatScale - (blockSize*0.25f), innerSize * beatScale - (blockSize*0.25f), ColorAlpha(WHITE, 0.4f));
                        DrawRectangleLines(bx, by, innerSize * beatScale, innerSize * beatScale, ColorAlpha(WHITE, 0.8f));
                    }
                }
            }
            currentX += 4.0f * blockSize; 
        }
    }
    
    void DrawCenteredButtonText(string text, Rectangle rec, float size, Color c) {
        float ttfSize = size * 6.0f; 
        Vector2 textSize = MeasureTextEx(arialFont, text.c_str(), ttfSize, 2.0f);
        
        if (textSize.x > rec.width * 0.85f) {
            ttfSize *= (rec.width * 0.85f) / textSize.x;
            textSize = MeasureTextEx(arialFont, text.c_str(), ttfSize, 2.0f);
        }

        float px = rec.x + (rec.width - textSize.x) / 2.0f;
        float py = rec.y + (rec.height - textSize.y) / 2.0f;
        DrawTextEx(arialFont, text.c_str(), {px, py}, ttfSize, 2.0f, c);
    }

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
        float sw = SW; float sh = SH;
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
        
        float cx = sw / 2.0f;
        float cy = sh / 2.0f;
        
        float logoY = cy - 120.0f;
        float rotation = progress * 2.0f; 
        float radius = Lerp(0.0f, 80.0f, Clamp(progress * 2.0f, 0.0f, 1.0f)); 
        
        DrawHexagonWire({cx, logoY}, radius, rotation, 4.0f, mainCol);
        DrawHexagonWire({cx, logoY}, radius * 0.8f, -rotation * 1.5f, 2.0f, secCol);
        
        float sF = min(sw, sh) / 1080.0f; 
        
        if (progress > 1.0f) {
            float bAlpha = Clamp((progress - 1.0f) * 2.0f, 0.0f, 1.0f) * alpha;
            float bSize = 12.0f * sF;
            DrawTetrisText("B", cx - MeasureTetrisText("B", bSize)/2, logoY - (bSize*2.5f), bSize, 0.0f, false, ColorAlpha(WHITE, bAlpha));
        }
        
        float t1Size = 14.0f * sF; 
        float t2Size = 8.0f * sF;
        
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
    
    int GetRandomPiece() { uniform_int_distribution<int> dist(0, pieces.size() - 1); return dist(rng); }

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
            if (currentMusicTrack > 1) { currentMusicTrack = 1; LoadNextMusic(); }
            else musicEnabled = false; 
        }
    }

    void ShuffleMusic() {
        vector<int> availableTracks;
        for (int i = 1; i <= 14; i++) {
            if (FileExists(TextFormat("music%d.mp3", i))) {
                availableTracks.push_back(i);
            }
        }

        if (availableTracks.size() > 1) {
            int prev = currentMusicTrack;
            for (int i = 0; i < 14; i++) {
                int randomIdx = GetRandomValue(0, (int)availableTracks.size() - 1);
                currentMusicTrack = availableTracks[randomIdx];
                if (currentMusicTrack != prev) break;
            }
        } else if (availableTracks.size() == 1) {
            currentMusicTrack = availableTracks[0];
        } else {
            int prev = currentMusicTrack;
            for(int i = 0; i < 14; i++) {
                currentMusicTrack = GetRandomValue(1, 14);
                if (currentMusicTrack != prev) break;
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

        int p2 = GetRandomPiece();
        nextPiece = pieces[p2].shape;
        nextColor = pieces[p2].colorID;

        canHold = true; 

        if (!IsValidMove(currentPiece, currentX, currentY)) {
            if (lives > 0) {
                lives--; 
                for(int i=0; i<BOARD_HEIGHT; i++) {
                    for(int j=0; j<currentGridWidth; j++) {
                        if(board[i][j] != 0) {
                            if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 10.0f);
                            board[i][j] = 0;
                        }
                    }
                }
                TocarSom(sndGameOver); 
                cameraShakeTimer = 0.5f; cameraShakeIntensity = 2.5f; damageVignette = 1.0f;
                currentX = currentGridWidth / 2 - currentPiece[0].size() / 2;
                currentY = -5; renderFallY = -5.0f; renderX = currentX;
            } else if (totalContinues > 0) {
                isContinuing = true; continueTimer = 9.99f; TocarSom(sndGameOver);
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
                        if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(currentX + j, currentY + i), pieceColors[currentColor], 8, 5.0f);
                    }
                }
            }
        }

        if (blockPlacedOut) {
            if (lives > 0) {
                lives--; 
                for(int i=0; i<BOARD_HEIGHT; i++) {
                    for(int j=0; j<currentGridWidth; j++) {
                        if(board[i][j] != 0) {
                            if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 10.0f);
                            board[i][j] = 0;
                        }
                    }
                }
                TocarSom(sndGameOver); 
                cameraShakeTimer = 0.5f; cameraShakeIntensity = 2.5f; damageVignette = 1.0f;
                SpawnPiece();
            } else if (totalContinues > 0) {
                isContinuing = true; continueTimer = 9.99f; TocarSom(sndGameOver);
            } else {
                gameOver = true; TocarSom(sndGameOver);
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
                mensagemEspecial = (currentLanguage == 0) ? "SISTEMA EXPANDIDO" : "SYSTEM EXPANDED"; 
                timerMensagem = 3.0f;
                cameraShakeTimer = 1.0f; cameraShakeIntensity = 3.5f;
                hitStopTimer = 0.2f; 
                TocarSom(sndClear4); 
            } else {
                mensagemEspecial = (currentLanguage == 0) ? "PODER MAXIMO!" : "MAX POWER OVERDRIVE!"; 
                score += 2000 * level; timerMensagem = 3.0f;
                if (graphicsQuality > 0) SpawnParticles3D({0, BOARD_HEIGHT / 2.0f, 0}, C_GOLD, 150, 25.0f, 3);
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
                    if (graphicsQuality > 0) SpawnParticles3D(blockPos, pieceColors[board[i][j]], 25, 20.0f, board[i][j]);
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
            
            if (isExpansiveMode) {
                stars += linesClearedNow;
                if (stars >= 10) {
                    stars -= 10; nextIsBrilliant = true; 
                    mensagemEspecial = (currentLanguage == 0) ? "PECA MAGICA PRONTA" : "MAGIC PIECE READY"; 
                    timerMensagem = 3.0f;
                    TocarSom(sndClear2);
                }
            }

            cameraShakeTimer = 0.5f + (linesClearedNow * 0.15f);
            cameraShakeIntensity = linesClearedNow * 2.5f; 
            cameraFovTarget = 45.0f - (linesClearedNow * 3.0f); 
            hitStopTimer = linesClearedNow * 0.05f; 
            
            if(linesClearedNow >= 4) {
                goldTint = 1.0f; 
                if (graphicsQuality > 0) SpawnShockwave({0, avgClearPos.y, 0}, C_GOLD);
            }

            int ptsGained = 0;
            if (linesClearedNow == 1) { ptsGained = 100 * level; TocarSom(sndClear1); } 
            else if (linesClearedNow == 2) { ptsGained = 300 * level; TocarSom(sndClear2); } 
            else if (linesClearedNow == 3) { ptsGained = 500 * level; TocarSom(sndClear3); } 
            else if (linesClearedNow >= 4) { ptsGained = 800 * level; cameraShakeIntensity = 6.0f; TocarSom(sndClear4); }
            
            ptsGained *= comboCount;
            score += ptsGained;
            
            if (comboCount == 2) { mensagemEspecial = (currentLanguage == 0) ? "BOM!" : "NICE!"; timerMensagem = 2.0f; }
            else if (comboCount == 3) { mensagemEspecial = (currentLanguage == 0) ? "MUITO BOM!" : "VERY NICE!"; timerMensagem = 2.0f; }
            else if (comboCount == 4) { mensagemEspecial = (currentLanguage == 0) ? "INCRIVEL!" : "INCREDIBLE!"; timerMensagem = 3.0f; }
            else if (comboCount >= 5) { mensagemEspecial = (currentLanguage == 0) ? "MODO DEUS!" : "GOD MODE!"; timerMensagem = 3.0f; goldTint = 1.0f; }
            else if (linesClearedNow >= 4) { mensagemEspecial = "TETRABETTA!"; timerMensagem = 3.0f; }
            else if (linesClearedNow == 3) { mensagemEspecial = (currentLanguage == 0) ? "IMPRESSIONANTE" : "IMPRESSIVE"; timerMensagem = 2.0f; }
            else if (linesClearedNow == 2) { mensagemEspecial = (currentLanguage == 0) ? "BOM" : "GOOD"; timerMensagem = 2.0f; }

            SpawnFloatingText(avgClearPos, "+" + to_string(ptsGained), C_GOLD, 1.5f + (linesClearedNow * 0.5f));
            
            if (comboCount > 1) {
                SpawnFloatingText({avgClearPos.x, avgClearPos.y + 2.0f, avgClearPos.z}, "COMBO X" + to_string(comboCount), C_GOLD, 2.0f);
            }

            if (bossActive && isBossMode) {
                bossHp -= linesClearedNow; 
                string dmgStr = (currentLanguage == 0) ? "DANO CHEFE -" : "BOSS DMG -";
                SpawnFloatingText({0, avgClearPos.y + 4.0f, 0}, dmgStr + to_string(linesClearedNow), C_RED, 2.5f);
                if (bossHp <= 0) {
                    bossActive = false; 
                    bossCinematicSpinTimer = 0.0f; 
                    
                    if (bossEncounterCount == 1) linesUntilBoss = 45;
                    else if (bossEncounterCount == 2) linesUntilBoss = 80;
                    else if (bossEncounterCount == 3) linesUntilBoss = 110;
                    else if (bossEncounterCount == 4) linesUntilBoss = 150;
                    else linesUntilBoss = 150 + (bossEncounterCount - 4) * 50; 
                    
                    score += 5000 * level;
                    if (graphicsQuality > 0) SpawnParticles3D({0, currentGridElevation + 20.0f, -5.0f}, C_GOLD, 400, 60.0f, 8); 
                    cameraShakeTimer = 2.0f; cameraShakeIntensity = 3.0f;
                    hitStopTimer = 0.5f; 
                    mensagemEspecial = (currentLanguage == 0) ? "VIRUS EXCLUIDO!" : "VIRUS DELETED!"; 
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
                    if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(j, i), pieceColors[board[i][j]], 10, 25.0f, board[i][j]);
                    board[i][j] = 0;
                }
            }
        }

        if (blocksDestroyed > 0) {
            score += blocksDestroyed * 50 * level;
            mensagemEspecial = (currentLanguage == 0) ? "PURGACAO DO SISTEMA!!!" : "SYSTEM PURGE!!!"; 
            timerMensagem = 3.0f;
            cameraShakeTimer = 1.5f; cameraShakeIntensity = 4.0f;
            cameraFovTarget = 35.0f; 
            nukeSpinAngle = PI * 4.0f; 
            hitStopTimer = 0.3f;
            goldTint = 1.0f;
            if (graphicsQuality > 0) SpawnShockwave({0, currentGridElevation, 0}, C_GOLD);
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
                if (graphicsQuality > 0) SpawnParticles3D({0, currentGridElevation + 20.0f, -5.0f}, C_RED, 800, 80.0f); 
                cameraShakeTimer = 2.0f; cameraShakeIntensity = 3.0f;
                hitStopTimer = 0.5f; 
                mensagemEspecial = (currentLanguage == 0) ? "CHEFE ELIMINADO!" : "BOSS PURGED!"; 
                timerMensagem = 4.0f;
            }
        }
    }

    void DrawSciFiBlock3D(Vector3 pos, Color baseCol, bool isReflection, bool isGhost = false, bool isBrilliant = false, float scale = 1.0f) {
        float s = CUBE_SIZE * 0.85f * scale; 

        if (isGhost) {
            rlDisableDepthMask();
            DrawCubeWires(pos, s, s, s, ColorAlpha(WHITE, 0.5f)); 
            rlEnableDepthMask();
        } else {
            Color coreCol = baseCol; 
            coreCol.a = 255; 

            if (isBrilliant) {
                float t = (float)GetTime() * 10.0f;
                coreCol = ColorFromHSV(fmod(t * 60.0f, 360.0f), 1.0f, 1.0f);
                coreCol.a = 255; 
            }
            
            DrawCube(pos, s, s, s, coreCol);
            
            if (graphicsQuality >= 1) { 
                DrawCubeWires(pos, s, s, s, BLACK); 
            }
        }
    }

    void DrawProceduralEnvironment() {
        rlBegin(RL_LINES);
        for(const auto& s : starfield) {
            Color tailCol = ColorAlpha(s.color, 0.0f);
            Vector3 tail = { 
                s.pos.x, 
                s.pos.y,
                s.pos.z - (s.speed * 0.6f)
            }; 
            
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

        if (currentState == PLAYING && !isContinuing) {
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
                        
                        Color virusColor = (GetRandomValue(0, 10) > 8) ? GRAY : C_RED;
                        
                        Vector3 bPos = {
                            ((float)j - offX + 0.5f) * 2.5f + glitchX, 
                            ((float)-i + offY - 0.5f) * 2.5f + glitchY, 
                            glitchZ
                        };
                        
                        DrawCube(bPos, 2.2f, 2.2f, 2.2f, virusColor);
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
        
        int cW = GetScreenWidth() > 0 ? GetScreenWidth() : SCREEN_WIDTH;
        int cH = GetScreenHeight() > 0 ? GetScreenHeight() : SCREEN_HEIGHT;
        bgRenderTarget = LoadRenderTexture(cW, cH);
        
        nextPieceRT = LoadRenderTexture(260, 260); 
        holdPieceRT = LoadRenderTexture(260, 260); 

        SetTextureFilter(bgRenderTarget.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(nextPieceRT.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(holdPieceRT.texture, TEXTURE_FILTER_BILINEAR);

        for(int i = 0; i < 400; i++) {
            starfield.push_back({
                { GetRandomFloat(-300.0f, 300.0f), GetRandomFloat(-100.0f, 200.0f), GetRandomFloat(-400.0f, 0.0f) },
                GetRandomFloat(80.0f, 250.0f), 1.0f, ColorAlpha(WHITE, GetRandomFloat(0.3f, 1.0f)) 
            });
        }

        sndMove = LoadSound("move.mp3"); sndRotate = LoadSound("rotate.mp3"); sndDrop = LoadSound("drop.mp3");
        sndClear1 = LoadSound("clear1.mp3"); sndClear2 = LoadSound("clear2.mp3"); sndClear3 = LoadSound("clear3.mp3");
        sndClear4 = LoadSound("clear4.mp3"); sndGameOver = LoadSound("gameover.mp3");
        
        // Inicializa a Fonte Padrão
        arialFont = LoadFont("arial.ttf");
        SetTextureFilter(arialFont.texture, TEXTURE_FILTER_BILINEAR);
        
        ShuffleMusic(); 
        SpawnPiece();

        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        hitStopLoc = GetShaderLocation(postProcessShader, "hitStop");
        dmgVignetteLoc = GetShaderLocation(postProcessShader, "damageVignette");
        goldTintLoc = GetShaderLocation(postProcessShader, "goldTint");
        
        float res[2] = { (float)cW, (float)cH };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
    }

    ~JogoTetris3D() {
        if (sndMusic.stream.buffer != NULL) DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        UnloadRenderTexture(bgRenderTarget); 
        UnloadRenderTexture(nextPieceRT); 
        UnloadRenderTexture(holdPieceRT); 
        UnloadShader(postProcessShader);
        
        UnloadFont(arialFont);
        
        UnloadSound(sndMove); UnloadSound(sndRotate); UnloadSound(sndDrop); UnloadSound(sndClear1);
        UnloadSound(sndClear2); UnloadSound(sndClear3); UnloadSound(sndClear4); UnloadSound(sndGameOver);
        
        if (sndMusic.stream.buffer != NULL) UnloadMusicStream(sndMusic);
    }

    void Update(float dt) {
        
        int cW = GetScreenWidth();
        int cH = GetScreenHeight();
        if (cW > 0 && cH > 0 && (bgRenderTarget.texture.width != cW || bgRenderTarget.texture.height != cH)) {
            UnloadRenderTexture(bgRenderTarget);
            bgRenderTarget = LoadRenderTexture(cW, cH);
            SetTextureFilter(bgRenderTarget.texture, TEXTURE_FILTER_BILINEAR);
            float res[2] = { (float)cW, (float)cH };
            SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);
        }

        if (musicEnabled && sndMusic.stream.buffer != NULL) {
            UpdateMusicStream(sndMusic);
            if (GetMusicTimePlayed(sndMusic) >= GetMusicTimeLength(sndMusic) - 0.1f) {
                currentMusicTrack++;
                if (currentMusicTrack > 14) currentMusicTrack = 1; 
                LoadNextMusic(); 
            }
            musicPulse = Lerp(musicPulse, globalMusicAmplitude * 15.0f, dt * 20.0f);
        } else musicPulse = Lerp(musicPulse, 0.0f, dt * 5.0f);

        ProcessTouchInputs();

        if (hitStopTimer > 0.0f) {
            hitStopTimer -= GetFrameTime(); 
            dt *= 0.1f; 
        }

        if (graphicsQuality > 0) {
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
        }

        if (damageVignette > 0.0f) damageVignette = Lerp(damageVignette, 0.0f, dt * 2.0f);
        if (goldTint > 0.0f) goldTint = Lerp(goldTint, 0.0f, dt * 1.5f); 
        if (currentRotAngle != 0.0f) currentRotAngle = Lerp(currentRotAngle, 0.0f, dt * 15.0f); 
        
        if (btnMusicPressed) {
            musicEnabled = !musicEnabled;
            if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic);
            TocarSom(sndMove);
        }

        if (btnShufflePressed) {
            ShuffleMusic();
            TocarSom(sndMove);
        }
        if (btnNextPressed || IsKeyPressed(KEY_K)) {
            currentMusicTrack++;
            if (currentMusicTrack > 14) currentMusicTrack = 1;
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
            int maxSel = 5; 

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { (*sel)--; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { (*sel)++; TocarSom(sndMove); }
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
                    if (*sel == 0) { ToggleFullscreen(); isFullscreen = !isFullscreen; }
                    else if (*sel == 1) sfxEnabled = !sfxEnabled;
                    else if (*sel == 2) { musicEnabled = !musicEnabled; if(!musicEnabled) PauseMusicStream(sndMusic); else ResumeMusicStream(sndMusic); }
                    else if (*sel == 3) { graphicsQuality++; if(graphicsQuality > 2) graphicsQuality = 0; }
                    else if (*sel == 4) { currentLanguage = (currentLanguage == 0) ? 1 : 0; }
                    else if (*sel == 5) currentState = MENU;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) || btnPausePressed || btnEscPressed) if(currentState == SETTINGS) currentState = MENU;
        }
        else if (currentState == CREDITS) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || btnDropPressed || btnPausePressed || btnEscPressed) { TocarSom(sndDrop); currentState = MENU; }
        }
        else if (currentState == PLAYING) {
            
            if (isContinuing) {
                continueTimer -= dt;
                
                if (IsKeyPressed(KEY_Y) || btnYesPressed) {
                    isContinuing = false;
                    totalContinues--; 
                    lives = 3; 
                    for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
                    SpawnPiece();
                    TocarSom(sndClear2);
                } 
                else if (IsKeyPressed(KEY_N) || btnNoPressed || continueTimer <= 0.0f) {
                    isContinuing = false;
                    gameOver = true; 
                    TocarSom(sndGameOver);
                }
                return; 
            }

            if (IsKeyPressed(KEY_P) || btnPausePressed) { isPaused = !isPaused; TocarSom(sndMove); }
            if (IsKeyPressed(KEY_ESCAPE) || btnMenuPressed || btnEscPressed) showExitPrompt = !showExitPrompt; 

            if (showExitPrompt) {
                if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_Y) || btnYesPressed) { showExitPrompt = false; currentState = MENU; }
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
                bossOrbitAngle += dt * 1.5f; 
                
                bossCinematicCooldown -= dt;
                if (bossCinematicCooldown <= 0.0f && bossCinematicSpinTimer <= 0.0f) {
                    bossCinematicSpinTimer = 2.0f; 
                    bossCinematicCooldown = GetRandomFloat(20.0f, 30.0f); 
                    mensagemEspecial = (currentLanguage == 0) ? "AVISO!" : "WARNING!"; 
                    timerMensagem = 2.0f;
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
                if (comboTimer > 0.0f) {
                    comboTimer -= dt;
                    if (comboTimer <= 0.0f) {
                        comboTimer = 0.0f;
                        if (comboCount > 0) {
                            if (graphicsQuality > 0) SpawnParticles3D({0, currentGridElevation + BOARD_HEIGHT/2.0f, 0}, C_GOLD, 80, 50.0f, 3);
                            if (graphicsQuality > 0) SpawnParticles3D({0, currentGridElevation + BOARD_HEIGHT/2.0f, 0}, C_RED, 60, 80.0f, 1);
                            TocarSom(sndDrop); 
                            cameraShakeTimer = 0.5f; 
                            cameraShakeIntensity = 3.5f;
                        }
                        comboCount = 0; 
                    }
                }

                float speed = fmax(0.08f, 0.8f - (level * 0.03f)); 
                fallTimer += dt;

                if ((IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL) || btnNukePressed) && bombs > 0) { bombs--; NukeBoard(); }

                if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT) || btnHoldPressed) {
                    if (canHold) {
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
                        if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 10, 5.0f);
                    }
                }

                cameraBankAngle = Lerp(cameraBankAngle, 0.0f, dt * 10.0f); 

                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || btnLeftPressed) {
                    if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
                    moveLeftTimer = 0.0f; 
                } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A) || btnLeftDown) {
                    moveLeftTimer += dt;
                    if (moveLeftTimer >= DAS_DELAY) {
                        moveLeftTimer -= ARR_RATE;
                        if (IsValidMove(currentPiece, currentX - 1, currentY)) { currentX--; TocarSom(sndMove); }
                    }
                } else { moveLeftTimer = 0.0f; }

                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || btnRightPressed) {
                    if (IsValidMove(currentPiece, currentX + 1, currentY)) { currentX++; TocarSom(sndMove); if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); }
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
                    if (IsValidMove(rotated, currentX, currentY)) { 
                        currentPiece = rotated; TocarSom(sndRotate); 
                        currentRotAngle = 90.0f; 
                        if (graphicsQuality > 0) SpawnParticles3D(GetWorldPos(currentX, currentY), pieceColors[currentColor], 5, 5.0f);
                    } else if (IsValidMove(rotated, currentX - 1, currentY)) { 
                        currentPiece = rotated; currentX--; TocarSom(sndRotate); currentRotAngle = 90.0f;
                    } else if (IsValidMove(rotated, currentX + 1, currentY)) { 
                        currentPiece = rotated; currentX++; TocarSom(sndRotate); currentRotAngle = 90.0f;
                    }
                }
                
                if (IsKeyPressed(KEY_SPACE) || btnDropPressed) { 
                    int dropDist = 0;
                    while (IsValidMove(currentPiece, currentX, currentY + 1)) { currentY++; dropDist++; }
                    score += dropDist * 2;
                    if (graphicsQuality > 0) SpawnShockwave(GetWorldPos(currentX + currentPiece[0].size()/2.0f, currentY + currentPiece.size()/2.0f), WHITE);
                    LockPiece(); fallTimer = 0;
                    cameraShakeTimer = 0.3f; cameraShakeIntensity = fmax(2.0f, dropDist * 0.2f); 
                } 
                else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S) || btnDownDown) { 
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

        float sw = SW;
        float sh = SH;
        
        float zoomMultiplier = 0.6f + ((currentGridWidth - 10) * 0.08f);
        float gridZoom = (currentGridWidth - 10) * zoomMultiplier + manualZoomOffset;
        
        float baseCamY = 3.5f; 
        float baseTargetY = 11.5f;
        float finalDist = 34.0f + gridZoom;

        cameraFovTarget = Lerp(cameraFovTarget, 45.0f, dt * 2.0f); 
        currentGridElevation = Lerp(currentGridElevation, (bossActive || bossEntryAnim > 0.01f) ? Lerp(2.0f, 7.0f, bossEntryAnim) : 2.0f, dt * 3.0f);

        float baseOrbitAngle = (float)GetTime() * 0.2f + nukeSpinAngle; 
        
        float camX = manualCamPan.x - 1.0f + (float)sin(baseOrbitAngle) * 2.0f + (float)sin(manualCamAngleX) * finalDist;
        float camZ = manualCamPan.z + (float)cos(manualCamAngleX) * finalDist;
        float camY = manualCamPan.y + currentGridElevation + baseCamY + (float)sin(manualCamAngleY) * finalDist;

        Vector3 targetPosNormal;
        targetPosNormal.x = camX; targetPosNormal.y = camY; targetPosNormal.z = camZ;
        
        Vector3 targetTargetNormal;
        targetTargetNormal.x = manualCamPan.x; targetTargetNormal.y = currentGridElevation + baseTargetY + manualCamPan.y; targetTargetNormal.z = manualCamPan.z;

        float bossDist = 20.0f + sin(bossOrbitAngle * 0.45f) * 15.0f + (gridZoom * 0.5f); 
        
        currentBossPos.x = manualCamPan.x + (float)sin(bossOrbitAngle) * bossDist;
        currentBossPos.y = manualCamPan.y + currentGridElevation + Lerp(45.0f, 12.0f + sin(bossOrbitAngle * 0.7f) * 14.0f, bossEntryAnim);
        currentBossPos.z = manualCamPan.z + (float)cos(bossOrbitAngle) * bossDist;

        Vector3 targetPosBoss;
        targetPosBoss.x = targetPosNormal.x; targetPosBoss.y = targetPosNormal.y; targetPosBoss.z = targetPosNormal.z + 5.0f;
        
        Vector3 targetTargetBoss;
        targetTargetBoss.x = manualCamPan.x; targetTargetBoss.y = currentGridElevation + 12.0f + manualCamPan.y; targetTargetBoss.z = manualCamPan.z;

        if (bossActive && bossCinematicSpinTimer > 0.0f) {
            float spinProgress = 1.0f - (bossCinematicSpinTimer / 2.0f); 
            float easedSpin = spinProgress * spinProgress * (3.0f - 2.0f * spinProgress); 
            float spinAngle = easedSpin * PI * 2.0f + manualCamAngleX; 
            
            float radius = finalDist + 5.0f; 
            
            targetPosBoss.x = manualCamPan.x + (float)sin(spinAngle) * radius;
            targetPosBoss.y = manualCamPan.y + currentGridElevation + 5.0f + sin(spinProgress*PI)*10.0f + (float)sin(manualCamAngleY) * radius;
            targetPosBoss.z = manualCamPan.z + (float)cos(spinAngle) * radius;
            
            targetTargetBoss.x = manualCamPan.x;
            targetTargetBoss.y = currentGridElevation + 12.0f + manualCamPan.y;
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
            Camera3D queueCam = { { 0.0f, 0.0f, 12.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE }; 
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
                if (graphicsQuality > 0) DrawProceduralEnvironment(); 
                DrawPlayfieldAndPieces(false);
                if (graphicsQuality > 0) DrawBossEncounter();
                UpdateAndDrawParticles3D(GetFrameTime());
            EndMode3D();
            
            DrawFloatingTexts(GetFrameTime());
        }
        
        EndTextureMode();

        ClearBackground(BLACK);
        float sw = SW;
        float sh = SH;

        Rectangle source = { 0, 0, (float)bgRenderTarget.texture.width, -(float)bgRenderTarget.texture.height };
        Rectangle dest = { 0, 0, sw, sh };
        
        if (graphicsQuality >= 1) {
            BeginShaderMode(postProcessShader);
            DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
            EndShaderMode();
        } else {
            DrawTexturePro(bgRenderTarget.texture, source, dest, {0,0}, 0.0f, WHITE);
        }

        DrawUI();
        DrawHUDTouch();
    }

    void DrawUI() {
        float sw = SW;
        float sh = SH;

        if (currentState == INTRO) return; 

        if (currentState == MENU) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.7f));
            
            float titleBlockSize = 24.0f;
            string title = "TETRABETTA";
            float titleWidth = MeasureTetrisText(title, titleBlockSize);
            float logoStartX = (sw / 2.0f) - (titleWidth / 2.0f);
            DrawTetrisText(title, logoStartX, 100.0f, titleBlockSize, musicPulse, true, WHITE);

            const char* menuItemsPT[] = { "MODO CLASSICO", "MODO EXPANSIVO", "MODO CHEFE", "CONFIGURACOES", "CREDITOS", "SAIR" };
            const char* menuItemsEN[] = { "CLASSIC RUN", "EXPANSIVE RUN", "BOSS RUSH", "SYSTEM CONFIG", "CREDITS", "LOGOUT" };
            const char** menuItems = (currentLanguage == 0) ? menuItemsPT : menuItemsEN;

            for (int i = 0; i < 6; i++) {
                Color c = (i == menuSelection) ? WHITE : GRAY;
                float itemBlockSize = 10.0f;
                
                string text = menuItems[i];
                float baseWidth = MeasureTetrisText(text, itemBlockSize);
                float itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);
                
                float yPos = 300.0f + i * 100.0f; 

                Rectangle itemRect = { itemStartX - 40.0f, yPos - 20.0f, baseWidth + 80.0f, itemBlockSize * 5.0f + 40.0f };
                if (i == menuSelection) {
                     DrawRectangleRounded(itemRect, 0.3f, 10, ColorAlpha(WHITE, 0.2f));
                     DrawRectangleRoundedLines(itemRect, 0.3f, 10, ColorAlpha(WHITE, 0.5f));
                } else {
                     DrawRectangleRounded(itemRect, 0.3f, 10, ColorAlpha(BLACK, 0.6f));
                     DrawRectangleRoundedLines(itemRect, 0.3f, 10, ColorAlpha(GRAY, 0.3f));
                }

                DrawTetrisText(text, itemStartX, yPos, itemBlockSize, 0.0f, false, c);
            }

            string inst = (currentLanguage == 0) ? "TOQUE NA OPCAO DESEJADA" : "TOUCH THE DESIRED OPTION";
            float instSize = 14.0f; 
            DrawTetrisText(inst, sw/2 - MeasureTetrisText(inst, instSize)/2, sh - 100.0f, instSize, 0.0f, false, C_CYAN);
        } 
        else if (currentState == SETTINGS) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.8f));
            
            string title = (currentLanguage == 0) ? "CONFIGURACOES" : "SYSTEM CONFIG";
            float titleSize = 12.0f;
            float titleWidth = MeasureTetrisText(title, titleSize);
            DrawTetrisText(title, (sw/2.0f) - (titleWidth/2.0f), 150.0f, titleSize, musicPulse * 0.2f, false, C_CYAN);

            string opt1 = (currentLanguage == 0) ? string("TELA CHEIA: ") + (isFullscreen ? "ON" : "OFF") : string("FULLSCREEN: ") + (isFullscreen ? "ON" : "OFF");
            string opt2 = (currentLanguage == 0) ? string("EFEITOS SONOROS: ") + (sfxEnabled ? "ON" : "OFF") : string("HAPTIC SFX: ") + (sfxEnabled ? "ON" : "OFF");
            string opt3 = (currentLanguage == 0) ? string("MUSICA: ") + (musicEnabled ? "ON" : "OFF") : string("SYNTHWAVE: ") + (musicEnabled ? "ON" : "OFF");
            
            string gQualPT = (graphicsQuality == 2 ? "ALTO" : (graphicsQuality == 1 ? "MED" : "BAIXO"));
            string gQualEN = (graphicsQuality == 2 ? "HIGH" : (graphicsQuality == 1 ? "MED" : "LOW"));
            string opt4 = (currentLanguage == 0) ? string("GRAFICOS: ") + gQualPT : string("GRAPHICS: ") + gQualEN;
            
            string opt5 = (currentLanguage == 0) ? "IDIOMA: PT-BR" : "LANGUAGE: ENGLISH";
            string opt6 = (currentLanguage == 0) ? "VOLTAR" : "RETURN";

            const char* setItems[] = { opt1.c_str(), opt2.c_str(), opt3.c_str(), opt4.c_str(), opt5.c_str(), opt6.c_str() };
            for (int i = 0; i < 6; i++) {
                Color c = (i == settingsSelection) ? WHITE : GRAY;
                float itemSize = 9.0f;
                
                float baseWidth = MeasureTetrisText(setItems[i], itemSize);
                float itemStartX = (sw / 2.0f) - (baseWidth / 2.0f);
                float yPos = 300.0f + i * 100.0f; 

                Rectangle itemRect = { itemStartX - 40.0f, yPos - 20.0f, baseWidth + 80.0f, itemSize * 5.0f + 40.0f };
                if (i == settingsSelection) {
                     DrawRectangleRounded(itemRect, 0.3f, 10, ColorAlpha(WHITE, 0.2f));
                     DrawRectangleRoundedLines(itemRect, 0.3f, 10, ColorAlpha(WHITE, 0.5f));
                } else {
                     DrawRectangleRounded(itemRect, 0.3f, 10, ColorAlpha(BLACK, 0.6f));
                     DrawRectangleRoundedLines(itemRect, 0.3f, 10, ColorAlpha(GRAY, 0.3f));
                }

                DrawTetrisText(setItems[i], itemStartX, yPos, itemSize, 0.0f, false, c);
            }

            string inst = (currentLanguage == 0) ? "TOQUE NA OPCAO DESEJADA" : "TOUCH THE DESIRED OPTION";
            float instSize = 14.0f;
            DrawTetrisText(inst, sw/2 - MeasureTetrisText(inst, instSize)/2, sh - 100.0f, instSize, 0.0f, false, C_CYAN);
        }
        else if (currentState == CREDITS) {
            DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.9f));
            
            float size1 = 8.0f;
            float size2 = 12.0f;
            
            string t1 = (currentLanguage == 0) ? "PRODUTOR E PROGRAMADOR" : "PRODUCER AND CODER";
            DrawTetrisText(t1, sw/2 - MeasureTetrisText(t1, size1)/2, sh * 0.4f, size1, 0, false, C_ORANGE);
            
            string t2 = "IGOR BETTARELLO XAVIER";
            DrawTetrisText(t2, sw/2 - MeasureTetrisText(t2, size2)/2, sh * 0.5f, size2, musicPulse*0.2f, true, WHITE); 

            Rectangle recRet = { sw/2.0f - 150.0f, sh - 150.0f, 300.0f, 80.0f };
            DrawRectangleRounded(recRet, 0.5f, 10, ColorAlpha(C_RED, 0.5f));
            DrawRectangleRoundedLines(recRet, 0.5f, 10, C_ORANGE);
            DrawCenteredButtonText((currentLanguage == 0) ? "VOLTAR" : "RETURN", recRet, 5.0f, WHITE);
        }
        else if (currentState == PLAYING) {
            
            DrawFrameWithCubes2D({40, 40, 420, 500}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f)); 
            
            DrawTetrisText(string(currentLanguage == 0 ? "PONTOS: " : "SCORE: ") + TextFormat("%08d", score), 60, 95, 4.0f, 0, false, WHITE);
            DrawTetrisText(string(currentLanguage == 0 ? "NIVEL: " : "LEVEL: ") + TextFormat("%02d", level), 60, 140, 4.0f, 0, false, C_ORANGE);
            DrawTetrisText(string(currentLanguage == 0 ? "VIDAS: " : "LIVES: ") + TextFormat("%d", lives), 60, 185, 4.0f, 0, false, C_RED); 
            DrawTetrisText(string(currentLanguage == 0 ? "FICHAS: " : "CONTINUES: ") + TextFormat("%d", totalContinues), 60, 230, 4.0f, 0, false, C_RED); 
            
            int textStartY = 280; 
            
            if (isExpansiveMode) {
                DrawTetrisText((currentLanguage == 0 ? "CARGA MAGICA:" : "MAGIC CHARGE:"), 60, textStartY, 3.0f, 0, false, WHITE);
                DrawRectangleLines(60, textStartY + 30, 300, 20, C_YELLOW);
                DrawRectangle(60, textStartY + 30, 300 * (stars/10.0f), 20, ColorAlpha(C_YELLOW, 0.7f + sin(GetTime()*10)*0.3f));
                textStartY += 70;
            }

            string hText = (currentLanguage == 0 ? "PECA RESERVA" : "HOLD PIECE");
            DrawTetrisText(hText, 60, textStartY, 3.0f, 0, false, canHold ? C_GOLD : GRAY);
            
            DrawFrameWithCubes2D({60, (float)textStartY + 40, 140, 140}, 4.0f, canHold ? C_GOLD : GRAY, ColorAlpha(BLACK, 0.5f));
            Rectangle sourceHRT = { 0, 0, (float)holdPieceRT.texture.width, -(float)holdPieceRT.texture.height };
            Rectangle destHRT = { 65.0f, (float)textStartY + 45.0f, 130.0f, 130.0f };
            if (!gameOver && !isPaused && !isContinuing) {
                DrawTexturePro(holdPieceRT.texture, sourceHRT, destHRT, {0, 0}, 0.0f, WHITE);
            }
            
            DrawFrameWithCubes2D({sw - 360.0f, 40.0f, 320.0f, 280.0f}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f));
            
            string qText = (currentLanguage == 0 ? "PROXIMA PECA" : "NEXT BLOCK"); 
            DrawTetrisText(qText, sw - 200 - MeasureTetrisText(qText, 4.5f)/2, 60, 4.5f, 0, false, C_GOLD); 

            Rectangle sourceRT = { 0, 0, (float)nextPieceRT.texture.width, -(float)nextPieceRT.texture.height };
            Rectangle destRT = { sw - 330.0f, 80.0f, 260.0f, 260.0f };
            if (!gameOver && !isPaused && !isContinuing) {
                DrawTexturePro(nextPieceRT.texture, sourceRT, destRT, {0, 0}, 0.0f, WHITE);
            }

            if (bossActive && isBossMode) {
                float glitchX = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;
                float glitchY = (GetRandomValue(0, 10) > 8) ? GetRandomFloat(-8.0f, 8.0f) : 0.0f;

                float bossW = 800;
                float bossCX = sw/2 - bossW/2;
                float bY = 40.0f; 
                
                DrawFrameWithCubes2D({bossCX + glitchX, bY + glitchY, bossW, 80.0f}, 8.0f, C_RED, ColorAlpha(C_BG, 0.7f));
                
                string bName = (currentLanguage == 0) ? TextFormat("ANOMALIA: OMEGARED V.%d", bossEncounterCount) : TextFormat("ANOMALY: OMEGARED V.%d", bossEncounterCount);
                float bSize = 5.0f;
                DrawTetrisText(bName, sw/2 - MeasureTetrisText(bName, bSize)/2 + glitchX, bY + 10 + glitchY, bSize, 0, false, C_RED);
                
                float hpRatio = (float)bossHp / (10.0f + (bossEncounterCount * 5.0f)); 
                DrawRectangleRounded({bossCX + 20 + glitchX, bY + 45.0f + glitchY, (bossW - 40) * hpRatio, 20.0f}, 0.5f, 8, C_ORANGE);
            }

            if (comboCount > 0 && comboTimer > 0.0f && !isContinuing) {
                float popScale = 1.0f + (musicPulse * 0.05f);
                float comboSize = 8.0f * popScale;
                string cMult = TextFormat("COMBO X%d", comboCount);
                float cWidth = MeasureTetrisText(cMult, comboSize);
                
                DrawTetrisTextGlowing(cMult, sw/2.0f - cWidth/2.0f, sh/2.0f - 80.0f, comboSize, musicPulse * 0.2f);
                
                string tVal = TextFormat("%.2f", comboTimer);
                float tSize = 10.0f * popScale;
                float tPulse = (comboTimer <= 2.0f) ? abs(sin((float)GetTime() * 15.0f)) * 0.8f : 0.0f;
                float tWidth = MeasureTetrisText(tVal, tSize);

                if (comboTimer <= 2.0f) {
                    DrawTetrisText(tVal, sw/2.0f - tWidth/2.0f, sh/2.0f, tSize, tPulse, false, C_RED);
                } else {
                    DrawTetrisTextGlowing(tVal, sw/2.0f - tWidth/2.0f, sh/2.0f, tSize, tPulse);
                }
            }

            if (timerMensagem > 0 && !isContinuing) {
                float popScale = ElasticEaseOut(1.0f - (timerMensagem / 3.0f));
                float fontSize = 12.0f * popScale; 
                float textWidth = MeasureTetrisText(mensagemEspecial, fontSize);
                DrawTetrisTextGlowing(mensagemEspecial, sw/2 - textWidth/2, sh/3 - 50.0f, fontSize, 0.0f);
            }

            if (isPaused && !isContinuing) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.8f));
                string pMsg = (currentLanguage == 0) ? "JOGO PAUSADO" : "SYSTEM PAUSED";
                float pSize = 10.0f;
                DrawTetrisText(pMsg, sw/2 - MeasureTetrisText(pMsg, pSize)/2, sh/2 - 30, pSize, 0, false, C_GOLD);
            }

            if (isContinuing) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.85f));
                
                string cMsg = (currentLanguage == 0) ? "CONTINUAR?" : "CONTINUE?";
                float cSize = 12.0f;
                DrawTetrisText(cMsg, sw/2 - MeasureTetrisText(cMsg, cSize)/2, sh/2 - 140, cSize, 0, false, C_ORANGE);
                
                int timeInt = (int)ceil(continueTimer);
                if (timeInt < 0) timeInt = 0;
                string tMsg = to_string(timeInt);
                float numberSize = 25.0f;
                DrawTetrisText(tMsg, sw/2 - MeasureTetrisText(tMsg, numberSize)/2, sh/2 - 20, numberSize, musicPulse*0.3f, false, C_RED);
            }

            if (gameOver) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(C_RED, 0.4f)); 
                DrawRectangle(0, sh/2 - 100, sw, 200, ColorAlpha(BLACK, 0.9f));
                string failMsg = (currentLanguage == 0) ? "FIM DE JOGO" : "GAME OVER";
                float fSize = 12.0f;
                DrawTetrisText(failMsg, sw/2 - MeasureTetrisText(failMsg, fSize)/2, sh/2 - 80, fSize, 0, false, C_RED);
                string rebMsg = (currentLanguage == 0) ? "APERTE EM QUALQUER LUGAR PRA VOLTAR AO MENU INICIAL" : "PRESS ANYWHERE TO RETURN TO MAIN MENU";
                float rebSize = 3.5f;
                DrawTetrisText(rebMsg, sw/2 - MeasureTetrisText(rebMsg, rebSize)/2, sh/2 + 40, rebSize, 0, false, WHITE);
            }

            if (showExitPrompt) {
                DrawRectangle(0, 0, sw, sh, ColorAlpha(BLACK, 0.95f));
                string extMsg = (currentLanguage == 0) ? "SAIR DO JOGO?" : "ABORT SIMULATION?";
                float extSize = 10.0f;
                DrawTetrisText(extMsg, sw/2 - MeasureTetrisText(extMsg, extSize)/2, sh/2 - 60, extSize, 0, false, C_ORANGE);
            }
        }
    }

    void DrawHUDTouch() {
        if(currentState == INTRO) return;
        if(currentState == MENU || currentState == SETTINGS || currentState == CREDITS) return;
        
        Color cDpad = ColorAlpha(WHITE, 0.15f);
        Color cAction = ColorAlpha(C_CYAN, 0.15f);
        Color cAlert = ColorAlpha(C_ORANGE, 0.15f);
        Color cPressed = ColorAlpha(WHITE, 0.4f);

        if (showExitPrompt || isContinuing) {
            DrawRectangleRounded(recBtnYes, 0.5f, 10, btnYesPressed ? cPressed : ColorAlpha(C_GREEN, 0.2f));
            DrawCenteredButtonText((currentLanguage == 0 ? "SIM" : "YES"), recBtnYes, 6.0f, WHITE);

            DrawRectangleRounded(recBtnNo, 0.5f, 10, btnNoPressed ? cPressed : ColorAlpha(C_RED, 0.2f));
            DrawCenteredButtonText((currentLanguage == 0 ? "NAO" : "NO"), recBtnNo, 6.0f, WHITE);
            return;
        }

        if (currentState == PLAYING) {
            DrawRectangleRounded(recBtnLeft, 0.5f, 10, btnLeftDown ? cPressed : cDpad);
            DrawCenteredButtonText("<", recBtnLeft, 8.0f, WHITE);

            DrawRectangleRounded(recBtnRight, 0.5f, 10, btnRightDown ? cPressed : cDpad);
            DrawCenteredButtonText(">", recBtnRight, 8.0f, WHITE);
            
            DrawRectangleRounded(recBtnDown, 0.5f, 10, btnDownDown ? cPressed : cDpad);
            DrawCenteredButtonText("V", recBtnDown, 8.0f, WHITE);

            DrawRectangleRounded(recBtnDrop, 0.5f, 10, btnDropPressed ? cPressed : cAction);
            DrawCenteredButtonText((currentLanguage == 0 ? "QUEDA" : "DROP"), recBtnDrop, 6.0f, WHITE);

            DrawRectangleRounded(recBtnRotate, 0.5f, 10, btnRotatePressed ? cPressed : cAction);
            DrawCenteredButtonText((currentLanguage == 0 ? "GIRAR" : "ROT"), recBtnRotate, 6.0f, WHITE);
            
            DrawRectangleRounded(recBtnHold, 0.5f, 10, btnHoldPressed ? cPressed : cAction);
            DrawCenteredButtonText("HOLD", recBtnHold, 5.0f, WHITE);

            DrawRectangleRounded(recBtnShuffle, 0.5f, 10, btnShufflePressed ? cPressed : ColorAlpha(GRAY, 0.3f));
            DrawCenteredButtonText((currentLanguage == 0 ? "MISTURAR" : "SHUFFLE"), recBtnShuffle, 4.0f, WHITE);

            DrawRectangleRounded(recBtnNext, 0.5f, 10, btnNextPressed ? cPressed : ColorAlpha(GRAY, 0.3f));
            DrawCenteredButtonText((currentLanguage == 0 ? "PROX. MUSICA" : "NEXT MUSIC"), recBtnNext, 4.0f, WHITE);
            
            if (bombs > 0) {
                DrawRectangleRounded(recBtnNuke, 0.5f, 10, btnNukePressed ? cPressed : cAlert);
                DrawCenteredButtonText((currentLanguage == 0 ? "BOMBA" : "BOMB"), recBtnNuke, 5.0f, WHITE);
            }

            DrawRectangleRounded(recBtnPause, 0.5f, 10, btnPausePressed ? cPressed : ColorAlpha(GRAY, 0.3f));
            DrawCenteredButtonText((currentLanguage == 0 ? "PAUSAR" : "PAUSE"), recBtnPause, 5.0f, WHITE);
            
            DrawRectangleRounded(recBtnMusic, 0.5f, 10, btnMusicPressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredButtonText((currentLanguage == 0 ? "MUSICA" : "MUSIC"), recBtnMusic, 5.0f, WHITE);

            DrawRectangleRounded(recBtnMenu, 0.5f, 10, btnMenuPressed ? cPressed : ColorAlpha(GRAY, 0.2f));
            DrawCenteredButtonText("MENU", recBtnMenu, 5.0f, WHITE);
            
            DrawRectangleRounded(recBtnEsc, 0.5f, 10, btnEscPressed ? cPressed : ColorAlpha(GRAY, 0.3f));
            DrawCenteredButtonText((currentLanguage == 0 ? "SAIR" : "ESC"), recBtnEsc, 5.0f, WHITE);
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j] = 0;
        score = 0; level = 1; lives = 3; totalContinues = 3; bombs = 2; stars = 0; 
        
        currentGridWidth = isExpansiveMode ? 10 : 14; 
        
        nextIsBrilliant = false; currentIsBrilliant = false; linesClearedTotal = 0;
        
        comboCount = 0; 
        comboTimer = 0.0f; 
        
        isContinuing = false;
        continueTimer = 0.0f;
        
        holdPiece.clear(); canHold = true;

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
    
    #if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
        InitWindow(0, 0, "TeTRABeTTA"); 
        LockAndroidOrientation(2); 
    #else
        InitWindow(2400, 1080, "TeTRABeTTA"); 
    #endif

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