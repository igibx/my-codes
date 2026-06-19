// ============================================================================
// PROJETO: TETRABETTA - GOD TIER EDITION
// AUTOR: Igor Bettarello Xavier / IA
// FORMATO: Arquivo Único (main.cpp)
// DESCRIÇÃO: Engine VFX Custom, Tetris Font, Settings, Touch Menu, Áudio, Wallkick
// DEPENDÊNCIA: Raylib
// ============================================================================

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <string>
#include <vector>

using namespace std;

// ----------------------------------------------------------------------------
// 1. CONSTANTES E MACROS
// ----------------------------------------------------------------------------
#define BOARD_W 10
#define BOARD_H 24
#define VISIBLE_H 20
#define CELL_SIZE 32

// Limites da Engine de VFX (Ultra)
#define MAX_PARTICLES 800
#define MAX_TEXTS 30
#define MAX_RAIN 150
#define MAX_SHOCKWAVES 20
#define MAX_BEAMS 15

#define FPS_TARGET 60

// Cores Neon
#define COLOR_BG        (Color){ 3, 5, 10, 255 }
#define COLOR_GRID      (Color){ 20, 30, 50, 200 }
#define COLOR_CYAN      (Color){ 0, 255, 255, 255 }
#define COLOR_GOLD      (Color){ 255, 230, 0, 255 }
#define COLOR_MAGENTA   (Color){ 255, 0, 255, 255 }
#define COLOR_GREEN     (Color){ 0, 255, 120, 255 }
#define COLOR_RED       (Color){ 255, 40, 60, 255 }
#define COLOR_BLUE      (Color){ 40, 120, 255, 255 }
#define COLOR_ORANGE    (Color){ 255, 140, 0, 255 }

// ----------------------------------------------------------------------------
// 2. ENUMS E STRUCTS
// ----------------------------------------------------------------------------
typedef enum { STATE_BOOT, STATE_MENU, STATE_SETTINGS, STATE_PLAYING_ARCADE, STATE_PLAYING_BOSS, STATE_LINE_CLEAR, STATE_GAME_OVER, STATE_VICTORY } GameState;
typedef enum { PIECE_NONE = 0, PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L } PieceType;

typedef struct { PieceType type; int rotation; int x, y; Color color; } GamePiece;

typedef struct { bool active; Vector2 pos, prev_pos, vel; float life, maxLife, size; Color color; } Particle;
typedef struct { bool active; Vector2 pos; char text[32]; Color color; float life, maxLife; } FloatingText;
typedef struct { float x, y, speed; int length; Color color; } DigitalRain;
typedef struct { bool active; Vector2 pos; float radius, max_radius, thickness; Color color; } Shockwave;
typedef struct { bool active; Rectangle rect; float life, maxLife; Color color; bool is_horizontal; } Beam;

// ----------------------------------------------------------------------------
// 3. PROTÓTIPOS
// ----------------------------------------------------------------------------
void init_game(); void reset_session(GameState mode);
void spawn_piece(); void set_next_piece();
bool check_collision(GamePiece p);
void lock_current_piece(); void move_piece(int dx, int dy);
void rotate_current_piece(); void hard_drop(); void hold_piece();
void process_lines(); void activate_pulse();
void handle_input(); void update_gameplay(float dt);
void update_vfx(float dt); void update_boss(float dt);
void spawn_particles(int x, int y, Color c, int count, float speedMult);
void spawn_floating_text(int x, int y, const char* text, Color c);
void spawn_shockwave(int x, int y, float max_r, float thick, Color c);
void spawn_beam(int x, int y, int w, int h, Color c, float life, bool horizontal);
void draw_block_neon_2d(int x, int y, int size, Color c, bool isGhost, int alphaMult);
void draw_cyber_background(); void draw_board(); void draw_piece(GamePiece p, bool isGhost);
void draw_hud(); void draw_vfx(); void draw_menu(); void draw_settings();
void draw_boss(); void draw_crt_overlay(); void draw_game_over_or_victory();
Color get_piece_color(PieceType type); int get_fall_speed();
void draw_prompts();

float MeasureMyText(const char* text, float blockSize);
void DrawMyText(const char* text, float x, float y, float blockSize, Color c);

// Idiomas e Áudio
const char* T(const char* pt, const char* en);
void init_audio_system(); void update_audio_system(); void unload_audio_system();

// ----------------------------------------------------------------------------
// 4. DADOS DAS PEÇAS (SRS 4x4)
// ----------------------------------------------------------------------------
const int PIECE_DATA[8][4][4][4] = {
    { { {0} } },
    { { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, { {0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0} }, { {0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0} } },
    { { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} } },
    { { {0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0} }, { {0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} }, { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} } },
    { { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0} }, { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0} } },
    { { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} }, { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} } },
    { { {0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0} }, { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} } },
    { { {0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0} }, { {1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} }, { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0} } }
};

// ----------------------------------------------------------------------------
// 5. GLOBAIS E SETTINGS
// ----------------------------------------------------------------------------
GameState current_state = STATE_BOOT;
int board[BOARD_H][BOARD_W];
GamePiece current_piece; PieceType next_piece_type, hold_piece_type;
bool can_hold = true;

int score = 0, level = 1, lines_cleared = 0, energy = 0, combo = 0;
int player_lives = 3;
float fall_timer = 0.0f, lock_timer = 0.0f, state_timer = 0.0f;
int lines_to_clear[4]; int num_lines_to_clear = 0;

Particle particles[MAX_PARTICLES]; FloatingText floating_texts[MAX_TEXTS];
DigitalRain rain[MAX_RAIN]; Shockwave shockwaves[MAX_SHOCKWAVES]; Beam beams[MAX_BEAMS];

float pulse_anim_timer = 0.0f, soft_drop_timer = 0.0f, screen_shake = 0.0f;
int board_x_offset = 0, board_y_offset = 0; float move_timer = 0.0f;

// Menu & Boss Globals
int menu_selection = 0;
float boss_hp = 5000.0f, boss_max_hp = 5000.0f, boss_attack_timer = 20.0f, boss_laser_anim = 0.0f;
float boss_respawn_timer = 0.0f, boss_attack_laser_anim = 0.0f;
int boss_level = 1;
bool is_boss_mode = false;

// Settings Globals
bool music_on = true;
bool sfx_on = true;
int graphics_level = 3; // 0: Low, 1: Med, 2: High, 3: Ultra
int settings_selection = 0;
bool lang_en = false; // PT-BR é o padrão

int get_particle_limit() { return (graphics_level == 0) ? 50 : (graphics_level == 1) ? 200 : (graphics_level == 2) ? 500 : MAX_PARTICLES; }
int get_rain_limit() { return (graphics_level == 0) ? 20 : (graphics_level == 1) ? 50 : (graphics_level == 2) ? 100 : MAX_RAIN; }

// Áudio Globals
Music bgm[15];
int current_music = 0;
Sound sfx_move, sfx_rotate, sfx_clear1, sfx_clear2, sfx_clear3, sfx_clear4, sfx_attack, sfx_impact;

// Prompts e Touch Globals
bool prompt_app_exit = false;
bool prompt_to_menu = false;
Vector2 touch_start_pos;
bool is_touch_dragging = false;

// Helpers de Tradução
const char* T(const char* pt, const char* en) { return lang_en ? en : pt; }

// ----------------------------------------------------------------------------
// 6. RENDERIZAÇÃO DE TEXTO (FONTE COMUM)
// ----------------------------------------------------------------------------
float MeasureMyText(const char* text, float blockSize) {
    return (float)MeasureText(text, (int)(blockSize * 10.0f));
}

