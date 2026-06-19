/**
 * Pássaros Furiosos (Angry Birds Clone) em C++
 * Motor de Física e Virtual Resolution Scaling (Letterbox)
 */

#include "raylib.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

// ============================================================================
// 1. MOTOR MATEMÁTICO (VETORES 2D)
// ============================================================================
struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
    Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    Vec2& operator+=(const Vec2& v) { x += v.x; y += v.y; return *this; }
    
    float dot(const Vec2& v) const { return x * v.x + y * v.y; }
    float magSq() const { return x * x + y * y; }
    float mag() const { return std::sqrt(magSq()); }
    
    Vec2 normalize() const {
        float m = mag();
        return (m == 0) ? Vec2(0, 0) : Vec2(x / m, y / m);
    }
};

inline float Clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// ============================================================================
// 2. SISTEMA FÍSICO E ENTIDADES
// ============================================================================
enum ShapeType { SHAPE_CIRCLE, SHAPE_BOX };
// Renomeado para PhysMaterial para evitar colisão com o struct do Raylib no Android NDK
enum PhysMaterial { MAT_WOOD, MAT_STONE, MAT_GLASS, MAT_BIRD, MAT_PIG, MAT_GROUND };

struct Body {
    unsigned int id;
    Vec2 pos;
    Vec2 vel;
    Vec2 force;
    
    float mass;
    float invMass;
    float restitution; 
    float friction;    
    
    ShapeType shape;
    float width, height;
    float radius;        
    
    PhysMaterial material;
    float health;
    float maxHealth;
    bool isStatic;
    bool isDestroyed;
    bool isDamaged;
    Color color;

    Body(ShapeType s, float m, Vec2 p) : shape(s), mass(m), pos(p) {
        id = GetRandomValue(1, 1000000);
        vel = Vec2(0,0);
        force = Vec2(0,0);
        invMass = (m == 0.0f) ? 0.0f : 1.0f / m;
        isStatic = (m == 0.0f);
        isDestroyed = false;
        isDamaged = false;
        health = 100;
        maxHealth = 100;
        restitution = 0.2f;
        friction = 0.5f;
    }

    void ApplyForce(const Vec2& f) { force += f; }
    void ApplyImpulse(const Vec2& impulse) { vel += impulse * invMass; }
};

struct Manifold {
    std::shared_ptr<Body> A;
    std::shared_ptr<Body> B;
    float penetration;
    Vec2 normal;
    bool isColliding;
};

bool AABBvsAABB(Manifold& m) {
    Vec2 n = m.B->pos - m.A->pos;
    float a_ex = m.A->width / 2.0f;
    float a_ey = m.A->height / 2.0f;
    float b_ex = m.B->width / 2.0f;
    float b_ey = m.B->height / 2.0f;

    float x_overlap = a_ex + b_ex - std::abs(n.x);
    if (x_overlap > 0) {
        float y_overlap = a_ey + b_ey - std::abs(n.y);
        if (y_overlap > 0) {
            if (x_overlap < y_overlap) {
                m.normal = n.x < 0 ? Vec2(-1, 0) : Vec2(1, 0);
                m.penetration = x_overlap;
            } else {
                m.normal = n.y < 0 ? Vec2(0, -1) : Vec2(0, 1);
                m.penetration = y_overlap;
            }
            return true;
        }
    }
    return false;
}

bool CirclevsCircle(Manifold& m) {
    Vec2 n = m.B->pos - m.A->pos;
    float distSq = n.magSq();
    float radiusSum = m.A->radius + m.B->radius;

    if (distSq > radiusSum * radiusSum) return false;

    float dist = std::sqrt(distSq);
    if (dist != 0) {
        m.penetration = radiusSum - dist;
        m.normal = n / dist;
    } else {
        m.penetration = m.A->radius;
        m.normal = Vec2(1, 0);
    }
    return true;
}

