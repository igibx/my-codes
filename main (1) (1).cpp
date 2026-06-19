// Neon Pulse Matrix
// Scaffold técnico em um único arquivo .cpp
// Estilo: C procedural compilado como C++
// Dependência: raylib (para desktop e Android)
// Objetivo: servir como base altamente detalhada para a implementação completa

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================
// 1. CONSTANTES GERAIS
// ============================================================

#define GAME_WIDTH          720
#define GAME_HEIGHT         1280
#define TARGET_FPS          60

#define BOARD_W             10
#define BOARD_H             24        // 20 visíveis + 4 ocultas
#define BOARD_VISIBLE_H     20
#define BLOCK_SIZE          (GAME_WIDTH / BOARD_W)

#define BOARD_OFFSET_X      0
#define BOARD_OFFSET_Y      (GAME_HEIGHT - (BOARD_VISIBLE_H * BLOCK_SIZE)) / 2

#define NEXT_PIECE_OFFSET_X (BOARD_OFFSET_X + BOARD_W * BLOCK_SIZE + 20)
#define NEXT_PIECE_OFFSET_Y BOARD_OFFSET_Y

#define HOLD_PIECE_OFFSET_X (BOARD_OFFSET_X - 4 * BLOCK_SIZE - 20)
#define HOLD_PIECE_OFFSET_Y BOARD_OFFSET_Y

#define MAX_PARTICLES       512
#define MAX_FLASH_LINES     4
#define ENERGY_MAX          100
#define NEXT_QUEUE_SIZE     5

#define STATE_BOOT          0
#define STATE_MENU          1
#define STATE_PLAYING       2
#define STATE_LINE_CLEAR    3
#define STATE_PAUSED        4
#define STATE_GAME_OVER     5
#define STATE_EXIT          6

#define PIECE_I             0
#define PIECE_O             1
#define PIECE_T             2
#define PIECE_S             3
#define PIECE_Z             4
#define PIECE_J             5
#define PIECE_L             6
#define PIECE_COUNT         7

#define ROT_0               0
#define ROT_1               1
#define ROT_2               2
#define ROT_3               3

#define CELL_EMPTY          0

#define INPUT_LEFT          0x0001
#define INPUT_RIGHT         0x0002
#define INPUT_DOWN          0x0004
#define INPUT_ROTATE_CW     0x0008
#define INPUT_ROTATE_CCW    0x0010
#define INPUT_HARD_DROP     0x0020
#define INPUT_HOLD          0x0040
#define INPUT_PULSE         0x0080
#define INPUT_PAUSE         0x0100
#define INPUT_CONFIRM       0x0200

// ============================================================
// 2. TIPOS AUXILIARES
// ============================================================

typedef struct GamePiece {
    int type;
    int rotation;
    int x;
    int y;
    Color color;
} GamePiece;

typedef struct Particle {
    int active;
    Vector2 position;
    Vector2 velocity;
    float life;
    float maxLife;
    Color color;
    float size;
} Particle;

typedef struct LineFlash {
    int active;
    int row;
    float timer;
    float duration;
} LineFlash;

typedef struct InputState {
    unsigned int pressed;
    unsigned int held;
    Vector2 touchStart;
    Vector2 touchEnd;
    float touchTime;
    int touchActive;
} InputState;

typedef struct ScoreSystem {
    int score;
    int level;
    int lines;
    int combo;
    int backToBack;
    int lastClearWasBig;
} ScoreSystem;

typedef struct EnergySystem {
    int value;
    int ready;
    float pulseWaveTimer;
    int pulseWaveActive;
    int pulseTargetRow;
} EnergySystem;

typedef struct GameRuntime {
    int appState;
    int running;

    int board[BOARD_H][BOARD_W];
    GamePiece currentPiece;
    GamePiece nextQueue[NEXT_QUEUE_SIZE];
    int nextQueueCount;

    GamePiece holdPiece;
    int holdUsedThisTurn;
    int hasHoldPiece;

    ScoreSystem score;
    EnergySystem energy;

    Particle particles[MAX_PARTICLES];
    LineFlash flashes[MAX_FLASH_LINES];

    float gravityTimer;
    float gravityInterval;
    float lockTimer;
    float lockDelay;
    int touchingGround;

    float lineClearTimer;
    float lineClearDuration;
    int clearRows[4];
    int clearRowCount;

    float hudPulse;
    float menuAnim;
    float gameOverAnim;

    unsigned int randomSeed;
} GameRuntime;

// ============================================================
// 3. VARIÁVEIS GLOBAIS
// ============================================================

static GameRuntime gGame;
static InputState gInput;

// ============================================================
// 4. PALETA DE CORES (raylib Color)
// ============================================================