void DrawMyText(const char* text, float x, float y, float blockSize, Color c) {
    DrawText(text, (int)x, (int)y, (int)(blockSize * 10.0f), c);
}

// ----------------------------------------------------------------------------
// AUDIO SYSTEM
// ----------------------------------------------------------------------------
void init_audio_system() {
    InitAudioDevice();
    for (int i = 0; i < 15; i++) bgm[i] = LoadMusicStream(TextFormat("m%d.mp3", i + 1));
    sfx_move = LoadSound("move.mp3");
    sfx_rotate = LoadSound("rotate.mp3");
    sfx_clear1 = LoadSound("clear1.mp3");
    sfx_clear2 = LoadSound("clear2.mp3");
    sfx_clear3 = LoadSound("clear3.mp3");
    sfx_clear4 = LoadSound("clear4.mp3");
    sfx_attack = LoadSound("attack.mp3");
    sfx_impact = LoadSound("impact.mp3");
    current_music = GetRandomValue(0, 14);
    if (music_on) PlayMusicStream(bgm[current_music]);
}

void update_audio_system() {
    if (!music_on) return;
    UpdateMusicStream(bgm[current_music]);
    if (GetMusicTimePlayed(bgm[current_music]) >= GetMusicTimeLength(bgm[current_music]) - 0.1f) {
        StopMusicStream(bgm[current_music]);
        current_music = (current_music + 1) % 15;
        PlayMusicStream(bgm[current_music]);
    }
}

void unload_audio_system() {
    for (int i = 0; i < 15; i++) UnloadMusicStream(bgm[i]);
    UnloadSound(sfx_move); UnloadSound(sfx_rotate);
    UnloadSound(sfx_clear1); UnloadSound(sfx_clear2);
    UnloadSound(sfx_clear3); UnloadSound(sfx_clear4);
    UnloadSound(sfx_attack); UnloadSound(sfx_impact);
    CloseAudioDevice();
}

// ----------------------------------------------------------------------------
// 7. INICIALIZAÇÃO
// ----------------------------------------------------------------------------
void init_game() {
    InitWindow(540, 960, "TETRABETTA - GOD TIER");
    SetExitKey(KEY_NULL); // Para não fechar no ESC direto
    SetTargetFPS(FPS_TARGET);
    srand((unsigned int)time(NULL));

    init_audio_system();

    board_x_offset = (GetScreenWidth() - (BOARD_W * CELL_SIZE)) / 2 - 50; 
    board_y_offset = (GetScreenHeight() - (VISIBLE_H * CELL_SIZE)) / 2 + 30;

    for(int i=0; i<MAX_PARTICLES; i++) particles[i].active = false;
    for(int i=0; i<MAX_TEXTS; i++) floating_texts[i].active = false;
    for(int i=0; i<MAX_SHOCKWAVES; i++) shockwaves[i].active = false;
    for(int i=0; i<MAX_BEAMS; i++) beams[i].active = false;
    
    for(int i=0; i<MAX_RAIN; i++) {
        rain[i].x = (float)GetRandomValue(0, GetScreenWidth());
        rain[i].y = (float)GetRandomValue(-1000, GetScreenHeight());
        rain[i].speed = (float)GetRandomValue(100, 400);
        rain[i].length = GetRandomValue(20, 80);
        rain[i].color = (GetRandomValue(0,10)>8) ? COLOR_CYAN : (Color){0, 150, 100, 100};
    }

    current_state = STATE_MENU;
}

void reset_session(GameState mode) {
    for (int y = 0; y < BOARD_H; y++) for (int x = 0; x < BOARD_W; x++) board[y][x] = 0;
    score = 0; level = 1; lines_cleared = 0; energy = 0; combo = 0;
    hold_piece_type = PIECE_NONE;
    player_lives = 3;
    is_boss_mode = (mode == STATE_PLAYING_BOSS);
    if (is_boss_mode) { 
        boss_level = 1; boss_max_hp = 5000.0f; boss_hp = boss_max_hp; 
        boss_attack_timer = 20.0f; boss_respawn_timer = 0.0f; boss_attack_laser_anim = 0.0f; 
    }
    set_next_piece(); spawn_piece();
    current_state = mode;
}

Color get_piece_color(PieceType type) {
    switch(type) {
        case PIECE_I: return COLOR_CYAN;   case PIECE_O: return COLOR_GOLD;
        case PIECE_T: return COLOR_MAGENTA;case PIECE_S: return COLOR_GREEN;
        case PIECE_Z: return COLOR_RED;    case PIECE_J: return COLOR_BLUE;
        case PIECE_L: return COLOR_ORANGE; default: return BLANK;
    }
}
int get_fall_speed() {
    int speed = 800 - ((level - 1) * 70); return speed < 100 ? 100 : speed;
}

// ----------------------------------------------------------------------------
// 8. SISTEMA DE EFEITOS ESPECIAIS (VFX ENGINE)
// ----------------------------------------------------------------------------
void spawn_particles(int x, int y, Color c, int count, float speedMult) {
    for (int i = 0; i < get_particle_limit() && count > 0; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].pos = (Vector2){ (float)x, (float)y };
            particles[i].prev_pos = particles[i].pos;
            float angle = (float)(GetRandomValue(0, 360)) * DEG2RAD;
            float speed = (float)GetRandomValue(50, 400) * speedMult;
            particles[i].vel = (Vector2){ cosf(angle)*speed, sinf(angle)*speed };
            particles[i].life = (float)GetRandomValue(30, 80) / 100.0f;
            particles[i].maxLife = particles[i].life;
            particles[i].color = c;
            particles[i].size = (float)GetRandomValue(2, 6);
            count--;
        }
    }
}
void spawn_floating_text(int x, int y, const char* text, Color c) {
    for (int i = 0; i < MAX_TEXTS; i++) {
        if (!floating_texts[i].active) {
            floating_texts[i].active = true;
            floating_texts[i].pos = (Vector2){ (float)x, (float)y };
            strcpy(floating_texts[i].text, text);
            floating_texts[i].color = c;
            floating_texts[i].life = 1.2f;
            floating_texts[i].maxLife = 1.2f;
            break;
        }
    }
}
void spawn_shockwave(int x, int y, float max_r, float thick, Color c) {
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (!shockwaves[i].active) {
            shockwaves[i].active = true;
            shockwaves[i].pos = (Vector2){(float)x, (float)y};
            shockwaves[i].radius = 1.0f;
            shockwaves[i].max_radius = max_r;
            shockwaves[i].thickness = thick;
            shockwaves[i].color = c;
            break;
        }
    }
}
void spawn_beam(int x, int y, int w, int h, Color c, float life, bool horizontal) {
    for (int i = 0; i < MAX_BEAMS; i++) {
        if (!beams[i].active) {
            beams[i].active = true;
            beams[i].rect = (Rectangle){(float)x, (float)y, (float)w, (float)h};
            beams[i].color = c;
            beams[i].life = life; beams[i].maxLife = life; beams[i].is_horizontal = horizontal;
            break;
        }
    }
}

