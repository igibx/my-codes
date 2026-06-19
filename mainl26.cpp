// ========================================================================================
// SUPERGEMINI CREATOR ENGINE - V24.0 (THE 3RD PERSON & PLAYER UPDATE)
// Architect: Gemini God Mode | Executive Producer: Igor Bettarello Xavier (OMEGARED)
// Features: 3rd Person Camera, Physical Player (Fox), Transform Keys, Smart Rotations!
// ========================================================================================

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "raylib.h"
#include "rlgl.h" 
#include "raymath.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include <memory>
#include <atomic>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <deque>

using namespace std::chrono_literals;

// ==============================================================================
// 1. SISTEMAS CORE E SHADERS GLSL AVANÇADOS
// ==============================================================================

const char* vertexShaderStr = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
uniform mat4 mvp;
uniform mat4 matModel;
void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    mat3 normalMatrix = transpose(inverse(mat3(matModel)));
    fragNormal = normalize(normalMatrix * vertexNormal);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* fragmentShaderStr = R"(
#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDir;
uniform vec3 viewPos;
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 norm = normalize(fragNormal);
    vec3 light = normalize(lightDir);
    
    float diff = max(dot(norm, light), 0.3);
    vec3 viewDir = normalize(viewPos - fragPosition);
    float rim = 1.0 - max(dot(viewDir, norm), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    
    vec3 reflectDir = reflect(-light, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    
    vec3 lighting = vec3(diff) + vec3(rim * 0.4) + vec3(spec * 0.3);
    lighting = max(lighting, vec3(0.45)); 
    
    finalColor = texelColor * colDiffuse * vec4(lighting, 1.0);
}
)";

class SettingsManager {
public:
    bool musicOn = true; bool sfxOn = true; int language = 0; 
    const char* Tr(const char* pt, const char* en) const { return (language == 0) ? pt : en; }
};
SettingsManager Config;

class AudioManager {
public:
    void Init() { InitAudioDevice(); }
    void Close() { CloseAudioDevice(); }
    void PlayUIHover() { if(Config.sfxOn) { /* Som */ } }
    void PlayUISelect() { if(Config.sfxOn) { /* Som */ } }
};
AudioManager Audio;

// ==============================================================================
// FOREST PACK MANAGER (Gerencia as árvores reais)
// ==============================================================================
class ForestManager {
public:
    std::vector<Model> trees;
    std::vector<Model> bushes;
    std::vector<Model> rocks;
    std::vector<Model> grasses;
    
    void Init() {
        const char* treePaths[] = {
            "src/forestpack/Assets/gltf/Tree_1_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Tree_2_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Tree_3_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Tree_4_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Tree_Bare_1_A_Color1.gltf"
        };
        const char* bushPaths[] = {
            "src/forestpack/Assets/gltf/Bush_1_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Bush_2_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Bush_3_A_Color1.gltf"
        };
        const char* rockPaths[] = {
            "src/forestpack/Assets/gltf/Rock_1_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Rock_2_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Rock_3_A_Color1.gltf"
        };
        const char* grassPaths[] = {
            "src/forestpack/Assets/gltf/Grass_1_A_Color1.gltf",
            "src/forestpack/Assets/gltf/Grass_2_A_Color1.gltf"
        };
        
        for(auto path : treePaths) if(FileExists(path)) trees.push_back(LoadModel(path));
        for(auto path : bushPaths) if(FileExists(path)) bushes.push_back(LoadModel(path));
        for(auto path : rockPaths) if(FileExists(path)) rocks.push_back(LoadModel(path));
        for(auto path : grassPaths) if(FileExists(path)) grasses.push_back(LoadModel(path));
    }
    
    void Unload() {
        for(auto& m : trees) UnloadModel(m);
        for(auto& m : bushes) UnloadModel(m);
        for(auto& m : rocks) UnloadModel(m);
        for(auto& m : grasses) UnloadModel(m);
    }
};
ForestManager Forest;

// ==============================================================================
// 2. ESTADO GLOBAL, MATEMÁTICA E VFX
// ==============================================================================
std::mutex mundoMutex;
std::map<std::pair<int, int>, float> modificacoesTerreno;
std::map<std::pair<int, int>, float> modificacoesAlvo; 

struct BlocoFixo { Vector3 pos; Color cor; };
std::vector<BlocoFixo> construcoesFixas;
std::deque<BlocoFixo> filaConstrucao; 
float timerConstrucao = 0.0f;

struct Particula { Vector3 pos; Vector3 vel; Color cor; float vida; float maxVida; float tamanho; };
std::vector<Particula> particulas;

int biomaAtual = 0, biomaAlvo = 0;   
float biomaBlend = 1.0f;
int climaGlobal = 0;   
float timeScale = 1.0f;
bool modoWireframe = false, godModeAtivo = false, luzesAtivas = false, escudoAtivo = false, camuflagemAtiva = false, modoFesta = false;
float gravidadeBase = 20.0f; 
float cameraShake = 0.0f; 

struct EntidadeDivina { int tipo; Vector3 pos; Vector3 vel; float raio; float tempoVida; };
std::vector<EntidadeDivina> entidadesGlobais;

float RandPseudo(float x, float z) { float res = sin(x * 12.9898f + z * 78.233f) * 43758.5453f; return res - floor(res); }
float RandFloat() { return (float)rand() / (float)RAND_MAX; }
float Hash2D(float x, float y) { float n = sin(x * 12.9898f + y * 78.233f) * 43758.5453123f; return n - floor(n); }
float SmoothNoise(float x, float y) {
    float xi = floor(x); float yi = floor(y);
    float f00 = Hash2D(xi, yi); float f10 = Hash2D(xi + 1.0f, yi); float f01 = Hash2D(xi, yi + 1.0f); float f11 = Hash2D(xi + 1.0f, yi + 1.0f);
    float u = x - xi; float v = y - yi; u = u * u * (3.0f - 2.0f * u); v = v * v * (3.0f - 2.0f * v);
    return f00*(1.0f-u)*(1.0f-v) + f10*u*(1.0f-v) + f01*(1.0f-u)*v + f11*u*v;
}
float FBM(float x, float y, int octaves) {
    float v = 0.0f; float a = 0.5f;
    for (int i=0; i<octaves; i++) { v += a * SmoothNoise(x, y); x *= 2.0f; y *= 2.0f; a *= 0.5f; }
    return v;
}

Color LerpColor(Color c1, Color c2, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return { (unsigned char)(c1.r + (c2.r - c1.r) * t), (unsigned char)(c1.g + (c2.g - c1.g) * t), (unsigned char)(c1.b + (c2.b - c1.b) * t), (unsigned char)(c1.a + (c2.a - c1.a) * t) };
}

void EmitirParticulas(Vector3 pos, Color cor, int qtd, float velMax) {
    for(int i=0; i<qtd; i++) {
        Vector3 v = { (RandFloat()-0.5f)*velMax, RandFloat()*velMax*1.5f, (RandFloat()-0.5f)*velMax };
        particulas.push_back({pos, v, cor, 1.0f + RandFloat(), 1.0f + RandFloat(), RandFloat()*0.3f + 0.1f});
    }
}

float GerarAlturaTerrenoBase(float x, float z) { return FBM(x * 0.03f, z * 0.03f, 4) * 40.0f - 15.0f; }

float LerAlturaMundo(float x, float z) {
    float base = GerarAlturaTerrenoBase(x, z); int px = round(x); int pz = round(z);
    auto it = modificacoesTerreno.find({px, pz});
    if (it != modificacoesTerreno.end()) return base + it->second;
    return base;
}

Color ObterCorBiomaPuro(int bioma, float x, float z, float altura, Vector3 normal) {
    float slope = 1.0f - normal.y; float pathNoise = abs(FBM(x * 0.02f, z * 0.02f, 2) - 0.5f); 
    if (bioma == 1) { if (slope > 0.5f) return { 190, 150, 110, 255 }; return { 255, 235, 180, 255 }; } 
    else if (bioma == 2) { if (slope > 0.4f) return { 50, 40, 40, 255 }; return { 100, 30, 30, 255 }; } 
    else if (bioma == 3) { if (slope > 0.6f) return { 180, 220, 240, 255 }; return { 250, 255, 255, 255 }; } 
    else if (bioma == 4) { if (slope > 0.3f) return { 10, 10, 25, 255 }; if (pathNoise < 0.02f) return {0, 255, 255, 255}; return { 30, 0, 50, 255 }; } 
    else if (bioma == 5) { if (slope > 0.5f) return { 50, 0, 100, 255 }; return { 80, 255, 100, 255 }; } 
    else if (bioma == 6) { if (slope > 0.5f) return { 255, 105, 180, 255 }; return { 255, 182, 193, 255 }; } 
    
    if (slope > 0.45f) return { 150, 150, 155, 255 }; if (altura > 18.0f) return { 255, 250, 250, 255 }; 
    if (pathNoise < 0.04f) return { 220, 190, 150, 255 }; 
    float grassVar = SmoothNoise(x * 0.5f, z * 0.5f) * 15; return { (unsigned char)(120 + grassVar), (unsigned char)(200 + grassVar), (unsigned char)(80 + grassVar), 255 };
}

Color ObterCorBiomaDinâmico(float x, float z, float altura, Vector3 normal) {
    Color cAtual = ObterCorBiomaPuro(biomaAtual, x, z, altura, normal);
    if (biomaBlend < 1.0f) { Color cAlvo = ObterCorBiomaPuro(biomaAlvo, x, z, altura, normal); return LerpColor(cAtual, cAlvo, biomaBlend); }
    return cAtual;
}

// ==============================================================================
// 3. O JOGADOR (FÍSICA, ANIMAÇÃO E FORMAS)
// ==============================================================================
class PlayerCharacter {
public:
    Vector3 posicao;
    float anguloRotacao;
    Model modeloFox;
    Texture2D texDiffuse;
    ModelAnimation* animacoes;
    int animCount;
    float animFrameCounter;
    int currentAnimIndex;
    bool temModelo;
    int formaAtual; // 0=Fox, 1=Esfera, 2=Cubo

    void Init() {
        posicao = {0, 15, 0};
        anguloRotacao = 0.0f;
        temModelo = false;
        animCount = 0;
        animFrameCounter = 0.0f;
        currentAnimIndex = 0;
        formaAtual = 0;
    }

    void Carregar(Shader shaderLuz) {
        if (FileExists("src/fox/fox.glb")) {
            modeloFox = LoadModel("src/fox/fox.glb");
            animacoes = LoadModelAnimations("src/fox/fox.glb", &animCount);
            if (FileExists("src/fox/Textures/fox_diffuse.png")) {
                texDiffuse = LoadTexture("src/fox/Textures/fox_diffuse.png");
                modeloFox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texDiffuse;
            }
            // Aplica o poderoso shader da V23 na Fox!
            for (int i = 0; i < modeloFox.materialCount; i++) modeloFox.materials[i].shader = shaderLuz;
            temModelo = true;
        }
    }

    void Unload() {
        if (temModelo) {
            UnloadTexture(texDiffuse);
            UnloadModel(modeloFox);
            if (animCount > 0) UnloadModelAnimations(animacoes, animCount);
        }
    }

    void Atualizar(float dt, Vector3 moveDir, float speedMultReal, bool isJumping) {
        bool isMoving = Vector3Length(moveDir) > 0.01f;
        
        // Rotacionar suavemente na direção do movimento WASD
        if (isMoving) {
            float targetAngle = atan2(moveDir.x, moveDir.z) * RAD2DEG;
            float deltaAngulo = targetAngle - anguloRotacao;
            while (deltaAngulo > 180.0f) deltaAngulo -= 360.0f;
            while (deltaAngulo < -180.0f) deltaAngulo += 360.0f;
            anguloRotacao += deltaAngulo * dt * 12.0f;
        }

        if (temModelo && animCount > 0) {
            int nextAnim = 0;
            if (isJumping) nextAnim = (animCount > 3) ? 3 : 0; // Pulo
            else if (isMoving) {
                if (speedMultReal > 1.2f) nextAnim = (animCount > 2) ? 2 : ((animCount > 1) ? 1 : 0); // Corre
                else nextAnim = (animCount > 1) ? 1 : 0; // Anda
            }
            
            if (nextAnim != currentAnimIndex) {
                currentAnimIndex = nextAnim;
                animFrameCounter = 0.0f;
            }
            
            float frameStep = dt * 60.0f;
            if (currentAnimIndex == 1) frameStep *= (speedMultReal > 0 ? speedMultReal : 1.0f);
            if (currentAnimIndex == 2) frameStep *= (speedMultReal > 0 ? speedMultReal * 0.8f : 1.0f);

            animFrameCounter += frameStep;
            if (animFrameCounter >= animacoes[currentAnimIndex].frameCount) animFrameCounter = 0;
            UpdateModelAnimation(modeloFox, animacoes[currentAnimIndex], (int)animFrameCounter);
        }
    }

    void Desenhar(bool isGodMode) {
        float r = 0.8f;
        mundoMutex.lock(); float alturaSombra = LerAlturaMundo(posicao.x, posicao.z) + 0.05f; mundoMutex.unlock();
        
        if (formaAtual == 0 && temModelo) {
            // Sombra da Fox projetada no chão
            DrawModelEx(modeloFox, {posicao.x, alturaSombra, posicao.z}, {0,1,0}, anguloRotacao, {1.0f, 0.01f, 1.0f}, Fade(BLACK, 0.6f));
            // Fox (Brilha se estiver no God Mode!)
            DrawModelEx(modeloFox, posicao, {0,1,0}, anguloRotacao, {1.0f, 1.0f, 1.0f}, isGodMode ? Fade(SKYBLUE, 0.8f) : WHITE);
        } else {
            DrawCylinder({posicao.x, alturaSombra, posicao.z}, r, r, 0.05f, 16, Fade(BLACK, 0.6f));
            Color c = isGodMode ? SKYBLUE : ORANGE;
            if (formaAtual == 1) DrawSphere({posicao.x, posicao.y + r, posicao.z}, r, c);
            else if (formaAtual == 2) DrawCube({posicao.x, posicao.y + r, posicao.z}, r*2.2f, r*2.2f, r*2.2f, c);
            else DrawSphere({posicao.x, posicao.y + r, posicao.z}, r, c);
        }
    }
};

// ==============================================================================
// 4. IA NLP AVANÇADA PARA ZUBA
// ==============================================================================
struct RespostaIA { std::string texto; std::map<std::string, std::string> comandos; };
std::string ToLowerString(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); }); return s; }
bool Match(const std::string& in, const std::vector<std::string>& keys) { for (const auto& k : keys) if (in.find(k) != std::string::npos) return true; return false; }

