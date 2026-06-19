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
// VARIÁVEL GLOBAL PARA O MODELO 3D CUSTOMIZADO
// =====================================================================
Model glbPieceModel; 

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
uniform float damageVignette;

void main() {
    vec2 uv = fragTexCoord;
    
    vec4 texColor = texture(texture0, uv);
    
    // Aberração Cromática leve
    float caOffset = 0.002 * sin(time * 2.0);
    float r = texture(texture0, uv + vec2(caOffset, 0.0)).r;
    float g = texColor.g;
    float b = texture(texture0, uv - vec2(caOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);
    
    // Vignette (Escurecimento nas bordas)
    vec2 center = uv - 0.5;
    float dist = length(center);
    float vignette = smoothstep(0.8, 0.3, dist);
    color *= vignette;
    
    // Damage Vignette (Avermelhado quando toma dano/perde linha do boss)
    color.r += damageVignette * smoothstep(0.4, 0.8, dist);
    
    // Scanlines sutis
    float scanline = sin(uv.y * resolution.y * 1.5) * 0.04;
    color -= scanline;
    
    finalColor = vec4(color, texColor.a);
}
)";

// =====================================================================
// CONFIGURAÇÕES E CONSTANTES GLOBAIS
// =====================================================================
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int ROWS = 20;
const int COLS = 10;
const float BLOCK_SIZE = 1.0f;
const float FALL_SPEED_START = 0.8f;
const float FALL_SPEED_MIN = 0.1f;

// =====================================================================
// FUNÇÃO UTILITÁRIA DE DESENHO (AGORA USANDO O MODELO GLB)
// =====================================================================
void DrawNeonCube(Vector3 position, float width, float height, float length, Color color) {
    Color baseColor = { (unsigned char)(color.r * 0.8f), (unsigned char)(color.g * 0.8f), (unsigned char)(color.b * 0.8f), color.a };
    
    // SUBSTITUIÇÃO: Em vez de desenhar um cubo, desenha o modelo GLB.
    // Usamos o width como fator de escala (geralmente ~0.95f no código).
    DrawModel(glbPieceModel, position, width, baseColor);
    
    // O wireframe original foi mantido para preservar o efeito "Neon" em volta da peça GLB.
    DrawCubeWires(position, width + 0.05f, height + 0.05f, length + 0.05f, color);
}

// =====================================================================
// SISTEMA DE PARTÍCULAS (MANTIDO INTACTO)
// =====================================================================
struct Particle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
};

// =====================================================================
// TEXTOS FLUTUANTES (MANTIDO INTACTO)
// =====================================================================
struct FloatingText {
    Vector3 position;
    string text;
    Color color;
    float life;
    float maxLife;
};

// =====================================================================
// DEFINIÇÕES DAS PEÇAS (MANTIDO INTACTO)
// =====================================================================
const vector<vector<vector<int>>> TETROMINOS = {
    {{1, 1, 1, 1}}, // I
    {{1, 1}, {1, 1}}, // O
    {{0, 1, 0}, {1, 1, 1}}, // T
    {{1, 0, 0}, {1, 1, 1}}, // J
    {{0, 0, 1}, {1, 1, 1}}, // L
    {{0, 1, 1}, {1, 1, 0}}, // S
    {{1, 1, 0}, {0, 1, 1}}  // Z
};

const Color COLORS[] = {
    BLANK,
    { 0, 255, 255, 255 }, // I - Ciano
    { 255, 255, 0, 255 }, // O - Amarelo
    { 128, 0, 128, 255 }, // T - Roxo
    { 0, 0, 255, 255 },   // J - Azul
    { 255, 165, 0, 255 }, // L - Laranja
    { 0, 255, 0, 255 },   // S - Verde
    { 255, 0, 0, 255 },   // Z - Vermelho
    { 200, 200, 200, 255} // Boss Block - Cinza
};

Color GetColorFromValue(int val) {
    if (val >= 0 && val <= 8) return COLORS[val];
    return MAGENTA; // Fallback
}

struct Piece {
    vector<vector<int>> shape;
    int colorIdx;
    int x, y;
    float rotationTimer = 0.0f;
    float currentRotAngle = 0.0f;
    float targetRotAngle = 0.0f;
};

// =====================================================================
// CLASSE PRINCIPAL DO JOGO (MANTIDA INTACTA, SÓ USA O DRAWCUBE MODIFICADO)
// =====================================================================
class Game {
public:
    int grid[ROWS][COLS];
    Piece currentPiece;
    Piece holdPiece;
    bool canHold;
    bool gameOver;
    bool isPaused;
    int score;
    int linesCleared;
    int level;
    float fallTimer;
    float fallSpeed;
    
    // Audio
    Music bgm[3];
    int currentBgmIndex;
    Sound clearSound;
    Sound dropSound;
    Sound moveSound;
    Sound rotateSound;
    Sound gameOverSound;
    Sound levelUpSound;
    Sound holdSound;
    Sound bossWarningSound;
    Sound explosionSound;

    // Efeitos e Shaders
    vector<Particle> particles;
    vector<FloatingText> floatingTexts;
    float timePlayed;
    float globalPulse;
    float hitStopTimer;
    float damageVignette;
    Shader postProcessShader;
    int timeLoc, pulseLoc, hitStopLoc, damageVignetteLoc, resLoc;
    RenderTexture2D targetBuffer;
    
    // Câmera Dinâmica
    Camera3D camera;
    Vector3 defaultCamPos;
    Vector3 defaultCamTarget;
    float camShakeTimer;
    float camShakeIntensity;
    Vector3 manualCamPan;
    