// ----------------------------------------------------------------------------
// 9. MECÂNICAS CENTRAIS E LÓGICA DO JOGO
// ----------------------------------------------------------------------------
void set_next_piece() { next_piece_type = (PieceType)(GetRandomValue(1, 7)); }
void spawn_piece() {
    current_piece.type = next_piece_type; current_piece.rotation = 0; current_piece.x = 3; current_piece.y = 0; 
    current_piece.color = get_piece_color(current_piece.type); can_hold = true; fall_timer = 0; lock_timer = 0; soft_drop_timer = 0;
    set_next_piece();
    if (check_collision(current_piece)) {
        player_lives--;
        if (player_lives > 0) {
            for (int y = 0; y < BOARD_H; y++) for (int x = 0; x < BOARD_W; x++) board[y][x] = 0;
            spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + 200, "SYSTEM RESTORED!", COLOR_GREEN);
        } else {
            current_state = STATE_GAME_OVER;
        }
    }
}
bool check_collision(GamePiece p) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (PIECE_DATA[p.type][p.rotation][r][c]) {
                int bx = p.x + c, by = p.y + r;
                if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
                if (by >= 0 && board[by][bx] != 0) return true;
            }
        }
    }
    return false;
}
void move_piece(int dx, int dy) {
    GamePiece temp = current_piece; temp.x += dx; temp.y += dy;
    if (!check_collision(temp)) { 
        current_piece = temp; 
        if (dy > 0) score += 1; 
        else if (sfx_on) PlaySound(sfx_move);
    }
}

// Wallkick implementado
void rotate_current_piece() {
    GamePiece temp = current_piece; temp.rotation = (temp.rotation + 1) % 4;
    if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    
    // Tenta Kick para Direita
    temp.x += 1; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    // Tenta Kick para Esquerda (x-2 pois já somou +1 antes)
    temp.x -= 2; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    // Kick Extremo Direita (Peça I)
    temp.x += 3; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    // Kick Extremo Esquerda (Peça I)
    temp.x -= 4; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    
    // Reseta X, Tenta Subir (Kick de Chão)
    temp.x = current_piece.x;
    temp.y -= 1; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    
    // Diagonal Direita/Subindo
    temp.x += 1; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
    // Diagonal Esquerda/Subindo
    temp.x -= 2; if (!check_collision(temp)) { current_piece = temp; if(sfx_on) PlaySound(sfx_rotate); return; }
}

void hard_drop() {
    int drop_dist = 0, start_y = current_piece.y;
    while (!check_collision(current_piece)) { current_piece.y++; drop_dist++; }
    current_piece.y--; 
    
    int min_x = 4, max_x = -1;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (PIECE_DATA[current_piece.type][current_piece.rotation][r][c]) {
                if (c < min_x) min_x = c;
                if (c > max_x) max_x = c;
            }
        }
    }
    int piece_px_w = (max_x - min_x + 1) * CELL_SIZE;
    int px = board_x_offset + (current_piece.x + min_x) * CELL_SIZE;

    int cx = board_x_offset + current_piece.x * CELL_SIZE + (CELL_SIZE*2);
    int cy = board_y_offset + (current_piece.y - (BOARD_H-VISIBLE_H)) * CELL_SIZE + CELL_SIZE;
    int sy = board_y_offset + (start_y - (BOARD_H-VISIBLE_H)) * CELL_SIZE;
    
    if(graphics_level > 0) {
        spawn_beam(px, sy, piece_px_w, cy - sy, current_piece.color, 0.15f, false);
        spawn_shockwave(cx, cy, 150.0f, 10.0f, current_piece.color);
    }
    spawn_particles(cx, cy, current_piece.color, 40, 1.5f);
    screen_shake = 0.3f; score += (drop_dist * 2); lock_current_piece();
}
void hold_piece() {
    if (!can_hold) return;
    if (hold_piece_type == PIECE_NONE) { hold_piece_type = current_piece.type; spawn_piece(); } 
    else {
        PieceType temp = current_piece.type; current_piece.type = hold_piece_type; current_piece.rotation = 0; current_piece.x = 3; current_piece.y = 0; 
        current_piece.color = get_piece_color(current_piece.type); hold_piece_type = temp;
    }
    can_hold = false;
    if(graphics_level > 0) spawn_shockwave(board_x_offset - 20, board_y_offset + 150, 80.0f, 5.0f, COLOR_MAGENTA);
}
void lock_current_piece() {
    if (sfx_on) PlaySound(sfx_impact);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (PIECE_DATA[current_piece.type][current_piece.rotation][r][c]) {
                int by = current_piece.y + r, bx = current_piece.x + c;
                if (by >= 0 && by < BOARD_H) {
                    board[by][bx] = current_piece.type;
                    int px = board_x_offset + bx * CELL_SIZE + CELL_SIZE/2;
                    int py = board_y_offset + (by - (BOARD_H-VISIBLE_H)) * CELL_SIZE + CELL_SIZE/2;
                    if (by >= (BOARD_H-VISIBLE_H)) spawn_particles(px, py, WHITE, 8, 1.0f);
                }
            }
        }
    }
    num_lines_to_clear = 0;
    for (int y = 0; y < BOARD_H; y++) {
        bool isFull = true;
        for (int x = 0; x < BOARD_W; x++) if (board[y][x] == 0) { isFull = false; break; }
        if (isFull) lines_to_clear[num_lines_to_clear++] = y;
    }
    if (num_lines_to_clear > 0) {
        current_state = STATE_LINE_CLEAR; state_timer = 0.5f; screen_shake = 0.3f + (num_lines_to_clear * 0.15f); 
        for (int i = 0; i < num_lines_to_clear; i++) {
            int line_y = lines_to_clear[i];
            int py = board_y_offset + (line_y - (BOARD_H-VISIBLE_H)) * CELL_SIZE;
            if(graphics_level > 0) spawn_beam(board_x_offset, py, BOARD_W*CELL_SIZE, CELL_SIZE, COLOR_CYAN, 0.5f, true);
        }
        
        if(sfx_on) {
            if(num_lines_to_clear == 1) PlaySound(sfx_clear1);
            else if(num_lines_to_clear == 2) PlaySound(sfx_clear2);
            else if(num_lines_to_clear == 3) PlaySound(sfx_clear3);
            else PlaySound(sfx_clear4);
        }

    } else { combo = 0; spawn_piece(); }
}
void process_lines() {
    for (int i = 0; i < num_lines_to_clear; i++) {
        int line_y = lines_to_clear[i];
        int py = board_y_offset + (line_y - (BOARD_H-VISIBLE_H)) * CELL_SIZE + CELL_SIZE/2;
        if(graphics_level > 1) spawn_shockwave(board_x_offset + BOARD_W*CELL_SIZE/2, py, 300.0f, 15.0f, COLOR_CYAN);
        spawn_particles(board_x_offset + BOARD_W*CELL_SIZE/2, py, COLOR_CYAN, 60, 3.0f);
        for (int y = line_y; y > 0; y--) for (int x = 0; x < BOARD_W; x++) board[y][x] = board[y-1][x];
    }
    int base_pts = 0;
    if (num_lines_to_clear == 1) base_pts = 100;
    else if (num_lines_to_clear == 2) { base_pts = 300; spawn_floating_text(board_x_offset+BOARD_W*CELL_SIZE/2, board_y_offset+200, "DOUBLE IMPACT!", COLOR_GREEN); }
    else if (num_lines_to_clear == 3) { base_pts = 500; spawn_floating_text(board_x_offset+BOARD_W*CELL_SIZE/2, board_y_offset+200, "TRIPLE BREACH!", COLOR_ORANGE); }
    else if (num_lines_to_clear == 4) { base_pts = 800; spawn_floating_text(board_x_offset+BOARD_W*CELL_SIZE/2, board_y_offset+200, "TETRABETTA!!", COLOR_CYAN); }
    
    combo++; score += (base_pts * level) + (50 * combo);
    if(combo > 1) spawn_floating_text(board_x_offset-30, board_y_offset+250, TextFormat("COMBO X%d", combo), COLOR_MAGENTA);
    lines_cleared += num_lines_to_clear; level = (lines_cleared / 10) + 1;
    energy += (20 * num_lines_to_clear) + (10 * (combo-1)); if (energy > 100) energy = 100;
    
    if (is_boss_mode && boss_respawn_timer <= 0.0f) {
        float dmg = (float)(base_pts * combo * 2); boss_hp -= dmg;
        spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, 120, TextFormat("-%d DMG", (int)dmg), COLOR_RED);
        if (boss_hp <= 0) {
            boss_respawn_timer = 90.0f;
            boss_level++;
            boss_max_hp += 2500.0f;
            spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + 150, "BOSS OFFLINE - 1:30", COLOR_GREEN);
        }
    }
    num_lines_to_clear = 0; spawn_piece();
}
void activate_pulse() {
    if (energy < 100) return;
    energy = 0; pulse_anim_timer = 1.0f; score += 1000; screen_shake = 0.8f;
    if(graphics_level > 0) spawn_beam(board_x_offset, board_y_offset, BOARD_W*CELL_SIZE, VISIBLE_H*CELL_SIZE, COLOR_MAGENTA, 1.0f, false);
    
    if (is_boss_mode && boss_respawn_timer <= 0.0f) {
        boss_hp -= 1500; boss_laser_anim = 1.5f;
        spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, 80, "ORBITAL STRIKE!", COLOR_RED);
        if (boss_hp <= 0) {
            boss_respawn_timer = 90.0f;
            boss_level++;
            boss_max_hp += 2500.0f;
            spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + 150, "BOSS OFFLINE - 1:30", COLOR_GREEN);
        }
    }
    int cleared = 0;
    for (int y = BOARD_H - 1; y >= 0 && cleared < 4; y--) {
        bool hasBlock = false;
        for (int x = 0; x < BOARD_W; x++) if (board[y][x] != 0) { hasBlock = true; break; }
        if (hasBlock) {
            for (int k = y; k > 0; k--) for (int x = 0; x < BOARD_W; x++) board[k][x] = board[k-1][x];
            cleared++; y++; 
            int py = board_y_offset + (y - (BOARD_H-VISIBLE_H)) * CELL_SIZE + CELL_SIZE/2;
            if(graphics_level > 1) spawn_shockwave(board_x_offset + BOARD_W*CELL_SIZE/2, py, 400.0f, 20.0f, COLOR_MAGENTA);
            spawn_particles(board_x_offset + BOARD_W*CELL_SIZE/2, py, COLOR_MAGENTA, 60, 3.5f);
        }
    }
}

