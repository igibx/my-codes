/*******************************************************************************************
*
* TETRABETTA: AAA SUPER EDITION - Jogo estilo Puzzle 3D para Android
* Desenvolvido por: IGOR BETTARELLO CARVALHO XAVIER
* Motor: Raylib (C Puro) - Ultra Performance Edition
*
* REVOLUTION FEATURES: 
* - Grid Alinhado Perfeitamente (Pixel-Perfect 3D)
* - Controles Arcade Otimizados e Abaixados (Sem colisão visual)
* - Efeitos Visuais 3D (Partículas e Textos saltam em direção ao jogador)
* - Câmera Dinâmica inteligente (Afasta-se no Modo Expansão)
* - Motor de Áudio MP3 (Pronto para ler os seus arquivos)
* - As 7 Peças Clássicas de 4 Blocos
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// --- CONSTANTES DA TELA ---
const int SCREEN_WIDTH = 1080;
const int SCREEN_HEIGHT = 2400;

#define MAX_GRID_WIDTH 20
#define MAX_GRID_HEIGHT 32
#define MAX_PARTICLES 400
#define MAX_PRAISES 10
#define MAX_STARS 150

typedef enum GameState {
    MENU, PLAYING_CLASSIC, PLAYING_BOSS, PLAYING_EXPANSION, CONFIG, CREDITS, GAME_OVER, CONTINUE_SCREEN
} GameState;

typedef enum Language { PT_BR, EN_US } Language;

// --- CONFIGURAÇÕES ---
struct GameConfig {
    bool soundEnabled;
    bool musicEnabled;
    bool highQuality;
    Language lang;
} config = { true, true, true, PT_BR };

// --- ESTRUTURAS DO JOGO ---
typedef struct Piece {
    Vector2 blocks[4];
    int numBlocks;
    Color color;
    int size;
} Piece;

typedef struct Particle {
    Vector3 pos;
    Vector3 vel;
    Vector3 rotAxis;
    float rotAngle;
    Color color;
    float life;
    float maxLife;
    float size;
    bool active;
} Particle;

typedef struct Praise {
    char text[32];
    Vector2 pos;
    Color color;
    float life;
    float maxLife;
    float scale;
    bool active;
} Praise;

typedef struct Star {
    Vector3 pos;
    float speed;
    float size;
} Star;

// --- VARIÁVEIS GLOBAIS ---
GameState currentState = MENU;
int menuSelection = 0;
const int MENU_MAX_OPTIONS = 6;
Font mainFont;

// Grid e Gameplay
int gridWidth = 10;
int gridHeight = 20;
Color grid[MAX_GRID_WIDTH][MAX_GRID_HEIGHT];

int lives = 3;
int continues = 3;
int score = 0;
int level = 1;
float dropTimer = 0.0f;
float dropSpeed = 1.0f;

Piece currentPiece;
Vector2 piecePos;
float continueTimer = 9.9f;

// Efeitos Visuais (VFX) e Câmera Dinâmica
Particle particles[MAX_PARTICLES];
Praise praises[MAX_PRAISES];
Star stars[MAX_STARS];
float screenShake = 0.0f;
float warpSpeedMult = 1.0f;
float laserEffectTimer = 0.0f;
float targetCamZ = 28.0f; // Alvo dinâmico da câmera
Camera3D camera = { 0 };

// Combos e GOD MODE
int comboCounter = 0;
float comboTimer = 0.0f;
float powerGauge = 0.0f; // 0.0 a 100.0
float bossRotation = 0.0f;
float bossAttackTimer = 0.0f;

// Sons MP3 (Ficheiros Externos)
Sound fxMove, fxRotate, fxDrop, fxClear, fxCombo, fxGodMode;
Music bgMusic;

// --- MOTOR DE ÁUDIO MP3 ---
void InitGameAudio() {
    InitAudioDevice();
    fxMove = LoadSound("move.mp3");
    fxRotate = LoadSound("rotate.mp3");
    fxDrop = LoadSound("drop.mp3");
    fxClear = LoadSound("clear.mp3");
    fxCombo = LoadSound("combo.mp3");
    fxGodMode = LoadSound("godmode.mp3");
    bgMusic = LoadMusicStream("bgm.mp3");
    if (bgMusic.stream.buffer != NULL) {
        bgMusic.looping = true;
        PlayMusicStream(bgMusic);
    }
}
void PlayGameSound(Sound s) { if (config.soundEnabled && s.stream.buffer != NULL) PlaySound(s); }

// --- TEXTOS ---
const char* GetText(int id) {
    const char* texts_pt[] = { "MODO CLASSICO", "MODO CHEFE", "MODO EXPANSAO", "CONFIGURACAO", "CREDITOS", "SAIR", "CONTINUAR?", "SIM", "NAO", "VIDAS: ", "PONTOS: ", "NIVEL: ", "CRIADO POR: IGOR BETTARELLO", "SONS", "MUSICA", "QUALIDADE", "IDIOMA", "VOLTAR" };
    const char* texts_en[] = { "CLASSIC MODE", "BOSS MODE", "EXPANSION MODE", "SETTINGS", "CREDITS", "EXIT", "CONTINUE?", "YES", "NO", "LIVES: ", "SCORE: ", "LEVEL: ", "CREATED BY: IGOR BETTARELLO", "SOUNDS", "MUSIC", "QUALITY", "LANGUAGE", "BACK" };
    return (config.lang == PT_BR) ? texts_pt[id] : texts_en[id];
}

// --- UTILITÁRIOS VISUAIS E MATEMÁTICOS ---
Color DarkenColor(Color c, float factor) { return (Color){ (unsigned char)(c.r * (1.0f - factor)), (unsigned char)(c.g * (1.0f - factor)), (unsigned char)(c.b * (1.0f - factor)), c.a }; }
Color LightenColor(Color c, float factor) { return (Color){ (unsigned char)(c.r + (255 - c.r) * factor), (unsigned char)(c.g + (255 - c.g) * factor), (unsigned char)(c.b + (255 - c.b) * factor), c.a }; }
Color HexToCol(unsigned int hex) { return (Color){ (hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF }; }

// Função CRÍTICA: Calcula a posição exata 3D de cada bloco para alinhar perfeitamente!
Vector3 CalcBlockPos(float x, float y, int gW, int gH) {
    return (Vector3){ x - (gW / 2.0f) + 0.5f, (gH / 2.0f) - y - 0.5f, 0.0f };
}

bool IsTouchInRect(Rectangle r) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r)) return true;
    for (int i = 0; i < GetTouchPointCount(); i++) if (CheckCollisionPointRec(GetTouchPosition(i), r)) return true;
    return false;
}

// Botão de Fliperama AAA
void DrawSuperButton(Rectangle r, const char* text, Color color, bool isPressed) {
    Color shadow = DarkenColor(color, 0.6f);
    Color base = isPressed ? DarkenColor(color, 0.2f) : color;
    Color highlight = LightenColor(color, 0.3f);
    float offset = isPressed ? 0.0f : 12.0f;
    
    DrawRectangleRounded((Rectangle){ r.x, r.y + offset, r.width, r.height }, 0.3f, 16, shadow);
    DrawRectangleRounded((Rectangle){ r.x, r.y, r.width, r.height }, 0.3f, 16, base);
    DrawRectangleRounded((Rectangle){ r.x + 8, r.y + 8, r.width - 16, 20 }, 0.5f, 8, highlight);
    DrawRectangleRoundedLines((Rectangle){ r.x, r.y, r.width, r.height }, 0.3f, 16, WHITE);
    
    Vector2 tSize = MeasureTextEx(mainFont, text, r.height * 0.35f, 2);
    DrawTextEx(mainFont, text, (Vector2){ r.x + r.width/2 - tSize.x/2, r.y + r.height/2 - tSize.y/2 + (isPressed?5:0) }, r.height * 0.35f, 2, WHITE);
}

void DrawNeonCube(Vector3 pos, Color c, float scale, bool isGhost) {
    if (isGhost) {
        DrawCubeWires(pos, scale, scale, scale, Fade(c, 0.7f));
        DrawCube(pos, scale*0.8f, scale*0.8f, scale*0.8f, Fade(c, 0.15f));
    } else {
        DrawCube(pos, scale*0.9f, scale*0.9f, scale*0.9f, DarkenColor(c, 0.1f));
        DrawCubeWires(pos, scale*1.05f, scale*1.05f, scale*1.05f, LightenColor(c, 0.5f));
    }
}

// --- SISTEMA DE EFEITOS 3D (VFX) ---
void InitStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].pos = (Vector3){ (float)GetRandomValue(-400, 400)/10.0f, (float)GetRandomValue(-500, 500)/10.0f, (float)GetRandomValue(-100, 200)/10.0f };
        stars[i].speed = (float)GetRandomValue(10, 50)/10.0f; stars[i].size = (float)GetRandomValue(1, 4)/20.0f;
    }
}

void SpawnParticles(Vector3 pos, Color color, int count) {
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (!particles[j].active) {
                particles[j].active = true;
                particles[j].pos = pos;
                // VOANDO PARA A TELA (Velocidade Z Positiva Forte!)
                particles[j].vel = (Vector3){ (float)GetRandomValue(-120, 120)/10.0f, (float)GetRandomValue(-50, 150)/10.0f, (float)GetRandomValue(100, 350)/10.0f };
                particles[j].rotAxis = (Vector3){ (float)GetRandomValue(0,10)/10.0f, (float)GetRandomValue(0,10)/10.0f, (float)GetRandomValue(0,10)/10.0f };
                particles[j].rotAngle = 0.0f;
                particles[j].color = color;
                particles[j].life = 1.0f; particles[j].maxLife = 1.0f + (float)GetRandomValue(0, 50)/100.0f;
                particles[j].size = 0.6f + (float)GetRandomValue(0, 40)/100.0f;
                break;
            }
        }
    }
}

void SpawnPraise(const char* text, Color c) {
    for (int i = 0; i < MAX_PRAISES; i++) {
        if (!praises[i].active) {
            praises[i].active = true;
            strcpy(praises[i].text, text);
            praises[i].pos = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 100 };
            praises[i].color = c;
            praises[i].life = 1.5f; praises[i].maxLife = 1.5f;
            praises[i].scale = 0.1f; // Começa minúsculo e cresce pra tela
            break;
        }
    }
}

void UpdateVFX(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].pos.x += particles[i].vel.x * dt;
            particles[i].pos.y -= particles[i].vel.y * dt;
            particles[i].pos.z += particles[i].vel.z * dt; // Vem na direção do jogador
            particles[i].vel.y += 20.0f * dt; // Gravidade
            particles[i].rotAngle += 360.0f * dt;
            particles[i].life -= dt;
            if (particles[i].life <= 0) particles[i].active = false;
        }
    }
    
    for (int i = 0; i < MAX_PRAISES; i++) {
        if (praises[i].active) {
            praises[i].scale += 4.5f * dt; // Efeito "Na sua cara" 3D Pop-out
            praises[i].life -= dt;
            if (praises[i].life <= 0) praises[i].active = false;
        }
    }
    
    warpSpeedMult = (comboTimer > 0.0f) ? 5.0f : 1.0f;
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].pos.z += stars[i].speed * warpSpeedMult * dt;
        if (stars[i].pos.z > 20.0f) {
            stars[i].pos.z = -50.0f; stars[i].pos.x = (float)GetRandomValue(-400, 400)/10.0f; stars[i].pos.y = (float)GetRandomValue(-500, 500)/10.0f;
        }
    }
    
    if (screenShake > 0.0f) screenShake -= dt * 15.0f;
    if (screenShake < 0.0f) screenShake = 0.0f;
    if (laserEffectTimer > 0.0f) laserEffectTimer -= dt;
    
    if (comboTimer > 0.0f) { comboTimer -= dt; if (comboTimer <= 0.0f) comboCounter = 0; }
    
    // Zoom suave da câmera no modo expansão
    camera.position.z = Lerp(camera.position.z, targetCamZ, dt * 2.0f);
}

// --- AS 7 PEÇAS CLÁSSICAS ---
Piece GetRandomPiece() {
    Piece p = {0};
    int type = GetRandomValue(0, 6);
    p.numBlocks = 4;
    switch (type) {
        case 0: p.blocks[0]=(Vector2){0,1}; p.blocks[1]=(Vector2){1,1}; p.blocks[2]=(Vector2){2,1}; p.blocks[3]=(Vector2){3,1}; p.color=SKYBLUE; p.size=4; break; // I
        case 1: p.blocks[0]=(Vector2){0,0}; p.blocks[1]=(Vector2){0,1}; p.blocks[2]=(Vector2){1,1}; p.blocks[3]=(Vector2){2,1}; p.color=BLUE; p.size=3; break; // J
        case 2: p.blocks[0]=(Vector2){2,0}; p.blocks[1]=(Vector2){0,1}; p.blocks[2]=(Vector2){1,1}; p.blocks[3]=(Vector2){2,1}; p.color=ORANGE; p.size=3; break; // L
        case 3: p.blocks[0]=(Vector2){0,0}; p.blocks[1]=(Vector2){1,0}; p.blocks[2]=(Vector2){0,1}; p.blocks[3]=(Vector2){1,1}; p.color=YELLOW; p.size=2; break; // O
        case 4: p.blocks[0]=(Vector2){1,0}; p.blocks[1]=(Vector2){2,0}; p.blocks[2]=(Vector2){0,1}; p.blocks[3]=(Vector2){1,1}; p.color=GREEN; p.size=3; break; // S
        case 5: p.blocks[0]=(Vector2){1,0}; p.blocks[1]=(Vector2){0,1}; p.blocks[2]=(Vector2){1,1}; p.blocks[3]=(Vector2){2,1}; p.color=PURPLE; p.size=3; break; // T
        case 6: p.blocks[0]=(Vector2){0,0}; p.blocks[1]=(Vector2){1,0}; p.blocks[2]=(Vector2){1,1}; p.blocks[3]=(Vector2){2,1}; p.color=RED; p.size=3; break; // Z
    }
    return p;
}

// --- LÓGICA DO JOGO ---
void ResetBoard() { for (int x = 0; x < MAX_GRID_WIDTH; x++) for (int y = 0; y < MAX_GRID_HEIGHT; y++) grid[x][y] = BLANK; }

void StartGame(GameState mode) {
    currentState = mode;
    score = 0; level = 1; dropSpeed = 1.0f; comboCounter = 0; powerGauge = 0.0f; laserEffectTimer = 0.0f;
    
    gridWidth = (mode == PLAYING_EXPANSION) ? 10 : 10;
    gridHeight = (mode == PLAYING_EXPANSION) ? 16 : 20;
    
    ResetBoard();
    currentPiece = GetRandomPiece();
    piecePos = (Vector2){ (float)(gridWidth / 2 - currentPiece.size / 2), 0 };
    
    // Câmera perfeitamente apontada para o centro do tabuleiro
    targetCamZ = 28.0f;
    camera.position = (Vector3){ 0.0f, 0.0f, targetCamZ };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f; camera.projection = CAMERA_PERSPECTIVE;
}

bool CheckCollision(Piece p, Vector2 pos) {
    for (int i = 0; i < p.numBlocks; i++) {
        int nx = (int)(pos.x + p.blocks[i].x); int ny = (int)(pos.y + p.blocks[i].y);
        if (nx < 0 || nx >= gridWidth || ny >= gridHeight) return true;
        if (ny >= 0 && grid[nx][ny].a > 0) return true;
    }
    return false;
}

Vector2 GetGhostPosition() {
    Vector2 ghost = piecePos;
    while (!CheckCollision(currentPiece, ghost)) ghost.y++;
    ghost.y--; return ghost;
}

void LockPiece() {
    PlayGameSound(fxDrop); screenShake = 1.5f; 
    
    for (int i = 0; i < currentPiece.numBlocks; i++) {
        int nx = (int)(piecePos.x + currentPiece.blocks[i].x);
        int ny = (int)(piecePos.y + currentPiece.blocks[i].y);
        if (ny >= 0 && ny < gridHeight && nx >= 0 && nx < gridWidth) {
            grid[nx][ny] = currentPiece.color;
            SpawnParticles(CalcBlockPos(nx, ny, gridWidth, gridHeight), currentPiece.color, 4);
        }
    }
    
    int linesCleared = 0;
    for (int y = gridHeight - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < gridWidth; x++) if (grid[x][y].a == 0) { full = false; break; }
        
        if (full) {
            linesCleared++;
            for (int x = 0; x < gridWidth; x++) SpawnParticles(CalcBlockPos(x, y, gridWidth, gridHeight), grid[x][y], 8);
            for (int yy = y; yy > 0; yy--) for (int x = 0; x < gridWidth; x++) grid[x][yy] = grid[x][yy-1];
            for (int x = 0; x < gridWidth; x++) grid[x][0] = BLANK;
            y++;
        }
    }
    
    if (linesCleared > 0) {
        comboCounter++; comboTimer = 5.0f; powerGauge += linesCleared * 20.0f; if (powerGauge > 100.0f) powerGauge = 100.0f;
        score += (linesCleared * 100) * level * comboCounter;
        level = (score / 1000) + 1;
        dropSpeed = 1.0f - (level * 0.05f); if (dropSpeed < 0.1f) dropSpeed = 0.1f;
        
        if (comboCounter > 1) { PlayGameSound(fxCombo); SpawnPraise(TextFormat("COMBO X%d!", comboCounter), YELLOW); screenShake = 3.0f; } 
        else { PlayGameSound(fxClear); if (linesCleared >= 4) SpawnPraise("TETRIS!", SKYBLUE); else if (linesCleared >= 2) SpawnPraise("OTIMO!", GREEN); }
        
        if (currentState == PLAYING_EXPANSION) {
            if (level % 2 == 0 && gridWidth < MAX_GRID_WIDTH) gridWidth += 2;
            if (level % 3 == 0 && gridHeight < MAX_GRID_HEIGHT) {
                gridHeight += 2;
                targetCamZ += 2.5f; // Câmara afasta-se automaticamente para não cortar!
            }
        }
    }
    
    currentPiece = GetRandomPiece();
    piecePos = (Vector2){ (float)(gridWidth / 2 - currentPiece.size / 2), 0 };
    
    if (CheckCollision(currentPiece, piecePos)) {
        lives--;
        if (lives <= 0) { if (continues > 0) { currentState = CONTINUE_SCREEN; continueTimer = 9.9f; } else currentState = GAME_OVER; } 
        else ResetBoard();
    }
}

void ActivateGodMode() {
    if (powerGauge >= 100.0f) {
        PlayGameSound(fxGodMode); powerGauge = 0.0f; screenShake = 8.0f; laserEffectTimer = 1.0f; SpawnPraise("GOD MODE!", GOLD);
        for (int y = gridHeight - 1; y >= gridHeight - 5; y--) {
            if (y < 0) break;
            for (int x = 0; x < gridWidth; x++) {
                if (grid[x][y].a > 0) { SpawnParticles(CalcBlockPos(x, y, gridWidth, gridHeight), WHITE, 15); grid[x][y] = BLANK; score += 50; }
            }
        }
    }
}

// --- RENDERIZAÇÃO ---
void DrawBackground3D() {
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, HexToCol(0x050514FF), HexToCol(0x000005FF));
    BeginMode3D(camera);
    for (int i = 0; i < MAX_STARS; i++) DrawCube(stars[i].pos, stars[i].size, stars[i].size, stars[i].size, Fade(WHITE, (stars[i].pos.z + 50.0f) / 70.0f));
    EndMode3D();
}

void DrawGameScreen() {
    DrawBackground3D();
    
    Camera3D shakeCam = camera;
    if (screenShake > 0.0f) {
        shakeCam.target.x += (float)GetRandomValue(-100, 100)/100.0f * (screenShake / 4.0f);
        shakeCam.target.y += (float)GetRandomValue(-100, 100)/100.0f * (screenShake / 4.0f);
    }
    
    BeginMode3D(shakeCam);
    rlPushMatrix();
    rlTranslatef(0.0f, 8.0f, 0.0f); // Move toda a cena mais para cima do ecrã
    
    if (laserEffectTimer > 0.0f) {
        DrawCylinder((Vector3){0, -gridHeight/2.0f + 2.5f, 0}, 15.0f, 15.0f, 6.0f, 32, Fade(GOLD, laserEffectTimer));
        DrawCylinderWires((Vector3){0, -gridHeight/2.0f + 2.5f, 0}, 16.0f, 16.0f, 6.5f, 32, WHITE);
    }

    // Grid Alinhado Limpo (Sem cubos vazios a poluir)
    DrawCube((Vector3){0, 0, -0.6f}, gridWidth, gridHeight, 0.1f, Fade(BLACK, 0.6f));
    DrawCubeWires((Vector3){0, 0, -0.6f}, gridWidth, gridHeight, 0.1f, Fade(SKYBLUE, 0.4f));
    for(int i=0; i<=gridWidth; i++) { float vx = i - (gridWidth / 2.0f); DrawLine3D((Vector3){vx, gridHeight/2.0f, -0.5f}, (Vector3){vx, -gridHeight/2.0f, -0.5f}, Fade(DARKGRAY, 0.3f)); }
    for(int i=0; i<=gridHeight; i++) { float vy = i - (gridHeight / 2.0f); DrawLine3D((Vector3){-gridWidth/2.0f, vy, -0.5f}, (Vector3){gridWidth/2.0f, vy, -0.5f}, Fade(DARKGRAY, 0.3f)); }
    
    // Blocos
    for (int x = 0; x < gridWidth; x++) {
        for (int y = 0; y < gridHeight; y++) {
            if (grid[x][y].a > 0) DrawNeonCube(CalcBlockPos(x, y, gridWidth, gridHeight), grid[x][y], 0.9f, false);
        }
    }
    
    // Ghost Piece
    Vector2 ghostPos = GetGhostPosition();
    for (int i = 0; i < currentPiece.numBlocks; i++) DrawNeonCube(CalcBlockPos(ghostPos.x + currentPiece.blocks[i].x, ghostPos.y + currentPiece.blocks[i].y, gridWidth, gridHeight), currentPiece.color, 0.9f, true);
    
    // Peça Atual
    for (int i = 0; i < currentPiece.numBlocks; i++) DrawNeonCube(CalcBlockPos(piecePos.x + currentPiece.blocks[i].x, piecePos.y + currentPiece.blocks[i].y, gridWidth, gridHeight), currentPiece.color, 0.9f, false);
    
    // Partículas
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            rlPushMatrix();
            rlTranslatef(particles[i].pos.x, particles[i].pos.y, particles[i].pos.z);
            rlRotatef(particles[i].rotAngle, particles[i].rotAxis.x, particles[i].rotAxis.y, particles[i].rotAxis.z);
            DrawCube((Vector3){0,0,0}, particles[i].size, particles[i].size, particles[i].size, Fade(particles[i].color, particles[i].life/particles[i].maxLife));
            rlPopMatrix();
        }
    }
    
    if (currentState == PLAYING_BOSS) {
        rlPushMatrix(); rlTranslatef(0.0f, gridHeight/2.0f + 4.0f, 0.0f); rlRotatef(bossRotation, 1.0f, 1.0f, 1.0f); DrawNeonCube((Vector3){0,0,0}, PURPLE, 3.0f, false); rlPopMatrix();
    }
    
    rlPopMatrix();
    EndMode3D();
    
    // HUD
    DrawTextEx(mainFont, TextFormat("%s%d", GetText(10), score), (Vector2){ 40, 40 }, 55, 2, WHITE);
    DrawTextEx(mainFont, TextFormat("%s%d", GetText(11), level), (Vector2){ 40, 110 }, 40, 2, LIGHTGRAY);
    DrawTextEx(mainFont, TextFormat("%s%d", GetText(9), lives), (Vector2){ SCREEN_WIDTH - 300.0f, 40 }, 55, 2, RED);
    
    if (comboTimer > 0.0f) { DrawRectangle(40, 170, (int)(350 * (comboTimer/5.0f)), 25, HexToCol(0xFFCC00FF)); DrawTextEx(mainFont, TextFormat("COMBO X%d", comboCounter), (Vector2){ 45, 205 }, 35, 2, WHITE); }
    
    // --- CONTROLES BAIXOS E AJUSTADOS ---
    float uiY = SCREEN_HEIGHT - 480; // Muito mais em baixo!
    
    // Joystick s/ Cima
    Rectangle btnLeft =  { 50,  uiY + 140, 140, 140 };
    Rectangle btnDown =  { 220, uiY + 140, 140, 140 };
    Rectangle btnRight = { 390, uiY + 140, 140, 140 };
    
    DrawSuperButton(btnDown, "v", GRAY, IsTouchInRect(btnDown));
    DrawSuperButton(btnLeft, "<", GRAY, IsTouchInRect(btnLeft));
    DrawSuperButton(btnRight, ">", GRAY, IsTouchInRect(btnRight));
    
    // Ação: ROT restabelecido e DROP ajustado
    Rectangle btnRot =  { SCREEN_WIDTH - 250, uiY - 30,  180, 180 };
    Rectangle btnDrop = { SCREEN_WIDTH - 300, uiY + 180, 230, 230 };
    
    DrawSuperButton(btnRot, "ROT", BLUE, IsTouchInRect(btnRot));
    DrawSuperButton(btnDrop, "DROP", RED, IsTouchInRect(btnDrop));

    // God Mode
    Rectangle btnGodMode = { SCREEN_WIDTH/2 - 160, uiY - 140, 320, 90 };
    if (powerGauge >= 100.0f) {
        DrawSuperButton(btnGodMode, "GOD MODE", GOLD, IsTouchInRect(btnGodMode));
        if (IsTouchInRect(btnGodMode)) ActivateGodMode();
    } else {
        DrawRectangleRounded(btnGodMode, 0.3f, 16, Fade(DARKGRAY, 0.5f));
        DrawRectangleRounded((Rectangle){btnGodMode.x, btnGodMode.y, (powerGauge/100.0f)*320, 90}, 0.3f, 16, Fade(GOLD, 0.5f));
        DrawTextEx(mainFont, "POWER", (Vector2){ btnGodMode.x + 80, btnGodMode.y + 25 }, 40, 2, LIGHTGRAY);
    }
    
    // Elogios (VFX Alpha e Scale)
    for (int i = 0; i < MAX_PRAISES; i++) {
        if (praises[i].active) {
            Vector2 tSize = MeasureTextEx(mainFont, praises[i].text, 80 * praises[i].scale, 2);
            DrawTextEx(mainFont, praises[i].text, (Vector2){ praises[i].pos.x - tSize.x/2, praises[i].pos.y }, 80 * praises[i].scale, 2, Fade(praises[i].color, praises[i].life/praises[i].maxLife));
        }
    }
    
    // INPUT IN-GAME
    static float moveTimer = 0.0f; moveTimer += GetFrameTime();
    if (moveTimer > 0.10f) { 
        if (IsTouchInRect(btnLeft) || IsKeyPressed(KEY_LEFT)) { piecePos.x--; if (CheckCollision(currentPiece, piecePos)) piecePos.x++; else PlayGameSound(fxMove); moveTimer = 0.0f; }
        if (IsTouchInRect(btnRight) || IsKeyPressed(KEY_RIGHT)) { piecePos.x++; if (CheckCollision(currentPiece, piecePos)) piecePos.x--; else PlayGameSound(fxMove); moveTimer = 0.0f; }
        if (IsTouchInRect(btnDown) || IsKeyPressed(KEY_DOWN)) { piecePos.y++; if (CheckCollision(currentPiece, piecePos)) piecePos.y--; score += 2; moveTimer = 0.0f; }
    }
    
    static bool rotPressed = false;
    if ((IsTouchInRect(btnRot) || IsKeyPressed(KEY_UP)) && !rotPressed) {
        Piece rotated = currentPiece;
        if (rotated.size > 2) {
            for (int i = 0; i < rotated.numBlocks; i++) { float temp = rotated.blocks[i].x; rotated.blocks[i].x = (rotated.size - 1) - rotated.blocks[i].y; rotated.blocks[i].y = temp; }
        }
        if (!CheckCollision(rotated, piecePos)) { currentPiece = rotated; PlayGameSound(fxRotate); }
        rotPressed = true;
    } else if (!IsTouchInRect(btnRot) && !IsKeyDown(KEY_UP)) rotPressed = false;
    
    static bool dropPressed = false;
    if ((IsTouchInRect(btnDrop) || IsKeyPressed(KEY_SPACE)) && !dropPressed) {
        while (!CheckCollision(currentPiece, piecePos)) { piecePos.y++; score += 5; } piecePos.y--; LockPiece(); dropPressed = true;
    } else if (!IsTouchInRect(btnDrop) && !IsKeyDown(KEY_SPACE)) dropPressed = false;
}

// --- MAIN LOOP ---
int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TETRABETTA AAA");
    mainFont = GetFontDefault();
    SetTargetFPS(60);
    
    InitGameAudio(); InitStars();

    Rectangle btnMenuUp = { SCREEN_WIDTH/2.0f - 150, SCREEN_HEIGHT - 650, 120, 120 };
    Rectangle btnMenuDown = { SCREEN_WIDTH/2.0f - 150, SCREEN_HEIGHT - 450, 120, 120 };
    Rectangle btnMenuOK = { SCREEN_WIDTH/2.0f + 50, SCREEN_HEIGHT - 550, 180, 120 };
    bool btnUpLock = false, btnDownLock = false, btnOKLock = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        if (bgMusic.stream.buffer != NULL && config.musicEnabled) UpdateMusicStream(bgMusic);
        UpdateVFX(dt);

        BeginDrawing(); ClearBackground(BLACK);

        switch (currentState) {
            case MENU: {
                DrawBackground3D();
                DrawTextEx(mainFont, "TETRABETTA", (Vector2){ SCREEN_WIDTH/2.0f - MeasureTextEx(mainFont, "TETRABETTA", 130, 2).x/2, 250 }, 130, 2, SKYBLUE);
                DrawTextEx(mainFont, "AAA EDITION", (Vector2){ SCREEN_WIDTH/2.0f - MeasureTextEx(mainFont, "AAA EDITION", 60, 2).x/2, 380 }, 60, 2, GOLD);
                for (int i = 0; i < MENU_MAX_OPTIONS; i++) {
                    Color c = (i == menuSelection) ? YELLOW : WHITE; Vector2 size = MeasureTextEx(mainFont, GetText(i), 60, 2);
                    DrawTextEx(mainFont, GetText(i), (Vector2){ SCREEN_WIDTH/2.0f - size.x/2, 600.0f + i * 100 }, 60, 2, c);
                }
                DrawSuperButton(btnMenuUp, "^", GRAY, IsTouchInRect(btnMenuUp)); DrawSuperButton(btnMenuDown, "v", GRAY, IsTouchInRect(btnMenuDown)); DrawSuperButton(btnMenuOK, "OK", GREEN, IsTouchInRect(btnMenuOK));

                if (IsTouchInRect(btnMenuUp)) { if (!btnUpLock) { menuSelection--; if (menuSelection < 0) menuSelection = MENU_MAX_OPTIONS - 1; PlayGameSound(fxMove); btnUpLock = true; } } else btnUpLock = false;
                if (IsTouchInRect(btnMenuDown)) { if (!btnDownLock) { menuSelection++; if (menuSelection >= MENU_MAX_OPTIONS) menuSelection = 0; PlayGameSound(fxMove); btnDownLock = true; } } else btnDownLock = false;
                if (IsTouchInRect(btnMenuOK)) {
                    if (!btnOKLock) {
                        btnOKLock = true; PlayGameSound(fxClear);
                        if (menuSelection == 0) StartGame(PLAYING_CLASSIC); else if (menuSelection == 1) StartGame(PLAYING_BOSS); else if (menuSelection == 2) StartGame(PLAYING_EXPANSION);
                        else if (menuSelection == 3) currentState = CONFIG; else if (menuSelection == 4) currentState = CREDITS; else if (menuSelection == 5) CloseWindow();
                    }
                } else btnOKLock = false;
            } break;

            case PLAYING_CLASSIC: case PLAYING_BOSS: case PLAYING_EXPANSION: {
                dropTimer += dt;
                if (dropTimer >= dropSpeed) { dropTimer = 0.0f; piecePos.y++; if (CheckCollision(currentPiece, piecePos)) { piecePos.y--; LockPiece(); } }
                if (currentState == PLAYING_BOSS) {
                    bossRotation += 80.0f * dt; bossAttackTimer += dt;
                    if (bossAttackTimer > 8.0f) {
                        bossAttackTimer = 0.0f; int rx = GetRandomValue(0, gridWidth - 1); int ry = gridHeight - 1;
                        while (ry > 0 && grid[rx][ry].a > 0) ry--;
                        grid[rx][ry] = DARKGRAY; screenShake = 3.0f; PlayGameSound(fxDrop);
                    }
                }
                DrawGameScreen();
            } break;
            
            case CONTINUE_SCREEN: {
                DrawBackground3D(); continueTimer -= dt;
                DrawTextEx(mainFont, GetText(6), (Vector2){ SCREEN_WIDTH/2.0f - MeasureTextEx(mainFont, GetText(6), 110, 2).x/2, 450 }, 110, 2, RED);
                DrawTextEx(mainFont, TextFormat("%d", (int)continueTimer), (Vector2){ SCREEN_WIDTH/2.0f - 60, 650 }, 220, 2, YELLOW);
                Rectangle btnSim = { SCREEN_WIDTH/2.0f - 350, 1200, 300, 200 }; Rectangle btnNao = { SCREEN_WIDTH/2.0f + 50,  1200, 300, 200 };
                DrawSuperButton(btnSim, GetText(7), GREEN, IsTouchInRect(btnSim)); DrawSuperButton(btnNao, GetText(8), RED, IsTouchInRect(btnNao));
                if (IsTouchInRect(btnSim)) { continues--; lives = 3; ResetBoard(); currentState = PLAYING_CLASSIC; } else if (IsTouchInRect(btnNao) || continueTimer <= 0.0f) currentState = GAME_OVER;
            } break;

            case GAME_OVER: {
                DrawBackground3D(); DrawTextEx(mainFont, "GAME OVER", (Vector2){ SCREEN_WIDTH/2.0f - MeasureTextEx(mainFont, "GAME OVER", 150, 2).x/2, SCREEN_HEIGHT/2.0f - 200 }, 150, 2, RED);
                Rectangle btnGOMenu = { SCREEN_WIDTH/2.0f - 250, SCREEN_HEIGHT/2.0f + 150, 500, 180 }; DrawSuperButton(btnGOMenu, "MENU", BLUE, IsTouchInRect(btnGOMenu));
                if (IsTouchInRect(btnGOMenu)) { lives = 3; continues = 3; currentState = MENU; }
            } break;
            
            case CONFIG: case CREDITS: {
                DrawBackground3D(); Rectangle btnBack = { SCREEN_WIDTH/2.0f - 250, SCREEN_HEIGHT - 400, 500, 180 };
                if (currentState == CONFIG) {
                    DrawTextEx(mainFont, GetText(3), (Vector2){ 100, 200 }, 100, 2, WHITE); Rectangle btnPT = { 100, 500, 400, 180 }; Rectangle btnEN = { 580, 500, 400, 180 };
                    DrawSuperButton(btnPT, "PT-BR", config.lang == PT_BR ? GREEN : GRAY, IsTouchInRect(btnPT)); DrawSuperButton(btnEN, "EN-US", config.lang == EN_US ? GREEN : GRAY, IsTouchInRect(btnEN));
                    if (IsTouchInRect(btnPT)) config.lang = PT_BR; if (IsTouchInRect(btnEN)) config.lang = EN_US;
                } else {
                    DrawTextEx(mainFont, GetText(4), (Vector2){ 100, 200 }, 100, 2, WHITE); DrawTextEx(mainFont, GetText(12), (Vector2){ SCREEN_WIDTH/2.0f - MeasureTextEx(mainFont, GetText(12), 45, 2).x/2, SCREEN_HEIGHT/2.0f }, 45, 2, SKYBLUE);
                }
                DrawSuperButton(btnBack, GetText(17), RED, IsTouchInRect(btnBack)); if (IsTouchInRect(btnBack)) currentState = MENU;
            } break;
        }
        EndDrawing();
    }
    if (bgMusic.stream.buffer != NULL) UnloadMusicStream(bgMusic); CloseAudioDevice(); CloseWindow(); return 0;
}