    // Animações da Grid
    float gridExpansionTimer;
    float currentGridElevation;
    float moveLeftTimer;
    float moveRightTimer;
    
    // Mecânicas Especiais
    string mensagemEspecial;
    float timerMensagem;
    int combo;
    bool backToBack;
    
    // BOSS FIGHT MECHANICS
    bool bossActive;
    float bossEntryAnim;
    int linesUntilBoss;
    int bossEncounterCount;
    float bossAttackTimer;
    float currentBossAttackDelay;
    float bossOrbitAngle;
    int bossHealth;
    float bossCinematicSpinTimer;
    float bossCinematicCooldown;
    float nukeSpinAngle;

    // UI State
    bool confirmExit;

    Game() {
        memset(grid, 0, sizeof(grid));
        score = 0; linesCleared = 0; level = 1; combo = 0; backToBack = false;
        fallTimer = 0.0f; fallSpeed = FALL_SPEED_START; timePlayed = 0.0f; globalPulse = 0.0f;
        camShakeTimer = 0.0f; camShakeIntensity = 0.0f;
        manualCamPan.x = 0.0f; manualCamPan.y = 0.0f; manualCamPan.z = 0.0f;
        canHold = true; holdPiece.colorIdx = 0; gameOver = false; isPaused = false;
        confirmExit = false;
        timerMensagem = 0.0f; mensagemEspecial = "";
        gridExpansionTimer = 1.0f; currentGridElevation = 0.0f;
        moveLeftTimer = 0.0f; moveRightTimer = 0.0f;
        hitStopTimer = 0.0f; damageVignette = 0.0f;
        
        // Boss Configs
        bossActive = false; bossEntryAnim = 0.0f; linesUntilBoss = 15; bossEncounterCount = 0;
        bossAttackTimer = 0.0f; currentBossAttackDelay = 15.0f; bossOrbitAngle = 0.0f;
        bossHealth = 0; bossCinematicSpinTimer = 0.0f; bossCinematicCooldown = 15.0f;
        nukeSpinAngle = 0.0f;
    }