// ----------------------------------------------------------------------------
// 10. UPDATES E INPUT
// ----------------------------------------------------------------------------
void handle_input() {
    Vector2 mPos = GetMousePosition();

    // Atalho Fullscreen
    if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
        ToggleFullscreen();
    }

    // Prompts Ativos (Bloqueiam os outros inputs)
    if (prompt_app_exit || prompt_to_menu) {
        if (IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_S)) {
            if (prompt_app_exit) CloseWindow();
            else { prompt_to_menu = false; current_state = STATE_MENU; }
        } else if (IsKeyPressed(KEY_N)) {
            prompt_app_exit = false; prompt_to_menu = false;
        }
        
        // Touch para Sim/Não
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            int cx = GetScreenWidth()/2;
            int cy = GetScreenHeight() - 150;
            Rectangle btn_yes = { (float)cx - 100, (float)cy + 20, 80, 40 };
            Rectangle btn_no = { (float)cx + 20, (float)cy + 20, 80, 40 };

            if (CheckCollisionPointRec(mPos, btn_yes)) {
                if (prompt_app_exit) CloseWindow();
                else { prompt_to_menu = false; current_state = STATE_MENU; }
            } else if (CheckCollisionPointRec(mPos, btn_no)) {
                prompt_app_exit = false; prompt_to_menu = false;
            }
        }
        return;
    }

    // Sistema Global de ESC
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (current_state == STATE_MENU) prompt_app_exit = true;
        else if (current_state == STATE_PLAYING_ARCADE || current_state == STATE_PLAYING_BOSS) prompt_to_menu = true;
        else if (current_state == STATE_SETTINGS) current_state = STATE_MENU;
        return;
    }

    // MENU PRINCIPAL
    if (current_state == STATE_MENU) {
        if (IsKeyPressed(KEY_UP)) menu_selection = (menu_selection - 1 < 0) ? 3 : menu_selection - 1;
        if (IsKeyPressed(KEY_DOWN)) menu_selection = (menu_selection + 1) % 4;
        
        const char* menu_opts[4] = { T("MODO ARCADE", "ARCADE RUN"), T("MODO CHEFAO", "BOSS ENGAGE"), T("CONFIGURACOES", "SETTINGS"), T("DESLIGAR", "SHUTDOWN") };
        int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 3;
        for (int i = 0; i < 4; i++) {
            float w = MeasureMyText(menu_opts[i], 3.0f);
            Rectangle btn = { cx - w/2 - 20, (float)cy + 120 + (i*50), w + 40, 40 };
            if (CheckCollisionPointRec(mPos, btn)) {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) menu_selection = i;
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) goto EXEC_MENU;
            }
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        EXEC_MENU:
            if (menu_selection == 0) reset_session(STATE_PLAYING_ARCADE);
            else if (menu_selection == 1) reset_session(STATE_PLAYING_BOSS);
            else if (menu_selection == 2) current_state = STATE_SETTINGS;
            else prompt_app_exit = true;
        }
        return;
    }

    // SETTINGS
    if (current_state == STATE_SETTINGS) {
        if (IsKeyPressed(KEY_UP)) settings_selection = (settings_selection - 1 < 0) ? 4 : settings_selection - 1;
        if (IsKeyPressed(KEY_DOWN)) settings_selection = (settings_selection + 1) % 5;
        
        int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 4;
        for (int i = 0; i < 5; i++) {
            Rectangle btn = { (float)cx - 150, (float)cy + 90 + (i*60), 300, 50 };
            if (CheckCollisionPointRec(mPos, btn)) {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) settings_selection = i;
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) goto EXEC_SETTING;
            }
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        EXEC_SETTING:
            if (settings_selection == 0) {
                music_on = !music_on;
                if (music_on) PlayMusicStream(bgm[current_music]); else PauseMusicStream(bgm[current_music]);
            }
            else if (settings_selection == 1) sfx_on = !sfx_on;
            else if (settings_selection == 2) graphics_level = (graphics_level + 1) % 4;
            else if (settings_selection == 3) lang_en = !lang_en;
            else if (settings_selection == 4) current_state = STATE_MENU;
        }
        return;
    }

    // GAMEPLAY CONTROLS
    if (current_state != STATE_PLAYING_ARCADE && current_state != STATE_PLAYING_BOSS) return;

    if (IsKeyPressed(KEY_LEFT)) { move_piece(-1, 0); move_timer = 0; }
    if (IsKeyPressed(KEY_RIGHT)) { move_piece(1, 0); move_timer = 0; }
    if (IsKeyDown(KEY_LEFT)) { move_timer += GetFrameTime(); if (move_timer > 0.15f) { move_piece(-1, 0); move_timer -= 0.08f; } }
    if (IsKeyDown(KEY_RIGHT)) { move_timer += GetFrameTime(); if (move_timer > 0.15f) { move_piece(1, 0); move_timer -= 0.08f; } }

    if (IsKeyPressed(KEY_UP)) rotate_current_piece();
    if (IsKeyPressed(KEY_DOWN)) { move_piece(0, 1); soft_drop_timer = 0; } 
    else if (IsKeyDown(KEY_DOWN)) {
        soft_drop_timer += GetFrameTime();
        if (soft_drop_timer > 0.04f) { move_piece(0, 1); soft_drop_timer -= 0.04f; }
    } else soft_drop_timer = 0;

    if (IsKeyPressed(KEY_SPACE)) hard_drop();
    if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) hold_piece();
    if (IsKeyPressed(KEY_C)) activate_pulse();

    // Sistema Touch Avançado (Swipes e HUD)
    bool touch_in_hud = false;
    int hud_x = board_x_offset + BOARD_W * CELL_SIZE + 20;
    int hud_y = board_y_offset;
    Rectangle holdRect = { (float)hud_x, (float)hud_y + 120, 100, 100 };
    Rectangle pulseRect = { (float)hud_x, (float)hud_y + 510, 100, 50 };
    Rectangle menuRect = { (float)hud_x, (float)hud_y + 570, 100, 50 };

    if (CheckCollisionPointRec(mPos, holdRect) || CheckCollisionPointRec(mPos, pulseRect) || CheckCollisionPointRec(mPos, menuRect)) {
        touch_in_hud = true;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        touch_start_pos = mPos;
        is_touch_dragging = false;

        // Botoes do HUD prioridade
        if (CheckCollisionPointRec(mPos, holdRect)) { hold_piece(); }
        else if (CheckCollisionPointRec(mPos, pulseRect)) { activate_pulse(); }
        else if (CheckCollisionPointRec(mPos, menuRect)) { prompt_to_menu = true; }
    }
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !touch_in_hud) {
        float dx = mPos.x - touch_start_pos.x;
        float dy = mPos.y - touch_start_pos.y;

        // Limites refinados para evitar acionar rotação por acidente se houver arrasto
        if (dy > 180 && dy > fabs(dx)) { // Swipe MUITO longo pra baixo = Hard Drop
            hard_drop();
            touch_start_pos = mPos; // Reseta o local base pro dedo
            is_touch_dragging = true;
        } else if (dy > 40 && dy > fabs(dx)) { // Soft drop (puxando a peça devagar para baixo)
            move_piece(0, 1);
            touch_start_pos.y += 40; 
            is_touch_dragging = true; 
        } else if (dx > 40) { // Swipe Direita gradual
            move_piece(1, 0);
            touch_start_pos.x += 40; 
            is_touch_dragging = true;
        } else if (dx < -40) { // Swipe Esquerda gradual
            move_piece(-1, 0);
            touch_start_pos.x -= 40; 
            is_touch_dragging = true;
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (!is_touch_dragging && !touch_in_hud) { // Foi apenas um tap rápido na tela fora do menu
            rotate_current_piece();
        }
    }
}