RespostaIA ProcessarIAInterna(std::string inputJogador, float alturaAtual) {
    std::string in = ToLowerString(inputJogador); std::map<std::string, std::string> cmds; std::string resp = ""; bool acted = false;
    bool isOff = Match(in, {"off", "desliga", " pare ", "reverte", "cancela", "tira ", "remove", "desfaz", "pare com", "desative", "chega de", "zera", "normal"});

    if (Match(in, {"cubo", "quadrad", "caixa"})) { cmds["FORMA"] = isOff ? "ESFERA" : "CUBO"; resp += isOff ? Config.Tr("Retornando ao normal. ", "Returning to normal. ") : Config.Tr("Zuba Cubico! ", "Cubic Zuba! "); acted = true; } 
    else if (Match(in, {"piramide", "triangulo"}) && !Match(in, {"constru", "faca"})) { cmds["FORMA"] = isOff ? "ESFERA" : "PIRAMIDE"; resp += isOff ? Config.Tr("Normal. ", "Normal. ") : Config.Tr("Zuba Piramidal. ", "Pyramidal Zuba. "); acted = true; } 
    else if (Match(in, {"cilindro", "tubo", "lata"})) { cmds["FORMA"] = "CILINDRO"; resp += Config.Tr("Forma cilindrica assumida! ", "Cylindrical shape assumed! "); acted = true; }
    else if (Match(in, {"anel", "torus", "rosquinha"})) { cmds["FORMA"] = "TORUS"; resp += Config.Tr("Virei uma rosquinha! ", "I became a donut! "); acted = true; }
    else if (Match(in, {"achatad", "panqueca", "pizza"})) { cmds["FORMA"] = "ACHATADO"; resp += Config.Tr("Fui esmagado! ", "I got squashed! "); acted = true; }
    else if (Match(in, {"esfera", "bola", "redond"})) { cmds["FORMA"] = "ESFERA"; resp += Config.Tr("Retornando a perfeicao redondinha. ", "Returning to round perfection. "); acted = true; }

    if (Match(in, {"gigante", "imenso", "cresca", "enorme"})) { cmds["TAMANHO"] = isOff ? "NORMAL" : "GIGANTE"; resp += isOff ? Config.Tr("Zuba encolhendo. ", "Zuba shrinking. ") : Config.Tr("Zuba GIGANTE! ", "GIANT Zuba! "); acted = true; } 
    else if (Match(in, {"pequen", "minúscul", "diminua", "encolh"})) { cmds["TAMANHO"] = isOff ? "NORMAL" : "PEQUENO"; resp += isOff ? Config.Tr("Tamanho restaurado. ", "Size restored. ") : Config.Tr("Mini Zuba. ", "Mini Zuba. "); acted = true; }

    if (Match(in, {"tempestade", "raio", "trovao"})) { cmds["CLIMA"] = isOff ? "LIMPO" : "TEMPESTADE"; resp += isOff ? Config.Tr("Ceus limpos. ", "Clear skies. ") : Config.Tr("Invocando os raios! ", "Summoning lightning! "); acted = true; }
    if (Match(in, {"chuv", "chova"})) { cmds["CLIMA"] = isOff ? "LIMPO" : "CHUVA"; resp += isOff ? Config.Tr("Chuva parou. ", "Rain stopped. ") : Config.Tr("Chovendo. ", "Raining. "); acted = true; }
    if (Match(in, {"nevoeiro", "nebl", "bruma"})) { cmds["CLIMA"] = isOff ? "LIMPO" : "NEVOEIRO"; resp += isOff ? Config.Tr("Sem nevoa. ", "No fog. ") : Config.Tr("Misterio no ar. ", "Mystery in the air. "); acted = true; }
    if (Match(in, {"aurora", "luzes no ceu"})) { cmds["CLIMA"] = isOff ? "LIMPO" : "AURORA"; resp += isOff ? Config.Tr("Ceus normais. ", "Normal skies. ") : Config.Tr("Lindas luzes celestiais! ", "Beautiful celestial lights! "); acted = true; }
    if (Match(in, {"eclipse", "escuridao"})) { cmds["CLIMA"] = isOff ? "LIMPO" : "ECLIPSE"; resp += isOff ? Config.Tr("Sol voltou. ", "Sun is back. ") : Config.Tr("O sol desapareceu! ", "The sun disappeared! "); acted = true; }

    if (Match(in, {"deserto", "areia"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "DESERTO"; resp += Config.Tr("Areia quente! ", "Hot sand! "); acted = true; }
    if (Match(in, {"magma", "lava", "vulcao"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "MAGMA"; resp += Config.Tr("O chao e lava! ", "Floor is lava! "); acted = true; }
    if (Match(in, {"gelo", "neve", "frio"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "GELO"; resp += Config.Tr("Congelando tudo! ", "Freezing everything! "); acted = true; }
    if (Match(in, {"cyberpunk", "neon"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "CYBERPUNK"; resp += Config.Tr("Mundo distopico! ", "Dystopian world! "); acted = true; }
    if (Match(in, {"alien", "extraterrestre"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "ALIEN"; resp += Config.Tr("Que mundo estranho... ", "What a weird world... "); acted = true; }
    if (Match(in, {"doce", "candy", "rosa"})) { cmds["BIOMA"] = isOff ? "NORMAL" : "DOCE"; resp += Config.Tr("Tudo feito de acucar! ", "Everything made of sugar! "); acted = true; }

    if (Match(in, {"congela", "pare o tempo"})) { cmds["TEMPO"] = isOff ? "NORMAL" : "CONGELAR"; resp += Config.Tr("ZAWARDO! ", "Time frozen! "); acted = true; }
    if (Match(in, {"acelera", "rapido"})) { cmds["TEMPO"] = isOff ? "NORMAL" : "ACELERAR"; resp += Config.Tr("Hyper speed! ", "Hyper speed! "); acted = true; }
    if (Match(in, {"lenta", "devagar", "matrix tempo"})) { cmds["TEMPO"] = isOff ? "NORMAL" : "LENTO"; resp += Config.Tr("Bullet time ativado. ", "Bullet time activated. "); acted = true; }
    if (Match(in, {"gravidade", "lua"})) { cmds["TEMPO"] = isOff ? "GRAVIDADE_NORMAL" : "GRAVIDADE_LUNAR"; resp += Config.Tr("Flutuando! ", "Floating! "); acted = true; }

    if (Match(in, {"wireframe", "matrix"})) { cmds["HACK"] = isOff ? "WIREFRAME_OFF" : "WIREFRAME_ON"; resp += Config.Tr("Hackeando a malha. ", "Hacking the mesh. "); acted = true; }
    if (Match(in, {"god mode", "deus"})) { cmds["HACK"] = isOff ? "GODMODE_OFF" : "GODMODE_ON"; resp += Config.Tr("Poder absoluto ativado. ", "Absolute power. "); acted = true; }
    if (Match(in, {"festa", "balada", "disco"})) { cmds["HACK"] = isOff ? "FESTA_OFF" : "FESTA_ON"; resp += Config.Tr("BOTA O SOM NA CAIXA! ", "DROP THE BEAT! "); acted = true; }
    if (Match(in, {"luzes", "ilumina"})) { cmds["HACK"] = isOff ? "LUZES_OFF" : "LUZES_ON"; resp += Config.Tr("Iluminando! ", "Lighting up! "); acted = true; }

    if (Match(in, {"escudo", "barreira"})) { cmds["DEFESA"] = isOff ? "ESCUDO_OFF" : "ESCUDO_ON"; resp += Config.Tr("Campo de forca! ", "Force field! "); acted = true; }
    if (Match(in, {"camufla", "invisi"})) { cmds["DEFESA"] = isOff ? "CAMUFLAGEM_OFF" : "CAMUFLAGEM_ON"; resp += Config.Tr("Modo furtivo! ", "Stealth mode! "); acted = true; }
    if (Match(in, {"domo", "redoma"})) { cmds["DEFESA"] = "DOMO"; resp += Config.Tr("Domo de protecao absoluto! ", "Absolute protection dome! "); acted = true; }
    if (Match(in, {"fogo", "anel de fogo"})) { cmds["DEFESA"] = "ANEL_FOGO"; resp += Config.Tr("Criando barreira flamejante! ", "Creating flaming barrier! "); acted = true; }

    if (Match(in, {"atira", "laser", "destrua"})) { cmds["ACAO"] = "ATIRAR"; resp += Config.Tr("Pew pew! ", "Pew pew! "); acted = true; }
    if (Match(in, {"gire", "roda", "piao"})) { cmds["ACAO"] = "GIRAR"; resp += Config.Tr("Estou tonto! ", "I'm dizzy! "); acted = true; }
    if (Match(in, {"pule", "salta", "voe"})) { cmds["ACAO"] = "PULAR"; resp += Config.Tr("Boing! ", "Boing! "); acted = true; }
    if (Match(in, {"dance", "rebola"})) { cmds["ACAO"] = "DANCAR"; resp += Config.Tr("Olha o passinho do Zuba! ", "Watch Zuba's moves! "); acted = true; }
    if (Match(in, {"segu", "volta"})) { cmds["ACAO"] = "SEGUIR"; resp += Config.Tr("Estou a caminho! ", "On my way! "); acted = true; }
    if (Match(in, {"frente", "explore"})) { cmds["ACAO"] = "IR_FRENTE"; resp += Config.Tr("Indo investigar! ", "Investigating! "); acted = true; }
    if (Match(in, {"para", "parad", "fique ai"})) { cmds["ACAO"] = "PARAR"; resp += Config.Tr("Ficarei aqui. ", "I'll stay here. "); acted = true; }

    if (Match(in, {"montanha", "morro"})) { cmds["TERRENO"] = "MONTANHA"; resp += Config.Tr("Levantando terra! ", "Lifting earth! "); acted = true; }
    if (Match(in, {"planicie", "achata", "plano"})) { cmds["TERRENO"] = "PLANICIE"; resp += Config.Tr("Amassando tudo. ", "Flattening all. "); acted = true; }
    if (Match(in, {"abismo", "buraco"})) { cmds["TERRENO"] = "ABISMO"; resp += Config.Tr("Cuidado com a queda! ", "Mind the gap! "); acted = true; }
    if (Match(in, {"lago", "agua"})) { cmds["TERRENO"] = "LAGO"; resp += Config.Tr("Enchendo a piscina! ", "Filling the pool! "); acted = true; }
    if (Match(in, {"canion", "desfiladeiro"})) { cmds["TERRENO"] = "CANION"; resp += Config.Tr("Rachando a terra ao meio! ", "Splitting the earth! "); acted = true; }
    if (Match(in, {"ilha"})) { cmds["TERRENO"] = "ILHA"; resp += Config.Tr("Sua ilha particular! ", "Your private island! "); acted = true; }

    if (Match(in, {"constru", "faca", "crie"}) && Match(in, {"piramide"})) { cmds["CONSTRUCAO"] = "PIRAMIDE"; resp += Config.Tr("Zuba Farao! ", "Pharaoh Zuba! "); acted = true; }
    if (Match(in, {"torre", "castelo"})) { cmds["CONSTRUCAO"] = "TORRE"; resp += Config.Tr("Torre gigante. ", "Giant tower. "); acted = true; }
    if (Match(in, {"ponte"})) { cmds["CONSTRUCAO"] = "PONTE"; resp += Config.Tr("Ponte segura. ", "Safe bridge. "); acted = true; }
    if (Match(in, {"muralha", "muro"})) { cmds["CONSTRUCAO"] = "MURALHA"; resp += Config.Tr("A grande muralha! ", "The great wall! "); acted = true; }
    if (Match(in, {"casa", "cabana"})) { cmds["CONSTRUCAO"] = "CASA"; resp += Config.Tr("Lar doce lar construido. ", "Home sweet home built. "); acted = true; }
    if (Match(in, {"templo", "santuario"})) { cmds["CONSTRUCAO"] = "TEMPLO"; resp += Config.Tr("Templo dos Deuses antigo! ", "Ancient Temple of Gods! "); acted = true; }
    if (Match(in, {"monolito", "pilar"})) { cmds["CONSTRUCAO"] = "MONOLITO"; resp += Config.Tr("Monolito misterioso erguido. ", "Mysterious monolith raised. "); acted = true; }

    if (Match(in, {"espada", "lamina"})) { cmds["ITEM"] = "ESPADA"; resp += Config.Tr("Espada em maos! ", "Sword in hand! "); acted = true; }
    if (Match(in, {"arco"})) { cmds["ITEM"] = "ARCO"; resp += Config.Tr("Arco em maos. ", "Bow in hand. "); acted = true; }
    if (Match(in, {"picareta"})) { cmds["ITEM"] = "PICARETA"; resp += Config.Tr("Bora minerar! ", "Let's mine! "); acted = true; }
    if (Match(in, {"varinha", "cajado"})) { cmds["ITEM"] = "VARINHA"; resp += Config.Tr("Magia no ar! ", "Magic in the air! "); acted = true; }
    if (Match(in, {"lanterna", "tocha"})) { cmds["ITEM"] = "LANTERNA"; resp += Config.Tr("Haja luz! ", "Let there be light! "); acted = true; }
    if (Match(in, {"escudo de mao", "broquel"})) { cmds["ITEM"] = "ESCUDO_ITEM"; resp += Config.Tr("Defesa empunhada! ", "Defense wielded! "); acted = true; }
    if (Match(in, {"limpa", "guarde", "solta"})) { cmds["ITEM"] = "NADA"; resp += Config.Tr("Itens guardados. ", "Items stored. "); acted = true; }

    if (Match(in, {"meteoro", "asteroide"})) { cmds["ATAQUE"] = "METEOROS"; resp += Config.Tr("Pedras flamejantes! ", "Flaming rocks! "); acted = true; }
    if (Match(in, {"buraco negro", "singularidade"})) { cmds["ATAQUE"] = "BURACO_NEGRO"; resp += Config.Tr("Engolindo tudo! ", "Swallowing everything! "); acted = true; }
    if (Match(in, {"choque", "explosao"})) { cmds["ATAQUE"] = "ONDA_CHOQUE"; resp += Config.Tr("BOOM! ", "BOOM! "); acted = true; }
    if (Match(in, {"orbital", "laser do ceu", "extermina"})) { cmds["ATAQUE"] = "ORBITAL"; resp += Config.Tr("Aviso: Raio Orbital Imminente! ", "Warning: Orbital Strike Imminent! "); acted = true; }

    if (Match(in, {"passaro", "ave"})) { cmds["INVOCAR"] = "PASSARO"; resp += Config.Tr("Piu piu brilhante! ", "Shiny tweet! "); acted = true; }
    if (Match(in, {"golem", "gigante de pedra"})) { cmds["INVOCAR"] = "GOLEM"; resp += Config.Tr("Golem invocado. ", "Golem summoned. "); acted = true; }
    if (Match(in, {"dragao", "monstro voador"})) { cmds["INVOCAR"] = "DRAGAO"; resp += Config.Tr("ROAAAAR! O Dragao chegou! ", "ROAAAAR! The Dragon has arrived! "); acted = true; }
    if (Match(in, {"disco voador", "ovni", "ufo"})) { cmds["INVOCAR"] = "UFO"; resp += Config.Tr("Eles estao entre nos... ", "They are among us... "); acted = true; }

    if (Match(in, {"reset", "apaga tudo"})) { cmds["HACK"] = "RESET"; resp += Config.Tr("Mundo limpo! ", "World reset! "); acted = true; }

    if (!acted) {
        if (Match(in, {"ola", "oi", "bom dia"})) resp = Config.Tr("Oieee! Eu sou o Zuba! O que vamos fazer?", "Hiii! I'm Zuba! What are we doing?");
        else resp = Config.Tr("Nao entendi direito. Pode repetir?", "Didn't quite get that. Can you repeat?");
    }
    return {resp, cmds}; 
}

// ==============================================================================
// 5. ZUBA (COMPANHEIRO MÁGICO 3D) 
// ==============================================================================
struct MensagemChat { std::string remetente; std::string texto; Color cor; };

std::vector<std::string> QuebrarTextoEx(const std::string& texto, int larguraMax, Font font, int fontSize) {
    std::vector<std::string> linhas; std::string palavraAtual = ""; std::string linhaAtual = "";
    for (char c : texto) {
        if (c == ' ' || c == '\n') {
            if (MeasureTextEx(font, (linhaAtual + palavraAtual + " ").c_str(), fontSize, 1).x < larguraMax) linhaAtual += palavraAtual + " ";
            else { if (!linhaAtual.empty()) linhas.push_back(linhaAtual); linhaAtual = palavraAtual + " "; }
            palavraAtual = ""; if (c == '\n' && !linhaAtual.empty()) { linhas.push_back(linhaAtual); linhaAtual = ""; }
        } else palavraAtual += c;
    }
    if (!palavraAtual.empty()) {
        if (MeasureTextEx(font, (linhaAtual + palavraAtual).c_str(), fontSize, 1).x < larguraMax) { linhaAtual += palavraAtual; linhas.push_back(linhaAtual); } 
        else { if (!linhaAtual.empty()) linhas.push_back(linhaAtual); linhas.push_back(palavraAtual); }
    } else if (!linhaAtual.empty()) linhas.push_back(linhaAtual);
    return linhas;
}

enum EstadoAura { ORBITAR, PARADA, MOVER_ALVO };

class CompanheiroMagico {
public:
    Vector3 posicao; Vector3 alvoMovimento; Vector3 alvoLaser;
    std::string nome; float tempoOrbita; EstadoAura estadoFisico;
    std::string formaAtual; float escalaAtual; float escalaAlvo;
    float distanciaOrbitaAtual; float distanciaOrbitaAlvo;
    float tempoCinematico; float tempoLaser; std::string itemEquipado; 
    std::vector<MensagemChat> historico; std::mutex chatMutex;
    std::atomic<bool> pensando; std::atomic<bool> forcadoScroll; 

    Model modelo3D; Texture2D texturaAvatar; 
    ModelAnimation* animacoes; int animCount; 
    float animFrameCounter; int ultimoAnimIndex;
    bool temModelo; bool temTextura; float anguloRotacao;

    float zubaPuloTimer = 0.0f; float zubaGiroVelocidade = 0.0f; bool zubaDancando = false; bool zubaAndando = false;

    CompanheiroMagico() : posicao({0, 15, 0}), alvoMovimento({0,0,0}), alvoLaser({0,0,0}), nome("Zuba"), tempoOrbita(0.0f), estadoFisico(ORBITAR), 
                          formaAtual("ESFERA"), escalaAtual(1.0f), escalaAlvo(1.0f), distanciaOrbitaAtual(4.0f), distanciaOrbitaAlvo(4.0f),
                          tempoCinematico(0.0f), tempoLaser(0.0f), itemEquipado(""), pensando(false), forcadoScroll(false),
                          animacoes(nullptr), animCount(0), animFrameCounter(0.0f), ultimoAnimIndex(0), temModelo(false), temTextura(false), anguloRotacao(0.0f), zubaAndando(false) {}
    
    void Carregar(const char* pathModelo, const char* pathTextura, Shader shaderLuz) {
        if (FileExists(pathModelo)) {
            modelo3D = LoadModel(pathModelo); animacoes = LoadModelAnimations(pathModelo, &animCount); temModelo = true;
            for (int i=0; i<modelo3D.materialCount; i++) modelo3D.materials[i].shader = shaderLuz;
            if (FileExists(pathTextura)) { texturaAvatar = LoadTexture(pathTextura); modelo3D.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texturaAvatar; temTextura = true; }
        }
    }

    void Unload() {
        if (temModelo) {
            if (temTextura) UnloadTexture(texturaAvatar); UnloadModel(modelo3D);
            if (animCount > 0) UnloadModelAnimations(animacoes, animCount);
        }
    }

    void Iniciar() {
        historico.clear(); formaAtual = "ESFERA"; escalaAtual = 1.0f; escalaAlvo = 1.0f; distanciaOrbitaAtual = 4.0f; distanciaOrbitaAlvo = 4.0f; itemEquipado = ""; estadoFisico = ORBITAR; zubaPuloTimer = 0.0f; zubaGiroVelocidade = 0.0f; zubaDancando = false;
        AdicionarMensagem(nome, Config.Tr("Sistemas V24! Jogador Fisico Integrado e 3a Pessoa Pronta!", "V24 Systems! Physical Player Integrated and 3rd Person Ready!"), GOLD);
    }

    void AtualizarFisica(Vector3 posJogador, Vector3 forwardJogador, float dt) {
        float trueDelta = dt * timeScale; 
        tempoOrbita += trueDelta * 2.0f; 
        escalaAtual = Lerp(escalaAtual, escalaAlvo, dt * 2.0f); 
        distanciaOrbitaAtual = Lerp(distanciaOrbitaAtual, distanciaOrbitaAlvo, dt * 1.5f);
        if (tempoCinematico > 0) tempoCinematico -= dt; else distanciaOrbitaAlvo = 4.0f; 

        bool isMoving = false; float velZuba = 0.0f;
        float anguloAlvo = anguloRotacao;

        if (estadoFisico == ORBITAR) { 
            Vector3 diff = Vector3Subtract(posJogador, posicao); diff.y = 0; float dist = Vector3Length(diff);
            float distMinima = zubaAndando ? 2.0f : 3.5f; 
            
            if (dist > distMinima) { 
                diff = Vector3Normalize(diff); velZuba = (dist > 12.0f) ? 25.0f : 14.0f; 
                posicao.x += diff.x * velZuba * trueDelta; posicao.z += diff.z * velZuba * trueDelta;
                anguloAlvo = atan2(diff.x, diff.z) * RAD2DEG;
                isMoving = true; zubaAndando = true;
            } else {
                Vector3 dirOlhar = Vector3Subtract(posJogador, posicao); 
                anguloAlvo = atan2(dirOlhar.x, dirOlhar.z) * RAD2DEG;
                zubaAndando = false;
            }
        } 
        else if (estadoFisico == PARADA) { 
            Vector3 dirOlhar = Vector3Subtract(posJogador, posicao); anguloAlvo = atan2(dirOlhar.x, dirOlhar.z) * RAD2DEG;
        } 
        else if (estadoFisico == MOVER_ALVO) { 
            Vector3 diff = Vector3Subtract(alvoMovimento, posicao); diff.y = 0; float dist = Vector3Length(diff);
            if (dist > 1.0f) {
                diff = Vector3Normalize(diff); velZuba = 18.0f; 
                posicao.x += diff.x * velZuba * trueDelta; posicao.z += diff.z * velZuba * trueDelta;
                anguloAlvo = atan2(diff.x, diff.z) * RAD2DEG;
                isMoving = true;
            }
        }

        if (zubaGiroVelocidade == 0 && !zubaDancando) {
            float deltaAngulo = anguloAlvo - anguloRotacao;
            while (deltaAngulo > 180.0f) deltaAngulo -= 360.0f;
            while (deltaAngulo < -180.0f) deltaAngulo += 360.0f;
            anguloRotacao += deltaAngulo * trueDelta * 10.0f;
        }

        float puloY = 0.0f;
        if (zubaPuloTimer > 0) { zubaPuloTimer -= trueDelta * 2.0f; puloY = sin(zubaPuloTimer * PI) * 5.0f * escalaAtual; }
        if (zubaGiroVelocidade > 0) { anguloRotacao += zubaGiroVelocidade * trueDelta; zubaGiroVelocidade -= trueDelta * 100.0f; if (zubaGiroVelocidade < 0) zubaGiroVelocidade = 0; }
        if (zubaDancando) { anguloRotacao += sin(tempoOrbita * 5.0f) * 15.0f; puloY += abs(sin(tempoOrbita * 10.0f)) * 1.5f; }

        mundoMutex.lock(); float chaoAura = LerAlturaMundo(posicao.x, posicao.z); mundoMutex.unlock();
        float zubaYOffset = temModelo ? 0.0f : (0.2f * escalaAtual); 
        posicao.y = Lerp(posicao.y, chaoAura + zubaYOffset + puloY, trueDelta * 18.0f); 

        if (temModelo && animCount > 0) {
            int currentAnimIndex = 0;
            if (zubaPuloTimer > 0) currentAnimIndex = (animCount > 3) ? 3 : 0; 
            else if (isMoving) {
                if (velZuba > 15.0f) currentAnimIndex = (animCount > 2) ? 2 : ((animCount > 1) ? 1 : 0);
                else currentAnimIndex = (animCount > 1) ? 1 : 0;
            } else if (zubaDancando) currentAnimIndex = (animCount > 2) ? 2 : 0; 
            else currentAnimIndex = 0;

            if (currentAnimIndex != ultimoAnimIndex) { animFrameCounter = 0; ultimoAnimIndex = currentAnimIndex; }

            float speedMult = 1.0f;
            if (currentAnimIndex == 1) speedMult = (velZuba > 0.0f) ? (velZuba * 0.1f) : 1.5f; 
            else if (currentAnimIndex == 2) speedMult = (velZuba > 0.0f) ? (velZuba * 0.12f) : 2.0f; 
            else if (currentAnimIndex == 3) speedMult = 1.2f; 
            
            animFrameCounter += dt * 60.0f * speedMult;
            if (animFrameCounter >= animacoes[currentAnimIndex].frameCount) animFrameCounter = 0;
            UpdateModelAnimation(modelo3D, animacoes[currentAnimIndex], (int)animFrameCounter);
        }

        if (tempoLaser > 0) { 
            tempoLaser -= trueDelta; alvoLaser = Vector3Add(posJogador, Vector3Scale(forwardJogador, 30.0f)); 
            mundoMutex.lock(); alvoLaser.y = LerAlturaMundo(alvoLaser.x, alvoLaser.z) + 1.0f; mundoMutex.unlock(); 
            EmitirParticulas(alvoLaser, ORANGE, 5, 5.0f); 
        }

        if (!filaConstrucao.empty()) {
            timerConstrucao += dt;
            if (timerConstrucao > 0.02f) { 
                int vel = 1 + (filaConstrucao.size() / 20); 
                mundoMutex.lock();
                for (int i=0; i<vel && !filaConstrucao.empty(); i++) {
                    BlocoFixo b = filaConstrucao.front(); construcoesFixas.push_back(b);
                    EmitirParticulas(b.pos, GOLD, 3, 2.0f); filaConstrucao.pop_front();
                }
                mundoMutex.unlock(); timerConstrucao = 0.0f;
            }
        }
    }

    void Desenhar3D() {
        float r = 0.6f * escalaAtual; 
        
        if (!camuflagemAtiva && formaAtual == "ESFERA" && temModelo) {
            mundoMutex.lock(); float alturaSombra = LerAlturaMundo(posicao.x, posicao.z) + 0.05f; mundoMutex.unlock();
            DrawModelEx(modelo3D, {posicao.x, alturaSombra, posicao.z}, {0.0f, 1.0f, 0.0f}, anguloRotacao, {escalaAtual, 0.01f, escalaAtual}, Fade(BLACK, 0.6f));
        }

        Color coreColor = modoFesta ? Color{(unsigned char)(rand()%255), (unsigned char)(rand()%255), (unsigned char)(rand()%255), 200} : Color{255, 215, 0, 200};

        if (camuflagemAtiva) DrawSphereWires(posicao, r, 16, 16, Fade(BLUE, 0.2f)); 
        else {
            if (formaAtual == "CUBO") DrawCube({posicao.x, posicao.y + r, posicao.z}, r*2.2f, r*2.2f, r*2.2f, coreColor);
            else if (formaAtual == "PIRAMIDE") DrawCylinder({posicao.x, posicao.y, posicao.z}, 0.0f, r*1.8f, r*3.0f, 4, coreColor);
            else if (formaAtual == "CILINDRO") DrawCylinder({posicao.x, posicao.y + r, posicao.z}, r*1.5f, r*1.5f, r*3.0f, 16, coreColor);
            else if (formaAtual == "TORUS") { DrawCylinder({posicao.x, posicao.y + r, posicao.z}, r*1.5f, r*1.5f, r*0.5f, 16, coreColor); DrawCylinderWires({posicao.x, posicao.y + r, posicao.z}, r*1.5f, r*1.5f, r*0.5f, 16, ORANGE); }
            else if (formaAtual == "ACHATADO") { DrawSphereEx({posicao.x, posicao.y + r*0.3f, posicao.z}, r*1.5f, 16, 16, coreColor); } 
            else { 
                if (temModelo) {
                    DrawModelEx(modelo3D, posicao, {0.0f, 1.0f, 0.0f}, anguloRotacao, {escalaAtual, escalaAtual, escalaAtual}, modoFesta ? coreColor : WHITE);
                } else {
                    DrawSphere({posicao.x, posicao.y + r, posicao.z}, r, coreColor); DrawSphereWires({posicao.x, posicao.y + r, posicao.z}, r*1.3f, 16, 16, ORANGE); 
                }
            }
        }

        if (!camuflagemAtiva) {
            Vector3 handPos = {posicao.x + r*1.5f, posicao.y + r*1.5f, posicao.z};
            if (itemEquipado == "ESPADA") { DrawCube(handPos, 0.2f, r*3.0f, 0.2f, LIGHTGRAY); DrawCube({handPos.x, handPos.y - r*1.0f, handPos.z}, 0.8f, 0.2f, 0.2f, BROWN); } 
            else if (itemEquipado == "PICARETA") { DrawCube(handPos, 0.2f, r*3.0f, 0.2f, BROWN); DrawCube({handPos.x, handPos.y + r*1.0f, handPos.z}, r*2.0f, 0.2f, 0.2f, GRAY); } 
            else if (itemEquipado == "ARCO") { DrawCylinderEx({handPos.x, handPos.y - r*1.0f, handPos.z}, {handPos.x, handPos.y + r*1.0f, handPos.z}, 0.1f, 0.1f, 8, BROWN); DrawLine3D({handPos.x, handPos.y - r*1.0f, handPos.z}, {handPos.x, handPos.y + r*1.0f, handPos.z}, WHITE); }
            else if (itemEquipado == "VARINHA") { DrawCylinderEx({handPos.x, handPos.y - r*1.0f, handPos.z}, {handPos.x, handPos.y + r*1.0f, handPos.z}, 0.1f, 0.1f, 8, BLACK); DrawSphere({handPos.x, handPos.y + r*1.2f, handPos.z}, 0.3f, PURPLE); }
            else if (itemEquipado == "LANTERNA") { DrawCube(handPos, 0.5f, 0.8f, 0.5f, DARKGRAY); DrawCube({handPos.x, handPos.y, handPos.z}, 0.4f, 0.6f, 0.4f, YELLOW); }
            else if (itemEquipado == "ESCUDO_ITEM") { DrawCylinderEx({handPos.x, handPos.y, handPos.z}, {handPos.x+0.1f, handPos.y, handPos.z}, 1.5f, 1.5f, 16, GRAY); DrawCylinderWiresEx({handPos.x, handPos.y, handPos.z}, {handPos.x+0.1f, handPos.y, handPos.z}, 1.5f, 1.5f, 16, WHITE); }
        }
        if (tempoLaser > 0) { DrawCylinderEx({posicao.x, posicao.y + 1.0f, posicao.z}, alvoLaser, 0.2f * escalaAtual, 0.2f * escalaAtual, 8, Fade(RED, 0.8f)); DrawSphere(alvoLaser, 1.5f * escalaAtual, Fade(ORANGE, 0.9f)); }
    }

    void AdicionarMensagem(std::string remetente, std::string texto, Color cor) { std::lock_guard<std::mutex> lock(chatMutex); historico.push_back({remetente, texto, cor}); forcadoScroll = true; }

    void ProcessarConversa(std::string inputJogador, float alturaAtual, Vector3 posJogador, Vector3 forwardJogador) {
        AdicionarMensagem(Config.Tr("Voce", "You"), inputJogador, LIGHTGRAY); pensando = true;
        
        std::thread([this, inputJogador, alturaAtual, posJogador, forwardJogador]() {
            std::this_thread::sleep_for(800ms); 
            RespostaIA ia = ProcessarIAInterna(inputJogador, alturaAtual); auto& cmds = ia.comandos;
            
            if (cmds["FORMA"] != "" || cmds["TAMANHO"] != "" || cmds["TERRENO"] != "" || cmds["CONSTRUCAO"] != "" || cmds["ACAO"] == "ATIRAR" || cmds["ATAQUE"] != "") { tempoCinematico = 6.0f; distanciaOrbitaAlvo = 8.0f; }
            
            if (cmds["FORMA"] != "") formaAtual = cmds["FORMA"];
            if (cmds["TAMANHO"] != "") {
                if (cmds["TAMANHO"] == "PEQUENO") escalaAlvo = 0.5f; else if (cmds["TAMANHO"] == "NORMAL") escalaAlvo = 1.0f; else if (cmds["TAMANHO"] == "GRANDE") escalaAlvo = 1.5f; else if (cmds["TAMANHO"] == "GIGANTE") escalaAlvo = 3.5f;
            }
            if (cmds["ITEM"] != "") { if (cmds["ITEM"] == "NADA") itemEquipado = ""; else itemEquipado = cmds["ITEM"]; }
            
            if (cmds["ACAO"] == "PARAR") estadoFisico = PARADA; else if (cmds["ACAO"] == "SEGUIR") estadoFisico = ORBITAR;
            else if (cmds["ACAO"] == "IR_FRENTE") { estadoFisico = MOVER_ALVO; alvoMovimento = Vector3Add(posJogador, Vector3Scale(forwardJogador, 30.0f)); mundoMutex.lock(); alvoMovimento.y = LerAlturaMundo(alvoMovimento.x, alvoMovimento.z) + 5.0f; mundoMutex.unlock(); } 
            else if (cmds["ACAO"] == "ATIRAR") tempoLaser = 2.5f; 
            else if (cmds["ACAO"] == "PULAR") zubaPuloTimer = 1.0f;
            else if (cmds["ACAO"] == "GIRAR") zubaGiroVelocidade = 1000.0f;
            else if (cmds["ACAO"] == "DANCAR") zubaDancando = !zubaDancando;
            
            if (cmds["CLIMA"] != "") {
                if (cmds["CLIMA"] == "TEMPESTADE") climaGlobal = 2; else if (cmds["CLIMA"] == "CHUVA") climaGlobal = 1; else if (cmds["CLIMA"] == "LIMPO") climaGlobal = 0; else if (cmds["CLIMA"] == "NEVOEIRO") climaGlobal = 3; else if (cmds["CLIMA"] == "AURORA") climaGlobal = 4; else if (cmds["CLIMA"] == "ECLIPSE") climaGlobal = 5;
            }
            if (cmds["BIOMA"] != "") { 
                biomaAlvo = (cmds["BIOMA"]=="DESERTO")?1 : (cmds["BIOMA"]=="MAGMA")?2 : (cmds["BIOMA"]=="GELO")?3 : (cmds["BIOMA"]=="CYBERPUNK")?4 : (cmds["BIOMA"]=="ALIEN")?5 : (cmds["BIOMA"]=="DOCE")?6 : 0; 
                if (biomaAtual != biomaAlvo) biomaBlend = 0.0f; 
            }
            if (cmds["TEMPO"] != "") {
                if (cmds["TEMPO"] == "CONGELAR") timeScale = 0.0f; else if (cmds["TEMPO"] == "ACELERAR") timeScale = 5.0f; else if (cmds["TEMPO"] == "LENTO") timeScale = 0.2f; else if (cmds["TEMPO"] == "NORMAL") timeScale = 1.0f; 
                else if (cmds["TEMPO"] == "GRAVIDADE") gravidadeBase = 5.0f; else if (cmds["TEMPO"] == "GRAVIDADE_NORMAL") gravidadeBase = 20.0f;
            }
            
            if (cmds["ATAQUE"] == "BURACO_NEGRO") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 40.0f)); mundoMutex.lock(); entidadesGlobais.push_back({1, {dest.x, dest.y+20.0f, dest.z}, {0,0,0}, 10.0f, 10.0f}); mundoMutex.unlock(); cameraShake = 2.0f; } 
            else if (cmds["ATAQUE"] == "METEOROS") { mundoMutex.lock(); for (int i=0; i<5; i++) entidadesGlobais.push_back({2, {posJogador.x + (rand()%60 - 30), posJogador.y + 80.0f + (i*10), posJogador.z + (rand()%60 - 30)}, {0, -50.0f, 0}, 3.0f, 5.0f}); mundoMutex.unlock(); } 
            else if (cmds["ATAQUE"] == "ONDA_CHOQUE") { mundoMutex.lock(); entidadesGlobais.push_back({3, posJogador, {0,0,0}, 0.5f, 3.0f}); mundoMutex.unlock(); cameraShake = 1.0f; }
            else if (cmds["ATAQUE"] == "ORBITAL") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 20.0f)); mundoMutex.lock(); entidadesGlobais.push_back({10, dest, {0,0,0}, 5.0f, 4.0f}); mundoMutex.unlock(); cameraShake = 3.0f; }
            
            if (cmds["DEFESA"] != "") {
                if (cmds["DEFESA"] == "ESCUDO_ON") escudoAtivo = true; else if (cmds["DEFESA"] == "ESCUDO_OFF") escudoAtivo = false;
                else if (cmds["DEFESA"] == "CAMUFLAGEM_ON") camuflagemAtiva = true; else if (cmds["DEFESA"] == "CAMUFLAGEM_OFF") camuflagemAtiva = false;
                else if (cmds["DEFESA"] == "DOMO") { Vector3 dest = posJogador; mundoMutex.lock(); entidadesGlobais.push_back({11, dest, {0,0,0}, 20.0f, 30.0f}); mundoMutex.unlock(); }
                else if (cmds["DEFESA"] == "ANEL_FOGO") { Vector3 dest = posJogador; mundoMutex.lock(); entidadesGlobais.push_back({12, dest, {0,0,0}, 10.0f, 30.0f}); mundoMutex.unlock(); }
            }
            
            if (cmds["INVOCAR"] == "PASSARO") { mundoMutex.lock(); entidadesGlobais.push_back({4, {posJogador.x, posJogador.y + 10.0f, posJogador.z}, {0,0,0}, 0.5f, 60.0f}); mundoMutex.unlock(); } 
            else if (cmds["INVOCAR"] == "GOLEM") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 10.0f)); mundoMutex.lock(); entidadesGlobais.push_back({6, dest, {0,0,0}, 1.0f, 60.0f}); mundoMutex.unlock(); } 
            else if (cmds["INVOCAR"] == "DRAGAO") { mundoMutex.lock(); entidadesGlobais.push_back({8, {posJogador.x, posJogador.y + 30.0f, posJogador.z}, {0,0,0}, 2.0f, 60.0f}); mundoMutex.unlock(); }
            else if (cmds["INVOCAR"] == "UFO") { Vector3 dest = {posJogador.x, posJogador.y + 20.0f, posJogador.z}; mundoMutex.lock(); entidadesGlobais.push_back({9, dest, {0,0,0}, 5.0f, 60.0f}); mundoMutex.unlock(); }
            
            if (cmds["HACK"] != "") {
                if (cmds["HACK"] == "WIREFRAME_ON") modoWireframe = true; else if (cmds["HACK"] == "WIREFRAME_OFF") modoWireframe = false;
                else if (cmds["HACK"] == "LUZES_ON") luzesAtivas = true; else if (cmds["HACK"] == "LUZES_OFF") luzesAtivas = false;
                else if (cmds["HACK"] == "GODMODE_ON") godModeAtivo = true; else if (cmds["HACK"] == "GODMODE_OFF") godModeAtivo = false;
                else if (cmds["HACK"] == "FESTA_ON") modoFesta = true; else if (cmds["HACK"] == "FESTA_OFF") modoFesta = false;
                else if (cmds["HACK"] == "RESET") { mundoMutex.lock(); modificacoesTerreno.clear(); modificacoesAlvo.clear(); construcoesFixas.clear(); filaConstrucao.clear(); entidadesGlobais.clear(); particulas.clear(); biomaAlvo=0; biomaBlend=0.0f; climaGlobal=0; timeScale=1.0f; gravidadeBase=20.0f; escudoAtivo=false; camuflagemAtiva=false; godModeAtivo=false; modoWireframe=false; modoFesta=false; mundoMutex.unlock(); cameraShake = 1.5f; }
            }
            
            if (cmds["TERRENO"] != "") {
                Vector3 alvo = Vector3Add(posJogador, Vector3Scale(forwardJogador, 20.0f)); int cx = round(alvo.x); int cz = round(alvo.z);
                std::lock_guard<std::mutex> lock(mundoMutex);
                if (cmds["TERRENO"] == "MONTANHA") { for(int dx=-12; dx<=12; dx++) for(int dz=-12; dz<=12; dz++) if(sqrt(dx*dx + dz*dz) < 12) modificacoesAlvo[{cx+dx, cz+dz}] = modificacoesTerreno[{cx+dx, cz+dz}] + (12 - sqrt(dx*dx + dz*dz)) * 1.5f; } 
                else if (cmds["TERRENO"] == "ABISMO") { for(int dx=-8; dx<=8; dx++) for(int dz=-8; dz<=8; dz++) if(sqrt(dx*dx + dz*dz) < 8) modificacoesAlvo[{cx+dx, cz+dz}] = modificacoesTerreno[{cx+dx, cz+dz}] - (8 - sqrt(dx*dx + dz*dz)) * 2.0f; } 
                else if (cmds["TERRENO"] == "PLANICIE") { float hM = GerarAlturaTerrenoBase(cx, cz); for(int dx=-10; dx<=10; dx++) for(int dz=-10; dz<=10; dz++) if (sqrt(dx*dx + dz*dz) < 10) modificacoesAlvo[{cx+dx, cz+dz}] = hM - GerarAlturaTerrenoBase(cx+dx, cz+dz); } 
                else if (cmds["TERRENO"] == "LAGO") { for(int dx=-8; dx<=8; dx++) for(int dz=-8; dz<=8; dz++) if(sqrt(dx*dx + dz*dz) < 8) { modificacoesAlvo[{cx+dx, cz+dz}] -= 4.0f; filaConstrucao.push_back({{ (float)cx+dx, GerarAlturaTerrenoBase(cx, cz)-1.0f, (float)cz+dz }, BLUE}); } }
                else if (cmds["TERRENO"] == "CANION") { for(int d=-15; d<=15; d++) for(int w=-3; w<=3; w++) modificacoesAlvo[{cx+d, cz+w}] -= 15.0f; }
                else if (cmds["TERRENO"] == "ILHA") { for(int dx=-20; dx<=20; dx++) for(int dz=-20; dz<=20; dz++) { float dist = sqrt(dx*dx + dz*dz); if(dist < 10) modificacoesAlvo[{cx+dx, cz+dz}] += 5.0f; else if(dist < 20) modificacoesAlvo[{cx+dx, cz+dz}] -= 5.0f; } }
                cameraShake = 2.0f; 
            }

            if (cmds["CONSTRUCAO"] != "") {
                Vector3 alvo = Vector3Add(posJogador, Vector3Scale(forwardJogador, 20.0f)); int cx = round(alvo.x); int cz = round(alvo.z);
                std::lock_guard<std::mutex> lock(mundoMutex); float bA = LerAlturaMundo(cx, cz);
                if (cmds["CONSTRUCAO"] == "PIRAMIDE") { for (int y=0; y<8; y++) for (int x=-8+y; x<=8-y; x++) for (int z=-8+y; z<=8-y; z++) filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, GOLD}); } 
                else if (cmds["CONSTRUCAO"] == "TORRE") { for (int y=0; y<15; y++) for (int x=-2; x<=2; x++) for (int z=-2; z<=2; z++) if (x==-2||x==2||z==-2||z==2||y==14) filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, DARKGRAY}); } 
                else if (cmds["CONSTRUCAO"] == "PONTE") { for (int d=-10; d<=10; d++) for (int w=-1; w<=1; w++) filaConstrucao.push_back({{ (float)cx+d, bA, (float)cz+w }, BROWN}); } 
                else if (cmds["CONSTRUCAO"] == "MURALHA") { for (int d=-15; d<=15; d++) for (int y=0; y<5; y++) filaConstrucao.push_back({{ (float)cx+d, bA+y, (float)cz }, LIGHTGRAY}); } 
                else if (cmds["CONSTRUCAO"] == "CASA") { for (int y=0; y<4; y++) for (int x=-3; x<=3; x++) for (int z=-3; z<=3; z++) if (y==3||x==-3||x==3||z==-3||z==3) { if (y<2&&z==3&&x>=-1&&x<=1) continue; filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, ORANGE}); } }
                else if (cmds["CONSTRUCAO"] == "TEMPLO") { for(int y=0;y<6;y++) for(int x=-5;x<=5;x+=5) for(int z=-5;z<=5;z+=5) filaConstrucao.push_back({{(float)cx+x, bA+y, (float)cz+z}, WHITE}); for(int x=-6;x<=6;x++) for(int z=-6;z<=6;z++) filaConstrucao.push_back({{(float)cx+x, bA+6, (float)cz+z}, GOLD}); }
                else if (cmds["CONSTRUCAO"] == "MONOLITO") { for(int y=0;y<20;y++) for(int x=-1;x<=1;x++) for(int z=-1;z<=1;z++) filaConstrucao.push_back({{(float)cx+x, bA+y, (float)cz+z}, BLACK}); }
            }

            AdicionarMensagem(nome, ia.texto, GOLD); pensando = false;
        }).detach();
    }
};