    void Init() {
        // Câmera
        camera.position = { 0.0f, 12.0f, 28.0f };
        camera.target = { 0.0f, 0.0f, 0.0f };
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 55.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        defaultCamPos = camera.position;
        defaultCamTarget = camera.target;

        // Render Target para Shaders
        targetBuffer = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
        
        // Carregar Shaders
        postProcessShader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
        timeLoc = GetShaderLocation(postProcessShader, "time");
        pulseLoc = GetShaderLocation(postProcessShader, "pulse");
        hitStopLoc = GetShaderLocation(postProcessShader, "hitStop");
        damageVignetteLoc = GetShaderLocation(postProcessShader, "damageVignette");
        resLoc = GetShaderLocation(postProcessShader, "resolution");
        
        float res[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
        SetShaderValue(postProcessShader, resLoc, res, SHADER_UNIFORM_VEC2);

        // Áudio (Sons Sintetizados/Carregados - simulação, na prática precisaria dos arquivos, 
        // mas o raylib não falha fatalmente se não achar, apenas avisa no console).
        // Para um código "copiar e colar" funcionar liso sem arquivos extras, idealmente 
        // usaríamos Wave gerada, mas vamos manter a estrutura.
        clearSound = LoadSound("clear.wav");
        dropSound = LoadSound("drop.wav");
        moveSound = LoadSound("move.wav");
        rotateSound = LoadSound("rotate.wav");
        gameOverSound = LoadSound("gameover.wav");
        levelUpSound = LoadSound("levelup.wav");
        holdSound = LoadSound("hold.wav");
        bossWarningSound = LoadSound("boss_warning.wav");
        explosionSound = LoadSound("explosion.wav");
        
        bgm[0] = LoadMusicStream("bgm1.mp3");
        bgm[1] = LoadMusicStream("bgm2.mp3");
        bgm[2] = LoadMusicStream("bgm_boss.mp3"); // Música do boss
        
        currentBgmIndex = GetRandomValue(0, 1);
        PlayMusicStream(bgm[currentBgmIndex]);

        SpawnPiece();
    }

    void ShuffleMusic() {
        StopMusicStream(bgm[currentBgmIndex]);
        if (bossActive) {
            currentBgmIndex = 2; // Boss theme
        } else {
            currentBgmIndex = GetRandomValue(0, 1);
        }
        PlayMusicStream(bgm[currentBgmIndex]);
    }

    void Update() {
        UpdateMusicStream(bgm[currentBgmIndex]);
        
        float dt = GetFrameTime();
        
        // Atualização de variáveis globais de tempo e shaders
        if (hitStopTimer > 0) {
            hitStopTimer -= dt;
            if (hitStopTimer < 0) hitStopTimer = 0;
            // Durante hitstop, reduzimos o dt para criar o efeito de "pausa" no impacto
            dt *= 0.1f; 
        }
        
        if (damageVignette > 0) {
            damageVignette -= dt * 2.0f;
            if (damageVignette < 0) damageVignette = 0;
        }

        timePlayed += dt;
        globalPulse = (sin(timePlayed * 5.0f) + 1.0f) * 0.5f; // 0.0 a 1.0
        
        SetShaderValue(postProcessShader, timeLoc, &timePlayed, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, pulseLoc, &globalPulse, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, hitStopLoc, &hitStopTimer, SHADER_UNIFORM_FLOAT);
        SetShaderValue(postProcessShader, damageVignetteLoc, &damageVignette, SHADER_UNIFORM_FLOAT);

        // UI Updates
        if (timerMensagem > 0) timerMensagem -= dt;
        if (gridExpansionTimer > 0) gridExpansionTimer -= dt;
        if (moveLeftTimer > 0) moveLeftTimer -= dt;
        if (moveRightTimer > 0) moveRightTimer -= dt;
        
        // Atualiza Partículas
        for (auto it = particles.begin(); it != particles.end(); ) {
            it->position = Vector3Add(it->position, Vector3Scale(it->velocity, dt));
            it->velocity.y -= 15.0f * dt; // Gravidade mais forte
            it->life -= dt;
            if (it->life <= 0) it = particles.erase(it);
            else ++it;
        }

        // Atualiza Textos Flutuantes
        for (auto it = floatingTexts.begin(); it != floatingTexts.end(); ) {
            it->position.y += 2.0f * dt;
            it->life -= dt;
            if (it->life <= 0) it = floatingTexts.erase(it);
            else ++it;
        }

        // Atualiza Rotação Suave
        if (currentPiece.rotationTimer > 0) {
            currentPiece.rotationTimer -= dt * 15.0f; // Velocidade da animação de rotação
            if (currentPiece.rotationTimer <= 0) {
                currentPiece.rotationTimer = 0;
                currentPiece.currentRotAngle = currentPiece.targetRotAngle;
            } else {
                currentPiece.currentRotAngle = Lerp(currentPiece.targetRotAngle, currentPiece.currentRotAngle, currentPiece.rotationTimer);
            }
        }

        // --- SISTEMA DE BOSS ---
        if (bossActive) {
            UpdateBoss(dt);
        } else {
            // Chance de Boss Cinematic aleatória no meio do jogo
            if (bossCinematicCooldown > 0) bossCinematicCooldown -= dt;
            else if (!gameOver && !isPaused && GetRandomValue(0, 1000) < 2) {
                bossCinematicSpinTimer = 3.0f;
                bossCinematicCooldown = 20.0f;
            }
        }
        
        if (bossCinematicSpinTimer > 0) {
            bossCinematicSpinTimer -= dt;
            manualCamPan.x = sin(timePlayed * 4.0f) * 15.0f;
            manualCamPan.z = cos(timePlayed * 4.0f) * 15.0f - 28.0f;
        } else {
            manualCamPan.x = Lerp(manualCamPan.x, 0.0f, dt * 2.0f);
            manualCamPan.z = Lerp(manualCamPan.z, 0.0f, dt * 2.0f);
        }

        // Atualiza Camera Shake
        if (camShakeTimer > 0) {
            camShakeTimer -= dt;
            camera.target.x = defaultCamTarget.x + GetRandomValue(-100, 100) * 0.001f * camShakeIntensity;
            camera.target.y = defaultCamTarget.y + GetRandomValue(-100, 100) * 0.001f * camShakeIntensity;
        } else {
            camera.target.x = Lerp(camera.target.x, defaultCamTarget.x, dt * 10.0f);
            camera.target.y = Lerp(camera.target.y, defaultCamTarget.y, dt * 10.0f);
        }
        
        camera.position.x = defaultCamPos.x + manualCamPan.x;
        camera.position.z = defaultCamPos.z + manualCamPan.z;

        // Controle de Estado
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (!confirmExit) {
                if (!gameOver) isPaused = !isPaused;
            } else {
                confirmExit = false;
            }
        }

        if (gameOver || isPaused) {
            if (gameOver && IsKeyPressed(KEY_ENTER)) Reset();
            if (isPaused && IsKeyPressed(KEY_Q)) confirmExit = true;
            return;
        }

        // --- INPUTS DE MOVIMENTO ---
        fallTimer += dt;
        
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { MovePiece(-1, 0); moveLeftTimer = 0.15f; }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { MovePiece(1, 0); moveRightTimer = 0.15f; }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { MovePiece(0, 1); fallTimer = 0; }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) RotatePiece();
        if (IsKeyPressed(KEY_SPACE)) DropPiece();
        if (IsKeyPressed(KEY_C)) HoldPiece();