void update_vfx(float dt) {
    for(int i=0; i<get_rain_limit(); i++) {
        rain[i].y += rain[i].speed * dt;
        if(rain[i].y > GetScreenHeight() + 100) { rain[i].y = -100; rain[i].x = (float)GetRandomValue(0, GetScreenWidth()); }
    }
    for (int i = 0; i < get_particle_limit(); i++) {
        if (particles[i].active) {
            particles[i].prev_pos = particles[i].pos; particles[i].pos.x += particles[i].vel.x * dt;
            particles[i].pos.y += particles[i].vel.y * dt; particles[i].vel.y += 500.0f * dt; particles[i].vel.x *= 0.95f;
            particles[i].life -= dt; particles[i].size = (particles[i].life / particles[i].maxLife) * 6.0f;
            if (particles[i].life <= 0) particles[i].active = false;
        }
    }
    for (int i = 0; i < MAX_TEXTS; i++) {
        if (floating_texts[i].active) {
            floating_texts[i].pos.y -= 40.0f * dt; floating_texts[i].life -= dt;
            if (floating_texts[i].life <= 0) floating_texts[i].active = false;
        }
    }
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            shockwaves[i].radius += (shockwaves[i].max_radius - shockwaves[i].radius) * 5.0f * dt; shockwaves[i].thickness *= 0.9f; 
            if (shockwaves[i].radius >= shockwaves[i].max_radius - 2.0f) shockwaves[i].active = false;
        }
    }
    for (int i=0; i < MAX_BEAMS; i++) {
        if (beams[i].active) {
            beams[i].life -= dt;
            if(beams[i].is_horizontal) beams[i].rect.height *= 0.85f; else beams[i].rect.width *= 0.85f; 
            if (beams[i].life <= 0) beams[i].active = false;
        }
    }
}

void update_boss(float dt) {
    if (!is_boss_mode) return;

    if (boss_respawn_timer > 0.0f) {
        boss_respawn_timer -= dt;
        if (boss_respawn_timer <= 0.0f) {
            boss_hp = boss_max_hp;
            boss_attack_timer = (15.0f - (boss_level * 1.0f) < 5.0f) ? 5.0f : 15.0f - (boss_level * 1.0f);
            spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + 100, "WARNING: BOSS REVIVED", COLOR_RED);
        }
        return; 
    }

    if (boss_laser_anim > 0) boss_laser_anim -= dt;
    if (boss_attack_laser_anim > 0) boss_attack_laser_anim -= dt;
    
    boss_attack_timer -= dt;
    if (boss_attack_timer <= 0) {
        boss_attack_timer = (15.0f - (boss_level * 1.0f) < 5.0f) ? 5.0f : 15.0f - (boss_level * 1.0f);
        screen_shake = 0.5f;
        boss_attack_laser_anim = 1.0f; 
        if(sfx_on) PlaySound(sfx_attack);
        spawn_floating_text(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + 100, "CORRUPTION OVERRIDE!", COLOR_RED);
        if(graphics_level > 1) spawn_shockwave(board_x_offset + BOARD_W*CELL_SIZE/2, board_y_offset + VISIBLE_H*CELL_SIZE, 500.0f, 30.0f, COLOR_RED);
        
        for (int y = 0; y < BOARD_H - 1; y++) for (int x = 0; x < BOARD_W; x++) board[y][x] = board[y+1][x];
        int hole = GetRandomValue(0, BOARD_W - 1);
        for (int x = 0; x < BOARD_W; x++) board[BOARD_H-1][x] = (x == hole) ? 0 : GetRandomValue(1, 7);
    }
}

void update_gameplay(float dt) {
    if (pulse_anim_timer > 0) pulse_anim_timer -= dt;
    if (prompt_to_menu || prompt_app_exit) return; // Pausa game logic se prompt ativo

    if (current_state == STATE_PLAYING_ARCADE || current_state == STATE_PLAYING_BOSS) {
        update_boss(dt);
        fall_timer += dt * 1000.0f;
        int limit = get_fall_speed();
        if (fall_timer >= limit) {
            fall_timer = 0; GamePiece temp = current_piece; temp.y += 1;
            if (check_collision(temp)) {
                lock_timer += limit; if (lock_timer > 500) lock_current_piece();
            } else { current_piece = temp; lock_timer = 0; }
        }
    } 
    else if (current_state == STATE_LINE_CLEAR) {
        state_timer -= dt;
        if (state_timer <= 0) { process_lines(); current_state = is_boss_mode ? STATE_PLAYING_BOSS : STATE_PLAYING_ARCADE; }
    }
}