static Color gPalette[16] = {
    {  0,   0,   0,   0},   // 0 vazio
    {  0, 240, 255, 255},   // I - ciano
    {255, 215,  60, 255},   // O - dourado
    {255,  60, 220, 255},   // T - magenta
    { 70, 255, 120, 255},   // S - verde elétrico
    {255,  80, 130, 255},   // Z - vermelho rosado
    { 70, 120, 255, 255},   // J - azul
    {255, 150,  40, 255},   // L - laranja
    {255, 255, 255, 255},   // brilho branco
    { 20,  24,  38, 255},   // fundo profundo
    { 50,  10,  80, 255},   // roxo escuro
    { 20, 220, 255, 180},   // ciano translúcido
    {255, 255, 255,  40},   // grade sutil
    {255, 255, 255, 120},   // brilho médio
    {255, 200,  80, 255},   // dourado forte
    {180, 120, 255, 255}    // violeta de apoio
};

// ============================================================
// 5. DADOS DAS PEÇAS
//    Formato: [piece][rotation][4][4]
// ============================================================

static const int gTetromino[PIECE_COUNT][4][4][4] = {
    // I
    {
        {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
        {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}
    },
    // O
    {
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}
    },
    // T
    {
        {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // S
    {
        {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
        {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // Z
    {
        {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}
    },
    // J
    {
        {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}
    },
    // L
    {
        {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
        {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}
    }
};

// ============================================================
// 6. PROTÓTIPOS
// ============================================================

static void app_reset_runtime(void);
static void app_init_gameplay(void);
static void app_shutdown(void);

static int  rng_next_int(int maxValue);
static void board_clear_all(void);
static int  board_is_inside(int x, int y);
static int  board_is_occupied(int x, int y);
static int  piece_check_collision(const GamePiece* piece, int testX, int testY, int testRotation);
static void piece_spawn(void);
static void piece_commit_to_board(void);
static int  piece_try_move(int dx, int dy);
static int  piece_try_rotate(int dir);
static void piece_hard_drop(void);
static void piece_hold_swap(void);
static int  piece_get_ghost_y(void);
static void piece_fill_queue_if_needed(void);
static GamePiece piece_make_by_type(int type);

static int  board_find_full_lines(int outRows[4]);
static void board_remove_lines(const int rows[], int count);
static void board_collapse_above_row(int row);

static void score_add_drop_points(int cells, int hardDrop);
static void score_on_lines_cleared(int count);
static void score_update_level(void);

static void energy_add(int amount);
static int  energy_can_activate(void);
static void energy_activate_pulse(void);
static void energy_apply_pulse_effect(void);

static void particles_reset_all(void);
static void particles_spawn_block_burst(int cellX, int cellY, Color color, int amount);
static void particles_update(float dt);

static void flashes_reset_all(void);
static void flashes_add_line(int row);
static void flashes_update(float dt);

static void state_enter_menu(void);
static void state_enter_game(void);
static void state_enter_line_clear(void);
static void state_enter_game_over(void);

static void input_begin_frame(void);
static void input_poll(void);
static int  input_pressed(unsigned int mask);
static int  input_held(unsigned int mask);

static float gameplay_get_gravity_interval(int level);
static void  gameplay_lock_current_piece(void);
static void  gameplay_handle_line_results(void);
static void  gameplay_update(float dt);

static void draw_block(int screenX, int screenY, Color color, int isGhost);
static void draw_background(void);
static void draw_board(void);
static void draw_piece(const GamePiece* piece, int offsetX, int offsetY, int isGhost);
static void draw_hud(void);
static void draw_menu(void);
static void draw_game_over(void);
static void draw_particles(void);
static void draw_flashes(void);

// ============================================================
// 7. IMPLEMENTAÇÃO BÁSICA DE UTILITÁRIOS
// ============================================================

static void app_reset_runtime(void) {
    memset(&gGame, 0, sizeof(gGame));
    memset(&gInput, 0, sizeof(gInput));
    gGame.running = 1;
    gGame.appState = STATE_BOOT;
    gGame.randomSeed = (unsigned int)time(NULL);
    srand(gGame.randomSeed);
}

static void app_init_gameplay(void) {
    board_clear_all();
    particles_reset_all();
    flashes_reset_all();

    gGame.score.score = 0;
    gGame.score.level = 1;
    gGame.score.lines = 0;
    gGame.score.combo = -1;
    gGame.score.backToBack = 0;
    gGame.score.lastClearWasBig = 0;

    gGame.energy.value = 0;
    gGame.energy.ready = 0;
    gGame.energy.pulseWaveTimer = 0.0f;
    gGame.energy.pulseWaveActive = 0;
    gGame.energy.pulseTargetRow = -1;

    gGame.gravityTimer = 0.0f;
    gGame.gravityInterval = gameplay_get_gravity_interval(gGame.score.level);
    gGame.lockTimer = 0.0f;
    gGame.lockDelay = 0.45f;
    gGame.touchingGround = 0;

    gGame.lineClearTimer = 0.0f;
    gGame.lineClearDuration = 0.20f;
    gGame.clearRowCount = 0;

    gGame.holdUsedThisTurn = 0;
    gGame.hasHoldPiece = 0;
    gGame.nextQueueCount = 0;
    piece_fill_queue_if_needed();
    piece_spawn();
}

static void app_shutdown(void) {
    CloseWindow();
}

static int rng_next_int(int maxValue) {
    if (maxValue <= 0) return 0;
    return rand() % maxValue;
}

static void board_clear_all(void) {
    memset(gGame.board, 0, sizeof(gGame.board));
}

static int board_is_inside(int x, int y) {
    if (x < 0 || x >= BOARD_W) return 0;
    if (y < 0 || y >= BOARD_H) return 0;
    return 1;
}

static int board_is_occupied(int x, int y) {
    if (!board_is_inside(x, y)) return 1;
    return gGame.board[y][x] != CELL_EMPTY;
}

static GamePiece piece_make_by_type(int type) {
    GamePiece p;
    p.type = type;
    p.rotation = ROT_0;
    p.x = 3;
    p.y = 0;
    p.color = gPalette[type + 1];
    return p;
}

static void piece_fill_queue_if_needed(void) {
    while (gGame.nextQueueCount < NEXT_QUEUE_SIZE) {
        int type = rng_next_int(PIECE_COUNT);
        gGame.nextQueue[gGame.nextQueueCount++] = piece_make_by_type(type);
    }
}

static int piece_check_collision(const GamePiece* piece, int testX, int testY, int testRotation) {
    int py, px;

    for (py = 0; py < 4; ++py) {
        for (px = 0; px < 4; ++px) {
            if (gTetromino[piece->type][testRotation][py][px]) {
                int bx = testX + px;
                int by = testY + py;

                if (bx < 0 || bx >= BOARD_W) return 1;
                if (by >= BOARD_H) return 1;
                if (by >= 0 && gGame.board[by][bx] != CELL_EMPTY) return 1;
            }
        }
    }

    return 0;
}

static void piece_spawn(void) {
    int i;

    if (gGame.nextQueueCount <= 0) {
        piece_fill_queue_if_needed();
    }

    gGame.currentPiece = gGame.nextQueue[0];
    for (i = 1; i < gGame.nextQueueCount; ++i) {
        gGame.nextQueue[i - 1] = gGame.nextQueue[i];
    }
    gGame.nextQueueCount--;
    piece_fill_queue_if_needed();

    gGame.currentPiece.x = 3;
    gGame.currentPiece.y = 0;
    gGame.currentPiece.rotation = ROT_0;
    gGame.holdUsedThisTurn = 0;
    gGame.lockTimer = 0.0f;
    gGame.touchingGround = 0;

    if (piece_check_collision(&gGame.currentPiece, gGame.currentPiece.x, gGame.currentPiece.y, gGame.currentPiece.rotation)) {
        state_enter_game_over();
    }
}

static int piece_try_move(int dx, int dy) {
    int nx = gGame.currentPiece.x + dx;
    int ny = gGame.currentPiece.y + dy;
    int nr = gGame.currentPiece.rotation;

    if (!piece_check_collision(&gGame.currentPiece, nx, ny, nr)) {
        gGame.currentPiece.x = nx;
        gGame.currentPiece.y = ny;
        return 1;
    }
    return 0;
}

static int piece_try_rotate(int dir) {
    static const int kicks[5][2] = {
        { 0, 0}, { 1, 0}, {-1, 0}, { 0,-1}, { 2, 0}
    };

    int i;
    int newRot = (gGame.currentPiece.rotation + dir + 4) % 4;

    for (i = 0; i < 5; ++i) {
        int tx = gGame.currentPiece.x + kicks[i][0];
        int ty = gGame.currentPiece.y + kicks[i][1];
        if (!piece_check_collision(&gGame.currentPiece, tx, ty, newRot)) {
            gGame.currentPiece.x = tx;
            gGame.currentPiece.y = ty;
            gGame.currentPiece.rotation = newRot;
            return 1;
        }
    }

    return 0;
}

static void piece_commit_to_board(void) {
    int py, px;

    for (py = 0; py < 4; ++py) {
        for (px = 0; px < 4; ++px) {
            if (gTetromino[gGame.currentPiece.type][gGame.currentPiece.rotation][py][px]) {
                int bx = gGame.currentPiece.x + px;
                int by = gGame.currentPiece.y + py;
                if (board_is_inside(bx, by)) {
                    gGame.board[by][bx] = gGame.currentPiece.type + 1;
                    particles_spawn_block_burst(bx, by, gGame.currentPiece.color, 3);
                }
            }
        }
    }
}

static int piece_get_ghost_y(void) {
    GamePiece ghost = gGame.currentPiece;
    while (!piece_check_collision(&ghost, ghost.x, ghost.y + 1, ghost.rotation)) {
        ghost.y++;
    }
    return ghost.y;
}

static void piece_hard_drop(void) {
    int cells = 0;
    while (piece_try_move(0, 1)) {
        cells++;
    }
    score_add_drop_points(cells, 1);
    gameplay_lock_current_piece();
}

static void piece_hold_swap(void) {
    GamePiece temp;

    if (gGame.holdUsedThisTurn) return;

    if (!gGame.hasHoldPiece) {
        gGame.holdPiece = piece_make_by_type(gGame.currentPiece.type);
        gGame.hasHoldPiece = 1;
        piece_spawn();
    } else {
        temp = gGame.holdPiece;
        gGame.holdPiece = piece_make_by_type(gGame.currentPiece.type);
        gGame.currentPiece = piece_make_by_type(temp.type);
        gGame.currentPiece.x = 3;
        gGame.currentPiece.y = 0;
    }

    gGame.holdUsedThisTurn = 1;
}

static int board_find_full_lines(int outRows[4]) {
    int y, x;
    int count = 0;

    for (y = 0; y < BOARD_H; ++y) {
        int full = 1;
        for (x = 0; x < BOARD_W; ++x) {
            if (gGame.board[y][x] == CELL_EMPTY) {
                full = 0;
                break;
            }
        }
        if (full && count < 4) {
            outRows[count++] = y;
        }
    }

    return count;
}

static void board_collapse_above_row(int row) {
    int y, x;
    for (y = row; y > 0; --y) {
        for (x = 0; x < BOARD_W; ++x) {
            gGame.board[y][x] = gGame.board[y - 1][x];
        }
    }
    for (x = 0; x < BOARD_W; ++x) {
        gGame.board[0][x] = CELL_EMPTY;
    }
}

static void board_remove_lines(const int rows[], int count) {
    int i;
    for (i = 0; i < count; ++i) {
        board_collapse_above_row(rows[i]);
    }
}

static void score_add_drop_points(int cells, int hardDrop) {
    if (hardDrop) gGame.score.score += cells * 2;
    else gGame.score.score += cells;
}

static void score_on_lines_cleared(int count) {
    int base = 0;

    if (count > 0) gGame.score.combo++;
    else gGame.score.combo = -1;

    switch (count) {
        case 1: base = 100; break;
        case 2: base = 300; break;
        case 3: base = 500; break;
        case 4: base = 800; break;
        default: base = 0; break;
    }

    if (count > 0) {
        gGame.score.score += base * gGame.score.level;
        if (gGame.score.combo > 0) {
            gGame.score.score += 50 * gGame.score.combo * gGame.score.level;
        }
        gGame.score.lines += count;
        energy_add(20 * count + (gGame.score.combo > 0 ? 10 * gGame.score.combo : 0));
    }

    score_update_level();
}

static void score_update_level(void) {
    gGame.score.level = 1 + (gGame.score.lines / 10);
    gGame.gravityInterval = gameplay_get_gravity_interval(gGame.score.level);
}

static void energy_add(int amount) {
    gGame.energy.value += amount;
    if (gGame.energy.value >= ENERGY_MAX) {
        gGame.energy.value = ENERGY_MAX;
        gGame.energy.ready = 1;
    }
}

static int energy_can_activate(void) {
    return gGame.energy.ready && !gGame.energy.pulseWaveActive;
}

static void energy_activate_pulse(void) {
    if (!energy_can_activate()) return;

    gGame.energy.value = 0;
    gGame.energy.ready = 0;
    gGame.energy.pulseWaveActive = 1;
    gGame.energy.pulseWaveTimer = 0.0f;
    gGame.energy.pulseTargetRow = BOARD_H - 6;

    energy_apply_pulse_effect();
}

static void energy_apply_pulse_effect(void) {
    int y = gGame.energy.pulseTargetRow;
    int x;

    if (y < 0 || y >= BOARD_H) return;

    for (x = 0; x < BOARD_W; ++x) {
        if (gGame.board[y][x] != CELL_EMPTY) {
            particles_spawn_block_burst(x, y, gPalette[gGame.board[y][x]], 6);
            gGame.board[y][x] = CELL_EMPTY;
        }
    }
}

static void particles_reset_all(void) {
    memset(gGame.particles, 0, sizeof(gGame.particles));
}

static void particles_spawn_block_burst(int cellX, int cellY, Color color, int amount) {
    int i, j;
    float baseX = (float)(BOARD_OFFSET_X + cellX * BLOCK_SIZE + BLOCK_SIZE / 2);
    float baseY = (float)(BOARD_OFFSET_Y + (cellY - (BOARD_H - BOARD_VISIBLE_H)) * BLOCK_SIZE + BLOCK_SIZE / 2);

    for (i = 0; i < amount; ++i) {
        for (j = 0; j < MAX_PARTICLES; ++j) {
            if (!gGame.particles[j].active) {
                gGame.particles[j].active = 1;
                gGame.particles[j].position = (Vector2){baseX, baseY};
                gGame.particles[j].velocity = (Vector2){((float)(rng_next_int(200) - 100)) * 0.6f, ((float)(rng_next_int(200) - 140)) * 0.6f};
                gGame.particles[j].life = 0.0f;
                gGame.particles[j].maxLife = 0.4f + (float)rng_next_int(40) / 100.0f;
                gGame.particles[j].color = color;
                gGame.particles[j].size = 2.0f + (float)rng_next_int(4);
                break;
            }
        }
    }
}

static void particles_update(float dt) {
    int i;
    for (i = 0; i < MAX_PARTICLES; ++i) {
        Particle* p = &gGame.particles[i];
        if (!p->active) continue;

        p->life += dt;
        if (p->life >= p->maxLife) {
            p->active = 0;
            continue;
        }

        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->velocity.y += 180.0f * dt; // Gravidade simples para partículas
    }
}

static void flashes_reset_all(void) {
    memset(gGame.flashes, 0, sizeof(gGame.flashes));
}

static void flashes_add_line(int row) {
    int i;
    for (i = 0; i < MAX_FLASH_LINES; ++i) {
        if (!gGame.flashes[i].active) {
            gGame.flashes[i].active = 1;
            gGame.flashes[i].row = row;
            gGame.flashes[i].timer = 0.0f;
            gGame.flashes[i].duration = 0.20f;
            return;
        }
    }
}

static void flashes_update(float dt) {
    int i;
    for (i = 0; i < MAX_FLASH_LINES; ++i) {
        if (!gGame.flashes[i].active) continue;
        gGame.flashes[i].timer += dt;
        if (gGame.flashes[i].timer >= gGame.flashes[i].duration) {
            gGame.flashes[i].active = 0;
        }
    }
}

static void state_enter_menu(void) {
    gGame.appState = STATE_MENU;
    gGame.menuAnim = 0.0f;
}

static void state_enter_game(void) {
    app_init_gameplay();
    gGame.appState = STATE_PLAYING;
}

static void state_enter_line_clear(void) {
    gGame.appState = STATE_LINE_CLEAR;
    gGame.lineClearTimer = 0.0f;
}

static void state_enter_game_over(void) {
    gGame.appState = STATE_GAME_OVER;
    gGame.gameOverAnim = 0.0f;
}

static void input_begin_frame(void) {
    gInput.pressed = 0;
    // Reset touch state for new frame
    if (gInput.touchActive && !IsGestureDetected(GESTURE_HOLD)) {
        gInput.touchActive = 0;
        gInput.touchStart = (Vector2){0,0};
        gInput.touchEnd = (Vector2){0,0};
        gInput.touchTime = 0.0f;
    }
}

static void input_poll(void) {
    // Teclado (para testes em desktop)
    if (IsKeyPressed(KEY_LEFT)) gInput.pressed |= INPUT_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) gInput.pressed |= INPUT_RIGHT;
    if (IsKeyDown(KEY_DOWN)) gInput.held |= INPUT_DOWN;
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_UP)) gInput.pressed |= INPUT_ROTATE_CCW;
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_SPACE)) gInput.pressed |= INPUT_ROTATE_CW;
    if (IsKeyPressed(KEY_SPACE)) gInput.pressed |= INPUT_HARD_DROP;
    if (IsKeyPressed(KEY_C)) gInput.pressed |= INPUT_HOLD;
    if (IsKeyPressed(KEY_V)) gInput.pressed |= INPUT_PULSE;
    if (IsKeyPressed(KEY_P)) gInput.pressed |= INPUT_PAUSE;
    if (IsKeyPressed(KEY_ENTER)) gInput.pressed |= INPUT_CONFIRM;

    // Toque (para Android)
    if (GetTouchPointCount() > 0) {
        Vector2 touchPos = GetTouchPosition(0);

        if (!gInput.touchActive) {
            gInput.touchStart = touchPos;
            gInput.touchActive = 1;
            gInput.touchTime = 0.0f;
        } else {
            gInput.touchEnd = touchPos;
            gInput.touchTime += GetFrameTime();
        }

        // Gestos simples
        if (IsGestureDetected(GESTURE_TAP)) {
            // Tap na metade superior do tabuleiro para rotacionar
            if (touchPos.y < BOARD_OFFSET_Y + (BOARD_VISIBLE_H / 2) * BLOCK_SIZE) {
                gInput.pressed |= INPUT_ROTATE_CW;
            }
            // Tap na metade inferior para soft drop (ou apenas segurar)
            else {
                gInput.held |= INPUT_DOWN;
            }
        }
        if (IsGestureDetected(GESTURE_DRAG)) {
            float deltaX = gInput.touchEnd.x - gInput.touchStart.x;
            float deltaY = gInput.touchEnd.y - gInput.touchStart.y;

            // Movimento lateral
            if (fabs(deltaX) > BLOCK_SIZE / 2 && fabs(deltaY) < BLOCK_SIZE / 2) {
                if (deltaX > 0) gInput.pressed |= INPUT_RIGHT;
                else gInput.pressed |= INPUT_LEFT;
                gInput.touchStart = gInput.touchEnd; // Reset para evitar múltiplos movimentos por um único arrasto longo
            }
            // Hard drop (deslize rápido para baixo)
            else if (deltaY > BLOCK_SIZE * 1.5f && gInput.touchTime < 0.2f) {
                gInput.pressed |= INPUT_HARD_DROP;
                gInput.touchActive = 0; // Consumir o toque
            }
        }
        if (IsGestureDetected(GESTURE_DOUBLETAP)) {
            gInput.pressed |= INPUT_HARD_DROP;
        }

        // Botões virtuais (exemplo de área para Hold/Pulse)
        Rectangle holdButton = {HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y + 5 * BLOCK_SIZE, 4 * BLOCK_SIZE, 2 * BLOCK_SIZE};
        Rectangle pulseButton = {NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y + 5 * BLOCK_SIZE, 4 * BLOCK_SIZE, 2 * BLOCK_SIZE};

        if (CheckCollisionPointRec(touchPos, holdButton) && IsGestureDetected(GESTURE_TAP)) {
            gInput.pressed |= INPUT_HOLD;
        }
        if (CheckCollisionPointRec(touchPos, pulseButton) && IsGestureDetected(GESTURE_TAP)) {
            gInput.pressed |= INPUT_PULSE;
        }
    }
}

static int input_pressed(unsigned int mask) {
    return (gInput.pressed & mask) != 0;
}

static int input_held(unsigned int mask) {
    return (gInput.held & mask) != 0;
}

static float gameplay_get_gravity_interval(int level) {
    float t = 0.80f - (float)(level - 1) * 0.08f;
    if (t < 0.08f) t = 0.08f;
    return t;
}

static void gameplay_lock_current_piece(void) {
    gGame.touchingGround = 0;
    gGame.lockTimer = 0.0f;

    piece_commit_to_board();
    gameplay_handle_line_results();

    if (gGame.clearRowCount <= 0) {
        piece_spawn();
    }
}

static void gameplay_handle_line_results(void) {
    int i;
    gGame.clearRowCount = board_find_full_lines(gGame.clearRows);

    if (gGame.clearRowCount > 0) {
        for (i = 0; i < gGame.clearRowCount; ++i) {
            flashes_add_line(gGame.clearRows[i]);
        }
        score_on_lines_cleared(gGame.clearRowCount);
        state_enter_line_clear();
    } else {
        score_on_lines_cleared(0);
    }
}

static void gameplay_update(float dt) {
    if (gGame.appState == STATE_MENU) {
        gGame.menuAnim += dt;
        if (input_pressed(INPUT_CONFIRM)) {
            state_enter_game();
        }
        return;
    }

    if (gGame.appState == STATE_PAUSED) {
        if (input_pressed(INPUT_PAUSE)) {
            gGame.appState = STATE_PLAYING;
        }
        return;
    }

    if (gGame.appState == STATE_GAME_OVER) {
        gGame.gameOverAnim += dt;
        if (input_pressed(INPUT_CONFIRM)) {
            state_enter_menu();
        }
        return;
    }

    if (gGame.appState == STATE_LINE_CLEAR) {
        gGame.lineClearTimer += dt;
        if (gGame.lineClearTimer >= gGame.lineClearDuration) {
            board_remove_lines(gGame.clearRows, gGame.clearRowCount);
            gGame.clearRowCount = 0;
            gGame.appState = STATE_PLAYING;
            piece_spawn();
        }
        return;
    }

    if (gGame.appState != STATE_PLAYING) return;

    if (input_pressed(INPUT_PAUSE)) {
        gGame.appState = STATE_PAUSED;
        return;
    }

    if (input_held(INPUT_DOWN)) {
        gGame.gravityTimer += dt * 8.0f;
    }

    if (input_pressed(INPUT_LEFT))  piece_try_move(-1, 0);
    if (input_pressed(INPUT_RIGHT)) piece_try_move( 1, 0);
    if (input_pressed(INPUT_ROTATE_CW))  piece_try_rotate( 1);
    if (input_pressed(INPUT_ROTATE_CCW)) piece_try_rotate(-1);
    if (input_pressed(INPUT_HOLD)) piece_hold_swap();
    if (input_pressed(INPUT_PULSE)) energy_activate_pulse();
    if (input_pressed(INPUT_HARD_DROP)) {
        piece_hard_drop();
        return;
    }

    gGame.gravityTimer += dt;

    if (gGame.gravityTimer >= gGame.gravityInterval) {
        gGame.gravityTimer = 0.0f;
        if (!piece_try_move(0, 1)) {
            gGame.touchingGround = 1;
        } else {
            score_add_drop_points(1, 0);
            gGame.touchingGround = 0;
            gGame.lockTimer = 0.0f;
        }
    }

    if (gGame.touchingGround) {
        if (piece_check_collision(&gGame.currentPiece, gGame.currentPiece.x, gGame.currentPiece.y + 1, gGame.currentPiece.rotation)) {
            gGame.lockTimer += dt;
            if (gGame.lockTimer >= gGame.lockDelay) {
                gameplay_lock_current_piece();
            }
        } else {
            gGame.touchingGround = 0;
            gGame.lockTimer = 0.0f;
        }
    }

    if (gGame.energy.pulseWaveActive) {
        gGame.energy.pulseWaveTimer += dt;
        if (gGame.energy.pulseWaveTimer >= 0.25f) {
            gGame.energy.pulseWaveActive = 0;
            gGame.energy.pulseWaveTimer = 0.0f;
        }
    }
}

static void draw_block(int screenX, int screenY, Color color, int isGhost) {
    Color blockColor = color;
    if (isGhost) {
        blockColor.a = 100; // Transparência para ghost piece
    }

    DrawRectangle(screenX, screenY, BLOCK_SIZE, BLOCK_SIZE, blockColor);
    DrawRectangleLines(screenX, screenY, BLOCK_SIZE, BLOCK_SIZE, (Color){255, 255, 255, blockColor.a / 2});
    DrawRectangle(screenX + 2, screenY + 2, BLOCK_SIZE - 4, BLOCK_SIZE - 4, (Color){255, 255, 255, blockColor.a / 4});
}

static void draw_background(void) {
    ClearBackground(gPalette[9]); // Fundo profundo

    // Desenhar linhas horizontais animadas (simples)
    float time = GetTime();
    for (int i = 0; i < GAME_HEIGHT / 50; ++i) {
        int y = (int)(i * 50 + fmod(time * 10, 50));
        DrawLine(0, y, GAME_WIDTH, y, (Color){50, 50, 50, 50});
    }
}

static void draw_board(void) {
    // Desenhar moldura do tabuleiro
    DrawRectangleLinesEx((Rectangle){BOARD_OFFSET_X - 2, BOARD_OFFSET_Y - 2, BOARD_W * BLOCK_SIZE + 4, BOARD_VISIBLE_H * BLOCK_SIZE + 4}, 2, gPalette[11]);

    // Desenhar grade interna
    for (int y = 0; y < BOARD_VISIBLE_H; ++y) {
        for (int x = 0; x < BOARD_W; ++x) {
            DrawRectangleLines(BOARD_OFFSET_X + x * BLOCK_SIZE, BOARD_OFFSET_Y + y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, gPalette[12]);
        }
    }

    // Desenhar blocos fixos no tabuleiro
    for (int y = BOARD_H - BOARD_VISIBLE_H; y < BOARD_H; ++y) {
        for (int x = 0; x < BOARD_W; ++x) {
            if (gGame.board[y][x] != CELL_EMPTY) {
                draw_block(BOARD_OFFSET_X + x * BLOCK_SIZE, BOARD_OFFSET_Y + (y - (BOARD_H - BOARD_VISIBLE_H)) * BLOCK_SIZE, gPalette[gGame.board[y][x]], 0);
            }
        }
    }

    // Desenhar ghost piece
    int ghostY = piece_get_ghost_y();
    GamePiece ghost = gGame.currentPiece;
    ghost.y = ghostY;
    draw_piece(&ghost, BOARD_OFFSET_X, BOARD_OFFSET_Y - (BOARD_H - BOARD_VISIBLE_H) * BLOCK_SIZE, 1);

    // Desenhar peça atual
    draw_piece(&gGame.currentPiece, BOARD_OFFSET_X, BOARD_OFFSET_Y - (BOARD_H - BOARD_VISIBLE_H) * BLOCK_SIZE, 0);
}

static void draw_piece(const GamePiece* piece, int offsetX, int offsetY, int isGhost) {
    int py, px;
    for (py = 0; py < 4; ++py) {
        for (px = 0; px < 4; ++px) {
            if (gTetromino[piece->type][piece->rotation][py][px]) {
                int screenX = offsetX + (piece->x + px) * BLOCK_SIZE;
                int screenY = offsetY + (piece->y + py) * BLOCK_SIZE;
                if (piece->y + py >= (BOARD_H - BOARD_VISIBLE_H)) { // Renderizar apenas partes visíveis
                    draw_block(screenX, screenY, piece->color, isGhost);
                }
            }
        }
    }
}

static void draw_hud(void) {
    // NEXT Piece
    DrawText("NEXT", NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y - 30, 20, LIGHTGRAY);
    DrawRectangleLines(NEXT_PIECE_OFFSET_X - 5, NEXT_PIECE_OFFSET_Y - 5, 4 * BLOCK_SIZE + 10, 4 * BLOCK_SIZE + 10, gPalette[11]);
    if (gGame.nextQueueCount > 0) {
        draw_piece(&gGame.nextQueue[0], NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y, 0);
    }

    // HOLD Piece
    DrawText("HOLD", HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y - 30, 20, LIGHTGRAY);
    DrawRectangleLines(HOLD_PIECE_OFFSET_X - 5, HOLD_PIECE_OFFSET_Y - 5, 4 * BLOCK_SIZE + 10, 4 * BLOCK_SIZE + 10, gPalette[10]);
    if (gGame.hasHoldPiece) {
        draw_piece(&gGame.holdPiece, HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y, 0);
    }

    // Score, Level, Lines
    DrawText(TextFormat("SCORE: %d", gGame.score.score), HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y + 6 * BLOCK_SIZE, 20, WHITE);
    DrawText(TextFormat("LEVEL: %d", gGame.score.level), HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y + 7 * BLOCK_SIZE, 20, WHITE);
    DrawText(TextFormat("LINES: %d", gGame.score.lines), HOLD_PIECE_OFFSET_X, HOLD_PIECE_OFFSET_Y + 8 * BLOCK_SIZE, 20, WHITE);

    // Energy Bar
    DrawText("ENERGY", NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y + 6 * BLOCK_SIZE, 20, LIGHTGRAY);
    DrawRectangle(NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y + 7 * BLOCK_SIZE, 4 * BLOCK_SIZE, 20, DARKGRAY);
    DrawRectangle(NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y + 7 * BLOCK_SIZE, (int)(4 * BLOCK_SIZE * ((float)gGame.energy.value / ENERGY_MAX)), 20, gPalette[3]);
    DrawRectangleLines(NEXT_PIECE_OFFSET_X, NEXT_PIECE_OFFSET_Y + 7 * BLOCK_SIZE, 4 * BLOCK_SIZE, 20, gPalette[11]);
}

static void draw_menu(void) {
    DrawText("NEON PULSE MATRIX", GAME_WIDTH / 2 - MeasureText("NEON PULSE MATRIX", 40) / 2, GAME_HEIGHT / 4, 40, gPalette[1]);
    DrawText("Pressione ENTER ou toque para iniciar", GAME_WIDTH / 2 - MeasureText("Pressione ENTER ou toque para iniciar", 20) / 2, GAME_HEIGHT / 2, 20, WHITE);
}

static void draw_game_over(void) {
    DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){0, 0, 0, 180});
    DrawText("GAME OVER", GAME_WIDTH / 2 - MeasureText("GAME OVER", 50) / 2, GAME_HEIGHT / 3, 50, RED);
    DrawText(TextFormat("SCORE FINAL: %d", gGame.score.score), GAME_WIDTH / 2 - MeasureText(TextFormat("SCORE FINAL: %d", gGame.score.score), 30) / 2, GAME_HEIGHT / 2, 30, WHITE);
    DrawText("Pressione ENTER ou toque para o menu", GAME_WIDTH / 2 - MeasureText("Pressione ENTER ou toque para o menu", 20) / 2, GAME_HEIGHT * 2 / 3, 20, LIGHTGRAY);
}