bool CirclevsAABB(Manifold& m, bool flipped) {
    std::shared_ptr<Body> circle = flipped ? m.B : m.A;
    std::shared_ptr<Body> box = flipped ? m.A : m.B;

    Vec2 n = circle->pos - box->pos;
    Vec2 closest = n;
    
    float x_ext = box->width / 2.0f;
    float y_ext = box->height / 2.0f;

    closest.x = Clamp(closest.x, -x_ext, x_ext);
    closest.y = Clamp(closest.y, -y_ext, y_ext);

    bool inside = false;
    if (n.x == closest.x && n.y == closest.y) {
        inside = true;
        if (std::abs(n.x) > std::abs(n.y)) {
            closest.x = closest.x > 0 ? x_ext : -x_ext;
        } else {
            closest.y = closest.y > 0 ? y_ext : -y_ext;
        }
    }

    Vec2 normal = n - closest;
    float distSq = normal.magSq();
    float radius = circle->radius;

    if (!inside && distSq > radius * radius) return false;

    float dist = std::sqrt(distSq);
    m.normal = flipped ? (normal / -dist) : (normal / dist);
    m.penetration = radius - dist;

    if (inside) {
        m.normal = m.normal * -1.0f;
        m.penetration = radius + dist;
    }
    return true;
}

void ResolveCollision(Manifold& m) {
    Vec2 rv = m.B->vel - m.A->vel;
    float velAlongNormal = rv.dot(m.normal);

    if (velAlongNormal > 0) return;

    float e = std::min(m.A->restitution, m.B->restitution);
    float j = -(1.0f + e) * velAlongNormal;
    j /= m.A->invMass + m.B->invMass;

    Vec2 impulse = m.normal * j;
    m.A->ApplyImpulse(impulse * -1.0f);
    m.B->ApplyImpulse(impulse);

    float impactForce = j;
    // Limite aumentado de 40 para 180 para evitar que o peso da gravidade destrua a torre sozinha
    float damageThreshold = 180.0f;
    if (impactForce > damageThreshold) {
        float forceExceed = impactForce - damageThreshold;
        float dmgA = forceExceed * (m.A->material == MAT_STONE ? 0.2f : (m.A->material == MAT_GLASS ? 1.0f : 0.5f));
        float dmgB = forceExceed * (m.B->material == MAT_STONE ? 0.2f : (m.B->material == MAT_GLASS ? 1.0f : 0.5f));
        
        // O Pássaro é feito de "Borracha Mágica" e não recebe dano de contato
        if (!m.A->isStatic && m.A->material != MAT_BIRD) m.A->health -= dmgA;
        if (!m.B->isStatic && m.B->material != MAT_BIRD) m.B->health -= dmgB;
    }

    rv = m.B->vel - m.A->vel;
    Vec2 t = rv - (m.normal * rv.dot(m.normal));
    t = t.normalize();

    float jt = -rv.dot(t);
    jt /= m.A->invMass + m.B->invMass;

    float mu = std::min(m.A->friction, m.B->friction);
    Vec2 frictionImpulse;
    if (std::abs(jt) < j * mu) frictionImpulse = t * jt;
    else frictionImpulse = t * -j * mu;

    m.A->ApplyImpulse(frictionImpulse * -1.0f);
    m.B->ApplyImpulse(frictionImpulse);
}

void PositionalCorrection(Manifold& m) {
    const float percent = 0.2f; 
    const float slop = 0.01f;   
    Vec2 correction = m.normal * (std::max(m.penetration - slop, 0.0f) / (m.A->invMass + m.B->invMass)) * percent;
    if (!m.A->isStatic) m.A->pos = m.A->pos - correction * m.A->invMass;
    if (!m.B->isStatic) m.B->pos = m.B->pos + correction * m.B->invMass;
}

class PhysicsWorld {
public:
    std::vector<std::shared_ptr<Body>> bodies;
    Vec2 gravity = Vec2(0, 450.0f);

    void AddBody(std::shared_ptr<Body> body) { bodies.push_back(body); }

    void Step(float dt) {
        for (auto& b : bodies) {
            if (b->isStatic) continue;
            b->vel += (gravity + b->force * b->invMass) * dt;
            b->vel = b->vel * 0.99f; // Damping: Fricção global do ar para estabilizar as torres e impedir tremores
            b->pos += b->vel * dt;
            b->force = Vec2(0,0);
        }

        for (int i = 0; i < 6; i++) {
            for (size_t a = 0; a < bodies.size(); ++a) {
                for (size_t b = a + 1; b < bodies.size(); ++b) {
                    Body* A = bodies[a].get();
                    Body* B = bodies[b].get();

                    if (A->isStatic && B->isStatic) continue;

                    Manifold m;
                    m.A = bodies[a];
                    m.B = bodies[b];
                    m.isColliding = false;

                    if (A->shape == SHAPE_BOX && B->shape == SHAPE_BOX) m.isColliding = AABBvsAABB(m);
                    else if (A->shape == SHAPE_CIRCLE && B->shape == SHAPE_CIRCLE) m.isColliding = CirclevsCircle(m);
                    else if (A->shape == SHAPE_BOX && B->shape == SHAPE_CIRCLE) m.isColliding = CirclevsAABB(m, false);
                    else if (A->shape == SHAPE_CIRCLE && B->shape == SHAPE_BOX) m.isColliding = CirclevsAABB(m, true);

                    if (m.isColliding) {
                        ResolveCollision(m);
                        PositionalCorrection(m);
                    }
                }
            }
        }
    }
};