// ----------------------------------------------------------------------------
// 11. RENDERIZAÇÃO (GOD TIER 2D NEON)
// ----------------------------------------------------------------------------
void draw_cyber_background() {
    if(graphics_level == 0) return;
    BeginBlendMode(BLEND_ADDITIVE);
    for(int i=0; i<get_rain_limit(); i++) {
        DrawRectangle((int)rain[i].x, (int)rain[i].y, 2, rain[i].length, rain[i].color);
        DrawRectangle((int)rain[i].x - 1, (int)(rain[i].y + rain[i].length), 4, 4, WHITE);
    }
    float time = GetTime(); int cx = GetScreenWidth()/2;
    for(int i=-15; i<=15; i++) DrawLine(cx + (i*10), GetScreenHeight()/2, cx + (i*60), GetScreenHeight(), COLOR_GRID);
    for(int i=0; i<15; i++) {
        float y = GetScreenHeight()/2 + fmodf((float)(time*40 + i*i*3), (float)(GetScreenHeight()/2));
        Color c = COLOR_GRID; c.a = (unsigned char)(200 * (y - GetScreenHeight()/2) / (GetScreenHeight()/2));
        DrawLine(0, (int)y, GetScreenWidth(), (int)y, c);
    }
    EndBlendMode();
}

void draw_block_neon_2d(int x, int y, int size, Color c, bool isGhost, int alphaMult) {
    if (isGhost) {
        Color ghostFill = {255, 255, 255, 50};
        Color ghostBorder = {255, 255, 255, 180};
        DrawRectangle(x, y, size, size, ghostFill);
        DrawRectangleLines(x, y, size, size, ghostBorder);
        return;
    }
    c.a = (unsigned char)(255 * alphaMult / 100);
    
    if(graphics_level > 0) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangle(x - 8, y - 8, size + 16, size + 16, (Color){c.r, c.g, c.b, 30});
        DrawRectangle(x - 4, y - 4, size + 8, size + 8, (Color){c.r, c.g, c.b, 80});
        DrawRectangle(x, y, size, size, c);
        EndBlendMode();
    } else { DrawRectangle(x, y, size, size, c); } 
    
    DrawRectangle(x + size/4, y + size/4, size/2, size/2, (Color){255,255,255,200});
    DrawRectangleLines(x+1, y+1, size-2, size-2, (Color){255,255,255,200});
}

void draw_board() {
    DrawRectangle(board_x_offset, board_y_offset, BOARD_W*CELL_SIZE, VISIBLE_H*CELL_SIZE, (Color){0, 0, 0, 220});
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i <= BOARD_W; i++) DrawLine(board_x_offset + i*CELL_SIZE, board_y_offset, board_x_offset + i*CELL_SIZE, board_y_offset + VISIBLE_H*CELL_SIZE, (Color){40,60,80,100});
    for (int i = 0; i <= VISIBLE_H; i++) DrawLine(board_x_offset, board_y_offset + i*CELL_SIZE, board_x_offset + BOARD_W*CELL_SIZE, board_y_offset + i*CELL_SIZE, (Color){40,60,80,100});
    DrawRectangleLinesEx((Rectangle){ (float)board_x_offset-3, (float)board_y_offset-3, BOARD_W*CELL_SIZE+6, VISIBLE_H*CELL_SIZE+6 }, 3, COLOR_CYAN);
    EndBlendMode();

    for (int y = BOARD_H - VISIBLE_H; y < BOARD_H; y++) {
        bool clearing = false;
        if (current_state == STATE_LINE_CLEAR) { for(int i=0; i<num_lines_to_clear; i++) if(lines_to_clear[i] == y) clearing = true; }
        for (int x = 0; x < BOARD_W; x++) {
            if (board[y][x] != 0) {
                int px = board_x_offset + x * CELL_SIZE, py = board_y_offset + (y - (BOARD_H - VISIBLE_H)) * CELL_SIZE;
                if (clearing) { BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(px, py, CELL_SIZE, CELL_SIZE, WHITE); EndBlendMode(); } 
                else { draw_block_neon_2d(px, py, CELL_SIZE, get_piece_color((PieceType)board[y][x]), false, 100); }
            }
        }
    }
}

void draw_piece(GamePiece p, bool isGhost) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (PIECE_DATA[p.type][p.rotation][r][c]) {
                int by = p.y + r, bx = p.x + c;
                if (by >= BOARD_H - VISIBLE_H) draw_block_neon_2d(board_x_offset + bx * CELL_SIZE, board_y_offset + (by - (BOARD_H - VISIBLE_H)) * CELL_SIZE, CELL_SIZE, p.color, isGhost, 100);
            }
        }
    }
}

void draw_vfx() {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i=0; i<MAX_BEAMS; i++) {
        if(beams[i].active) {
            Color c = beams[i].color; c.a = (unsigned char)(255 * (beams[i].life/beams[i].maxLife));
            if (beams[i].is_horizontal) {
                DrawRectangleRec((Rectangle){beams[i].rect.x, beams[i].rect.y, beams[i].rect.width, beams[i].rect.height}, c);
                DrawRectangleRec((Rectangle){beams[i].rect.x, beams[i].rect.y + beams[i].rect.height/4, beams[i].rect.width, beams[i].rect.height/2}, WHITE);
            } else {
                Color fadeTop = c; fadeTop.a = 0;
                DrawRectangleGradientV((int)beams[i].rect.x, (int)beams[i].rect.y, (int)beams[i].rect.width, (int)beams[i].rect.height, fadeTop, c);
            }
        }
    }
    for (int i=0; i<MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            Color c = shockwaves[i].color; c.a = (unsigned char)(255 * (1.0f - (shockwaves[i].radius / shockwaves[i].max_radius)));
            DrawCircleLines(shockwaves[i].pos.x, shockwaves[i].pos.y, shockwaves[i].radius, c);
            DrawCircleLines(shockwaves[i].pos.x, shockwaves[i].pos.y, shockwaves[i].radius - 1.0f, c);
            DrawCircleLines(shockwaves[i].pos.x, shockwaves[i].pos.y, shockwaves[i].radius + 1.0f, WHITE);
        }
    }
    for (int i = 0; i < get_particle_limit(); i++) {
        if (particles[i].active) {
            Color c = particles[i].color; c.a = (unsigned char)(255 * (particles[i].life / particles[i].maxLife));
            DrawLineEx(particles[i].prev_pos, particles[i].pos, particles[i].size, c); DrawCircleV(particles[i].pos, particles[i].size/2, WHITE);
        }
    }
    EndBlendMode();
    for (int i = 0; i < MAX_TEXTS; i++) {
        if (floating_texts[i].active) {
            Color c = floating_texts[i].color; c.a = (unsigned char)(255 * (floating_texts[i].life / floating_texts[i].maxLife));
            DrawMyText(floating_texts[i].text, floating_texts[i].pos.x - MeasureMyText(floating_texts[i].text, 1.5f)/2, floating_texts[i].pos.y, 1.5f, c);
        }
    }
}

