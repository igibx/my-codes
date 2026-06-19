/**
 * Pássaros Furiosos (Angry Birds Clone) em C++
 * Física Inteligente, Loja, Habilidades e Fases Infinitas
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
enum PhysMaterial { MAT_WOOD, MAT_STONE, MAT_GLASS, MAT_BIRD, MAT_PIG, MAT_GROUND };

// Subtipos para diferenciar Pássaros e Porcos
enum BirdType { BIRD_RED, BIRD_YELLOW, BIRD_BLACK };
enum PigType { PIG_NORMAL, PIG_HELMET, PIG_KING };

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
    int subType; // Guarda o BirdType ou PigType
    bool abilityUsed;

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
        abilityUsed = false;
        subType = 0;
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

// CORREÇÃO: Função reescrita para garantir que os ponteiros nunca sejam invertidos!
bool CirclevsAABB(Manifold& m) {
    // Descobre automaticamente quem é o Círculo e quem é a Caixa
    bool A_is_circle = (m.A->shape == SHAPE_CIRCLE);
    std::shared_ptr<Body> circle = A_is_circle ? m.A : m.B;
    std::shared_ptr<Body> box = A_is_circle ? m.B : m.A;

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
    
    Vec2 finalNormal;
    if (dist == 0.0f) {
        finalNormal = Vec2(0, 1);
        dist = 0.01f;
    } else {
        finalNormal = normal / dist;
    }

    if (inside) finalNormal = finalNormal * -1.0f;
    
    // A Normal tem sempre de apontar do Corpo A para o Corpo B
    if (A_is_circle) finalNormal = finalNormal * -1.0f;

    m.normal = finalNormal;
    m.penetration = inside ? (radius + dist) : (radius - dist);

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

    // Dano por colisão
    float impactForce = j;
    float damageThreshold = 250.0f; // Limite alto para a gravidade não destruir as torres sozinha
    if (impactForce > damageThreshold) {
        float forceExceed = impactForce - damageThreshold;
        float dmgA = forceExceed * (m.A->material == MAT_STONE ? 0.2f : (m.A->material == MAT_GLASS ? 1.0f : 0.5f));
        float dmgB = forceExceed * (m.B->material == MAT_STONE ? 0.2f : (m.B->material == MAT_GLASS ? 1.0f : 0.5f));
        
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
            
            // FÍSICA INTELIGENTE DE ARRASTO (DAMPING):
            // O pássaro não sofre arrasto enquanto voa velozmente, para não ficar "lento".
            // Blocos quase parados sofrem arrasto forte para entrarem em repouso e as torres não tremerem.
            if (b->material != MAT_BIRD) {
                if (b->vel.magSq() < 150.0f) b->vel = b->vel * 0.90f; // Trava os blocos lentos
                else b->vel = b->vel * 0.995f; // Fricção normal do ar
            } else {
                b->vel = b->vel * 0.998f; // Pássaro voa livremente com mínimo arrasto
            }

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

                    // CORREÇÃO: Removemos o boolean que estava a confundir os ponteiros!
                    if (A->shape == SHAPE_BOX && B->shape == SHAPE_BOX) m.isColliding = AABBvsAABB(m);
                    else if (A->shape == SHAPE_CIRCLE && B->shape == SHAPE_CIRCLE) m.isColliding = CirclevsCircle(m);
                    else if (A->shape == SHAPE_BOX && B->shape == SHAPE_CIRCLE) m.isColliding = CirclevsAABB(m);
                    else if (A->shape == SHAPE_CIRCLE && B->shape == SHAPE_BOX) m.isColliding = CirclevsAABB(m);

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
// 3. ESTRUTURA DO JOGO, ECONOMIA E LOJA
// ============================================================================
enum GameState { STATE_MENU, STATE_IDLE, STATE_SHOP, STATE_DRAGGING, STATE_FLYING, STATE_WAITING, STATE_OVER, STATE_WIN };

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
    int coins = 50; // Começa com 50 moedas!
    int level = 1;
    int pigsLeft = 0;
    
    std::shared_ptr<Body> currentBird;
    Vec2 slingAnchor;
    
    std::vector<int> birdQueue; // Fila de Pássaros
    std::vector<Particle> particles;
    std::vector<Vec2> birdTrail;

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
        pigsLeft = 0;

        // Abastecer a fila base do nível (ganha pássaros grátis)
        birdQueue.clear();
        int qtd = std::min(3 + (level / 3), 6);
        for(int i = 0; i < qtd; i++) {
            birdQueue.push_back(BIRD_RED);
        }

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
        if (birdQueue.empty()) return;
        
        int type = birdQueue.front();
        birdQueue.erase(birdQueue.begin());

        currentBird = std::make_shared<Body>(SHAPE_CIRCLE, 10.0f, slingAnchor);
        currentBird->radius = 18.0f;
        currentBird->material = MAT_BIRD;
        currentBird->subType = type;
        currentBird->restitution = 0.4f;

        if (type == BIRD_RED) currentBird->color = RED;
        else if (type == BIRD_YELLOW) currentBird->color = YELLOW;
        else if (type == BIRD_BLACK) {
            currentBird->color = BLACK;
            currentBird->mass = 15.0f; // Pássaro Bomba é mais pesado
            currentBird->invMass = 1.0f / 15.0f;
        }
        
        world.AddBody(currentBird);
        state = STATE_IDLE;
    }

    void SpawnPig(float x, float y, int type) {
        auto pig = std::make_shared<Body>(SHAPE_CIRCLE, 5.0f, Vec2(x, y));
        pig->material = MAT_PIG;
        pig->subType = type;
        
        if (type == PIG_NORMAL) {
            pig->radius = 20.0f; pig->health = pig->maxHealth = 50; pig->color = GREEN;
        } else if (type == PIG_HELMET) {
            pig->radius = 20.0f; pig->health = pig->maxHealth = 150; pig->color = DARKGREEN;
            pig->mass = 8.0f; pig->invMass = 1.0f/8.0f;
        } else if (type == PIG_KING) {
            pig->radius = 30.0f; pig->health = pig->maxHealth = 300; pig->color = LIME;
            pig->mass = 12.0f; pig->invMass = 1.0f/12.0f;
        }

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

        // Variedade de Porcos baseada no Nível
        int pType = PIG_NORMAL;
        if (lvl > 2 && GetRandomValue(0, 100) > 60) pType = PIG_HELMET;
        if (lvl > 4 && GetRandomValue(0, 100) > 85) pType = PIG_KING;

        if (type == 0) { 
            CreateBlock(x - 35, y - 40, 20, 80, mat); 
            CreateBlock(x + 35, y - 40, 20, 80, mat); 
            CreateBlock(x, y - 90, 100, 20, mat);     
            SpawnPig(x, y - (pType==PIG_KING ? 30 : 20), pType); 
        } else if (type == 1) { 
            float s = 40;
            CreateBlock(x - s, y - s/2, s, s, mat);
            CreateBlock(x, y - s/2, s, s, mat);
            CreateBlock(x + s, y - s/2, s, s, mat);
            CreateBlock(x - s/2, y - s*1.5f, s, s, mat);
            CreateBlock(x + s/2, y - s*1.5f, s, s, mat);
            CreateBlock(x, y - s*2.5f, s, s, mat);
            SpawnPig(x, y - s*3 - (pType==PIG_KING ? 30 : 20), pType); 
        } else { 
            CreateBlock(x - 30, y - 30, 20, 60, mat);
            CreateBlock(x + 30, y - 30, 20, 60, mat);
            CreateBlock(x, y - 70, 80, 20, mat);
            SpawnPig(x, y - (pType==PIG_KING ? 30 : 20), pType);
        }
    }

    void SpawnParticles(Vec2 pos, Color c, int count = 8) {
        for (int i = 0; i < count; i++) {
            Particle p;
            p.pos = pos;
            p.vel = Vec2(GetRandomValue(-150, 150)/10.0f, GetRandomValue(-250, 0)/10.0f);
            p.color = c;
            p.life = 1.0f;
            p.size = GetRandomValue(3, 8);
            particles.push_back(p);
        }
    }

    void Explode(Vec2 pos, float radius, float force) {
        SpawnParticles(pos, ORANGE, 40);
        SpawnParticles(pos, RED, 20);
        for(auto& b : world.bodies) {
            if(b->isStatic) continue;
            Vec2 dir = b->pos - pos;
            float distSq = dir.magSq();
            if(distSq < radius * radius && distSq > 0) {
                float dist = std::sqrt(distSq);
                float falloff = 1.0f - (dist / radius);
                b->ApplyImpulse((dir / dist) * (force * falloff));
                b->health -= (force * falloff * 0.2f); // Causa dano físico por explosão
            }
        }
    }

    bool CheckClick(Vec2 vMouse, Rectangle rec, bool isTapped) {
        if (!isTapped) return false;
        return CheckCollisionPointRec(Vector2{vMouse.x, vMouse.y}, rec);
    }

    void Update(float dt, Vec2 vMouse, bool isTouched, bool isTapped) {
        if (state == STATE_MENU || state == STATE_OVER || state == STATE_WIN) return;

        // ================= LOJA =================
        Rectangle shopBtn = { (float)virtualW - 150.0f, 80.0f, 130.0f, 50.0f };
        
        if (state == STATE_IDLE && CheckClick(vMouse, shopBtn, isTapped)) {
            state = STATE_SHOP;
            return;
        }

        if (state == STATE_SHOP) {
            // Lógica de Compras
            Rectangle btnYellow = { (float)virtualW/2 - 230.0f, 480.0f, 160.0f, 50.0f };
            Rectangle btnBlack  = { (float)virtualW/2 + 70.0f, 480.0f, 160.0f, 50.0f };
            Rectangle btnClose  = { (float)virtualW/2 - 100.0f, 600.0f, 200.0f, 50.0f };

            if (CheckClick(vMouse, btnYellow, isTapped)) {
                if (coins >= 50) { coins -= 50; birdQueue.insert(birdQueue.begin(), BIRD_YELLOW); }
            }
            if (CheckClick(vMouse, btnBlack, isTapped)) {
                if (coins >= 100) { coins -= 100; birdQueue.insert(birdQueue.begin(), BIRD_BLACK); }
            }
            if (CheckClick(vMouse, btnClose, isTapped)) {
                state = STATE_IDLE;
                if (!currentBird) SpawnBird(); // Atualiza o pássaro ativo na funda se tiver comprado
            }
            return;
        }

        // ================= JOGABILIDADE =================
        if (state == STATE_IDLE || state == STATE_DRAGGING) {
            if (isTouched) {
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
                    currentBird->vel = diff * 10.0f; // <--- MULTIPLICADOR DE VELOCIDADE AUMENTADO (De 5.0 para 10.0)!
                    state = STATE_FLYING;
                } else {
                    currentBird->pos = slingAnchor;
                    state = STATE_IDLE;
                }
            }
        }

        // Trava Posicional no Estilingue
        if (state == STATE_IDLE && currentBird) {
            // Se comprámos um pássaro e o atual é de outro tipo, atualiza o atual
            if (!birdQueue.empty() && currentBird->subType != birdQueue.front()) {
                currentBird->isDestroyed = true;
                currentBird = nullptr;
                SpawnBird();
            } else {
                currentBird->pos = slingAnchor;
                currentBird->vel = Vec2(0,0);
            }
        }

        world.Step(dt);

        // ================= HABILIDADES E VOO =================
        if (state == STATE_FLYING && currentBird) {
            
            // Ativar Habilidades ao Tocar na Tela!
            if (isTapped && !currentBird->abilityUsed) {
                if (currentBird->subType == BIRD_YELLOW) {
                    currentBird->vel = currentBird->vel * 2.5f; // Turbo Dash!
                    currentBird->vel.y = 0; // Vai reto
                    currentBird->abilityUsed = true;
                    SpawnParticles(currentBird->pos, YELLOW, 15);
                } else if (currentBird->subType == BIRD_BLACK) {
                    Explode(currentBird->pos, 200.0f, 1500.0f); // Kaboom!
                    currentBird->isDestroyed = true;
                }
            }

            if (!currentBird->isDestroyed) {
                if (currentBird->vel.magSq() > 100.0f && GetRandomValue(0,100) > 60) {
                    birdTrail.push_back(currentBird->pos);
                    if (birdTrail.size() > 50) birdTrail.erase(birdTrail.begin());
                }

                bool stopped = currentBird->vel.magSq() < 10.0f;
                bool outOfBounds = currentBird->pos.x > virtualW + 100 || currentBird->pos.x < -100;

                if (stopped || outOfBounds) {
                    state = STATE_WAITING;
                }
            } else {
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
                    if (!birdQueue.empty()) SpawnBird();
                    else state = STATE_OVER;
                }
            }
        }

        // ================= DESTRUIÇÃO E RECOMPENSAS =================
        for (auto& b : world.bodies) {
            if (b->health < b->maxHealth * 0.5f) b->isDamaged = true;
            if (b->health <= 0 && !b->isDestroyed) {
                b->isDestroyed = true;
                SpawnParticles(b->pos, b->color);
                
                if (b->material == MAT_PIG) { 
                    score += 5000; 
                    coins += 15; // Ganha 15 moedas por porco
                    pigsLeft--; 
                } else {
                    score += 500;
                    coins += 1;  // Ganha 1 moeda por bloco
                }
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
            score += birdQueue.size() * 10000;
            coins += birdQueue.size() * 20; // Bônus de moedas ao vencer
        }
    }

    void Draw() {
        ClearBackground(Color{ 135, 206, 235, 255 }); 

        // Cenário
        DrawCircle(virtualW * 0.8f, virtualH * 0.2f, 60, GOLD);
        DrawCircle(virtualW * 0.2f, virtualH * 0.15f, 40, Color{255,255,255,200});
        DrawCircle(virtualW * 0.25f, virtualH * 0.15f, 50, Color{255,255,255,200});
        DrawCircle(virtualW * 0.3f, virtualH * 0.15f, 40, Color{255,255,255,200});

        for (const auto& t : birdTrail) {
            DrawCircleV(Vector2{t.x, t.y}, 4, Color{255,255,255,150});
        }

        DrawRectangle(slingAnchor.x + 5, slingAnchor.y - 20, 15, virtualH - slingAnchor.y, Color{139, 69, 19, 255});

        // Entidades Físicas
        for (const auto& b : world.bodies) {
            if (b->shape == SHAPE_BOX) {
                Rectangle rec = { b->pos.x - b->width/2, b->pos.y - b->height/2, b->width, b->height };
                Color c = b->isDamaged ? ColorBrightness(b->color, -0.3f) : b->color;
                DrawRectangleRec(rec, c);
                DrawRectangleLinesEx(rec, 1, BLACK);
            } else if (b->shape == SHAPE_CIRCLE) {
                Color c = b->isDamaged ? ColorBrightness(b->color, -0.3f) : b->color;
                
                // Representação Visual Aprimorada
                if (b->material == MAT_BIRD && b->subType == BIRD_YELLOW) {
                    // Pássaro Amarelo desenhado como um Triângulo!
                    DrawTriangle(Vector2{b->pos.x, b->pos.y - b->radius - 5}, 
                                 Vector2{b->pos.x - b->radius, b->pos.y + b->radius}, 
                                 Vector2{b->pos.x + b->radius, b->pos.y + b->radius}, c);
                    DrawCircle(b->pos.x + 5, b->pos.y, 4, WHITE);
                    DrawCircle(b->pos.x + 6, b->pos.y, 2, BLACK);
                } else {
                    DrawCircleV(Vector2{b->pos.x, b->pos.y}, b->radius, c);
                    DrawCircleLines(b->pos.x, b->pos.y, b->radius, BLACK);
                    
                    if (b->material == MAT_BIRD) { 
                        DrawCircle(b->pos.x + 5, b->pos.y - 5, 5, WHITE); 
                        DrawCircle(b->pos.x + 6, b->pos.y - 5, 2, BLACK); 
                        if (b->subType == BIRD_RED) DrawTriangle(Vector2{b->pos.x+b->radius, b->pos.y}, Vector2{b->pos.x+5, b->pos.y-5}, Vector2{b->pos.x+5, b->pos.y+5}, YELLOW); 
                        else if (b->subType == BIRD_BLACK) DrawRectangle(b->pos.x-3, b->pos.y-b->radius-8, 6, 8, ORANGE); // Pavio
                    } else if (b->material == MAT_PIG) {
                        DrawCircle(b->pos.x + 5, b->pos.y - 3, 4, WHITE);
                        DrawCircle(b->pos.x - 5, b->pos.y - 3, 4, WHITE);
                        DrawCircle(b->pos.x + 5, b->pos.y - 3, 1.5f, BLACK);
                        DrawCircle(b->pos.x - 5, b->pos.y - 3, 1.5f, BLACK);
                        DrawEllipse(b->pos.x, b->pos.y + 5, 6, 4, Color{50,205,50,255}); 
                        
                        // Detalhes Inimigos
                        if (b->subType == PIG_HELMET) {
                            DrawCircleSector(Vector2{b->pos.x, b->pos.y}, b->radius, 180, 360, 0, GRAY);
                        } else if (b->subType == PIG_KING) {
                            DrawRectangle(b->pos.x - 10, b->pos.y - b->radius - 15, 20, 15, GOLD);
                        }
                    }
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

        // UI HUD
        DrawText(TextFormat("Nivel: %d", level), 20, 20, 30, WHITE);
        DrawText(TextFormat("Passaros na Fila: %lu", birdQueue.size()), 20, 60, 30, WHITE);
        DrawText(TextFormat("Moedas: %d", coins), 20, 100, 30, YELLOW);
        DrawText(TextFormat("Pontos: %d", score), virtualW - 250, 20, 30, WHITE);

        if (state == STATE_IDLE) {
            DrawRectangle(virtualW - 150, 80, 130, 50, ORANGE);
            DrawRectangleLines(virtualW - 150, 80, 130, 50, BLACK);
            DrawText("LOJA", virtualW - 115, 95, 20, BLACK);
        }

        if (state == STATE_SHOP) DrawShopUI();
        else if (state == STATE_MENU) DrawModal("PASSAROS FURIOSOS", "Toque para Jogar");
        else if (state == STATE_OVER) DrawModal("FIM DE JOGO!", TextFormat("Sem Passaros...\nToque para Reiniciar", score));
        else if (state == STATE_WIN) DrawModal("NIVEL CONCLUIDO!", TextFormat("Belo Tiro! +Moedas\nToque para Prox. Nivel", score));
    }

    void DrawShopUI() {
        DrawRectangle(0, 0, virtualW, virtualH, Color{0,0,0,200});
        DrawText("LOJA DE PASSAROS", virtualW/2 - 200, 100, 40, GOLD);
        DrawText(TextFormat("Suas Moedas: %d", coins), virtualW/2 - 120, 160, 30, YELLOW);

        // Amarelo
        DrawRectangle(virtualW/2 - 250, 250, 200, 300, LIGHTGRAY);
        DrawTriangle(Vector2{(float)virtualW/2 - 150.0f, 280.0f}, Vector2{(float)virtualW/2 - 200.0f, 350.0f}, Vector2{(float)virtualW/2 - 100.0f, 350.0f}, YELLOW);
        DrawText("CHUCK", virtualW/2 - 190, 380, 25, BLACK);
        DrawText("Tocar no Ar:\nDash Super Veloz", virtualW/2 - 230, 420, 15, DARKGRAY);
        DrawRectangle(virtualW/2 - 230, 480, 160, 50, coins >= 50 ? GREEN : GRAY);
        DrawText("50 Moedas", virtualW/2 - 210, 495, 20, WHITE);

        // Preto
        DrawRectangle(virtualW/2 + 50, 250, 200, 300, LIGHTGRAY);
        DrawCircle(virtualW/2 + 150, 320, 40, BLACK);
        DrawText("BOMB", virtualW/2 + 115, 380, 25, BLACK);
        DrawText("Tocar no Ar:\nExplosao em Area", virtualW/2 + 70, 420, 15, DARKGRAY);
        DrawRectangle(virtualW/2 + 70, 480, 160, 50, coins >= 100 ? GREEN : GRAY);
        DrawText("100 Moedas", virtualW/2 + 90, 495, 20, WHITE);

        // Botão Fechar
        DrawRectangle(virtualW/2 - 100, 600, 200, 50, RED);
        DrawText("VOLTAR", virtualW/2 - 45, 615, 20, WHITE);
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
// 4. MAIN LOOP 
// ============================================================================
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Passaros Furiosos");
    SetTargetFPS(60);

    Game game;
    RenderTexture2D target = LoadRenderTexture(game.virtualW, game.virtualH);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose()) {
        float scale = std::min((float)GetScreenWidth() / game.virtualW, (float)GetScreenHeight() / game.virtualH);
        
        Vector2 rawMouse = GetMousePosition();
        Vec2 virtualMouse(
            (rawMouse.x - (GetScreenWidth() - (game.virtualW * scale)) * 0.5f) / scale,
            (rawMouse.y - (GetScreenHeight() - (game.virtualH * scale)) * 0.5f) / scale
        );

        bool isTouched = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0;
        bool isTapped = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP);

        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (isTapped && game.state != STATE_IDLE && game.state != STATE_DRAGGING && game.state != STATE_SHOP && game.state != STATE_FLYING) {
            if (game.state == STATE_MENU || game.state == STATE_OVER) {
                game.score = 0;
                game.coins = 50;
                game.InitLevel(1);
            } else if (game.state == STATE_WIN) {
                game.InitLevel(game.level + 1);
            }
        }

        game.Update(dt, virtualMouse, isTouched, isTapped);

        BeginTextureMode(target);
        game.Draw();
        EndTextureMode();

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