// ============================================================================
// 3. ESTRUTURA DO JOGO (COM RESOLUÇÃO VIRTUAL)
// ============================================================================
enum GameState { STATE_MENU, STATE_IDLE, STATE_DRAGGING, STATE_FLYING, STATE_WAITING, STATE_OVER, STATE_WIN };

struct Particle {
    Vec2 pos, vel;
    Color color;
    float life;
    float size;
};

class Game {
public:
    PhysicsWorld world;
    GameState state;
    
    int score = 0;
    int level = 1;
    int birdsLeft = 3;
    int pigsLeft = 0;
    
    std::shared_ptr<Body> currentBird;
    Vec2 slingAnchor;
    
    std::vector<Particle> particles;
    std::vector<Vec2> birdTrail;

    // Resolução virtual interna sempre fixa
    const int virtualW = 1280;
    const int virtualH = 720;

    Game() {
        state = STATE_MENU;
        slingAnchor = Vec2(virtualW * 0.15f, virtualH - 250);
    }

    void InitLevel(int lvl) {
        world.bodies.clear();
        particles.clear();
        birdTrail.clear();
        
        level = lvl;
        birdsLeft = std::min(3 + (level / 3), 5);
        pigsLeft = 0;

        // Chão Estático
        auto ground = std::make_shared<Body>(SHAPE_BOX, 0.0f, Vec2(virtualW / 2.0f, virtualH - 25.0f));
        ground->width = virtualW * 2.0f;
        ground->height = 50.0f;
        ground->material = MAT_GROUND;
        ground->color = Color{ 34, 139, 34, 255 }; 
        world.AddBody(ground);

        float startX = virtualW * 0.55f;
        float endX = virtualW * 0.9f;
        float baseY = virtualH - 50.0f;
        
        int numStructures = std::min(2 + (level / 2), 6);
        float spacing = (endX - startX) / numStructures;

        for (int i = 0; i < numStructures; i++) {
            float x = startX + (i * spacing) + GetRandomValue(-10, 10);
            BuildStructure(x, baseY, level);
        }

        SpawnBird();
        state = STATE_IDLE;
    }

    void SpawnBird() {
        if (birdsLeft <= 0) return;
        currentBird = std::make_shared<Body>(SHAPE_CIRCLE, 10.0f, slingAnchor);
        currentBird->radius = 18.0f;
        currentBird->material = MAT_BIRD;
        currentBird->color = RED;
        currentBird->restitution = 0.4f;
        world.AddBody(currentBird);
        state = STATE_IDLE;
    }

    void SpawnPig(float x, float y) {
        auto pig = std::make_shared<Body>(SHAPE_CIRCLE, 5.0f, Vec2(x, y));
        pig->radius = 20.0f;
        pig->material = MAT_PIG;
        pig->color = GREEN;
        pig->health = pig->maxHealth = 50;
        world.AddBody(pig);
        pigsLeft++;
    }

    void CreateBlock(float x, float y, float w, float h, PhysMaterial mat) {
        float mass = (mat == MAT_STONE) ? 30.0f : (mat == MAT_GLASS) ? 5.0f : 15.0f;
        auto block = std::make_shared<Body>(SHAPE_BOX, mass, Vec2(x, y));
        block->width = w; block->height = h;
        block->material = mat;
        
        if (mat == MAT_WOOD) { block->color = Color{ 210, 180, 140, 255 }; block->health = 100; }
        else if (mat == MAT_STONE) { block->color = GRAY; block->health = 250; }
        else { block->color = Color{ 173, 216, 230, 200 }; block->health = 40; }
        
        block->maxHealth = block->health;
        world.AddBody(block);
    }