void draw_hud() {
    int hud_x = board_x_offset + BOARD_W * CELL_SIZE + 20; int hud_y = board_y_offset;
    auto draw_panel = [](int x, int y, int w, int h, const char* title, Color color) {
        BeginBlendMode(BLEND_ADDITIVE); DrawRectangleLines(x, y, w, h, color); DrawRectangle(x, y, w, 20, color); EndBlendMode();
        DrawMyText(title, x + 5, y + 5, 1.5f, WHITE); 
    };
    int mini_sz = 16; 

    draw_panel(hud_x, hud_y, 100, 100, T("PROXIMA", "NEXT"), COLOR_CYAN);
    int nx = hud_x + 18, ny = hud_y + 35; if (next_piece_type == PIECE_I || next_piece_type == PIECE_O) nx += 8; 
    for (int r=0; r<4; r++) for (int c=0; c<4; c++) if (PIECE_DATA[next_piece_type][0][r][c]) draw_block_neon_2d(nx + c*mini_sz, ny + r*mini_sz, mini_sz, get_piece_color(next_piece_type), false, 100);

    draw_panel(hud_x, hud_y + 120, 100, 100, T("ESPERA", "HOLD"), COLOR_MAGENTA);
    if (hold_piece_type != PIECE_NONE) {
        int hx = hud_x + 18, hy = hud_y + 155; if (hold_piece_type == PIECE_I || hold_piece_type == PIECE_O) hx += 8;
        for (int r=0; r<4; r++) for (int c=0; c<4; c++) if (PIECE_DATA[hold_piece_type][0][r][c]) draw_block_neon_2d(hx + c*mini_sz, hy + r*mini_sz, mini_sz, get_piece_color(hold_piece_type), false, can_hold?100:30);
    }

    draw_panel(hud_x, hud_y + 240, 100, 200, "SYSTEM OP", COLOR_GOLD);
    DrawMyText(T("PONTOS", "SCORE"), hud_x + 5, hud_y + 265, 1.2f, LIGHTGRAY);
    DrawMyText(TextFormat("%06d", score), hud_x + 5, hud_y + 280, 2.0f, COLOR_GOLD);
    DrawMyText(T("NIVEL", "LEVEL"), hud_x + 5, hud_y + 315, 1.2f, LIGHTGRAY);
    DrawMyText(TextFormat("%02d", level), hud_x + 5, hud_y + 330, 2.0f, WHITE);
    DrawMyText(T("LINHAS", "LINES"), hud_x + 5, hud_y + 355, 1.2f, LIGHTGRAY);
    DrawMyText(TextFormat("%04d", lines_cleared), hud_x + 5, hud_y + 370, 2.0f, WHITE);
    DrawMyText(T("VIDAS", "LIVES"), hud_x + 5, hud_y + 395, 1.2f, LIGHTGRAY);
    DrawMyText(TextFormat("%d", player_lives), hud_x + 5, hud_y + 410, 2.0f, COLOR_RED);

    draw_panel(hud_x, hud_y + 450, 100, 50, T("ENERGIA", "ENERGY"), COLOR_GREEN);
    DrawRectangleLines(hud_x + 5, hud_y + 475, 90, 20, DARKGRAY);
    BeginBlendMode(BLEND_ADDITIVE);
    Color eCol = energy >= 100 ? COLOR_RED : COLOR_GREEN;
    DrawRectangle(hud_x + 5, hud_y + 475, (int)(90 * (energy / 100.0f)), 20, eCol);
    if(energy >= 100) DrawRectangle(hud_x + 5, hud_y + 475, 90, 20, (Color){255,255,255,(unsigned char)(fabs(sinf(GetTime()*10))*100)});
    EndBlendMode();
    
    draw_panel(hud_x, hud_y + 510, 100, 50, "PULSE", energy >= 100 ? COLOR_RED : DARKGRAY);
    if (energy >= 100) DrawMyText("READY", hud_x + 20, hud_y + 535, 1.5f, WHITE);

    draw_panel(hud_x, hud_y + 570, 100, 50, "MENU", COLOR_BLUE);
    DrawMyText("EXIT", hud_x + 25, hud_y + 595, 1.5f, WHITE);
}

void draw_boss() {
    if (!is_boss_mode) return;
    int cx = board_x_offset + (BOARD_W*CELL_SIZE)/2, cy = board_y_offset - 60;
    
    if (boss_respawn_timer > 0.0f) {
        DrawMyText(TextFormat("BOSS REBOOT: %02d", (int)boss_respawn_timer), cx - 60, cy - 20, 1.5f, COLOR_GREEN);
        return; 
    }

    if (boss_laser_anim > 0) {
        BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(board_x_offset, cy, BOARD_W*CELL_SIZE, GetScreenHeight(), (Color){255, 0, 0, (unsigned char)(200 * boss_laser_anim)}); EndBlendMode();
    }
    
    if (boss_attack_laser_anim > 0.0f) {
        BeginBlendMode(BLEND_ADDITIVE); 
        Color laserC = (Color){255, 0, 0, (unsigned char)(255 * boss_attack_laser_anim)};
        float thick = 15.0f + sinf(GetTime()*40)*10.0f;
        DrawRectangleRec({ (float)cx - thick/2, (float)cy, thick, (float)GetScreenHeight() }, laserC);
        EndBlendMode();
    }

    float t = GetTime();
    BeginBlendMode(BLEND_ADDITIVE);
    DrawPolyLines((Vector2){(float)cx, (float)cy}, 3, 50 + sinf(t*3)*10, t*50, COLOR_RED); DrawPolyLines((Vector2){(float)cx, (float)cy}, 6, 70, -t*30, COLOR_ORANGE);
    EndBlendMode();
    DrawCircle(cx, cy, 30 + sinf(t*5)*5, COLOR_RED); DrawCircle(cx, cy, 15, BLACK); DrawCircle(cx + 5, cy - 5, 5, WHITE);
    
    DrawRectangle(board_x_offset, cy - 40, BOARD_W*CELL_SIZE, 10, DARKGRAY);
    BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(board_x_offset, cy - 40, (int)((BOARD_W*CELL_SIZE) * max(0.0f, boss_hp / boss_max_hp)), 10, COLOR_RED); EndBlendMode();
    
    DrawMyText(TextFormat("CORE AI HP - LV %d", boss_level), board_x_offset, cy - 55, 1.5f, WHITE);
    DrawMyText(TextFormat("ATK IN %.1f", boss_attack_timer), cx + 40, cy - 20, 1.2f, COLOR_ORANGE);
}

void draw_menu() {
    draw_cyber_background();
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 3;

    float tW = MeasureMyText("TETRABETTA", 6.0f);
    DrawMyText("TETRABETTA", cx - tW/2, cy - 70, 6.0f, COLOR_CYAN);
    float sW = MeasureMyText("GOD TIER ENGINE", 2.0f);
    DrawMyText("GOD TIER ENGINE", cx - sW/2, cy + 30, 2.0f, COLOR_GOLD);

    const char* menu_opts[4] = { T("MODO ARCADE", "ARCADE RUN"), T("MODO CHEFAO", "BOSS ENGAGE"), T("CONFIGURACOES", "SETTINGS"), T("DESLIGAR", "SHUTDOWN") };

    for (int i = 0; i < 4; i++) {
        Color c = (i == menu_selection) ? COLOR_RED : DARKGRAY;
        float size = (i == menu_selection) ? 3.0f : 2.5f;
        
        float w = MeasureMyText(menu_opts[i], size);
        float by = cy + 120 + (i*50);
        
        BeginBlendMode(BLEND_ADDITIVE);
        if(i == menu_selection) DrawRectangle(cx - 150, by - 10, 300, 40, (Color){255,0,0,50});
        EndBlendMode();
        
        DrawMyText(menu_opts[i], cx - w/2, by, size, c);
        if (i == menu_selection) DrawMyText(">", cx - w/2 - 30, by, size, COLOR_RED);
    }
}