        // Movimento Contínuo (DAS - Delayed Auto Shift simulado)
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            if (moveLeftTimer <= 0) { MovePiece(-1, 0); moveLeftTimer = 0.05f; }
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            if (moveRightTimer <= 0) { MovePiece(1, 0); moveRightTimer = 0.05f; }
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            fallSpeed = FALL_SPEED_MIN; 
        } else {
            fallSpeed = max(FALL_SPEED_MIN, FALL_SPEED_START - (level * 0.05f));
        }

        // Queda Natural
        if (fallTimer >= fallSpeed) {
            if (!MovePiece(0, 1)) LockPiece();
            fallTimer = 0;
        }
    }

    void TriggerBoss() {
        bossActive = true;
        bossEncounterCount++;
        bossEntryAnim = 3.0f; // 3 Segundos de animação de entrada
        bossHealth = 5 + (bossEncounterCount * 2); // Boss fica mais forte a cada vez
        linesUntilBoss = 15 + (bossEncounterCount * 5);
        currentBossAttackDelay = max(5.0f, 15.0f - (bossEncounterCount * 1.5f));
        bossAttackTimer = currentBossAttackDelay;
        
        PlaySound(bossWarningSound);
        ShuffleMusic();
        
        ShowMessage("WARNING: BOSS INCOMING!", RED);
        ScreenShake(0.8f, 2.0f);
        hitStopTimer = 0.5f;
    }
    
    void UpdateBoss(float dt) {
        bossOrbitAngle += dt * 30.0f;
        
        if (bossEntryAnim > 0) {
            bossEntryAnim -= dt;
            manualCamPan.y = sin(timePlayed * 10.0f) * (bossEntryAnim * 2.0f);
            return; // Boss não ataca durante entrada
        }
        
        bossAttackTimer -= dt;
        if (bossAttackTimer <= 0) {
            ExecuteBossAttack();
            bossAttackTimer = currentBossAttackDelay;
        }
    }
    
    void ExecuteBossAttack() {
        PlaySound(explosionSound);
        ScreenShake(0.5f, 1.0f);
        damageVignette = 1.0f;
        
        int attackType = GetRandomValue(0, 2);
        
        if (attackType == 0) {
            ShowMessage("BOSS: JUNK ROW!", PURPLE);
            // Empurra tudo pra cima e adiciona linha de lixo embaixo
            for (int y = 0; y < ROWS - 1; y++) {
                for (int x = 0; x < COLS; x++) {
                    grid[y][x] = grid[y+1][x];
                }
            }
            int hole = GetRandomValue(0, COLS - 1);
            for (int x = 0; x < COLS; x++) {
                grid[ROWS - 1][x] = (x == hole) ? 0 : 8; // 8 é Boss Block
            }
        } 
        else if (attackType == 1) {
            ShowMessage("BOSS: NUKE!", RED);
            nukeSpinAngle += 90.0f; // Visual effect trigger
            // Destrói uma área 3x3 aleatória (transforma em 0 ou blocos zumbis, vamos de 0 para não ser impossível)
            int cy = GetRandomValue(ROWS/2, ROWS-2);
            int cx = GetRandomValue(1, COLS-2);
            for(int i=-1; i<=1; i++) {
                for(int j=-1; j<=1; j++) {
                    if (grid[cy+i][cx+j] != 0) {
                        CreateExplosion(cx+j, cy+i, GetColorFromValue(grid[cy+i][cx+j]));
                        grid[cy+i][cx+j] = 0;
                    }
                }
            }
        }
        else {
            ShowMessage("BOSS: GRAVITY HEAVY!", ORANGE);
            // Força a peça atual a cair direto
            DropPiece();
        }
    }
    
    void DamageBoss() {
        bossHealth--;
        PlaySound(explosionSound);
        ScreenShake(0.4f, 0.5f);
        hitStopTimer = 0.2f;
        
        // Explode partículas na posição teórica do boss (acima da grid)
        for(int i=0; i<30; i++) {
            Particle p;
            p.position = { 0.0f, ROWS * 0.5f + 5.0f, 0.0f };
            p.velocity = { (float)GetRandomValue(-15, 15), (float)GetRandomValue(-5, 15), (float)GetRandomValue(-15, 15) };
            p.color = RED;
            p.life = p.maxLife = (float)GetRandomValue(5, 15) / 10.0f;
            p.size = (float)GetRandomValue(1, 5) / 10.0f;
            particles.push_back(p);
        }
        
        if (bossHealth <= 0) {
            ShowMessage("BOSS DEFEATED!", GREEN);
            score += 5000 * level;
            bossActive = false;
            ShuffleMusic();
        } else {
            string h = "BOSS HP: " + to_string(bossHealth);
            ShowMessage(h, YELLOW);
        }
    }

    void SpawnPiece() {
        currentPiece.colorIdx = GetRandomValue(1, 7);
        currentPiece.shape = TETROMINOS[currentPiece.colorIdx - 1];
        currentPiece.x = COLS / 2 - currentPiece.shape[0].size() / 2;
        currentPiece.y = 0;
        currentPiece.currentRotAngle = 0.0f;
        currentPiece.targetRotAngle = 0.0f;
        currentPiece.rotationTimer = 0.0f;
        
        if (!IsValid(currentPiece)) {
            gameOver = true;
            PlaySound(gameOverSound);
        }
    }

    bool IsValid(Piece p) {
        for (int y = 0; y < p.shape.size(); y++) {
            for (int x = 0; x < p.shape[y].size(); x++) {
                if (p.shape[y][x]) {
                    int nx = p.x + x;
                    int ny = p.y + y;
                    if (nx < 0 || nx >= COLS || ny >= ROWS) return false;
                    if (ny >= 0 && grid[ny][nx] != 0) return false;
                }
            }
        }
        return true;
    }

    bool MovePiece(int dx, int dy) {
        currentPiece.x += dx;
        currentPiece.y += dy;
        if (!IsValid(currentPiece)) {
            currentPiece.x -= dx;
            currentPiece.y -= dy;
            return false;
        }
        if (dx != 0) PlaySound(moveSound);
        return true;
    }

    void RotatePiece() {
        Piece temp = currentPiece;
        vector<vector<int>> rotated(temp.shape[0].size(), vector<int>(temp.shape.size()));
        for (int y = 0; y < temp.shape.size(); y++) {
            for (int x = 0; x < temp.shape[y].size(); x++) {
                rotated[x][temp.shape.size() - 1 - y] = temp.shape[y][x];
            }
        }
        temp.shape = rotated;
        
        // Wall kick simples
        if (!IsValid(temp)) {
            temp.x -= 1; if (IsValid(temp)) goto success;
            temp.x += 2; if (IsValid(temp)) goto success;
            temp.x -= 1; temp.y -= 1; if (IsValid(temp)) goto success;
            return; // Falhou
        }
        
    success:
        currentPiece = temp;
        currentPiece.targetRotAngle += 90.0f;
        currentPiece.rotationTimer = 1.0f;
        PlaySound(rotateSound);
    }

    void DropPiece() {
        int dropDistance = 0;
        while (MovePiece(0, 1)) { dropDistance++; }
        score += dropDistance * 2;
        LockPiece();
        PlaySound(dropSound);
        ScreenShake(0.1f, 0.2f);
    }

    void HoldPiece() {
        if (!canHold) return;
        PlaySound(holdSound);
        if (holdPiece.colorIdx == 0) {
            holdPiece = currentPiece;
            SpawnPiece();
        } else {
            Piece temp = currentPiece;
            currentPiece = holdPiece;
            holdPiece = temp;
            currentPiece.x = COLS / 2 - currentPiece.shape[0].size() / 2;
            currentPiece.y = 0;
            currentPiece.currentRotAngle = 0;
            currentPiece.targetRotAngle = 0;
        }
        canHold = false;
        
        // Efeito visual no hold
        for (int i = 0; i < 15; i++) {
            Particle p;
            p.position = { -COLS/2.0f - 4.0f, ROWS/2.0f - 2.0f, 0.0f };
            p.velocity = { (float)GetRandomValue(-5, 5), (float)GetRandomValue(-5, 5), (float)GetRandomValue(-2, 5) };
            p.color = GetColorFromValue(holdPiece.colorIdx);
            p.life = p.maxLife = 0.5f;
            p.size = 0.2f;
            particles.push_back(p);
        }
    }

    void LockPiece() {
        for (int y = 0; y < currentPiece.shape.size(); y++) {
            for (int x = 0; x < currentPiece.shape[y].size(); x++) {
                if (currentPiece.shape[y][x]) {
                    int ny = currentPiece.y + y;
                    int nx = currentPiece.x + x;
                    if (ny >= 0) grid[ny][nx] = currentPiece.colorIdx;
                    
                    // Partícula de lock
                    Particle p;
                    p.position = { nx - COLS/2.0f + 0.5f, (float)(ROWS - ny) - ROWS/2.0f + 0.5f, 0.5f };
                    p.velocity = { (float)GetRandomValue(-2, 2), (float)GetRandomValue(2, 5), (float)GetRandomValue(1, 4) };
                    p.color = GetColorFromValue(currentPiece.colorIdx);
                    p.life = p.maxLife = 0.5f;
                    p.size = 0.15f;
                    particles.push_back(p);
                }
            }
        }
        ClearLines();
        SpawnPiece();
        canHold = true;
        ScreenShake(0.05f, 0.1f);
    }

    void ClearLines() {
        int lines = 0;
        vector<int> clearedRows;
        
        for (int y = ROWS - 1; y >= 0; y--) {
            bool full = true;
            for (int x = 0; x < COLS; x++) {
                if (grid[y][x] == 0) { full = false; break; }
            }
            if (full) {
                lines++;
                clearedRows.push_back(y);
                for (int x = 0; x < COLS; x++) {
                    CreateExplosion(x, y, GetColorFromValue(grid[y][x]));
                }
            }
        }

        if (lines > 0) {
            PlaySound(clearSound);
            
            // Hitstop proporcional às linhas limpas
            hitStopTimer = 0.05f * lines;
            if (lines >= 4) hitStopTimer = 0.3f; // Tetris Hitstop

            // Remove as linhas
            for (int i = 0; i < clearedRows.size(); i++) {
                int rowToMove = clearedRows[i] + i; // Ajuste pois o grid desce
                for (int y = rowToMove; y > 0; y--) {
                    for (int x = 0; x < COLS; x++) {
                        grid[y][x] = grid[y - 1][x];
                    }
                }
                for (int x = 0; x < COLS; x++) grid[0][x] = 0;
            }

            linesCleared += lines;
            if (!bossActive) linesUntilBoss -= lines;
            
            int points = 0;
            switch (lines) {
                case 1: points = 100; break;
                case 2: points = 300; break;
                case 3: points = 500; break;
                case 4: points = 800; break;
            }
            
            if (lines == 4) {
                ScreenShake(0.5f, 0.8f);
                ShowMessage("TETRIS!", CYAN);
                if (backToBack) { points *= 1.5; ShowMessage("B2B TETRIS!", ORANGE); }
                backToBack = true;
            } else {
                ScreenShake(0.2f + (0.1f * lines), 0.3f + (0.1f * lines));
                backToBack = false;
            }
            
            combo++;
            points += 50 * combo;
            score += points * level;
            
            if (combo > 1) {
                string c = "COMBO x" + to_string(combo);
                ShowMessage(c, YELLOW);
            }

            if (linesCleared >= level * 10) {
                level++;
                PlaySound(levelUpSound);
                ShowMessage("LEVEL UP!", GREEN);
            }
            
            if (bossActive) {
                DamageBoss();
            } else if (linesUntilBoss <= 0) {
                TriggerBoss();
            }
        } else {
            combo = 0;
        }
    }

    void CreateExplosion(int x, int y, Color baseColor) {
        Vector3 worldPos = { x - COLS / 2.0f + 0.5f, (float)(ROWS - y) - ROWS / 2.0f + 0.5f, 0.0f };
        for (int i = 0; i < 15; i++) {
            Particle p;
            p.position = worldPos;
            p.velocity = { (float)GetRandomValue(-10, 10), (float)GetRandomValue(-10, 10), (float)GetRandomValue(-5, 15) };
            p.color = baseColor;
            p.color.r = min(255, p.color.r + 50); // Make it pop
            p.color.g = min(255, p.color.g + 50);
            p.color.b = min(255, p.color.b + 50);
            p.life = p.maxLife = (float)GetRandomValue(5, 15) / 10.0f;
            p.size = (float)GetRandomValue(2, 6) / 10.0f;
            particles.push_back(p);
        }
    }

    void ScreenShake(float time, float intensity) {
        camShakeTimer = time;
        camShakeIntensity = intensity;
    }

    void ShowMessage(string text, Color color) {
        mensagemEspecial = text;
        timerMensagem = 2.0f;
        
        FloatingText ft;
        ft.position = { 0, 5.0f, 5.0f };
        ft.text = text;
        ft.color = color;
        ft.life = ft.maxLife = 2.0f;
        floatingTexts.push_back(ft);
    }

    int GetGhostY() {
        Piece ghost = currentPiece;
        while (IsValid(ghost)) { ghost.y++; }
        return ghost.y - 1;
    }

    void Draw() {
        // --- RENDERIZAÇÃO 3D PARA TEXTURA (PARA SHADERS) ---
        BeginTextureMode(targetBuffer);
        ClearBackground({10, 10, 20, 255}); // Fundo escuro levemente azulado

        BeginMode3D(camera);

        // Desenha Background / Grid Dinâmica
        DrawGrid(30, 1.0f);
        
        // Efeito visual de expansão/batida da grid
        float gridAnimScale = 1.0f;
        if (gridExpansionTimer > 0) {
            gridAnimScale = 1.0f + (gridExpansionTimer * 0.1f);
        }

        // --- RENDERIZA GRID ESTÁTICA ---
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                if (grid[y][x] != 0) {
                    Vector3 pos = { x - COLS / 2.0f + 0.5f, (float)(ROWS - y) - ROWS / 2.0f + 0.5f, 0.0f };
                    Color color = GetColorFromValue(grid[y][x]);
                    
                    // Efeito de pulso para blocos da grid dependendo de quão alto estão
                    float h = (float)(ROWS - y) / ROWS;
                    if (h > 0.8f && globalPulse > 0.8f) {
                        color.r = min(255, color.r + 50);
                        color.g = min(255, color.g + 50);
                        color.b = min(255, color.b + 50);
                    }
                    
                    DrawNeonCube(pos, 0.95f, 0.95f, 0.95f, color);
                } else {
                    // Célula vazia (Wireframe bem fraco para guia)
                    Vector3 pos = { x - COLS / 2.0f + 0.5f, (float)(ROWS - y) - ROWS / 2.0f + 0.5f, -0.5f };
                    DrawCubeWires(pos, 1.0f, 1.0f, 0.1f, {50, 50, 60, 50});
                }
            }
        }

        // Borda da Grid
        DrawCubeWires({ 0.0f, 0.0f, 0.0f }, COLS, ROWS, 1.0f, {100, 100, 255, 100});

        // --- RENDERIZA GHOST PIECE ---
        if (!gameOver && !isPaused && !bossActive || (bossActive && bossEntryAnim <= 0)) {
            int ghostY = GetGhostY();
            Color ghostColor = GetColorFromValue(currentPiece.colorIdx);
            ghostColor.a = 50; // Semi-transparente
            
            rlPushMatrix();
            rlTranslatef(currentPiece.x - COLS/2.0f + currentPiece.shape[0].size()/2.0f, 
                         (float)(ROWS - ghostY) - ROWS/2.0f - currentPiece.shape.size()/2.0f + 1.0f, 0);
            rlRotatef(currentPiece.currentRotAngle, 0, 0, 1);
            rlTranslatef(-(currentPiece.shape[0].size()/2.0f), currentPiece.shape.size()/2.0f - 1.0f, 0);

            for (int y = 0; y < currentPiece.shape.size(); y++) {
                for (int x = 0; x < currentPiece.shape[y].size(); x++) {
                    if (currentPiece.shape[y][x]) {
                        Vector3 pos = { (float)x + 0.5f, (float)-y - 0.5f, 0.0f };
                        // Ghost piece com wireframe simples
                        DrawCubeWires(pos, 0.95f, 0.95f, 0.95f, ghostColor);
                    }
                }
            }
            rlPopMatrix();

            // --- RENDERIZA PEÇA ATUAL ---
            rlPushMatrix();
            
            // Suavização de queda e movimento (interpolação visual não implementada no model de dados para não complicar, 
            // mas a rotação está suavizada)
            float visualX = currentPiece.x;
            if (moveLeftTimer > 0) visualX -= moveLeftTimer * 2.0f;
            if (moveRightTimer > 0) visualX += moveRightTimer * 2.0f;

            rlTranslatef(visualX - COLS/2.0f + currentPiece.shape[0].size()/2.0f, 
                         (float)(ROWS - currentPiece.y) - ROWS/2.0f - currentPiece.shape.size()/2.0f + 1.0f, 0);
            
            rlRotatef(currentPiece.currentRotAngle, 0, 0, 1);
            rlTranslatef(-(currentPiece.shape[0].size()/2.0f), currentPiece.shape.size()/2.0f - 1.0f, 0);

            Color currColor = GetColorFromValue(currentPiece.colorIdx);
            for (int y = 0; y < currentPiece.shape.size(); y++) {
                for (int x = 0; x < currentPiece.shape[y].size(); x++) {
                    if (currentPiece.shape[y][x]) {
                        Vector3 pos = { (float)x + 0.5f, (float)-y - 0.5f, 0.0f };
                        DrawNeonCube(pos, 0.95f, 0.95f, 0.95f, currColor);
                    }
                }
            }
            rlPopMatrix();
        }

        // --- RENDERIZA BOSS ---
        if (bossActive) {
            rlPushMatrix();
            float bossY = ROWS * 0.5f + 5.0f; // Acima da grid
            if (bossEntryAnim > 0) {
                bossY += bossEntryAnim * 10.0f; // Cai do céu
            }
            
            rlTranslatef(0, bossY, 0);
            rlRotatef(bossOrbitAngle, 0, 1, 0);
            rlRotatef(sin(timePlayed * 2.0f) * 15.0f, 1, 0, 0); // Flutuação
            
            // Corpo do boss (vários cubos)
            Color bossCoreCol = {255, (unsigned char)(sin(timePlayed*10)*50 + 50), 50, 255};
            DrawCube({0,0,0}, 3.0f, 3.0f, 3.0f, {50, 50, 50, 255});
            DrawCubeWires({0,0,0}, 3.1f, 3.1f, 3.1f, bossCoreCol);
            
            // Olho do boss pulsante
            DrawCube({0, 0, 1.6f}, 1.0f * (1.0f + globalPulse*0.5f), 1.0f, 0.2f, RED);
            
            // Partículas em volta do boss se carregando ataque
            if (bossAttackTimer < 2.0f && bossEntryAnim <= 0) {
                DrawSphereWires({0,0,0}, 4.0f - bossAttackTimer, 8, 8, RED);
            }
            rlPopMatrix();
        }

        // Efeito Nuke Visual do Boss
        if (nukeSpinAngle > 0) {
            rlPushMatrix();
            rlRotatef(nukeSpinAngle * 5.0f, 0, 1, 0);
            DrawCylinderWires({0, -10, 0}, 15.0f, 15.0f, 30.0f, 16, {255, 0, 0, (unsigned char)(nukeSpinAngle)});
            rlPopMatrix();
            nukeSpinAngle -= GetFrameTime() * 100.0f;
            if (nukeSpinAngle < 0) nukeSpinAngle = 0;
        }

        // --- RENDERIZA PARTÍCULAS ---
        for (const auto& p : particles) {
            float scale = p.size * (p.life / p.maxLife);
            DrawCube(p.position, scale, scale, scale, p.color);
        }

        // --- RENDERIZA TEXTOS FLUTUANTES 3D (Simulados com Billboard/2D overlay depois) ---
        // (Serão desenhados no overlay 2D para melhor legibilidade)

        EndMode3D();
        EndTextureMode();

        // --- APLICA SHADER E DESENHA NA TELA ---
        BeginDrawing();
        ClearBackground(BLACK);

        BeginShaderMode(postProcessShader);
            // Inverte Y porque texturas do OpenGL são "de ponta cabeça" em relação a coordenadas de tela
            DrawTextureRec(targetBuffer.texture, { 0, 0, (float)targetBuffer.texture.width, (float)-targetBuffer.texture.height }, { 0, 0 }, WHITE);
        EndShaderMode();

        // --- UI 2D (Fora do Shader para não distorcer o texto) ---
        DrawText(TextFormat("SCORE: %d", score), 20, 20, 30, WHITE);
        DrawText(TextFormat("LEVEL: %d", level), 20, 60, 20, LIGHTGRAY);
        DrawText(TextFormat("LINES: %d", linesCleared), 20, 90, 20, LIGHTGRAY);
        
        if (!bossActive) {
            DrawText(TextFormat("NEXT BOSS IN: %d LINES", max(0, linesUntilBoss)), 20, 130, 20, ORANGE);
        } else {
            DrawText("!!! BOSS FIGHT !!!", 20, 130, 30, RED);
            DrawText(TextFormat("BOSS HP: %d", bossHealth), 20, 170, 20, MAROON);
            DrawRectangle(20, 200, 200 * (bossHealth / (float)(5 + bossEncounterCount*2)), 20, RED);
            DrawRectangleLines(20, 200, 200, 20, WHITE);
        }

        // UI do Hold
        DrawText("HOLD", SCREEN_WIDTH - 150, 20, 20, WHITE);
        DrawRectangleLines(SCREEN_WIDTH - 160, 50, 120, 120, LIGHTGRAY);
        if (holdPiece.colorIdx != 0) {
            // Desenha miniatura 2D da peça no Hold
            int offsetX = SCREEN_WIDTH - 140;
            int offsetY = 80;
            int cellSize = 20;
            Color hColor = GetColorFromValue(holdPiece.colorIdx);
            for (int y = 0; y < holdPiece.shape.size(); y++) {
                for (int x = 0; x < holdPiece.shape[y].size(); x++) {
                    if (holdPiece.shape[y][x]) {
                        DrawRectangle(offsetX + x * cellSize, offsetY + y * cellSize, cellSize - 2, cellSize - 2, hColor);
                    }
                }
            }
        }

        // Controles UI
        DrawText("CONTROLS:", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 160, 15, GRAY);
        DrawText("ARROWS/WASD - Move", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 140, 15, GRAY);
        DrawText("UP/W - Rotate", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 120, 15, GRAY);
        DrawText("SPACE - Hard Drop", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 100, 15, GRAY);
        DrawText("C - Hold Piece", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 80, 15, GRAY);
        DrawText("ESC - Pause", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 60, 15, GRAY);

        // Textos Flutuantes (Projetados do 3D para 2D)
        for (const auto& ft : floatingTexts) {
            Vector2 screenPos = GetWorldToScreen(ft.position, camera);
            float alpha = ft.life / ft.maxLife;
            Color c = ft.color;
            c.a = (unsigned char)(255 * alpha);
            // Efeito de Outline/Sombra
            DrawText(ft.text.c_str(), screenPos.x - MeasureText(ft.text.c_str(), 30)/2 + 2, screenPos.y + 2, 30, {0,0,0, c.a});
            DrawText(ft.text.c_str(), screenPos.x - MeasureText(ft.text.c_str(), 30)/2, screenPos.y, 30, c);
        }

        if (timerMensagem > 0 && mensagemEspecial != "") {
            int mw = MeasureText(mensagemEspecial.c_str(), 40);
            DrawText(mensagemEspecial.c_str(), SCREEN_WIDTH / 2 - mw / 2, SCREEN_HEIGHT / 2 - 100, 40, WHITE);
        }

        if (isPaused && !confirmExit) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 0, 0, 0, 150 });
            DrawText("PAUSED", SCREEN_WIDTH / 2 - MeasureText("PAUSED", 60) / 2, SCREEN_HEIGHT / 2 - 60, 60, WHITE);
            DrawText("Press ESC to Resume", SCREEN_WIDTH / 2 - MeasureText("Press ESC to Resume", 20) / 2, SCREEN_HEIGHT / 2 + 20, 20, LIGHTGRAY);
            DrawText("Press Q to Quit", SCREEN_WIDTH / 2 - MeasureText("Press Q to Quit", 20) / 2, SCREEN_HEIGHT / 2 + 50, 20, RED);
        }

        if (confirmExit) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 50, 0, 0, 200 });
            DrawText("ARE YOU SURE YOU WANT TO QUIT?", SCREEN_WIDTH / 2 - MeasureText("ARE YOU SURE YOU WANT TO QUIT?", 40) / 2, SCREEN_HEIGHT / 2 - 60, 40, WHITE);
            DrawText("Press ENTER to Confirm", SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Confirm", 20) / 2, SCREEN_HEIGHT / 2 + 20, 20, RED);
            DrawText("Press ESC to Cancel", SCREEN_WIDTH / 2 - MeasureText("Press ESC to Cancel", 20) / 2, SCREEN_HEIGHT / 2 + 50, 20, GREEN);
            
            if (IsKeyPressed(KEY_ENTER)) {
                // Fechar o jogo será tratado no loop principal
            }
        }

        if (gameOver) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 0, 0, 0, 200 });
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 60) / 2, SCREEN_HEIGHT / 2 - 60, 60, RED);
            DrawText(TextFormat("FINAL SCORE: %d", score), SCREEN_WIDTH / 2 - MeasureText(TextFormat("FINAL SCORE: %d", score), 30) / 2, SCREEN_HEIGHT / 2 + 10, 30, WHITE);
            DrawText("Press ENTER to Restart", SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Restart", 20) / 2, SCREEN_HEIGHT / 2 + 50, 20, LIGHTGRAY);
        }

        EndDrawing();
    }

    void Reset() {
        memset(grid, 0, sizeof(grid));
        score = 0; linesCleared = 0; level = 1; combo = 0; backToBack = false;
        fallTimer = 0.0f; fallSpeed = FALL_SPEED_START; timePlayed = 0.0f;
        canHold = true; holdPiece.colorIdx = 0; 
        camShakeTimer = 0.0f; camShakeIntensity = 0.0f;
        manualCamPan.x = 0.0f; manualCamPan.y = 0.0f; manualCamPan.z = 0.0f;
        
        gameOver = false; isPaused = false; timerMensagem = 0; mensagemEspecial = ""; 
        gridExpansionTimer = 0.0f; currentGridElevation = 2.0f; 
        moveLeftTimer = 0.0f; moveRightTimer = 0.0f; nukeSpinAngle = 0.0f;
        hitStopTimer = 0.0f; damageVignette = 0.0f;
        
        bossActive = false; bossEntryAnim = 0.0f; linesUntilBoss = 15; bossEncounterCount = 0; 
        currentBossAttackDelay = 15.0f; bossOrbitAngle = 0.0f; 
        bossCinematicSpinTimer = 0.0f; bossCinematicCooldown = 15.0f;
        
        particles.clear(); floatingTexts.clear();
        SpawnPiece();
        camera.position = defaultCamPos; camera.target = defaultCamTarget;
        
        ShuffleMusic(); // Seleciona uma nova música de fundo aleatória ao resetar
    }

    bool ShouldExit() { return confirmExit && IsKeyPressed(KEY_ENTER); }
    
    ~Game() {
        UnloadRenderTexture(targetBuffer);
        UnloadShader(postProcessShader);
        UnloadSound(clearSound);
        UnloadSound(dropSound);
        UnloadSound(moveSound);
        UnloadSound(rotateSound);
        UnloadSound(gameOverSound);
        UnloadSound(levelUpSound);
        UnloadSound(holdSound);
        UnloadSound(bossWarningSound);
        UnloadSound(explosionSound);
        UnloadMusicStream(bgm[0]);
        UnloadMusicStream(bgm[1]);
        UnloadMusicStream(bgm[2]);
    }
};

int main() {
    // MSAA 4x E HIGH DPI ATIVADOS DIRETAMENTE NA ENGINE
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TETRIS 3D AAA EXTENDED + SHADERS");
    InitAudioDevice();

    // =====================================================================
    // CARREGA O MODELO 3D (tetris.glb)
    // =====================================================================
    glbPieceModel = LoadModel("tetris.glb");

    SetTargetFPS(60);

    Game game;
    game.Init();

    while (!WindowShouldClose()) {
        if (game.ShouldExit()) break;
        
        game.Update();
        game.Draw();
    }

    // =====================================================================
    // DESCARREGA O MODELO 3D DA MEMÓRIA
    // =====================================================================
    UnloadModel(glbPieceModel);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}