    void BuildStructure(float x, float y, int lvl) {
        int type = GetRandomValue(0, 2);
        PhysMaterial mat = (GetRandomValue(0, lvl) > 2) ? MAT_STONE : MAT_WOOD;

        if (type == 0) { 
            // CORRIGIDO O BUG DA EXPLOSÃO INICIAL: Alarguei os pilares para não sobreporem o porco!
            CreateBlock(x - 35, y - 40, 20, 80, mat); 
            CreateBlock(x + 35, y - 40, 20, 80, mat); 
            CreateBlock(x, y - 90, 100, 20, mat);     
            SpawnPig(x, y - 20); 
        } else if (type == 1) { 
            float s = 40;
            CreateBlock(x - s, y - s/2, s, s, mat);
            CreateBlock(x, y - s/2, s, s, mat);
            CreateBlock(x + s, y - s/2, s, s, mat);
            CreateBlock(x - s/2, y - s*1.5f, s, s, mat);
            CreateBlock(x + s/2, y - s*1.5f, s, s, mat);
            CreateBlock(x, y - s*2.5f, s, s, mat);
            SpawnPig(x, y - s*3 - 20); // Assenta perfeitamente no topo
        } else { 
            CreateBlock(x - 30, y - 30, 20, 60, mat);
            CreateBlock(x + 30, y - 30, 20, 60, mat);
            CreateBlock(x, y - 70, 80, 20, mat);
            SpawnPig(x, y - 20);
        }
    }

    void SpawnParticles(Vec2 pos, Color c) {
        for (int i = 0; i < 8; i++) {
            Particle p;
            p.pos = pos;
            p.vel = Vec2(GetRandomValue(-100, 100)/10.0f, GetRandomValue(-200, 0)/10.0f);
            p.color = c;
            p.life = 1.0f;
            p.size = GetRandomValue(3, 8);
            particles.push_back(p);
        }
    }