void draw_settings() {
    draw_cyber_background();
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 4;

    const char* title = T("CONFIGURACOES", "SETTINGS");
    float tW = MeasureMyText(title, 4.0f);
    DrawMyText(title, cx - tW/2, cy - 60, 4.0f, COLOR_CYAN);

    const char* gfx_str[] = {"LOW", "MED", "HIGH", "ULTRA"};

    for (int i = 0; i < 5; i++) {
        Color c = (i == settings_selection) ? COLOR_RED : DARKGRAY;
        float size = (i == settings_selection) ? 3.0f : 2.0f;
        
        string opt = "";
        if (i == 0) opt = string(T("MUSICA: ", "MUSIC: ")) + (music_on ? "ON" : "OFF");
        else if (i == 1) opt = string(T("EFEITOS: ", "SFX: ")) + (sfx_on ? "ON" : "OFF");
        else if (i == 2) opt = string(T("GRAFICOS: ", "GRAPHICS: ")) + gfx_str[graphics_level];
        else if (i == 3) opt = string(T("IDIOMA: PT-BR", "LANGUAGE: ENG"));
        else if (i == 4) opt = T("VOLTAR", "BACK");

        float w = MeasureMyText(opt.c_str(), size);
        float by = cy + 100 + (i*60);
        
        BeginBlendMode(BLEND_ADDITIVE);
        if(i == settings_selection) DrawRectangle(cx - 150, by - 10, 300, 40, (Color){255,0,0,50});
        EndBlendMode();
        
        DrawMyText(opt.c_str(), cx - w/2, by, size, c);
        if (i == settings_selection) DrawMyText(">", cx - w/2 - 30, by, size, COLOR_RED);
    }

    float cw = MeasureMyText("CREDITS: IGOR BETTARELLO XAVIER", 1.5f);
    DrawMyText("CREDITS: IGOR BETTARELLO XAVIER", cx - cw/2, GetScreenHeight() - 40, 1.5f, WHITE);
}

void draw_game_over_or_victory() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 220});
    int cx = GetScreenWidth()/2;
    if (current_state == STATE_GAME_OVER) {
        float w = MeasureMyText(T("FALHA NO SISTEMA", "SYSTEM FAILURE"), 3.0f);
        DrawMyText(T("FALHA NO SISTEMA", "SYSTEM FAILURE"), cx - w/2, 300, 3.0f, COLOR_RED);
    } else {
        float w1 = MeasureMyText(T("CORE AI DESTRUIDO", "CORE AI DESTROYED"), 2.0f);
        DrawMyText(T("CORE AI DESTRUIDO", "CORE AI DESTROYED"), cx - w1/2, 300, 2.0f, COLOR_GREEN);
        float w2 = MeasureMyText(T("VITORIA!", "VICTORY!"), 4.0f);
        DrawMyText(T("VITORIA!", "VICTORY!"), cx - w2/2, 350, 4.0f, COLOR_GOLD);
    }
    string sText = TextFormat(T("PONTOS: %d", "FINAL SCORE: %d"), score);
    float ws = MeasureMyText(sText.c_str(), 2.0f);
    DrawMyText(sText.c_str(), cx - ws/2, 420, 2.0f, WHITE);
    
    if ((int)(GetTime() * 2) % 2 == 0) {
        string retText = T("PRESSIONE ENTER", "PRESS ENTER");
        float wr = MeasureMyText(retText.c_str(), 1.5f);
        DrawMyText(retText.c_str(), cx - wr/2, 500, 1.5f, COLOR_CYAN);
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) current_state = STATE_MENU;
}

void draw_prompts() {
    if (!prompt_app_exit && !prompt_to_menu) return;
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,200});
    int cx = GetScreenWidth()/2;
    int cy = GetScreenHeight() - 150;

    DrawRectangleLines(cx - 200, cy - 80, 400, 160, COLOR_RED);
    DrawRectangle(cx - 200, cy - 80, 400, 160, (Color){30,0,0,255});

    const char* txt = prompt_app_exit ? T("DESEJA SAIR? (S/N)", "QUIT GAME? (Y/N)") : T("VOLTAR AO MENU? (S/N)", "RETURN TO MENU? (Y/N)");
    float w = MeasureMyText(txt, 2.0f);
    DrawMyText(txt, cx - w/2, cy - 40, 2.0f, WHITE);

    DrawRectangleLines(cx - 100, cy + 20, 80, 40, COLOR_GREEN);
    DrawMyText(T("SIM", "YES"), cx - 80, cy + 30, 1.5f, COLOR_GREEN);

    DrawRectangleLines(cx + 20, cy + 20, 80, 40, COLOR_RED);
    DrawMyText(T("NAO", "NO"), cx + 45, cy + 30, 1.5f, COLOR_RED);
}

void draw_crt_overlay() {
    for(int y = 0; y < GetScreenHeight(); y += 3) DrawLine(0, y, GetScreenWidth(), y, (Color){0, 0, 0, 80});
    DrawRectangleGradientV(0, 0, GetScreenWidth(), 100, (Color){0,0,0,200}, BLANK);
    DrawRectangleGradientV(0, GetScreenHeight()-100, GetScreenWidth(), 100, BLANK, (Color){0,0,0,200});
}

// ----------------------------------------------------------------------------
// 12. MAIN LOOP
// ----------------------------------------------------------------------------
int main() {
    init_game(); Camera2D camera = { 0 }; camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        update_audio_system();

        if (current_state == STATE_MENU || current_state == STATE_SETTINGS || current_state == STATE_PLAYING_ARCADE || current_state == STATE_PLAYING_BOSS || current_state == STATE_LINE_CLEAR) {
            handle_input();
        }
        if (current_state == STATE_PLAYING_ARCADE || current_state == STATE_PLAYING_BOSS || current_state == STATE_LINE_CLEAR) {
            update_gameplay(dt);
        }
        
        update_vfx(dt);

        if (screen_shake > 0) {
            camera.offset.x = (float)GetRandomValue(-5, 5) * screen_shake * 15.0f; camera.offset.y = (float)GetRandomValue(-5, 5) * screen_shake * 15.0f;
            screen_shake -= dt;
        } else { camera.offset.x = 0; camera.offset.y = 0; }

        BeginDrawing(); ClearBackground(COLOR_BG); BeginMode2D(camera);

        if (current_state == STATE_MENU) draw_menu();
        else if (current_state == STATE_SETTINGS) draw_settings();
        else {
            draw_cyber_background(); draw_board(); draw_boss();
            
            if (current_state == STATE_PLAYING_ARCADE || current_state == STATE_PLAYING_BOSS) {
                GamePiece ghost = current_piece;
                while (!check_collision(ghost)) ghost.y++;
                ghost.y--;
                draw_piece(ghost, true); // Ghost White
                draw_piece(current_piece, false); // Active Piece
            }

            draw_hud(); draw_vfx(); 
            
            if (pulse_anim_timer > 0) {
                BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(board_x_offset, board_y_offset, BOARD_W*CELL_SIZE, VISIBLE_H*CELL_SIZE, (Color){255,0,255, (unsigned char)(100 * pulse_anim_timer)}); EndBlendMode();
            }

            if (current_state == STATE_GAME_OVER || current_state == STATE_VICTORY) draw_game_over_or_victory();
        }

        EndMode2D(); draw_crt_overlay(); 
        
        draw_prompts(); // Prompts (Pause/Exit) renderizados por ultimo (HUD Superior)

        EndDrawing();
    }
    unload_audio_system();
    CloseWindow(); return 0;
}