// ==============================================================================
// 6. MENUS AVANÇADOS (SETTINGS & CREDITS)
// ==============================================================================
enum GameState { MENU, PLAYING, SETTINGS, CREDITS }; 

void DrawSettingsMenu(int larguraTela, int alturaTela, int& menuSelecionado, Font font, GameState& estadoAtual) {
    auto DrawTextCenter = [&](const char* txt, float y, float size, Color c) { float w = MeasureTextEx(font, txt, size, 1).x; DrawTextEx(font, txt, {larguraTela/2.0f - w/2.0f, y}, size, 1, c); };
    
    DrawTextCenter(Config.Tr("CONFIGURACOES", "SETTINGS"), 100, 50, GOLD);
    
    const char* opcoes[] = { 
        TextFormat("%s: %s", Config.Tr("Musica", "Music"), Config.musicOn ? "ON" : "OFF"), 
        TextFormat("%s: %s", Config.Tr("Efeitos", "SFX"), Config.sfxOn ? "ON" : "OFF"), 
        TextFormat("%s: %s", Config.Tr("Idioma", "Language"), Config.language == 0 ? "PT-BR" : "ENGLISH"),
        Config.Tr("VOLTAR", "BACK")
    };
    
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) { menuSelecionado--; if(menuSelecionado < 0) menuSelecionado = 3; Audio.PlayUIHover(); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { menuSelecionado++; if(menuSelecionado > 3) menuSelecionado = 0; Audio.PlayUIHover(); }
    
    bool toggle = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT);
    if (toggle) Audio.PlayUISelect();

    for(int i = 0; i < 4; i++) {
        Rectangle btn = {(float)larguraTela/2 - 200, (float)alturaTela/2 - 100 + i*70, 400, 50}; bool hover = (i == menuSelecionado); 
        DrawRectangleRec(btn, hover ? Fade(LIGHTGRAY, 0.8f) : Fade(DARKGRAY, 0.5f)); DrawRectangleLinesEx(btn, 2, hover ? WHITE : GRAY);
        float tw = MeasureTextEx(font, opcoes[i], 20, 1).x; DrawTextEx(font, opcoes[i], {btn.x + 200 - tw/2, btn.y + 15}, 20, 1, hover ? BLACK : WHITE);
        
        if (hover && toggle) {
            if (i == 0) Config.musicOn = !Config.musicOn;
            else if (i == 1) Config.sfxOn = !Config.sfxOn;
            else if (i == 2) Config.language = (Config.language == 0) ? 1 : 0;
            else if (i == 3 && IsKeyPressed(KEY_ENTER)) estadoAtual = MENU;
        }
    }
}