    // Recebemos a posição virtual do rato (adaptada da tela de toque real)
    void Update(float dt, Vec2 vMouse, bool isTouched) {
        if (state == STATE_MENU || state == STATE_OVER || state == STATE_WIN) return;

        if (state == STATE_IDLE || state == STATE_DRAGGING) {
            if (isTouched) {
                // Aumentámos a Hitbox (de 40 para 70) para facilitar jogar com dedos gordos em telemóveis
                if (state == STATE_IDLE && currentBird && (vMouse - currentBird->pos).mag() < 70.0f) {
                    state = STATE_DRAGGING;
                }
                if (state == STATE_DRAGGING) {
                    Vec2 diff = vMouse - slingAnchor;
                    float dist = diff.mag();
                    if (dist > 120.0f) {
                        currentBird->pos = slingAnchor + diff.normalize() * 120.0f;
                    } else {
                        currentBird->pos = vMouse;
                    }
                    currentBird->vel = Vec2(0,0);
                }
            } else if (state == STATE_DRAGGING) {
            Vec2 diff = slingAnchor - currentBird->pos;
            if (diff.mag() > 20.0f) {
                currentBird->vel = diff * 5.0f; 
                state = STATE_FLYING;
                birdsLeft--;
            } else {
                currentBird->pos = slingAnchor;
                state = STATE_IDLE;
            }
        }
    }

    // Trava Posicional: Garante que o pássaro fique a levitar no estilingue enquanto espera
    if (state == STATE_IDLE && currentBird) {
        currentBird->pos = slingAnchor;
        currentBird->vel = Vec2(0,0);
    }

    world.Step(dt);

    if (state == STATE_FLYING && currentBird) {
            if (currentBird->vel.magSq() > 100.0f && GetRandomValue(0,100) > 60) {
                birdTrail.push_back(currentBird->pos);
                if (birdTrail.size() > 50) birdTrail.erase(birdTrail.begin());
            }

            bool stopped = currentBird->vel.magSq() < 10.0f;
            bool outOfBounds = currentBird->pos.x > virtualW + 100 || currentBird->pos.x < -100;

            if (stopped || outOfBounds) {
                state = STATE_WAITING;
            }
        }

        if (state == STATE_WAITING) {
            static float timer = 0;
            timer += dt;
            if (timer > 1.5f) {
                timer = 0;
                if (currentBird) currentBird->isDestroyed = true;
                currentBird = nullptr;

                if (pigsLeft > 0) {
                    if (birdsLeft > 0) SpawnBird();
                    else state = STATE_OVER;
                }
            }
        }

        for (auto& b : world.bodies) {
            if (b->health < b->maxHealth * 0.5f) b->isDamaged = true;
            if (b->health <= 0 && !b->isDestroyed) {
                b->isDestroyed = true;
                SpawnParticles(b->pos, b->color);
                if (b->material == MAT_PIG) { score += 5000; pigsLeft--; }
                else score += 500;
            }
        }

        world.bodies.erase(
            std::remove_if(world.bodies.begin(), world.bodies.end(), 
            [](const std::shared_ptr<Body>& b) { return b->isDestroyed; }), 
            world.bodies.end()
        );

        for (auto& p : particles) {
            p.pos += p.vel;
            p.vel.y += 0.5f;
            p.life -= dt * 1.5f;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](Particle& p) { return p.life <= 0; }), particles.end());

        if (pigsLeft <= 0 && state != STATE_WIN) {
            state = STATE_WIN;
            score += birdsLeft * 10000;
        }
    }

    void Draw() {
        ClearBackground(Color{ 135, 206, 235, 255 }); 

        DrawCircle(virtualW * 0.8f, virtualH * 0.2f, 60, GOLD);
        DrawCircle(virtualW * 0.2f, virtualH * 0.15f, 40, Color{255,255,255,200});
        DrawCircle(virtualW * 0.25f, virtualH * 0.15f, 50, Color{255,255,255,200});
        DrawCircle(virtualW * 0.3f, virtualH * 0.15f, 40, Color{255,255,255,200});

        for (const auto& t : birdTrail) {
            DrawCircleV(Vector2{t.x, t.y}, 4, Color{255,255,255,150});
        }

        DrawRectangle(slingAnchor.x + 5, slingAnchor.y - 20, 15, virtualH - slingAnchor.y, Color{139, 69, 19, 255});

        for (const auto& b : world.bodies) {
            if (b->shape == SHAPE_BOX) {
                Rectangle rec = { b->pos.x - b->width/2, b->pos.y - b->height/2, b->width, b->height };
                Color c = b->isDamaged ? ColorBrightness(b->color, -0.3f) : b->color;
                DrawRectangleRec(rec, c);
                DrawRectangleLinesEx(rec, 1, BLACK);
            } else if (b->shape == SHAPE_CIRCLE) {
                Color c = b->isDamaged ? ColorBrightness(b->color, -0.3f) : b->color;
                DrawCircleV(Vector2{b->pos.x, b->pos.y}, b->radius, c);
                DrawCircleLines(b->pos.x, b->pos.y, b->radius, BLACK);
                
                if (b->material == MAT_BIRD) { 
                    DrawCircle(b->pos.x + 5, b->pos.y - 5, 5, WHITE); 
                    DrawCircle(b->pos.x + 6, b->pos.y - 5, 2, BLACK); 
                    DrawTriangle(Vector2{b->pos.x+b->radius, b->pos.y}, Vector2{b->pos.x+5, b->pos.y-5}, Vector2{b->pos.x+5, b->pos.y+5}, YELLOW); 
                } else if (b->material == MAT_PIG) {
                    DrawCircle(b->pos.x + 5, b->pos.y - 3, 4, WHITE);
                    DrawCircle(b->pos.x - 5, b->pos.y - 3, 4, WHITE);
                    DrawCircle(b->pos.x + 5, b->pos.y - 3, 1.5f, BLACK);
                    DrawCircle(b->pos.x - 5, b->pos.y - 3, 1.5f, BLACK);
                    DrawEllipse(b->pos.x, b->pos.y + 5, 6, 4, Color{50,205,50,255}); 
                }
            }
        }

        if (state == STATE_DRAGGING && currentBird) {
            DrawLineEx(Vector2{slingAnchor.x - 10, slingAnchor.y - 10}, Vector2{currentBird->pos.x, currentBird->pos.y}, 6, Color{62, 39, 35, 255});
            DrawLineEx(Vector2{slingAnchor.x + 10, slingAnchor.y - 10}, Vector2{currentBird->pos.x, currentBird->pos.y}, 6, Color{62, 39, 35, 255});
        }

        DrawRectangle(slingAnchor.x - 10, slingAnchor.y + 20, 25, virtualH - slingAnchor.y - 50, Color{139, 69, 19, 255});

        for (const auto& p : particles) {
            Color pc = p.color;
            pc.a = (unsigned char)(255 * p.life);
            DrawCircleV(Vector2{p.pos.x, p.pos.y}, p.size, pc);
        }

        DrawText(TextFormat("Nivel: %d", level), 20, 20, 30, WHITE);
        DrawText(TextFormat("Passaros: %d", birdsLeft), 20, 60, 30, WHITE);
        DrawText(TextFormat("Pontos: %d", score), virtualW - 250, 20, 30, WHITE);

        if (state == STATE_MENU) DrawModal("PASSAROS FURIOSOS", "Toque para Jogar");
        else if (state == STATE_OVER) DrawModal("FIM DE JOGO!", TextFormat("Pontuacao: %d\nToque para Reiniciar", score));
        else if (state == STATE_WIN) DrawModal("NIVEL CONCLUIDO!", TextFormat("Incrivel! Pontos: %d\nToque para Prox. Nivel", score));
    }

    void DrawModal(const char* title, const char* subtitle) {
        DrawRectangle(0, 0, virtualW, virtualH, Color{0,0,0,150});
        DrawRectangle(virtualW/2 - 250, virtualH/2 - 150, 500, 300, GOLD);
        DrawRectangleLines(virtualW/2 - 250, virtualH/2 - 150, 500, 300, WHITE);
        
        int tWidth = MeasureText(title, 40);
        DrawText(title, virtualW/2 - tWidth/2, virtualH/2 - 80, 40, MAROON);
        
        int sWidth = MeasureText(subtitle, 20);
        DrawText(subtitle, virtualW/2 - sWidth/2, virtualH/2 + 20, 20, DARKGRAY);
    }
};