static void draw_particles(void) {
    int i;
    for (i = 0; i < MAX_PARTICLES; ++i) {
        Particle* p = &gGame.particles[i];
        if (!p->active) continue;

        Color pColor = p->color;
        pColor.a = (unsigned char)(255 * (1.0f - p->life / p->maxLife));
        DrawRectangle((int)p->position.x, (int)p->position.y, (int)p->size, (int)p->size, pColor);
    }
}

static void draw_flashes(void) {
    int i;
    for (i = 0; i < MAX_FLASH_LINES; ++i) {
        LineFlash* f = &gGame.flashes[i];
        if (!f->active) continue;

        float alpha = 1.0f - (f->timer / f->duration);
        Color flashColor = {255, 255, 255, (unsigned char)(255 * alpha)};
        DrawRectangle(BOARD_OFFSET_X, BOARD_OFFSET_Y + (f->row - (BOARD_H - BOARD_VISIBLE_H)) * BLOCK_SIZE, BOARD_W * BLOCK_SIZE, BLOCK_SIZE, flashColor);
    }

    if (gGame.energy.pulseWaveActive) {
        float alpha = 1.0f - (gGame.energy.pulseWaveTimer / 0.25f);
        Color pulseColor = {20, 220, 255, (unsigned char)(255 * alpha)};
        DrawRectangle(BOARD_OFFSET_X, BOARD_OFFSET_Y + (gGame.energy.pulseTargetRow - (BOARD_H - BOARD_VISIBLE_H)) * BLOCK_SIZE, BOARD_W * BLOCK_SIZE, BLOCK_SIZE, pulseColor);
    }
}

// ============================================================
// 8. PONTO DE ENTRADA
// ============================================================

int main(void) {
    // Inicialização da janela e contexto raylib
    InitWindow(GAME_WIDTH, GAME_HEIGHT, "Neon Pulse Matrix");
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice(); // Inicializa o dispositivo de áudio

    app_reset_runtime();
    state_enter_menu();

    while (!WindowShouldClose() && gGame.appState != STATE_EXIT) {
        float dt = GetFrameTime();

        input_begin_frame();
        input_poll();
        gameplay_update(dt);
        particles_update(dt);
        flashes_update(dt);

        BeginDrawing();
            draw_background();

            if (gGame.appState == STATE_MENU) {
                draw_menu();
            } else {
                draw_board();
                draw_particles();
                draw_flashes();
                draw_hud();

                if (gGame.appState == STATE_PAUSED) {
                    DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){0, 0, 0, 120});
                    DrawText("PAUSADO", GAME_WIDTH / 2 - MeasureText("PAUSADO", 40) / 2, GAME_HEIGHT / 2 - 20, 40, WHITE);
                }
                else if (gGame.appState == STATE_GAME_OVER) {
                    draw_game_over();
                }
            }

        EndDrawing();
    }

    app_shutdown();
    return 0;
}