float scrollCreditos = 0.0f;
void DrawCreditsMenu(int larguraTela, int alturaTela, Font font, GameState& estadoAtual, float dt) {
    auto DrawTextCenter = [&](const char* txt, float y, float size, Color c) { float w = MeasureTextEx(font, txt, size, 1).x; DrawTextEx(font, txt, {larguraTela/2.0f - w/2.0f, y}, size, 1, c); };
    
    scrollCreditos += dt * 60.0f; 
    float startY = alturaTela - scrollCreditos + 300.0f;

    DrawTextCenter("SUPERGEMINI CREATOR ENGINE", startY, 50, GOLD);
    DrawTextCenter("V24.0 - THE 3RD PERSON & PLAYER UPDATE", startY + 60, 20, LIGHTGRAY);
    
    DrawTextCenter(Config.Tr("PRODUTOR EXECUTIVO", "EXECUTIVE PRODUCER"), startY + 200, 30, WHITE);
    DrawTextCenter("IGOR BETTARELLO XAVIER (OMEGARED)", startY + 240, 40, ORANGE);
    
    DrawTextCenter("CODE MASTER / AI ARCHITECT", startY + 360, 30, WHITE);
    DrawTextCenter("GEMINI GOD MODE", startY + 400, 40, SKYBLUE);
    
    DrawTextCenter(Config.Tr("OBRIGADO POR JOGAR!", "THANKS FOR PLAYING!"), startY + 600, 30, GOLD);

    DrawRectangle(0, 0, larguraTela, 80, Fade(BLACK, 0.8f)); 
    DrawRectangle(0, alturaTela-80, larguraTela, 80, Fade(BLACK, 0.8f)); 
    DrawTextCenter(Config.Tr("Pressione ESC para Voltar", "Press ESC to Go Back"), alturaTela - 50, 20, GRAY);
    
    if (IsKeyPressed(KEY_ESCAPE) || startY + 800 < 0) { estadoAtual = MENU; scrollCreditos = 0.0f; }
}