// ============================================================================
// 4. MAIN LOOP (Motor Cross-Platform com Escala Automática)
// ============================================================================
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    
    // Abrimos a janela de SO (pode ser o ecrã completo do Android)
    InitWindow(1280, 720, "Passaros Furiosos");
    SetTargetFPS(60);

    Game game;
    
    // O Segredo para funcionar em qualquer telemóvel: RenderTexture "Virtual"
    RenderTexture2D target = LoadRenderTexture(game.virtualW, game.virtualH);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose()) {
        
        // 1. Calcular proporções de escala entre a tela física e o nosso mundo (1280x720)
        float scale = std::min((float)GetScreenWidth() / game.virtualW, (float)GetScreenHeight() / game.virtualH);
        
        // 2. Mapeamento dos toques (dedos) reais para as coordenadas virtuais internas
        Vector2 rawMouse = GetMousePosition();
        Vec2 virtualMouse(
            (rawMouse.x - (GetScreenWidth() - (game.virtualW * scale)) * 0.5f) / scale,
            (rawMouse.y - (GetScreenHeight() - (game.virtualH * scale)) * 0.5f) / scale
        );

        bool isTouched = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0;
        bool isTapped = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP);

        // 3. Atualizar Físicas
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f; // Limitar frames engasgados

        // Lógica dos menus
        if (isTapped && game.state != STATE_IDLE && game.state != STATE_DRAGGING) {
            if (game.state == STATE_MENU || game.state == STATE_OVER) {
                game.score = 0;
                game.InitLevel(1);
            } else if (game.state == STATE_WIN) {
                game.InitLevel(game.level + 1);
            }
        }

        game.Update(dt, virtualMouse, isTouched);

        // 4. Desenhar o jogo inteiramente dentro da Textura (Câmara de vídeo)
        BeginTextureMode(target);
        game.Draw();
        EndTextureMode();

        // 5. Exibir a Textura Escalada perfeitamente na tela física (Mágica Letterbox)
        BeginDrawing();
        ClearBackground(BLACK);
        
        Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height };
        Rectangle destRec = { 
            (GetScreenWidth() - ((float)game.virtualW * scale)) * 0.5f, 
            (GetScreenHeight() - ((float)game.virtualH * scale)) * 0.5f, 
            (float)game.virtualW * scale, 
            (float)game.virtualH * scale 
        };
        
        DrawTexturePro(target.texture, sourceRec, destRec, Vector2{ 0, 0 }, 0.0f, WHITE);
        
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}