// ==============================================================================
// 7. MOTOR PRINCIPAL & GAME LOOP
// ==============================================================================
int main(void) {
    InitWindow(1920, 1080, "SUPERGEMINI CREATOR ENGINE - V24.0 THE 3RD PERSON UPDATE");
    ToggleFullscreen(); 
    SetExitKey(0); SetTargetFPS(60);
    Audio.Init(); 
    Forest.Init(); 

    Font fUniv = GetFontDefault();
    if (FileExists("arial.ttf")) {
        int codepoints[512] = {0}; for (int i = 0; i < 512; i++) codepoints[i] = i;
        fUniv = LoadFontEx("arial.ttf", 24, codepoints, 512); SetTextureFilter(fUniv.texture, TEXTURE_FILTER_BILINEAR); 
    }
    auto DrawTextCenter = [&](const char* txt, float y, float size, Color c) { float w = MeasureTextEx(fUniv, txt, size, 1).x; DrawTextEx(fUniv, txt, {GetScreenWidth()/2.0f - w/2.0f, y}, size, 1, c); };

    // CARREGA SHADER GLOBAL PARA TODOS!
    Shader globalShaderLuz = LoadShaderFromMemory(vertexShaderStr, fragmentShaderStr);
    int lightDirLoc = GetShaderLocation(globalShaderLuz, "lightDir");
    int viewPosLoc = GetShaderLocation(globalShaderLuz, "viewPos");
    float ldir[3] = { -0.6f, 1.0f, -0.6f };
    SetShaderValue(globalShaderLuz, lightDirLoc, ldir, SHADER_UNIFORM_VEC3);

    Camera3D camera = { 0 }; camera.position = (Vector3){ 0.0f, 15.0f, -5.0f }; camera.target = (Vector3){ 0.0f, 15.0f, 1.0f }; camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; camera.fovy = 60.0f; camera.projection = CAMERA_PERSPECTIVE;
    float cameraYaw = 0.0f; float cameraPitch = 0.0f;
    bool isThirdPerson = true;

    // INICIALIZA O JOGADOR E O ZUBA!
    PlayerCharacter jogador;
    jogador.Init();
    jogador.Carregar(globalShaderLuz);

    CompanheiroMagico companheiro;
    companheiro.Carregar("zubamodel.glb", "zubamodel.png", globalShaderLuz);

    bool modoChat = false; bool modoDiario = false; bool chatMinimizado = false; int paginaDiario = 0; std::string inputAtual = "";
    GameState estadoAtual = MENU; bool sairDoJogo = false; bool jogoIniciado = false; int menuSelecionado = 0; int chatScrollIndex = 0; 
    int settingsSelecionado = 0;

    std::vector<std::string> historicoJogador;
    int indexHistoricoJogador = 0;

    float velBase = 8.0f; float gravidade = 0.0f; bool noChao = true; float headBobTimer = 0.0f;
    const int RENDER_DIST = 60; 

    while (!WindowShouldClose() && !sairDoJogo) {
        float rawDt = GetFrameTime(); float dt = rawDt * timeScale; 
        int larguraTela = GetScreenWidth(); int alturaTela = GetScreenHeight();

        if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)) ToggleFullscreen();

        if (estadoAtual == MENU) {
            if (IsCursorHidden()) EnableCursor(); 
            
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) { menuSelecionado--; if (menuSelecionado < 0) menuSelecionado = 4; Audio.PlayUIHover(); }
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { menuSelecionado++; if (menuSelecionado > 4) menuSelecionado = 0; Audio.PlayUIHover(); }
            if (IsKeyPressed(KEY_ESCAPE) && jogoIniciado) { estadoAtual = PLAYING; DisableCursor(); }
            
            BeginDrawing(); ClearBackground((Color){ 10, 15, 25, 255 }); 
            DrawTextCenter("SUPERGEMINI CREATOR ENGINE", alturaTela/2 - 250, 50, GOLD);
            DrawTextCenter("V24.0 THE 3RD PERSON & PLAYER UPDATE", alturaTela/2 - 180, 20, LIGHTGRAY);
            DrawTextCenter(Config.Tr("Use Setas/WS e ENTER.", "Use Arrows/WS and ENTER."), alturaTela - 50, 20, GRAY);
            
            const char* opcoes[] = { 
                jogoIniciado ? Config.Tr("CONTINUAR JOGO", "RESUME GAME") : Config.Tr("NOVO JOGO", "NEW GAME"), 
                jogoIniciado ? Config.Tr("REINICIAR MUNDO", "RESTART WORLD") : Config.Tr("CARREGAR JOGO", "LOAD GAME"), 
                Config.Tr("CONFIGURACOES", "SETTINGS"), Config.Tr("CREDITOS", "CREDITS"), Config.Tr("SAIR", "QUIT")
            };
            
            for(int i = 0; i < 5; i++) {
                Rectangle btn = {(float)larguraTela/2 - 150, (float)alturaTela/2 - 100 + i*70, 300, 50}; bool hover = (i == menuSelecionado); 
                DrawRectangleRec(btn, hover ? Fade(LIGHTGRAY, 0.8f) : Fade(DARKGRAY, 0.5f)); DrawRectangleLinesEx(btn, 2, hover ? WHITE : GRAY);
                float tw = MeasureTextEx(fUniv, opcoes[i], 20, 1).x; DrawTextEx(fUniv, opcoes[i], {btn.x + 150 - tw/2, btn.y + 15}, 20, 1, hover ? BLACK : WHITE);
                
                if (hover && IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_ALT)) {
                    Audio.PlayUISelect();
                    if (i == 0) { estadoAtual = PLAYING; if(!jogoIniciado) companheiro.Iniciar(); jogoIniciado = true; DisableCursor(); }
                    else if (i == 1) { 
                        estadoAtual = PLAYING; mundoMutex.lock(); modificacoesTerreno.clear(); modificacoesAlvo.clear(); construcoesFixas.clear(); filaConstrucao.clear(); entidadesGlobais.clear(); particulas.clear();
                        biomaAtual=0; biomaAlvo=0; biomaBlend=1.0f; climaGlobal=0; timeScale=1.0f; gravidadeBase=20.0f; escudoAtivo=false; camuflagemAtiva=false; godModeAtivo=false; modoWireframe=false; modoFesta=false; cameraShake=0.0f;
                        mundoMutex.unlock(); companheiro.Iniciar(); jogador.Init(); chatMinimizado = false; jogoIniciado = true; DisableCursor(); 
                    }
                    else if (i == 2) { estadoAtual = SETTINGS; settingsSelecionado = 0; }
                    else if (i == 3) { estadoAtual = CREDITS; scrollCreditos = 0.0f; } 
                    else if (i == 4) sairDoJogo = true;
                }
            }
            EndDrawing();
        }
        else if (estadoAtual == SETTINGS) { BeginDrawing(); ClearBackground((Color){ 10, 15, 25, 255 }); DrawSettingsMenu(larguraTela, alturaTela, settingsSelecionado, fUniv, estadoAtual); EndDrawing(); }
        else if (estadoAtual == CREDITS) { BeginDrawing(); ClearBackground((Color){ 5, 10, 15, 255 }); DrawCreditsMenu(larguraTela, alturaTela, fUniv, estadoAtual, rawDt); EndDrawing(); }
        
        else if (estadoAtual == PLAYING) {
            
            // TRANSFORMAÇÕES E CÂMERA
            if (!modoChat && !modoDiario) {
                if (IsKeyPressed(KEY_C)) { isThirdPerson = !isThirdPerson; Audio.PlayUISelect(); }
                if (IsKeyPressed(KEY_ONE)) { jogador.formaAtual = 0; EmitirParticulas(jogador.posicao, WHITE, 20, 10.0f); Audio.PlayUISelect(); }
                if (IsKeyPressed(KEY_TWO)) { jogador.formaAtual = 1; EmitirParticulas(jogador.posicao, ORANGE, 20, 10.0f); Audio.PlayUISelect(); }
                if (IsKeyPressed(KEY_THREE)) { jogador.formaAtual = 2; EmitirParticulas(jogador.posicao, GOLD, 20, 10.0f); Audio.PlayUISelect(); }
            }

            if (IsKeyPressed(KEY_TAB) && !modoChat) { modoDiario = !modoDiario; paginaDiario = 0; }

            if (IsKeyPressed(KEY_T) && !modoChat && !modoDiario) { 
                modoChat = true; EnableCursor(); 
                while(GetCharPressed() > 0) {}
                indexHistoricoJogador = historicoJogador.size(); 
            } 
            else if ((IsKeyPressed(KEY_ESCAPE) || (IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_ALT))) && modoChat) {
                if (IsKeyPressed(KEY_ENTER) && !inputAtual.empty()) {
                    mundoMutex.lock(); float aL = LerAlturaMundo(jogador.posicao.x, jogador.posicao.z); mundoMutex.unlock();
                    // Zuba processa a fala e vira na direção do Jogador!
                    Vector3 fwdDir = { sinf(cameraYaw) * cosf(cameraPitch), sinf(cameraPitch), cosf(cameraYaw) * cosf(cameraPitch) };
                    companheiro.ProcessarConversa(inputAtual, aL, jogador.posicao, fwdDir); chatMinimizado = false; 
                    historicoJogador.push_back(inputAtual);
                }
                modoChat = false; inputAtual = ""; DisableCursor(); 
            } else if (IsKeyPressed(KEY_ESCAPE) && !modoChat) {
                if (modoDiario) modoDiario = false; else { estadoAtual = MENU; EnableCursor(); }
            }

            if (modoChat) {
                if (IsKeyPressed(KEY_UP)) { if (indexHistoricoJogador > 0) { indexHistoricoJogador--; inputAtual = historicoJogador[indexHistoricoJogador]; } }
                else if (IsKeyPressed(KEY_DOWN)) {
                    if (indexHistoricoJogador < (int)historicoJogador.size() - 1) { indexHistoricoJogador++; inputAtual = historicoJogador[indexHistoricoJogador]; }
                    else if (indexHistoricoJogador == (int)historicoJogador.size() - 1) { indexHistoricoJogador++; inputAtual = ""; }
                }
                int key = GetCharPressed();
                while (key > 0) { int bS=0; const char* uC=CodepointToUTF8(key, &bS); for(int b=0; b<bS; b++) inputAtual+=uC[b]; key = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE) && inputAtual.length() > 0) inputAtual.pop_back();
                float wheelMove = GetMouseWheelMove(); if (wheelMove != 0) chatScrollIndex -= (int)(wheelMove * 3);
            } else if (!modoDiario) {
                float velA = IsKeyDown(KEY_LEFT_SHIFT) ? velBase * 2.5f : velBase; if (godModeAtivo) velA *= 2.0f; 

                Vector2 mouseDelta = GetMouseDelta(); cameraYaw -= mouseDelta.x * 0.003f; cameraPitch -= mouseDelta.y * 0.003f; 
                if (cameraPitch > 1.5f) cameraPitch = 1.5f; if (cameraPitch < -1.5f) cameraPitch = -1.5f;

                // FÍSICA DO JOGADOR APLICADA AO PLAYER CHARACTER
                Vector3 forward = { sinf(cameraYaw) * cosf(cameraPitch), sinf(cameraPitch), cosf(cameraYaw) * cosf(cameraPitch) };
                Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z}); Vector3 right = { flatForward.z, 0.0f, -flatForward.x };

                Vector3 moveDir = {0, 0, 0};
                if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, flatForward);
                if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, flatForward);
                if (IsKeyDown(KEY_D)) moveDir = Vector3Subtract(moveDir, right);
                if (IsKeyDown(KEY_A)) moveDir = Vector3Add(moveDir, right);

                bool moves = Vector3Length(moveDir) > 0.01f;
                if (moves) moveDir = Vector3Normalize(moveDir);

                if (godModeAtivo) {
                    if (IsKeyDown(KEY_F)) moveDir.y += 1.0f; 
                    if (IsKeyDown(KEY_X)) moveDir.y -= 1.0f; // DESCER MUDA PARA "X"
                }

                float currentSpeed = moves ? velA : 0.0f;

                if (godModeAtivo) {
                    jogador.posicao.x += moveDir.x * currentSpeed * rawDt;
                    jogador.posicao.y += moveDir.y * currentSpeed * rawDt;
                    jogador.posicao.z += moveDir.z * currentSpeed * rawDt;
                    noChao = false;
                } else {
                    jogador.posicao.x += moveDir.x * currentSpeed * rawDt;
                    jogador.posicao.z += moveDir.z * currentSpeed * rawDt;

                    if (moves && noChao) { headBobTimer += rawDt * (velA * 2.0f); } else headBobTimer = 0;
                    
                    mundoMutex.lock(); float alturaChao = LerAlturaMundo(jogador.posicao.x, jogador.posicao.z); mundoMutex.unlock();
                    if (IsKeyPressed(KEY_SPACE) && noChao) { gravidade = 10.0f; noChao = false; }
                    jogador.posicao.y += gravidade * rawDt; 
                    gravidade -= gravidadeBase * rawDt; 
                    
                    if (jogador.posicao.y <= alturaChao) { 
                        jogador.posicao.y = alturaChao; 
                        gravidade = 0.0f; 
                        noChao = true; 
                    }
                }

                // Anima a Raposa (ou esfera/cubo)
                jogador.Atualizar(rawDt, {moveDir.x, 0, moveDir.z}, currentSpeed / velBase, !noChao && !godModeAtivo);

                // CÂMERA INTELIGENTE (Segue o jogador)
                if (isThirdPerson) {
                    float camDist = 7.0f;
                    camera.position = Vector3Subtract(jogador.posicao, Vector3Scale(forward, camDist));
                    camera.position.y += 3.0f; // Offset acima do jogador
                    camera.target = {jogador.posicao.x, jogador.posicao.y + 1.5f, jogador.posicao.z};
                } else {
                    float headOffset = noChao ? sin(headBobTimer) * 0.05f : 0.0f;
                    camera.position = {jogador.posicao.x, jogador.posicao.y + 1.5f + headOffset, jogador.posicao.z};
                    camera.target = Vector3Add(camera.position, forward);
                }
            }

            Vector3 fw = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            companheiro.AtualizarFisica(jogador.posicao, fw, rawDt); 

            mundoMutex.lock();
            for (auto& [key, alvo] : modificacoesAlvo) {
                float atual = modificacoesTerreno[key];
                if (abs(atual - alvo) > 0.1f) modificacoesTerreno[key] = Lerp(atual, alvo, rawDt * 1.5f); else modificacoesTerreno[key] = alvo;
            }
            if (biomaBlend < 1.0f) { biomaBlend += rawDt * 0.5f; if (biomaBlend >= 1.0f) { biomaAtual = biomaAlvo; biomaBlend = 1.0f; } }
            Vector3 cameraShakeOffset = {0,0,0};
            if (cameraShake > 0) { cameraShake -= rawDt; cameraShakeOffset = { (RandFloat()-0.5f)*cameraShake, (RandFloat()-0.5f)*cameraShake, (RandFloat()-0.5f)*cameraShake }; }

            for (auto& ent : entidadesGlobais) {
                if (ent.tipo == 2) { 
                    ent.pos.y += ent.vel.y * dt; float hc = LerAlturaMundo(ent.pos.x, ent.pos.z);
                    EmitirParticulas(ent.pos, ORANGE, 2, 1.0f); 
                    if (ent.pos.y <= hc) { 
                        ent.pos.y = hc; ent.vel.y = 0; ent.tipo = 3; ent.raio = 0.5f; cameraShake = 1.5f; EmitirParticulas(ent.pos, RED, 30, 20.0f); 
                        for(int dx=-4; dx<=4; dx++) for(int dz=-4; dz<=4; dz++) if(sqrt(dx*dx + dz*dz) < 4) modificacoesAlvo[{round(ent.pos.x)+dx, round(ent.pos.z)+dz}] = modificacoesTerreno[{round(ent.pos.x)+dx, round(ent.pos.z)+dz}] - 3.0f; 
                    }
                }
                else if (ent.tipo == 3) { ent.raio += dt * 15.0f; ent.tempoVida -= dt; } 
                else if (ent.tipo == 4) { ent.tempoVida -= dt; ent.pos.x = jogador.posicao.x + sin(ent.tempoVida*3.0f)*5.0f; ent.pos.y = jogador.posicao.y + 4.0f + sin(ent.tempoVida*5.0f); ent.pos.z = jogador.posicao.z + cos(ent.tempoVida*3.0f)*5.0f; } 
                else if (ent.tipo == 8) { ent.tempoVida -= dt; ent.pos.x += sin(ent.tempoVida*2.0f)*0.5f; ent.pos.z += cos(ent.tempoVida*1.5f)*0.5f; } 
                else if (ent.tipo == 9) { ent.tempoVida -= dt; ent.pos.y += sin(ent.tempoVida*4.0f)*0.05f; } 
                else if (ent.tipo == 10) { ent.tempoVida -= dt; EmitirParticulas(ent.pos, SKYBLUE, 5, 30.0f); if(ent.tempoVida < 3.5f && ent.tempoVida > 3.4f) { for(int dx=-6; dx<=6; dx++) for(int dz=-6; dz<=6; dz++) if(sqrt(dx*dx + dz*dz) < 6) modificacoesAlvo[{round(ent.pos.x)+dx, round(ent.pos.z)+dz}] -= 8.0f; } } 
                else if (ent.tipo == 1 || ent.tipo == 5 || ent.tipo == 6 || ent.tipo == 7 || ent.tipo == 11 || ent.tipo == 12) ent.tempoVida -= dt;
            }
            entidadesGlobais.erase(std::remove_if(entidadesGlobais.begin(), entidadesGlobais.end(), [](const EntidadeDivina& e) { return e.tempoVida <= 0; }), entidadesGlobais.end());
            for (auto& p : particulas) { p.vida -= rawDt; p.vel.y -= 9.8f * rawDt; p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, rawDt)); }
            particulas.erase(std::remove_if(particulas.begin(), particulas.end(), [](const Particula& p) { return p.vida <= 0; }), particulas.end());
            mundoMutex.unlock();

            BeginDrawing();
                Color corCeuAlvo = {135, 206, 235, 255}; if (climaGlobal == 3) corCeuAlvo = GRAY; else if (climaGlobal == 4) corCeuAlvo = {0, 50, 20, 255}; else if (climaGlobal == 5) corCeuAlvo = {5, 5, 10, 255}; else if (biomaAlvo == 2) corCeuAlvo = {40, 10, 10, 255}; else if (biomaAlvo == 3) corCeuAlvo = {173, 216, 230, 255}; else if (biomaAlvo == 4) corCeuAlvo = {10, 0, 30, 255}; else if (biomaAlvo == 5) corCeuAlvo = {30, 0, 50, 255}; else if (biomaAlvo == 6) corCeuAlvo = {255, 192, 203, 255};
                Color corCeuAtual = {135, 206, 235, 255}; if (climaGlobal == 3) corCeuAtual = GRAY; else if (climaGlobal == 4) corCeuAtual = {0, 50, 20, 255}; else if (climaGlobal == 5) corCeuAtual = {5, 5, 10, 255}; else if (biomaAtual == 2) corCeuAtual = {40, 10, 10, 255}; else if (biomaAtual == 3) corCeuAtual = {173, 216, 230, 255}; else if (biomaAtual == 4) corCeuAtual = {10, 0, 30, 255}; else if (biomaAtual == 5) corCeuAtual = {30, 0, 50, 255}; else if (biomaAtual == 6) corCeuAtual = {255, 192, 203, 255};
                Color corCeu = LerpColor(corCeuAtual, corCeuAlvo, biomaBlend); if (climaGlobal == 2 && rand()%100 > 95) corCeu = WHITE; 
                if (modoFesta) corCeu = {(unsigned char)(sin(GetTime()*3)*127+128), (unsigned char)(cos(GetTime()*2)*127+128), (unsigned char)(sin(GetTime()*5)*127+128), 255};

                ClearBackground(corCeu);

                Camera3D renderCam = camera; renderCam.position = Vector3Add(renderCam.position, cameraShakeOffset); renderCam.target = Vector3Add(renderCam.target, cameraShakeOffset);

                BeginMode3D(renderCam);
                    int rDistFinal = (climaGlobal == 3) ? 20 : RENDER_DIST; Vector3 lightDir = Vector3Normalize({ -0.6f, 1.0f, -0.6f }); 
                    auto ApplyFog = [&](Color cOriginal, float distCamera) -> Color {
                        float fogStart = rDistFinal * 0.4f; float fogEnd = rDistFinal * 0.95f; float f = Clamp((distCamera - fogStart) / (fogEnd - fogStart), 0.0f, 1.0f); return LerpColor(cOriginal, corCeu, f);
                    };

                    auto GetVertexColor = [&](float vx, float vz, float vh) -> Color {
                        float hL = LerAlturaMundo(vx - 1.0f, vz); float hR = LerAlturaMundo(vx + 1.0f, vz); float hD = LerAlturaMundo(vx, vz - 1.0f); float hU = LerAlturaMundo(vx, vz + 1.0f);
                        Vector3 n = Vector3Normalize({ hL - hR, 2.0f, hD - hU });
                        float intLuz = Clamp(0.6f + (Vector3DotProduct(n, lightDir) * 0.4f), 0.5f, 1.0f);
                        Color cb = ObterCorBiomaDinâmico(vx, vz, vh, n); Color cS = { (unsigned char)(cb.r * intLuz), (unsigned char)(cb.g * intLuz), (unsigned char)(cb.b * intLuz), cb.a };
                        if (modoFesta) cS = Color{(unsigned char)((vx*10+GetTime()*100)), (unsigned char)((vz*10-GetTime()*50)), 150, 255};
                        float distCamera = sqrt(pow(vx - camera.position.x, 2) + pow(vz - camera.position.z, 2)); return ApplyFog(cS, distCamera);
                    };

                    int playerChunkX = round(camera.position.x); int playerChunkZ = round(camera.position.z);
                    mundoMutex.lock(); 

                    if (climaGlobal == 0 && biomaAtual != 2 && biomaAlvo != 2 && !modoWireframe) {
                        rlBegin(RL_QUADS); rlColor4ub(255, 255, 255, 200); 
                        for (int x = -rDistFinal; x < rDistFinal; x+=4) {
                            for (int z = -rDistFinal; z < rDistFinal; z+=4) {
                                float distCentro = sqrt(x*x + z*z); if (distCentro > rDistFinal) continue;
                                float wX = playerChunkX + x; float wZ = playerChunkZ + z;
                                if (FBM(wX * 0.01f + GetTime() * 0.1f, wZ * 0.01f, 3) > 0.6f) { 
                                    Color cF = ApplyFog({255, 255, 255, 200}, distCentro); rlColor4ub(cF.r, cF.g, cF.b, cF.a);
                                    rlVertex3f(wX, 80.0f, wZ); rlVertex3f(wX, 80.0f, wZ + 4); rlVertex3f(wX + 4, 80.0f, wZ + 4); rlVertex3f(wX + 4, 80.0f, wZ);
                                }
                            }
                        }
                        rlEnd();
                    }
                    if (climaGlobal == 4) { 
                        for (int i=0; i<100; i++) DrawCube({camera.position.x + (float)(rand()%100-50), 60.0f + (float)sin(GetTime()+i)*10.0f, camera.position.z + (float)(rand()%100-50)}, 2.0f, 20.0f, 2.0f, Fade(GREEN, 0.3f));
                    }

                    if (climaGlobal == 0 && biomaAtual != 2 && biomaAtual != 3 && !modoWireframe && climaGlobal != 5) DrawSphere(Vector3Add(camera.position, Vector3Scale({ -0.6f, 0.6f, -0.6f }, rDistFinal * 1.2f)), 20.0f, {255, 250, 150, 255}); 
                    if (climaGlobal == 5) DrawSphere(Vector3Add(camera.position, Vector3Scale({ -0.6f, 0.6f, -0.6f }, rDistFinal * 1.2f)), 20.0f, BLACK); 

                    rlBegin(RL_QUADS);
                    for (int x = -rDistFinal; x < rDistFinal; x++) {
                        for (int z = -rDistFinal; z < rDistFinal; z++) {
                            float distCentro = sqrt(x*x + z*z); if (distCentro > rDistFinal) continue; 
                            float wX = playerChunkX + x; float wZ = playerChunkZ + z;
                            float h00 = LerAlturaMundo(wX, wZ); float h10 = LerAlturaMundo(wX + 1, wZ); float h01 = LerAlturaMundo(wX, wZ + 1); float h11 = LerAlturaMundo(wX + 1, wZ + 1);
                            
                            if (modoWireframe) {
                                Color cFinal = GetVertexColor(wX, wZ, h00);
                                rlEnd(); DrawLine3D({wX, h00, wZ}, {wX+1, h10, wZ}, cFinal); DrawLine3D({wX+1, h10, wZ}, {wX+1, h11, wZ+1}, cFinal); DrawLine3D({wX+1, h11, wZ+1}, {wX, h01, wZ+1}, cFinal); DrawLine3D({wX, h01, wZ+1}, {wX, h00, wZ}, cFinal); rlBegin(RL_QUADS);
                            } else {
                                Color c00 = GetVertexColor(wX, wZ, h00); Color c01 = GetVertexColor(wX, wZ + 1, h01); Color c11 = GetVertexColor(wX + 1, wZ + 1, h11); Color c10 = GetVertexColor(wX + 1, wZ, h10);
                                rlColor4ub(c00.r, c00.g, c00.b, c00.a); rlVertex3f(wX, h00, wZ); rlColor4ub(c01.r, c01.g, c01.b, c01.a); rlVertex3f(wX, h01, wZ + 1);
                                rlColor4ub(c11.r, c11.g, c11.b, c11.a); rlVertex3f(wX + 1, h11, wZ + 1); rlColor4ub(c10.r, c10.g, c10.b, c10.a); rlVertex3f(wX + 1, h10, wZ);
                            }
                        }
                    }
                    rlEnd();

                    if (!modoWireframe && biomaAtual != 4) {
                        int startX = ((int)floor(camera.position.x) - rDistFinal) & ~1; int startZ = ((int)floor(camera.position.z) - rDistFinal) & ~1;
                        int endX = ((int)floor(camera.position.x) + rDistFinal) & ~1; int endZ = ((int)floor(camera.position.z) + rDistFinal) & ~1;
                        for (int wX = startX; wX <= endX; wX+=2) {
                            for (int wZ = startZ; wZ <= endZ; wZ+=2) {
                                float distCentro = sqrt(pow(wX - camera.position.x, 2) + pow(wZ - camera.position.z, 2)); if (distCentro > rDistFinal) continue;
                                float h = LerAlturaMundo(wX, wZ);
                                Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract({(float)wX, LerAlturaMundo(wX, wZ+1), (float)wZ+1}, {(float)wX, h, (float)wZ}), Vector3Subtract({(float)wX+1, LerAlturaMundo(wX+1, wZ), (float)wZ+1}, {(float)wX, h, (float)wZ}))); if (n.y < 0) n = Vector3Scale(n, -1.0f); 
                                float slope = 1.0f - n.y; float pathNoise = abs(FBM(wX * 0.02f, wZ * 0.02f, 2) - 0.5f);

                                if (biomaAtual == 0 && biomaAlvo == 0 && slope < 0.35f && h >= 0.0f && h < 18.0f && pathNoise > 0.04f) {
                                    float prob = RandPseudo(wX, wZ); float forestZone = FBM(wX * 0.05f, wZ * 0.05f, 2); 
                                    float treeT = (forestZone > 0.55f) ? 0.85f : 0.98f; float bushT = (forestZone > 0.55f) ? 0.70f : 0.94f; float rockT = 0.88f; float grassT = 0.50f;
                                    Color tint = ApplyFog(WHITE, distCentro);

                                    if (prob > treeT && !Forest.trees.empty()) { int idx = (int)(prob * 10000) % Forest.trees.size(); DrawModelEx(Forest.trees[idx], {(float)wX, h, (float)wZ}, {0,1,0}, prob*360.0f, {1.0f, 1.0f, 1.0f}, tint); } 
                                    else if (prob > bushT && !Forest.bushes.empty()) { int idx = (int)(prob * 10000) % Forest.bushes.size(); DrawModelEx(Forest.bushes[idx], {(float)wX, h, (float)wZ}, {0,1,0}, prob*360.0f, {1.0f, 1.0f, 1.0f}, tint); } 
                                    else if (prob > rockT && !Forest.rocks.empty()) { int idx = (int)(prob * 10000) % Forest.rocks.size(); DrawModelEx(Forest.rocks[idx], {(float)wX, h, (float)wZ}, {0,1,0}, prob*360.0f, {1.0f, 1.0f, 1.0f}, tint); } 
                                    else if (prob > grassT && !Forest.grasses.empty()) { int idx = (int)(prob * 10000) % Forest.grasses.size(); DrawModelEx(Forest.grasses[idx], {(float)wX, h, (float)wZ}, {0,1,0}, prob*360.0f, {1.0f, 1.0f, 1.0f}, tint); }
                                }
                            }
                        }
                    }

                    if (!modoWireframe && (biomaAtual != 1 && biomaAlvo != 1) && (biomaAtual != 3 && biomaAlvo != 3)) {
                        rlBegin(RL_QUADS);
                        int startX = ((int)floor(camera.position.x) - rDistFinal) & ~1; int startZ = ((int)floor(camera.position.z) - rDistFinal) & ~1;
                        int endX = ((int)floor(camera.position.x) + rDistFinal) & ~1; int endZ = ((int)floor(camera.position.z) + rDistFinal) & ~1;
                        for (int wX = startX; wX <= endX; wX+=2) {
                            for (int wZ = startZ; wZ <= endZ; wZ+=2) {
                                float distCentro = sqrt(pow(wX - camera.position.x, 2) + pow(wZ - camera.position.z, 2)); if (distCentro > rDistFinal) continue; 
                                if (LerAlturaMundo(wX, wZ) < -0.8f || LerAlturaMundo(wX+2, wZ) < -0.8f || LerAlturaMundo(wX, wZ+2) < -0.8f) {
                                    Color cAguaAtual = (biomaAtual == 2) ? Color{255, 69, 0, 220} : (biomaAtual == 5) ? Color{0, 255, 100, 200} : Color{0, 191, 255, 180}; 
                                    Color cAguaAlvo = (biomaAlvo == 2) ? Color{255, 69, 0, 220} : (biomaAlvo == 5) ? Color{0, 255, 100, 200} : Color{0, 191, 255, 180};
                                    Color cF = ApplyFog(LerpColor(cAguaAtual, cAguaAlvo, biomaBlend), distCentro);
                                    rlColor4ub(cF.r, cF.g, cF.b, cF.a); rlVertex3f(wX, -0.8f, wZ); rlVertex3f(wX, -0.8f, wZ + 2); rlVertex3f(wX + 2, -0.8f, wZ + 2); rlVertex3f(wX + 2, -0.8f, wZ);
                                }
                            }
                        }
                        rlEnd();
                    }

                    for (const auto& b : construcoesFixas) {
                        float distB = sqrt(pow(b.pos.x - camera.position.x, 2) + pow(b.pos.z - camera.position.z, 2));
                        if (modoWireframe) DrawCubeWires(b.pos, 1.0f, 1.0f, 1.0f, b.cor); else DrawCube(b.pos, 1.0f, 1.0f, 1.0f, ApplyFog(b.cor, distB)); 
                    }

                    for (const auto& ent : entidadesGlobais) {
                        if (ent.tipo == 1) { DrawSphere(ent.pos, ent.raio, BLACK); DrawSphereWires(ent.pos, ent.raio+0.5f, 16, 16, PURPLE); DrawCircle3D(ent.pos, ent.raio+2.0f, {1,0,0}, 90.0f, Fade(PURPLE, 0.5f)); }
                        else if (ent.tipo == 2) DrawSphere(ent.pos, ent.raio, RED); else if (ent.tipo == 3) DrawCylinderWires(ent.pos, ent.raio, ent.raio, 0.5f, 16, WHITE); 
                        else if (ent.tipo == 4) DrawCylinder(ent.pos, 0.0f, 0.5f, 1.0f, 4, YELLOW); else if (ent.tipo == 5) DrawCylinder(ent.pos, 0.5f, 0.5f, 2.0f, 8, Fade(SKYBLUE, 0.6f)); 
                        else if (ent.tipo == 6) { DrawCube({ent.pos.x, ent.pos.y+1, ent.pos.z}, 1.5, 2, 1.5, DARKGRAY); DrawCube({ent.pos.x, ent.pos.y+3, ent.pos.z}, 1, 1, 1, GRAY); } 
                        else if (ent.tipo == 7) DrawCylinderWires(ent.pos, 2.0f, 2.0f, 0.1f, 16, MAGENTA); 
                        else if (ent.tipo == 8) { for(int d=0; d<5; d++) DrawSphere({ent.pos.x - d*1.5f, ent.pos.y + (float)sin(GetTime()*5+d)*0.5f, ent.pos.z}, 1.0f - d*0.1f, RED); DrawCube({ent.pos.x+1, ent.pos.y+0.5f, ent.pos.z+0.5f}, 0.5f, 0.5f, 0.5f, YELLOW); DrawCube({ent.pos.x+1, ent.pos.y+0.5f, ent.pos.z-0.5f}, 0.5f, 0.5f, 0.5f, YELLOW); } 
                        else if (ent.tipo == 9) { DrawCylinder(ent.pos, 5.0f, 5.0f, 1.0f, 16, GRAY); DrawSphere({ent.pos.x, ent.pos.y+0.5f, ent.pos.z}, 2.0f, Fade(SKYBLUE, 0.7f)); DrawCylinderEx(ent.pos, {ent.pos.x, ent.pos.y-10.0f, ent.pos.z}, 4.0f, 8.0f, 16, Fade(GREEN, 0.2f)); } 
                        else if (ent.tipo == 10) { DrawCylinderEx({ent.pos.x, ent.pos.y+100.0f, ent.pos.z}, ent.pos, ent.raio, ent.raio, 16, Fade(SKYBLUE, 0.8f)); DrawSphere(ent.pos, ent.raio*1.5f, WHITE); } 
                        else if (ent.tipo == 11) DrawSphereWires(ent.pos, ent.raio, 32, 32, Fade(SKYBLUE, 0.3f)); 
                        else if (ent.tipo == 12) DrawCylinderWires(ent.pos, ent.raio, ent.raio, 5.0f, 32, ORANGE); 
                    }
                    
                    for (const auto& p : particulas) { Color pc = p.cor; pc.a = (unsigned char)(255 * (p.vida/p.maxVida)); DrawCube(p.pos, p.tamanho, p.tamanho, p.tamanho, pc); }
                    mundoMutex.unlock();

                    if (climaGlobal == 1 || climaGlobal == 2) {
                        for (int i=0; i<300; i++) { Vector3 pR = {camera.position.x + (rand()%40-20), camera.position.y + (rand()%20), camera.position.z + (rand()%40-20)}; DrawLine3D(pR, {pR.x, pR.y-1.0f, pR.z}, Fade(BLUE, 0.5f)); }
                    }
                    if (escudoAtivo) DrawSphereWires(jogador.posicao, 3.0f, 16, 16, Fade(SKYBLUE, 0.5f));

                    // DESENHAR JOGADOR
                    if (isThirdPerson) {
                        float cpos[3] = { camera.position.x, camera.position.y, camera.position.z };
                        SetShaderValue(globalShaderLuz, viewPosLoc, cpos, SHADER_UNIFORM_VEC3);
                        jogador.Desenhar(godModeAtivo);
                    }

                    // DESENHAR COMPANHEIRO
                    companheiro.Desenhar3D(camera.position);
                EndMode3D();

                // UI & DIÁRIO DE BORDO
                if (!modoChat && !modoDiario) {
                    if(!isThirdPerson) { DrawLine(larguraTela/2 - 10, alturaTela/2, larguraTela/2 + 10, alturaTela/2, WHITE); DrawLine(larguraTela/2, alturaTela/2 - 10, larguraTela/2, alturaTela/2 + 10, WHITE); }
                    DrawTextEx(fUniv, Config.Tr("C: Visao 1a/3a | 1, 2, 3: Formas | T: Chat | TAB: Diario", "C: 1st/3rd Cam | 1, 2, 3: Forms | T: Chat | TAB: Logbook"), {20, 20}, 20, 1, LIGHTGRAY);
                    DrawTextEx(fUniv, Config.Tr("ESPACO: Pular | SHIFT: Correr | ESC: Menu", "SPACE: Jump | SHIFT: Sprint | ESC: Menu"), {20, 45}, 20, 1, GRAY);
                    if (timeScale == 0.0f) DrawTextCenter(Config.Tr("TEMPO CONGELADO", "TIME FROZEN"), 100, 24, SKYBLUE);
                    if (godModeAtivo) DrawTextEx(fUniv, Config.Tr("GOD MODE (F: Voar, X: Descer)", "GOD MODE (F: Fly, X: Descend)"), {20, 80}, 20, 1, GOLD);
                }

                if (modoDiario) {
                    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { if (paginaDiario < 11) paginaDiario++; }
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { if (paginaDiario > 0) paginaDiario--; }

                    DrawRectangle(larguraTela/2 - 420, 50, 840, alturaTela - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({(float)larguraTela/2 - 420, 50, 840, (float)alturaTela - 100}, 3, RED); 
                    DrawTextCenter(TextFormat(Config.Tr("TOMO DE ZUBA (Pag %d/12)", "ZUBA TOME (Pg %d/12)"), paginaDiario + 1), 80, 24, ORANGE);
                    
                    std::vector<std::string> pags = {
                        Config.Tr("ZUBA E O JOGADOR (NOVOS PODERES)\n\n[ TECLAS DO JOGADOR ]\n- 'C': Alterna Camera 1a e 3a Pessoa\n- '1, 2, 3': Mudar propria Forma\n- 'X': Descer no modo God Mode\n\n[ MUTACOES DO ZUBA ]\n- 'Zuba transforme-se em cubo'\n- 'Cilindro'\n- 'Normal' (cancela a forma)", "ZUBA AND PLAYER (NEW POWERS)\n\n[ PLAYER KEYS ]\n- 'C': Toggle 1st/3rd Camera\n- '1, 2, 3': Change own Form\n- 'X': Descend in God Mode\n\n[ ZUBA MUTATIONS ]\n- 'Zuba transform into cube'\n- 'Cylinder'\n- 'Normal' (cancels shape)"),
                        Config.Tr("ACOES FISICAS\n\n[ NAVEGACAO & MANOBRAS ]\n- 'Fique parado ai'\n- 'Volte a me seguir'\n- 'Va la para a frente!'\n- 'Atire um laser'\n- 'Pule!' / 'Salta!' (Zuba Pula)\n- 'Gire!' / 'Piao!' (Giro veloz)\n- 'Dance!' (Habilidade ritmica)\n- 'Acoes OFF' (desliga acao)", "PHYSICAL ACTIONS\n\n[ NAVIGATION & MOVES ]\n- 'Stop right there'\n- 'Follow me again'\n- 'Go forward!'\n- 'Shoot a laser'\n- 'Jump!' (Zuba Jumps)\n- 'Spin!' (Fast spin)\n- 'Dance!' (Rhythmic skill)\n- 'Actions OFF' (stops action)"),
                        Config.Tr("TERRAFORMACAO MASSIVA\n\n[ GOD TIER TERRAIN ]\n- 'Erga uma montanha!'\n- 'Achate o terreno.'\n- 'Abra um abismo.'\n- 'Inunde o lago.'\n- 'Abra um CANION!' (Fenda gigante)\n- 'Crie uma ILHA!' (Elevacao cercada)\n\nTransformacao procedural dinamica.", "MASSIVE TERRAFORMING\n\n[ GOD TIER TERRAIN ]\n- 'Raise a mountain!'\n- 'Flatten the terrain.'\n- 'Open an abyss.'\n- 'Flood the lake.'\n- 'Open a CANYON!' (Giant rift)\n- 'Create an ISLAND!' (Raised area)\n\nDynamic procedural transform."),
                        Config.Tr("SINTESE DE ITENS AVANCADA\n\n[ FORJA INSTANTANEA ]\n- 'Crie uma espada!'\n- 'Arco e flecha'\n- 'Picareta'\n- 'Varinha magica' (Poder arcano)\n- 'Lanterna' (Iluminacao)\n- 'Escudo de mao' (Defesa tática)\n- 'Guarde tudo' (limpa mãos)", "ADVANCED ITEM SYNTHESIS\n\n[ INSTANT FORGE ]\n- 'Create a sword!'\n- 'Bow and arrow'\n- 'Pickaxe'\n- 'Magic wand' (Arcane power)\n- 'Lantern' (Lighting)\n- 'Hand shield' (Tactical defense)\n- 'Store everything' (clears hands)"),
                        Config.Tr("ARQUITETURA PROCEDURAL\n\n[ CONSTRUTOR MASTER ]\n- 'Construa uma piramide gigante!'\n- 'Torre de vigia.'\n- 'Ponte' e 'Muralha gigante.'\n- 'Construa uma CASA.' (Lar doce lar)\n- 'Erga um TEMPLO.' (Estrutura mitica)\n- 'Crie um MONOLITO.' (Pilar escuro)\n\nBlocos gerados instantaneamente.", "PROCEDURAL ARCHITECTURE\n\n[ MASTER BUILDER ]\n- 'Build a giant pyramid!'\n- 'Watchtower.'\n- 'Bridge' and 'Giant wall.'\n- 'Build a HOUSE.' (Home sweet home)\n- 'Raise a TEMPLE.' (Mythic structure)\n- 'Create a MONOLITH.' (Dark pillar)\n\nBlocks generated instantly."),
                        Config.Tr("DOMINIO CLIMATICO\n\n[ ATMOSFERA GLOBAL ]\n- 'Invoque uma tempestade!'\n- 'Faca chover.'\n- 'Nevoeiro denso.'\n- 'Aurora Boreal!' (Luzes no ceu)\n- 'Eclipse Solar!' (Escuridao total)\n- 'Clima normal' (limpa o tempo)", "CLIMATE DOMAIN\n\n[ GLOBAL ATMOSPHERE ]\n- 'Summon a storm!'\n- 'Make it rain.'\n- 'Dense fog.'\n- 'Aurora Borealis!' (Sky lights)\n- 'Solar Eclipse!' (Total darkness)\n- 'Normal weather' (clears weather)"),
                        Config.Tr("ESPACO-TEMPO V24\n\n[ DOBRADORA DE REALIDADE ]\n- 'Congele o tempo!'\n- 'Acelere o tempo.'\n- 'Camera lenta!' (Bullet Time 0.2x)\n- 'Gravidade lunar'\n- 'Tempo normal' (Restaura Einstein)", "SPACE-TIME V24\n\n[ REALITY BENDER ]\n- 'Freeze time!'\n- 'Accelerate time.'\n- 'Slow motion!' (Bullet Time 0.2x)\n- 'Lunar gravity'\n- 'Normal time' (Restores Einstein)"),
                        Config.Tr("CRIADOR DE MUNDOS (BIOMAS)\n\n[ ECOSSISTEMAS ]\n- 'Bioma deserto.'\n- 'PROTOCOLO MAGMA!'\n- 'Gelo eterno.'\n- 'Mundo Cyberpunk' (Cores Neon Negras)\n- 'Mundo Alienigena' (Verde e Roxo)\n- 'Mundo Doce' (Rosa e Candy)\n- 'Bioma normal' (Reverte a natureza)", "WORLD CREATOR (BIOMES)\n\n[ ECOSYSTEMS ]\n- 'Desert biome.'\n- 'MAGMA PROTOCOL!'\n- 'Eternal ice.'\n- 'Cyberpunk World' (Dark Neon)\n- 'Alien World' (Green and Purple)\n- 'Candy World' (Pink Pastel)\n- 'Normal biome' (Reverts nature)"),
                        Config.Tr("MAGIA OFENSIVA DE DESTRUIÇÃO\n\n[ DESTRUICAO DIVINA ]\n- 'Chuva de meteoros!'\n- 'Buraco negro.'\n- 'Onda de choque.'\n- 'RAIO ORBITAL!' (Pilar letal dos ceus)\n\nA destruicao altera a topografia local permanentemente.", "OFFENSIVE DESTRUCTION MAGIC\n\n[ DIVINE DESTRUCTION ]\n- 'Meteor shower!'\n- 'Black hole.'\n- 'Shockwave.'\n- 'ORBITAL STRIKE!' (Lethal pillar from skies)\n\nDestruction alters local topography permanently."),
                        Config.Tr("DEFESA ABSOLUTA DE AREA\n\n[ BARREIRAS TATICAS ]\n- 'Gere um escudo magico' (Pessoal)\n- 'Camuflagem optica'\n- 'DOMO DE PROTECAO' (Cobre a area)\n- 'ANEL DE FOGO' (Barreira Flamejante)\n- 'Defesas OFF' (Desativa tudo)", "ABSOLUTE AREA DEFENSE\n\n[ TACTICAL BARRIERS ]\n- 'Generate magic shield' (Personal)\n- 'Optical camo'\n- 'PROTECTION DOME' (Covers area)\n- 'RING OF FIRE' (Flaming Barrier)\n- 'Defenses OFF' (Deactivates all)"),
                        Config.Tr("ENTIDADES E INVOCACAO\n\n[ SINTESE DE VIDA ]\n- 'Invoque um passaro.'\n- 'Golem de pedra.'\n- 'INVOQUE UM DRAGAO!' (Serpente voadora)\n- 'DISCO VOADOR!' (Contato OVNI alienigena)\n\nPossuem IA basal de navegacao e tempo de vida.", "ENTITIES AND SUMMONING\n\n[ SYNTHESIS OF LIFE ]\n- 'Summon a bird.'\n- 'Stone golem.'\n- 'SUMMON A DRAGON!' (Flying serpent)\n- 'FLYING SAUCER!' (Alien UFO contact)\n\nThey have basic navigation AI and lifespan."),
                        Config.Tr("HACKING DO MOTOR V24\n\n[ ACESSO CORE ADMIN ]\n- 'Modo Wireframe.' \n- 'Luzes bioluminescentes.'\n- 'Ative GOD MODE!' (Voo)\n- 'MODO FESTA!' (Cores piscantes malucas!)\n- 'RESET MUNDIAL' (Apaga tudo no multiverso!)", "V24 ENGINE HACKING\n\n[ CORE ADMIN ACCESS ]\n- 'Wireframe Mode.' \n- 'Bioluminescent lights.'\n- 'Activate GOD MODE!' (Flight)\n- 'PARTY MODE!' (Crazy flashing colors!)\n- 'WORLD RESET' (Erases everything in the multiverse!)")
                    };
                    DrawTextEx(fUniv, pags[paginaDiario].c_str(), {larguraTela/2.0f - 380.0f, 140}, 20, 1, LIGHTGRAY);
                    DrawTextCenter(Config.Tr("TAB para fechar | SETAS para mudar de pagina", "TAB to close | ARROWS to switch pages"), alturaTela - 60.0f, 18, GRAY);
                }

                if (!modoDiario) {
                    std::lock_guard<std::mutex> lock(companheiro.chatMutex);
                    
                    int larguraCaixaChat = larguraTela / 3 + 50; 
                    int alturaCaixa = chatMinimizado ? 40 : 230; 
                    int startY = alturaTela - alturaCaixa - 10; 

                    DrawRectangle(20, startY - 10, larguraCaixaChat, alturaCaixa, Fade(BLACK, 0.85f));
                    DrawRectangleLinesEx({20, (float)startY - 10, (float)larguraCaixaChat, (float)alturaCaixa}, 2, Fade(DARKBLUE, 0.8f));
                    
                    Rectangle btnMinMax = { 20.0f + larguraCaixaChat - 40.0f, (float)startY - 5.0f, 30.0f, 25.0f };
                    bool hoverBtn = CheckCollisionPointRec(GetMousePosition(), btnMinMax);
                    if (modoChat && hoverBtn && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { chatMinimizado = !chatMinimizado; Audio.PlayUISelect(); }
                    DrawRectangleRec(btnMinMax, hoverBtn ? Fade(RED, 0.8f) : Fade(DARKGRAY, 0.5f));
                    DrawTextEx(fUniv, chatMinimizado ? "+" : "-", {btnMinMax.x + 10, btnMinMax.y + 2}, 20, 1, WHITE);

                    if (!chatMinimizado) {
                        struct LinhaRender { std::string txt; float x; Color c; };
                        std::vector<LinhaRender> todasLinhasVisuais;
                        for (const auto& msg : companheiro.historico) {
                            std::string prefixo = msg.remetente + ": ";
                            std::vector<std::string> linhasTexto = QuebrarTextoEx(msg.texto, larguraCaixaChat - MeasureTextEx(fUniv, prefixo.c_str(), 18, 1).x - 40, fUniv, 18);
                            if (linhasTexto.empty()) continue;
                            todasLinhasVisuais.push_back({prefixo + linhasTexto[0], 30.0f, msg.cor}); float mTxt = 30.0f + MeasureTextEx(fUniv, prefixo.c_str(), 18, 1).x;
                            for (size_t l = 1; l < linhasTexto.size(); l++) todasLinhasVisuais.push_back({linhasTexto[l], mTxt, msg.cor});
                            todasLinhasVisuais.push_back({"", 30.0f, BLANK});
                        }
                        int maxLinhasNaTela = 7;
                        int maxScroll = todasLinhasVisuais.size() - maxLinhasNaTela; if (maxScroll < 0) maxScroll = 0;
                        if (companheiro.forcadoScroll) { chatScrollIndex = maxScroll; companheiro.forcadoScroll = false; }
                        if (chatScrollIndex < 0) chatScrollIndex = 0; if (chatScrollIndex > maxScroll) chatScrollIndex = maxScroll;
                        
                        if (maxScroll > 0) {
                            float tamBarra = (alturaCaixa - 60) * ((float)maxLinhasNaTela / todasLinhasVisuais.size());
                            DrawRectangle(20 + larguraCaixaChat - 15, startY + 25 + ((alturaCaixa - 60) - tamBarra) * ((float)chatScrollIndex / maxScroll), 6, tamBarra, GRAY);
                        }
                        
                        int desenhadas = 0;
                        int textY = startY + 20;
                        for (size_t i = chatScrollIndex; i < todasLinhasVisuais.size() && desenhadas < maxLinhasNaTela; i++) {
                            DrawTextEx(fUniv, todasLinhasVisuais[i].txt.c_str(), {todasLinhasVisuais[i].x, (float)textY}, 18, 1, todasLinhasVisuais[i].c); textY += 22; desenhadas++;
                        }
                    } else DrawTextEx(fUniv, companheiro.pensando ? Config.Tr("Zuba processando...", "Zuba processing...") : Config.Tr("Zuba aguarda. (Chat Oculto)", "Zuba awaits. (Hidden Chat)"), {30.0f, (float)startY}, 20, 1, GRAY);

                    if (companheiro.pensando && !chatMinimizado) DrawTextEx(fUniv, Config.Tr("Processando Magia NLP...", "Processing NLP Magic..."), {30.0f, (float)alturaTela - 60}, 16, 1, GRAY);

                    if (modoChat) {
                        DrawRectangle(20, alturaTela - 40, larguraCaixaChat, 30, Fade(DARKBLUE, 0.9f));
                        DrawTextEx(fUniv, TextFormat(Config.Tr("Voce: %s_", "You: %s_"), inputAtual.c_str()), {30.0f, (float)alturaTela - 35}, 18, 1, WHITE);
                        DrawTextEx(fUniv, Config.Tr("[ENTER] enviar | [ESC] fechar | [SETAS] Historico", "[ENTER] send | [ESC] close | [ARROWS] History"), {30.0f, (float)alturaTela - 60}, 14, 1, LIGHTGRAY);
                    }
                }
            EndDrawing();
        }
    }
    jogador.Unload();
    companheiro.DescarregarModelo();
    Forest.Unload(); 
    Audio.Close();
    UnloadFont(fUniv); CloseWindow(); return 0;
}