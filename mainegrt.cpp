// ========================================================================================
// SUPERGEMINI CREATOR ENGINE - V21.0 (THE ZUBA SHADER & ANIMATION FIX)
// Architect: Gemini God Mode | Executive Producer: Igor Bettarello Xavier (OMEGARED)
// Features: GLSL Real-Time Shading, Locked IDLE Animation, Fast Walk, Full NLP.
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
// 1. SISTEMAS CORE E SHADERS GLSL INJETADOS (NOVO!)
// ==============================================================================

// SHADER DE VÉRTICES (Calcula as normais do Zuba para a Sombra 3D)
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

// SHADER DE FRAGMENTOS (Gera o volume, iluminação e sombras suaves no modelo)
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
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 light = normalize(lightDir);
    float NdotL = max(dot(fragNormal, light), 0.55); // 0.55 é a luz ambiente (impede que fique 100% preto)
    finalColor = texelColor * colDiffuse * vec4(NdotL, NdotL, NdotL, 1.0);
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
bool modoWireframe = false, godModeAtivo = false, luzesAtivas = false, escudoAtivo = false, camuflagemAtiva = false;
float gravidadeBase = 20.0f; 
float cameraShake = 0.0f; 

struct EntidadeDivina { int tipo; Vector3 pos; Vector3 vel; float raio; float tempoVida; };
std::vector<EntidadeDivina> entidadesGlobais;

std::string ObterDataHoraAtual() {
    auto t = std::time(nullptr); auto tm = *std::localtime(&t); std::ostringstream oss;
    oss << std::put_time(&tm, "%d/%m/%Y %H:%M:%S"); return oss.str();
}

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
// 3. IA NLP AVANÇADA PARA ZUBA
// ==============================================================================
struct RespostaIA { std::string texto; std::map<std::string, std::string> comandos; };
std::string ToLowerString(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); }); return s; }
bool Match(const std::string& in, const std::vector<std::string>& keys) { for (const auto& k : keys) if (in.find(k) != std::string::npos) return true; return false; }

RespostaIA ProcessarIAInterna(std::string inputJogador, float alturaAtual) {
    std::string in = ToLowerString(inputJogador); std::map<std::string, std::string> cmds; std::string resp = ""; bool acted = false;
    bool isOff = Match(in, {"off", "desliga", " pare ", "reverte", "cancela", "tira ", "remove", "desfaz", "pare com", "desative", "chega de", "zera"});

    if (Match(in, {"cubo", "quadrad", "caixa"})) { 
        if (isOff) { cmds["FORMA"] = "ESFERA"; resp += Config.Tr("Retornando a minha forma peluda normal. ", "Returning to my fluffy normal shape. "); } else { cmds["FORMA"] = "CUBO"; resp += Config.Tr("Zuba Cubico ativado! ", "Cubic Zuba activated! "); } acted = true; 
    } else if (Match(in, {"piramide", "triangulo"}) && !Match(in, {"constru", "faca", "crie"})) { 
        if (isOff) { cmds["FORMA"] = "ESFERA"; resp += Config.Tr("Geometria cancelada. Zuba normal. ", "Geometry cancelled. Normal Zuba. "); } else { cmds["FORMA"] = "PIRAMIDE"; resp += Config.Tr("Zuba Piramidal pronto para acao. ", "Pyramidal Zuba ready for action. "); } acted = true; 
    } else if (Match(in, {"esfera", "bola", "redond"})) { cmds["FORMA"] = "ESFERA"; resp += Config.Tr("Retornando a perfeicao redondinha. ", "Returning to round perfection. "); acted = true; }

    if (Match(in, {"gigante", "imenso", "cresca", "enorme"})) { 
        if (isOff) { cmds["TAMANHO"] = "NORMAL"; resp += Config.Tr("Zuba encolhendo ao normal. ", "Zuba shrinking to normal. "); } else { cmds["TAMANHO"] = "GIGANTE"; resp += Config.Tr("Zuba GIGANTE! Ninguem me segura! ", "GIANT Zuba! Nobody can hold me! "); } acted = true; 
    } else if (Match(in, {"pequen", "minúscul", "diminua", "encolh"})) { 
        if (isOff) { cmds["TAMANHO"] = "NORMAL"; resp += Config.Tr("Tamanho restaurado. ", "Size restored. "); } else { cmds["TAMANHO"] = "PEQUENO"; resp += Config.Tr("Mini Zuba ativado. ", "Mini Zuba activated. "); } acted = true; 
    }

    if (Match(in, {"tempestade", "raio", "trovao", "tormenta", "temporal"})) { 
        if (isOff) { cmds["CLIMA"] = "LIMPO"; resp += Config.Tr("Tempestade dissipada. ", "Storm dissipated. "); } else { cmds["CLIMA"] = "TEMPESTADE"; resp += Config.Tr("Invocando os raios da natureza! ", "Summoning nature's lightning! "); } acted = true; 
    }
    if (Match(in, {"chuv", "chova", "agua do ceu"})) { 
        if (isOff) { cmds["CLIMA"] = "LIMPO"; resp += Config.Tr("Chuva interrompida. ", "Rain stopped. "); } else { cmds["CLIMA"] = "CHUVA"; resp += Config.Tr("Fazendo chover nas nossas terras. ", "Making it rain on our lands. "); } acted = true; 
    }
    if (Match(in, {"nevoeiro", "nebl", "nubla", "bruma", "nevoa"})) { 
        if (isOff) { cmds["CLIMA"] = "LIMPO"; resp += Config.Tr("Ventos limparam o nevoeiro. ", "Winds cleared the fog. "); } else { cmds["CLIMA"] = "NEVOEIRO"; resp += Config.Tr("Deixando tudo misterioso... ", "Making everything mysterious... "); } acted = true; 
    }
    
    if (Match(in, {"deserto", "areia", "seco"})) { 
        if (isOff) { cmds["BIOMA"] = "NORMAL"; resp += Config.Tr("Trazendo o verde de volta. ", "Bringing the green back. "); } else { cmds["BIOMA"] = "DESERTO"; resp += Config.Tr("Transformando tudo em areia quente! ", "Turning everything into hot sand! "); } acted = true; 
    }
    if (Match(in, {"magma", "lava", "inferno", "vulcao"})) { 
        if (isOff) { cmds["BIOMA"] = "NORMAL"; resp += Config.Tr("Magma desativado. Ufa, estava quente! ", "Magma OFF. Phew, it was hot! "); } else { cmds["BIOMA"] = "MAGMA"; resp += Config.Tr("PROTOCOLO MAGMA! O chao e lava! ", "MAGMA PROTOCOL! The floor is lava! "); } acted = true; 
    }
    if (Match(in, {"gelo", "inverno", "neve", "frio"})) { 
        if (isOff) { cmds["BIOMA"] = "NORMAL"; resp += Config.Tr("Derretendo o gelo todo. ", "Melting all the ice. "); } else { cmds["BIOMA"] = "GELO"; resp += Config.Tr("Congelando o mundo. Zuba de Neve! ", "Freezing the world. Snow Zuba! "); } acted = true; 
    }

    if (Match(in, {"congela", "pare o", "pare a"}) && Match(in, {"tempo", "animacao"})) { 
        if (isOff) { cmds["TEMPO"] = "NORMAL"; resp += Config.Tr("O tempo voltou a correr! ", "Time is running again! "); } else { cmds["TEMPO"] = "CONGELAR"; resp += Config.Tr("Tudo parado! O tempo congeleou! ", "Everything stopped! Time frozen! "); } acted = true; 
    }
    if (Match(in, {"acelera", "rápido", "rapido"})) { 
        if (isOff) { cmds["TEMPO"] = "NORMAL"; resp += Config.Tr("Velocidade normalizada. ", "Speed normalized. "); } else { cmds["TEMPO"] = "ACELERAR"; resp += Config.Tr("Velocidade maxima ativada! ", "Maximum speed activated! "); } acted = true; 
    }
    if (Match(in, {"gravidade", "pulo", "lua"})) { 
        if (isOff) { cmds["TEMPO"] = "GRAVIDADE_NORMAL"; resp += Config.Tr("Gravidade pesada da Terra restaurada. ", "Heavy Earth gravity restored. "); } else { cmds["TEMPO"] = "GRAVIDADE_LUNAR"; resp += Config.Tr("Gravidade zero! Vamos flutuar! ", "Zero gravity! Let's float! "); } acted = true; 
    }

    if (Match(in, {"wireframe", "matrix", "malha", "poligonos"})) { 
        if (isOff) { cmds["HACK"] = "WIREFRAME_OFF"; resp += Config.Tr("Saindo da Matrix. ", "Leaving the matrix. "); } else { cmds["HACK"] = "WIREFRAME_ON"; resp += Config.Tr("Hackeando os poligonos do mundo! ", "Hacking the world polygons! "); } acted = true; 
    }
    if (Match(in, {"god mode", "deus", "imortal"})) { 
        if (isOff) { cmds["HACK"] = "GODMODE_OFF"; resp += Config.Tr("God Mode desativado. Pise no chao. ", "God Mode deactivated. Step on the ground. "); } else { cmds["HACK"] = "GODMODE_ON"; resp += Config.Tr("Poder absoluto! Voe com o F! ", "Absolute power! Fly with F! "); } acted = true; 
    }
    if (Match(in, {"luzes", "ilumina", "vagalume"})) {
        if (isOff) { cmds["HACK"] = "LUZES_OFF"; resp += Config.Tr("Luzes apagadas. ", "Lights off. "); } else { cmds["HACK"] = "LUZES_ON"; resp += Config.Tr("Iluminando os caminhos! ", "Lighting up the paths! "); } acted = true;
    }

    if (Match(in, {"escudo", "barreira", "protecao"})) { 
        if (isOff) { cmds["DEFESA"] = "ESCUDO_OFF"; resp += Config.Tr("Escudo desativado. ", "Shield deactivated. "); } else { cmds["DEFESA"] = "ESCUDO_ON"; resp += Config.Tr("Zuba ativando campo de forca! ", "Zuba activating force field! "); } acted = true; 
    }
    if (Match(in, {"camufla", "invisi", "esconda"})) { 
        if (isOff) { cmds["DEFESA"] = "CAMUFLAGEM_OFF"; resp += Config.Tr("Voltei a ser visivel! ", "I'm visible again! "); } else { cmds["DEFESA"] = "CAMUFLAGEM_ON"; resp += Config.Tr("Modo ninja. Ninguem me ve! ", "Ninja mode. Nobody sees me! "); } acted = true; 
    }

    if (Match(in, {"atira", "laser", "dispar", "fogo", "destrua"})) { cmds["ACAO"] = "ATIRAR"; resp += Config.Tr("Pew pew! Raios laser! ", "Pew pew! Laser beams! "); acted = true; }
    if (Match(in, {"segu", "volta", "venha", "acompanh"})) { cmds["ACAO"] = "SEGUIR"; resp += Config.Tr("Estou a caminho! Me espere! ", "I'm on my way! Wait for me! "); acted = true; }
    if (Match(in, {"frente", "investig", "va para", "explore", "ande", "caminh"}) && !Match(in, {"constru", "faca"})) { cmds["ACAO"] = "IR_FRENTE"; resp += Config.Tr("Vou ver o que tem por ali! ", "I'll see what's over there! "); acted = true; }
    if (Match(in, {"para", "parad", "pare", "fique ai", "nao mova", "quieta"}) && !Match(in, {"tempo"})) { cmds["ACAO"] = "PARAR"; resp += Config.Tr("Ficarei quietinho aqui. ", "I'll stay quiet right here. "); acted = true; }

    if (Match(in, {"montanha", "morro", "pico", "eleve"})) { cmds["TERRENO"] = "MONTANHA"; resp += Config.Tr("Levantando essa terra toda! ", "Lifting all this earth! "); acted = true; }
    if (Match(in, {"planicie", "achata", "alisa", "plano"})) { cmds["TERRENO"] = "PLANICIE"; resp += Config.Tr("Amassando tudo para ficar lisinho. ", "Flattening everything to be smooth. "); acted = true; }
    if (Match(in, {"abismo", "cratera", "buraco", "fenda"})) { cmds["TERRENO"] = "ABISMO"; resp += Config.Tr("Cuidado com o buraco gigante! ", "Watch out for the giant hole! "); acted = true; }
    if (Match(in, {"lago", "agua", "oceano", "mar", "piscina"})) { cmds["TERRENO"] = "LAGO"; resp += Config.Tr("Hora de nadar! Enchendo o lago. ", "Swimming time! Filling the lake. "); acted = true; }

    if (Match(in, {"constru", "faca", "crie", "erga"}) && Match(in, {"piramide"})) { cmds["CONSTRUCAO"] = "PIRAMIDE"; resp += Config.Tr("Zuba e o Farao! Construindo piramide! ", "Zuba is the Pharaoh! Building pyramid! "); acted = true; }
    if (Match(in, {"torre", "castelo", "forte"})) { cmds["CONSTRUCAO"] = "TORRE"; resp += Config.Tr("Uma torre gigante para o meu chefe. ", "A giant tower for my boss. "); acted = true; }
    if (Match(in, {"ponte", "passarela"})) { cmds["CONSTRUCAO"] = "PONTE"; resp += Config.Tr("Fazendo uma ponte bem segura. ", "Making a safe bridge. "); acted = true; }
    if (Match(in, {"muralha", "muro", "parede"})) { cmds["CONSTRUCAO"] = "MURALHA"; resp += Config.Tr("Muralha de pedra saindo! ", "Stone wall coming right up! "); acted = true; }

    if (Match(in, {"espada", "arma", "lâmina", "lamina"})) { cmds["ITEM"] = "ESPADA"; resp += Config.Tr("Saca so essa espada que criei! ", "Check out this sword I created! "); acted = true; }
    if (Match(in, {"arco"})) { cmds["ITEM"] = "ARCO"; resp += Config.Tr("Arco em maos. ", "Bow in hands. "); acted = true; }
    if (Match(in, {"picareta", "ferramenta"})) { cmds["ITEM"] = "PICARETA"; resp += Config.Tr("Hora de minerar com essa picareta. ", "Mining time with this pickaxe. "); acted = true; }
    if (Match(in, {"limpa mao", "guarde", "tira a arma", "solta", "esconda a arma"})) { cmds["ITEM"] = "NADA"; resp += Config.Tr("Guardei os meus brinquedos. ", "Stored my toys. "); acted = true; }

    if (Match(in, {"meteoro", "asteroide", "cometa"})) { cmds["ATAQUE"] = "METEOROS"; resp += Config.Tr("Pedras flamejantes caindo! Corre! ", "Flaming rocks falling! Run! "); acted = true; }
    if (Match(in, {"buraco negro", "singularidade", "vazio"})) { cmds["ATAQUE"] = "BURACO_NEGRO"; resp += Config.Tr("Eita, invoquei um Buraco Negro! ", "Oops, I summoned a Black Hole! "); acted = true; }
    if (Match(in, {"choque", "empurre", "explosao"})) { cmds["ATAQUE"] = "ONDA_CHOQUE"; resp += Config.Tr("BOOM! Onda de choque enviada! ", "BOOM! Shockwave sent! "); acted = true; }
    
    if (Match(in, {"passaro", "ave", "pássaro"})) { cmds["INVOCAR"] = "PASSARO"; resp += Config.Tr("Olha o passarinho brilhante! ", "Look at the shiny bird! "); acted = true; }
    if (Match(in, {"golem", "monstro", "gigante de pedra"})) { cmds["INVOCAR"] = "GOLEM"; resp += Config.Tr("Acorda, Golem, vem brincar! ", "Wake up Golem, come play! "); acted = true; }
    if (Match(in, {"portal", "teleporte"})) { cmds["TEMPO"] = "PORTAL"; resp += Config.Tr("Portal magico acionado. ", "Magic portal activated. "); acted = true; }

    if (Match(in, {"reset", "apaga tudo", "recomec", "destrua o mundo"})) { cmds["HACK"] = "RESET"; resp += Config.Tr("Apagando minha bagunca. Mundo limpo! ", "Cleaning my mess. World reset! "); acted = true; }

    if (!acted) {
        if (Match(in, {"ola", "oi", "bom dia", "boa tarde"})) resp = Config.Tr("Oieee! Eu sou o Zuba! O que vamos fazer hoje?", "Hiii! I'm Zuba! What are we doing today?");
        else if (Match(in, {"hora"})) resp = Config.Tr("O meu estomago de pelucia diz que e hora da aventura!", "My plushie stomach says it's adventure time!");
        else resp = Config.Tr("Nao entendi direito, mestre. Pode repetir de outro jeito?", "Didn't quite get that, master. Can you rephrase?");
    }
    return {resp, cmds}; 
}

// ==============================================================================
// 4. ZUBA (COMPANHEIRO MÁGICO 3D) E SHADERS
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

    Model modelo3D;
    Texture2D texturaAvatar;
    Shader shaderLuz;
    int lightDirLoc;
    ModelAnimation* animacoes;
    int animCount;
    int animFrame;
    bool temModelo;
    bool temTextura;
    float anguloRotacao;

    CompanheiroMagico() : posicao({0, 15, 0}), alvoMovimento({0,0,0}), alvoLaser({0,0,0}), nome("Zuba"), tempoOrbita(0.0f), estadoFisico(ORBITAR), 
                          formaAtual("ESFERA"), escalaAtual(1.0f), escalaAlvo(1.0f), distanciaOrbitaAtual(4.0f), distanciaOrbitaAlvo(4.0f),
                          tempoCinematico(0.0f), tempoLaser(0.0f), itemEquipado(""), pensando(false), forcadoScroll(false),
                          animacoes(nullptr), animCount(0), animFrame(0), temModelo(false), temTextura(false), anguloRotacao(0.0f) {}
    
    void CarregarModeloAvatar(const char* pathModelo, const char* pathTextura) {
        if (FileExists(pathModelo)) {
            modelo3D = LoadModel(pathModelo);
            animacoes = LoadModelAnimations(pathModelo, &animCount);
            temModelo = true;

            // COMPILA E APLICA O NOVO SHADER DE LUZ NO ZUBA! (Volume 3D Mágico)
            shaderLuz = LoadShaderFromMemory(vertexShaderStr, fragmentShaderStr);
            lightDirLoc = GetShaderLocation(shaderLuz, "lightDir");
            float ldir[3] = { -0.6f, 1.0f, -0.6f };
            SetShaderValue(shaderLuz, lightDirLoc, ldir, SHADER_UNIFORM_VEC3);
            for (int i=0; i<modelo3D.materialCount; i++) modelo3D.materials[i].shader = shaderLuz;

            if (FileExists(pathTextura)) {
                texturaAvatar = LoadTexture(pathTextura);
                modelo3D.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texturaAvatar;
                temTextura = true;
            }
        }
    }

    void DescarregarModelo() {
        if (temModelo) {
            if (temTextura) UnloadTexture(texturaAvatar);
            UnloadShader(shaderLuz);
            UnloadModel(modelo3D);
            if (animCount > 0) UnloadModelAnimations(animacoes, animCount);
        }
    }

    void Iniciar() {
        historico.clear(); formaAtual = "ESFERA"; escalaAtual = 1.0f; escalaAlvo = 1.0f; distanciaOrbitaAtual = 4.0f; distanciaOrbitaAlvo = 4.0f;
        itemEquipado = ""; estadoFisico = ORBITAR;
        AdicionarMensagem(nome, Config.Tr("Sistemas V21! Shader de Sombra Ativo e Animacoes arranjadas!", "V21 Systems! Shadow Shader Active and Animations fixed!"), GOLD);
    }

    void AtualizarFisica(Vector3 posJogador, Vector3 forwardJogador, float dt) {
        float trueDelta = dt * timeScale; 
        tempoOrbita += trueDelta * 2.0f; 
        escalaAtual = Lerp(escalaAtual, escalaAlvo, dt * 2.0f); 
        distanciaOrbitaAtual = Lerp(distanciaOrbitaAtual, distanciaOrbitaAlvo, dt * 1.5f);
        if (tempoCinematico > 0) tempoCinematico -= dt; else distanciaOrbitaAlvo = 4.0f; 

        bool isMoving = false;
        float velZuba = 0.0f;

        if (estadoFisico == ORBITAR) { 
            Vector3 diff = Vector3Subtract(posJogador, posicao);
            diff.y = 0; 
            float dist = Vector3Length(diff);
            
            if (dist > 3.5f) { 
                diff = Vector3Normalize(diff);
                velZuba = (dist > 12.0f) ? 25.0f : 14.0f; // VELOCIDADE AUMENTADA! Corre que se farta!
                posicao.x += diff.x * velZuba * trueDelta;
                posicao.z += diff.z * velZuba * trueDelta;
                anguloRotacao = atan2(diff.x, diff.z) * RAD2DEG;
                isMoving = true;
            } else {
                Vector3 dirOlhar = Vector3Subtract(posJogador, posicao);
                anguloRotacao = atan2(dirOlhar.x, dirOlhar.z) * RAD2DEG;
            }
        } 
        else if (estadoFisico == PARADA) { 
            Vector3 dirOlhar = Vector3Subtract(posJogador, posicao);
            anguloRotacao = atan2(dirOlhar.x, dirOlhar.z) * RAD2DEG;
            // isMoving continua FALSE = Zuba Congela totalmente!
        } 
        else if (estadoFisico == MOVER_ALVO) { 
            Vector3 diff = Vector3Subtract(alvoMovimento, posicao);
            diff.y = 0;
            float dist = Vector3Length(diff);
            if (dist > 1.0f) {
                diff = Vector3Normalize(diff);
                velZuba = 18.0f; // Bem rapido a investigar
                posicao.x += diff.x * velZuba * trueDelta;
                posicao.z += diff.z * velZuba * trueDelta;
                anguloRotacao = atan2(diff.x, diff.z) * RAD2DEG;
                isMoving = true;
            }
        }

        // ANTI-CLIPPING
        mundoMutex.lock(); float chaoAura = LerAlturaMundo(posicao.x, posicao.z); mundoMutex.unlock();
        float zubaYOffset = 0.2f * escalaAtual; 
        posicao.y = Lerp(posicao.y, chaoAura + zubaYOffset, trueDelta * 18.0f); 

        // SMART ANIMATION ENGINE (CORREÇÃO ABSOLUTA DO IDLE)
        if (temModelo && animCount > 0) {
            int currentAnimIndex = 0;
            
            if (isMoving) {
                currentAnimIndex = (animCount > 1) ? 1 : 0; 
                // A animação corre de forma proporcional à velocidade nova (não fica mais em câmera lenta)
                animFrame += (int)(velZuba * 2.5f); 
            } else {
                currentAnimIndex = 0; 
                animFrame = 0; // FIX TOTAL: IDLE CONGELADO!
            }

            if (animFrame >= animacoes[currentAnimIndex].frameCount) animFrame = 0;
            UpdateModelAnimation(modelo3D, animacoes[currentAnimIndex], animFrame);
        }

        if (tempoLaser > 0) { 
            tempoLaser -= trueDelta; alvoLaser = Vector3Add(posJogador, Vector3Scale(forwardJogador, 30.0f)); 
            mundoMutex.lock(); alvoLaser.y = LerAlturaMundo(alvoLaser.x, alvoLaser.z) + 1.0f; mundoMutex.unlock(); 
            EmitirParticulas(alvoLaser, ORANGE, 5, 5.0f); 
        }

        if (!filaConstrucao.empty()) {
            timerConstrucao += dt;
            if (timerConstrucao > 0.03f) { 
                int vel = 1 + (filaConstrucao.size() / 30); 
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
        
        // A SOMBRA DO ZUBA PROJETADA NO CHÃO (Blob Shadow)
        if (!camuflagemAtiva && formaAtual == "ESFERA" && temModelo) {
            mundoMutex.lock(); float alturaSombra = LerAlturaMundo(posicao.x, posicao.z) + 0.1f; mundoMutex.unlock();
            DrawCylinder({posicao.x, alturaSombra, posicao.z}, 0.8f * escalaAtual, 0.8f * escalaAtual, 0.05f, 16, Fade(BLACK, 0.4f));
        }

        if (camuflagemAtiva) DrawSphereWires(posicao, r, 16, 16, Fade(BLUE, 0.2f)); 
        else {
            if (formaAtual == "CUBO") DrawCube({posicao.x, posicao.y + r, posicao.z}, r*2.2f, r*2.2f, r*2.2f, { 255, 215, 0, 200 });
            else if (formaAtual == "PIRAMIDE") DrawCylinder({posicao.x, posicao.y, posicao.z}, 0.0f, r*1.8f, r*3.0f, 4, { 255, 215, 0, 200 });
            else { 
                if (temModelo) {
                    float escalaAjuste = 1.0f; // Ajusta o tamanho base do Zuba aqui se necessário!
                    DrawModelEx(modelo3D, posicao, {0.0f, 1.0f, 0.0f}, anguloRotacao, {escalaAtual * escalaAjuste, escalaAtual * escalaAjuste, escalaAtual * escalaAjuste}, WHITE);
                } else {
                    DrawSphere({posicao.x, posicao.y + r, posicao.z}, r, { 255, 215, 0, 200 }); DrawSphereWires({posicao.x, posicao.y + r, posicao.z}, r*1.3f, 16, 16, ORANGE); 
                }
            }
        }

        if (!camuflagemAtiva) {
            if (itemEquipado == "ESPADA") { DrawCube({posicao.x + r*1.5f, posicao.y + r*1.5f, posicao.z}, 0.2f, r*3.0f, 0.2f, LIGHTGRAY); DrawCube({posicao.x + r*1.5f, posicao.y + r*0.5f, posicao.z}, 0.8f, 0.2f, 0.2f, BROWN); } 
            else if (itemEquipado == "PICARETA") { DrawCube({posicao.x + r*1.5f, posicao.y + r*1.5f, posicao.z}, 0.2f, r*3.0f, 0.2f, BROWN); DrawCube({posicao.x + r*1.5f, posicao.y + r*2.5f, posicao.z}, r*2.0f, 0.2f, 0.2f, GRAY); } 
            else if (itemEquipado == "ARCO") { DrawCylinderEx({posicao.x+r*1.5f, posicao.y+r*0.5f, posicao.z}, {posicao.x+r*1.5f, posicao.y+r*2.5f, posicao.z}, 0.1f, 0.1f, 8, BROWN); DrawLine3D({posicao.x+r*1.5f, posicao.y+r*0.5f, posicao.z}, {posicao.x+r*1.5f, posicao.y+r*2.5f, posicao.z}, WHITE); }
        }
        if (tempoLaser > 0) { DrawCylinderEx({posicao.x, posicao.y + 1.0f, posicao.z}, alvoLaser, 0.2f * escalaAtual, 0.2f * escalaAtual, 8, Fade(RED, 0.8f)); DrawSphere(alvoLaser, 1.5f * escalaAtual, Fade(ORANGE, 0.9f)); }
    }

    void AdicionarMensagem(std::string remetente, std::string texto, Color cor) { std::lock_guard<std::mutex> lock(chatMutex); historico.push_back({remetente, texto, cor}); forcadoScroll = true; }

    void ProcessarConversa(std::string inputJogador, float alturaAtual, Vector3 posJogador, Vector3 forwardJogador) {
        AdicionarMensagem(Config.Tr("Voce", "You"), inputJogador, LIGHTGRAY); pensando = true;
        
        std::thread([this, inputJogador, alturaAtual, posJogador, forwardJogador]() {
            std::this_thread::sleep_for(1200ms); 
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
            
            if (cmds["CLIMA"] != "") {
                if (cmds["CLIMA"] == "TEMPESTADE") climaGlobal = 2; else if (cmds["CLIMA"] == "CHUVA") climaGlobal = 1; else if (cmds["CLIMA"] == "LIMPO") climaGlobal = 0; else if (cmds["CLIMA"] == "NEVOEIRO") climaGlobal = 3;
            }
            if (cmds["BIOMA"] != "") { 
                biomaAlvo = (cmds["BIOMA"]=="DESERTO")?1 : (cmds["BIOMA"]=="MAGMA")?2 : (cmds["BIOMA"]=="GELO")?3 : 0; 
                if (biomaAtual != biomaAlvo) biomaBlend = 0.0f; 
            }
            if (cmds["TEMPO"] != "") {
                if (cmds["TEMPO"] == "CONGELAR") timeScale = 0.0f; else if (cmds["TEMPO"] == "ACELERAR") timeScale = 5.0f; else if (cmds["TEMPO"] == "NORMAL") timeScale = 1.0f; 
                else if (cmds["TEMPO"] == "GRAVIDADE") gravidadeBase = 5.0f; else if (cmds["TEMPO"] == "GRAVIDADE_NORMAL") gravidadeBase = 20.0f;
                else if (cmds["TEMPO"] == "PORTAL") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 15.0f)); mundoMutex.lock(); entidadesGlobais.push_back({7, dest, {0,0,0}, 2.0f, 15.0f}); mundoMutex.unlock(); }
            }
            
            if (cmds["ATAQUE"] == "BURACO_NEGRO") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 40.0f)); mundoMutex.lock(); entidadesGlobais.push_back({1, {dest.x, dest.y+20.0f, dest.z}, {0,0,0}, 10.0f, 10.0f}); mundoMutex.unlock(); cameraShake = 2.0f; } 
            else if (cmds["ATAQUE"] == "METEOROS") { mundoMutex.lock(); for (int i=0; i<5; i++) entidadesGlobais.push_back({2, {posJogador.x + (rand()%60 - 30), posJogador.y + 80.0f + (i*10), posJogador.z + (rand()%60 - 30)}, {0, -50.0f, 0}, 3.0f, 5.0f}); mundoMutex.unlock(); } 
            else if (cmds["ATAQUE"] == "ONDA_CHOQUE") { mundoMutex.lock(); entidadesGlobais.push_back({3, posJogador, {0,0,0}, 0.5f, 3.0f}); mundoMutex.unlock(); cameraShake = 1.0f; }
            
            if (cmds["DEFESA"] != "") {
                if (cmds["DEFESA"] == "ESCUDO_ON") escudoAtivo = true; else if (cmds["DEFESA"] == "ESCUDO_OFF") escudoAtivo = false;
                else if (cmds["DEFESA"] == "CAMUFLAGEM_ON") camuflagemAtiva = true; else if (cmds["DEFESA"] == "CAMUFLAGEM_OFF") camuflagemAtiva = false;
                else if (cmds["DEFESA"] == "DIAMANTE") { Vector3 alvo = Vector3Add(posJogador, Vector3Scale(forwardJogador, 10.0f)); mundoMutex.lock(); for (int x=-5; x<=5; x++) for (int y=0; y<6; y++) filaConstrucao.push_back({{(float)round(alvo.x)+x, LerAlturaMundo(alvo.x, alvo.z)+y, (float)round(alvo.z)}, SKYBLUE}); mundoMutex.unlock(); }
            }
            
            if (cmds["INVOCAR"] == "PASSARO") { mundoMutex.lock(); entidadesGlobais.push_back({4, {posJogador.x, posJogador.y + 10.0f, posJogador.z}, {0,0,0}, 0.5f, 60.0f}); mundoMutex.unlock(); } 
            else if (cmds["INVOCAR"] == "GOLEM") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 10.0f)); mundoMutex.lock(); entidadesGlobais.push_back({6, dest, {0,0,0}, 1.0f, 60.0f}); mundoMutex.unlock(); } 
            else if (cmds["INVOCAR"] == "CLONE") { Vector3 dest = Vector3Add(posJogador, Vector3Scale(forwardJogador, 5.0f)); mundoMutex.lock(); entidadesGlobais.push_back({5, dest, {0,0,0}, 1.0f, 30.0f}); mundoMutex.unlock(); }
            
            if (cmds["HACK"] != "") {
                if (cmds["HACK"] == "WIREFRAME_ON") modoWireframe = true; else if (cmds["HACK"] == "WIREFRAME_OFF") modoWireframe = false;
                else if (cmds["HACK"] == "LUZES_ON") luzesAtivas = true; else if (cmds["HACK"] == "LUZES_OFF") luzesAtivas = false;
                else if (cmds["HACK"] == "GODMODE_ON") godModeAtivo = true; else if (cmds["HACK"] == "GODMODE_OFF") godModeAtivo = false;
                else if (cmds["HACK"] == "RESET") { mundoMutex.lock(); modificacoesTerreno.clear(); modificacoesAlvo.clear(); construcoesFixas.clear(); filaConstrucao.clear(); entidadesGlobais.clear(); particulas.clear(); biomaAlvo=0; biomaBlend=0.0f; climaGlobal=0; timeScale=1.0f; gravidadeBase=20.0f; escudoAtivo=false; camuflagemAtiva=false; godModeAtivo=false; modoWireframe=false; mundoMutex.unlock(); cameraShake = 1.5f; }
            }
            
            if (cmds["TERRENO"] != "") {
                Vector3 alvo = Vector3Add(posJogador, Vector3Scale(forwardJogador, 20.0f)); int cx = round(alvo.x); int cz = round(alvo.z);
                std::lock_guard<std::mutex> lock(mundoMutex);
                if (cmds["TERRENO"] == "MONTANHA") { for(int dx=-12; dx<=12; dx++) for(int dz=-12; dz<=12; dz++) if(sqrt(dx*dx + dz*dz) < 12) modificacoesAlvo[{cx+dx, cz+dz}] = modificacoesTerreno[{cx+dx, cz+dz}] + (12 - sqrt(dx*dx + dz*dz)) * 1.5f; } 
                else if (cmds["TERRENO"] == "ABISMO") { for(int dx=-8; dx<=8; dx++) for(int dz=-8; dz<=8; dz++) if(sqrt(dx*dx + dz*dz) < 8) modificacoesAlvo[{cx+dx, cz+dz}] = modificacoesTerreno[{cx+dx, cz+dz}] - (8 - sqrt(dx*dx + dz*dz)) * 2.0f; } 
                else if (cmds["TERRENO"] == "PLANICIE") { float hM = GerarAlturaTerrenoBase(cx, cz); for(int dx=-10; dx<=10; dx++) for(int dz=-10; dz<=10; dz++) if (sqrt(dx*dx + dz*dz) < 10) modificacoesAlvo[{cx+dx, cz+dz}] = hM - GerarAlturaTerrenoBase(cx+dx, cz+dz); } 
                else if (cmds["TERRENO"] == "LAGO") { for(int dx=-8; dx<=8; dx++) for(int dz=-8; dz<=8; dz++) if(sqrt(dx*dx + dz*dz) < 8) { modificacoesAlvo[{cx+dx, cz+dz}] -= 4.0f; filaConstrucao.push_back({{ (float)cx+dx, GerarAlturaTerrenoBase(cx, cz)-1.0f, (float)cz+dz }, BLUE}); } }
                cameraShake = 2.0f; 
            }

            if (cmds["CONSTRUCAO"] != "") {
                Vector3 alvo = Vector3Add(posJogador, Vector3Scale(forwardJogador, 20.0f)); int cx = round(alvo.x); int cz = round(alvo.z);
                std::lock_guard<std::mutex> lock(mundoMutex); float bA = LerAlturaMundo(cx, cz);
                if (cmds["CONSTRUCAO"] == "PIRAMIDE") { for (int y=0; y<8; y++) for (int x=-8+y; x<=8-y; x++) for (int z=-8+y; z<=8-y; z++) filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, GOLD}); } 
                else if (cmds["CONSTRUCAO"] == "TORRE") { for (int y=0; y<15; y++) for (int x=-2; x<=2; x++) for (int z=-2; z<=2; z++) if (x==-2||x==2||z==-2||z==2||y==14) filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, DARKGRAY}); } 
                else if (cmds["CONSTRUCAO"] == "PONTE") { for (int d=-10; d<=10; d++) for (int w=-1; w<=1; w++) filaConstrucao.push_back({{ (float)cx+d, bA, (float)cz+w }, BROWN}); } 
                else if (cmds["CONSTRUCAO"] == "MURALHA") { for (int d=-15; d<=15; d++) for (int y=0; y<5; y++) filaConstrucao.push_back({{ (float)cx+d, bA+y, (float)cz }, LIGHTGRAY}); } 
                else if (cmds["CONSTRUCAO"] == "ABRIGO") { for (int y=0; y<4; y++) for (int x=-3; x<=3; x++) for (int z=-3; z<=3; z++) if (y==3||x==-3||x==3||z==-3||z==3) { if (y<2&&z==3&&x>=-1&&x<=1) continue; filaConstrucao.push_back({{ (float)cx+x, bA+y, (float)cz+z }, BROWN}); } }
            }

            AdicionarMensagem(nome, ia.texto, GOLD); pensando = false;
        }).detach();
    }
};

// ==============================================================================
// 5. MENUS AVANÇADOS (SETTINGS & CREDITS)
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
    DrawTextCenter("V21.0 - THE ZUBA SHADER UPDATE", startY + 60, 20, LIGHTGRAY);
    
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
// 6. MOTOR PRINCIPAL & GAME LOOP
// ==============================================================================
int main(void) {
    InitWindow(1920, 1080, "SUPERGEMINI CREATOR ENGINE - V21.0 THE ZUBA SHADER UPDATE");
    ToggleFullscreen(); 
    SetExitKey(0); SetTargetFPS(60);
    Audio.Init(); 

    Font fUniv = GetFontDefault();
    if (FileExists("arial.ttf")) {
        int codepoints[512] = {0}; for (int i = 0; i < 512; i++) codepoints[i] = i;
        fUniv = LoadFontEx("arial.ttf", 24, codepoints, 512); SetTextureFilter(fUniv.texture, TEXTURE_FILTER_BILINEAR); 
    }
    auto DrawTextCenter = [&](const char* txt, float y, float size, Color c) { float w = MeasureTextEx(fUniv, txt, size, 1).x; DrawTextEx(fUniv, txt, {GetScreenWidth()/2.0f - w/2.0f, y}, size, 1, c); };

    Camera3D camera = { 0 }; camera.position = (Vector3){ 0.0f, 15.0f, 0.0f }; camera.target = (Vector3){ 0.0f, 15.0f, 1.0f }; camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; camera.fovy = 60.0f; camera.projection = CAMERA_PERSPECTIVE;
    float cameraYaw = 0.0f; float cameraPitch = 0.0f;

    CompanheiroMagico companheiro;
    companheiro.CarregarModeloAvatar("zubamodel.glb", "zubamodel.png");

    bool modoChat = false; bool modoDiario = false; bool chatMinimizado = false; int paginaDiario = 0; std::string inputAtual = "";
    GameState estadoAtual = MENU; bool sairDoJogo = false; bool jogoIniciado = false; int menuSelecionado = 0; int chatScrollIndex = 0; 
    int settingsSelecionado = 0;

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
            DrawTextCenter("V21.0 THE ZUBA SHADER UPDATE", alturaTela/2 - 180, 20, LIGHTGRAY);
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
                        biomaAtual=0; biomaAlvo=0; biomaBlend=1.0f; climaGlobal=0; timeScale=1.0f; gravidadeBase=20.0f; escudoAtivo=false; camuflagemAtiva=false; godModeAtivo=false; modoWireframe=false; cameraShake=0.0f;
                        mundoMutex.unlock(); companheiro.Iniciar(); chatMinimizado = false; jogoIniciado = true; DisableCursor(); 
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
            Vector3 forward = { sinf(cameraYaw) * cosf(cameraPitch), sinf(cameraPitch), cosf(cameraYaw) * cosf(cameraPitch) };

            if (IsKeyPressed(KEY_TAB) && !modoChat) { modoDiario = !modoDiario; paginaDiario = 0; }

            if (IsKeyPressed(KEY_T) && !modoChat && !modoDiario) { modoChat = true; EnableCursor(); } 
            else if ((IsKeyPressed(KEY_ESCAPE) || (IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_ALT))) && modoChat) {
                if (IsKeyPressed(KEY_ENTER) && !inputAtual.empty()) {
                    mundoMutex.lock(); float aL = LerAlturaMundo(camera.position.x, camera.position.z); mundoMutex.unlock();
                    companheiro.ProcessarConversa(inputAtual, aL, camera.position, forward); chatMinimizado = false; 
                }
                modoChat = false; inputAtual = ""; DisableCursor(); 
            } else if (IsKeyPressed(KEY_ESCAPE) && !modoChat) {
                if (modoDiario) modoDiario = false; else { estadoAtual = MENU; EnableCursor(); }
            }

            if (modoChat) {
                int key = GetCharPressed();
                while (key > 0) { int bS=0; const char* uC=CodepointToUTF8(key, &bS); for(int b=0; b<bS; b++) inputAtual+=uC[b]; key = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE) && inputAtual.length() > 0) inputAtual.pop_back();
                float wheelMove = GetMouseWheelMove(); if (wheelMove != 0) chatScrollIndex -= (int)(wheelMove * 3);
            } else if (!modoDiario) {
                float velA = IsKeyDown(KEY_LEFT_SHIFT) ? velBase * 2.5f : velBase; if (godModeAtivo) velA *= 2.0f; 

                Vector2 mouseDelta = GetMouseDelta(); cameraYaw -= mouseDelta.x * 0.003f; cameraPitch -= mouseDelta.y * 0.003f; 
                if (cameraPitch > 1.5f) cameraPitch = 1.5f; if (cameraPitch < -1.5f) cameraPitch = -1.5f;

                forward = { sinf(cameraYaw) * cosf(cameraPitch), sinf(cameraPitch), cosf(cameraYaw) * cosf(cameraPitch) };
                Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z}); Vector3 right = { flatForward.z, 0.0f, -flatForward.x };

                bool moves = false;
                if (IsKeyDown(KEY_W)) { camera.position = Vector3Add(camera.position, Vector3Scale(flatForward, velA * rawDt)); moves = true; } 
                if (IsKeyDown(KEY_S)) { camera.position = Vector3Subtract(camera.position, Vector3Scale(flatForward, velA * rawDt)); moves = true; }
                if (IsKeyDown(KEY_D)) { camera.position = Vector3Subtract(camera.position, Vector3Scale(right, velA * rawDt)); moves = true; }
                if (IsKeyDown(KEY_A)) { camera.position = Vector3Add(camera.position, Vector3Scale(right, velA * rawDt)); moves = true; }

                if (godModeAtivo) {
                    if (IsKeyDown(KEY_F)) camera.position.y += velA * rawDt; if (IsKeyDown(KEY_C)) camera.position.y -= velA * rawDt;
                } else {
                    if (moves && noChao) { headBobTimer += rawDt * (velA * 2.0f); camera.position.y += sin(headBobTimer) * 0.05f; } else headBobTimer = 0;
                    mundoMutex.lock(); float alturaChao = LerAlturaMundo(camera.position.x, camera.position.z); mundoMutex.unlock();
                    if (IsKeyPressed(KEY_SPACE) && noChao) { gravidade = 10.0f; noChao = false; }
                    camera.position.y += gravidade * rawDt; gravidade -= gravidadeBase * rawDt; 
                    float alturaDesejadaOlhos = alturaChao + 2.5f; 
                    if (camera.position.y <= alturaDesejadaOlhos) { camera.position.y = alturaDesejadaOlhos; gravidade = 0.0f; noChao = true; }
                }
                camera.target = Vector3Add(camera.position, forward);
            }

            companheiro.AtualizarFisica(camera.position, forward, rawDt); 

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
                else if (ent.tipo == 4) { ent.tempoVida -= dt; ent.pos.x = camera.position.x + sin(ent.tempoVida*3.0f)*5.0f; ent.pos.y = camera.position.y + 4.0f + sin(ent.tempoVida*5.0f); ent.pos.z = camera.position.z + cos(ent.tempoVida*3.0f)*5.0f; } 
                else if (ent.tipo == 1 || ent.tipo == 5 || ent.tipo == 6 || ent.tipo == 7) ent.tempoVida -= dt;
            }
            entidadesGlobais.erase(std::remove_if(entidadesGlobais.begin(), entidadesGlobais.end(), [](const EntidadeDivina& e) { return e.tempoVida <= 0; }), entidadesGlobais.end());
            for (auto& p : particulas) { p.vida -= rawDt; p.vel.y -= 9.8f * rawDt; p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, rawDt)); }
            particulas.erase(std::remove_if(particulas.begin(), particulas.end(), [](const Particula& p) { return p.vida <= 0; }), particulas.end());
            mundoMutex.unlock();

            BeginDrawing();
                Color corCeuAlvo = {135, 206, 235, 255}; if (climaGlobal == 3) corCeuAlvo = GRAY; else if (biomaAlvo == 2) corCeuAlvo = {40, 10, 10, 255}; else if (biomaAlvo == 3) corCeuAlvo = {173, 216, 230, 255}; 
                Color corCeuAtual = {135, 206, 235, 255}; if (climaGlobal == 3) corCeuAtual = GRAY; else if (biomaAtual == 2) corCeuAtual = {40, 10, 10, 255}; else if (biomaAtual == 3) corCeuAtual = {173, 216, 230, 255}; 
                Color corCeu = LerpColor(corCeuAtual, corCeuAlvo, biomaBlend); if (climaGlobal == 2 && rand()%100 > 95) corCeu = WHITE; 

                ClearBackground(corCeu);

                Camera3D renderCam = camera; renderCam.position = Vector3Add(renderCam.position, cameraShakeOffset); renderCam.target = Vector3Add(renderCam.target, cameraShakeOffset);

                BeginMode3D(renderCam);
                    int rDistFinal = (climaGlobal == 3) ? 20 : RENDER_DIST; Vector3 lightDir = Vector3Normalize({ -0.6f, 1.0f, -0.6f }); 
                    auto ApplyFog = [&](Color cOriginal, float distCamera) -> Color {
                        float fogStart = rDistFinal * 0.4f; float fogEnd = rDistFinal * 0.95f; float f = Clamp((distCamera - fogStart) / (fogEnd - fogStart), 0.0f, 1.0f); return LerpColor(cOriginal, corCeu, f);
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

                    if (climaGlobal == 0 && biomaAtual != 2 && biomaAtual != 3 && !modoWireframe) DrawSphere(Vector3Add(camera.position, Vector3Scale({ -0.6f, 0.6f, -0.6f }, rDistFinal * 1.2f)), 20.0f, {255, 250, 150, 255}); 

                    rlBegin(RL_QUADS);
                    for (int x = -rDistFinal; x < rDistFinal; x++) {
                        for (int z = -rDistFinal; z < rDistFinal; z++) {
                            float distCentro = sqrt(x*x + z*z); if (distCentro > rDistFinal) continue; 
                            float wX = playerChunkX + x; float wZ = playerChunkZ + z;
                            float h00 = LerAlturaMundo(wX, wZ); float h10 = LerAlturaMundo(wX + 1, wZ); float h01 = LerAlturaMundo(wX, wZ + 1); float h11 = LerAlturaMundo(wX + 1, wZ + 1);
                            Vector3 v0 = {wX, h00, wZ}; Vector3 v1 = {wX, h01, wZ + 1}; Vector3 v2 = {wX + 1, h11, wZ + 1};
                            Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v1, v0), Vector3Subtract(v2, v0))); if (n.y < 0) n = Vector3Scale(n, -1.0f); 
                            float intLuz = Clamp(0.6f + (Vector3DotProduct(n, lightDir) * 0.4f), 0.5f, 1.0f); 
                            
                            Color cb = ObterCorBiomaDinâmico(wX, wZ, h00, n); Color cS = { (unsigned char)(cb.r * intLuz), (unsigned char)(cb.g * intLuz), (unsigned char)(cb.b * intLuz), cb.a };
                            Color cFinal = ApplyFog(cS, distCentro);
                            if (modoWireframe) {
                                rlEnd(); DrawLine3D({wX, h00, wZ}, {wX+1, h10, wZ}, cFinal); DrawLine3D({wX+1, h10, wZ}, {wX+1, h11, wZ+1}, cFinal); DrawLine3D({wX+1, h11, wZ+1}, {wX, h01, wZ+1}, cFinal); DrawLine3D({wX, h01, wZ+1}, {wX, h00, wZ}, cFinal); rlBegin(RL_QUADS);
                            } else {
                                rlColor4ub(cFinal.r, cFinal.g, cFinal.b, cFinal.a); rlVertex3f(wX, h00, wZ); rlVertex3f(wX, h01, wZ + 1); rlVertex3f(wX + 1, h11, wZ + 1); rlVertex3f(wX + 1, h10, wZ);
                            }
                        }
                    }
                    rlEnd();

                    if (!modoWireframe) {
                        for (int x = -rDistFinal; x < rDistFinal; x++) {
                            for (int z = -rDistFinal; z < rDistFinal; z++) {
                                float distCentro = sqrt(x*x + z*z); if (distCentro > rDistFinal) continue;
                                float wX = playerChunkX + x; float wZ = playerChunkZ + z; float h = LerAlturaMundo(wX, wZ);
                                Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract({wX, LerAlturaMundo(wX, wZ+1), wZ+1}, {wX, h, wZ}), Vector3Subtract({wX+1, LerAlturaMundo(wX+1, wZ), wZ+1}, {wX, h, wZ}))); if (n.y < 0) n = Vector3Scale(n, -1.0f); 
                                float slope = 1.0f - n.y; float pathNoise = abs(FBM(wX * 0.02f, wZ * 0.02f, 2) - 0.5f);

                                if (biomaAtual == 0 && biomaAlvo == 0 && slope < 0.3f && h >= 0.0f && h < 15.0f) {
                                    float prob = RandPseudo(wX, wZ);
                                    if (pathNoise > 0.05f && pathNoise < 0.1f && prob > 0.95f) { DrawCylinder({wX, h, wZ}, 0.05f, 0.05f, 1.0f, 4, ApplyFog(LIME, distCentro)); DrawSphere({wX, h + 1.0f, wZ}, 0.25f, ApplyFog(YELLOW, distCentro)); DrawSphere({wX+0.1f, h + 1.0f, wZ+0.1f}, 0.15f, ApplyFog({100, 50, 0, 255}, distCentro)); 
                                    } else if (pathNoise > 0.1f) {
                                        if (prob > 0.99f) { DrawCylinder({wX, h, wZ}, 0.15f, 0.15f, 1.0f, 5, ApplyFog({101, 67, 33, 255}, distCentro)); DrawCylinder({wX, h+1.0f, wZ}, 1.2f, 0.0f, 2.5f, 5, ApplyFog({34, 139, 34, 255}, distCentro)); DrawCylinder({wX, h+2.0f, wZ}, 0.9f, 0.0f, 2.0f, 5, ApplyFog({46, 184, 46, 255}, distCentro)); 
                                        } else if (prob > 0.95f) DrawSphere({wX, h + 0.2f, wZ}, 0.2f, ApplyFog({148, 0, 211, 255}, distCentro)); 
                                    }
                                } else if (biomaAtual == 1 && biomaAlvo == 1 && h >= 0.0f && h < 5.0f) {
                                    if (RandPseudo(wX, wZ) > 0.99f) { DrawCylinder({wX, h, wZ}, 0.3f, 0.3f, 1.5f, 6, ApplyFog(LIME, distCentro)); DrawSphere({wX, h+1.5f, wZ}, 0.3f, ApplyFog(LIME, distCentro)); }
                                }
                            }
                        }
                    }

                    if (!modoWireframe && (biomaAtual != 1 && biomaAlvo != 1) && (biomaAtual != 3 && biomaAlvo != 3)) {
                        rlBegin(RL_QUADS);
                        for (int x = -rDistFinal; x < rDistFinal; x+=2) {
                            for (int z = -rDistFinal; z < rDistFinal; z+=2) {
                                float distCentro = sqrt(x*x + z*z); if (distCentro > rDistFinal) continue; 
                                float wX = playerChunkX + x; float wZ = playerChunkZ + z;
                                if (LerAlturaMundo(wX, wZ) < -0.8f || LerAlturaMundo(wX+2, wZ) < -0.8f || LerAlturaMundo(wX, wZ+2) < -0.8f) {
                                    Color cAguaAtual = (biomaAtual == 2) ? Color{255, 69, 0, 220} : Color{0, 191, 255, 180}; Color cAguaAlvo = (biomaAlvo == 2) ? Color{255, 69, 0, 220} : Color{0, 191, 255, 180};
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
                    }
                    
                    for (const auto& p : particulas) { Color pc = p.cor; pc.a = (unsigned char)(255 * (p.vida/p.maxVida)); DrawCube(p.pos, p.tamanho, p.tamanho, p.tamanho, pc); }
                    mundoMutex.unlock();

                    if (climaGlobal == 1 || climaGlobal == 2) {
                        for (int i=0; i<300; i++) { Vector3 pR = {camera.position.x + (rand()%40-20), camera.position.y + (rand()%20), camera.position.z + (rand()%40-20)}; DrawLine3D(pR, {pR.x, pR.y-1.0f, pR.z}, Fade(BLUE, 0.5f)); }
                    }
                    if (escudoAtivo) DrawSphereWires(camera.position, 3.0f, 16, 16, Fade(SKYBLUE, 0.5f));

                    companheiro.Desenhar3D();
                EndMode3D();

                // UI & DIÁRIO DE BORDO
                if (!modoChat && !modoDiario) {
                    DrawLine(larguraTela/2 - 10, alturaTela/2, larguraTela/2 + 10, alturaTela/2, WHITE); DrawLine(larguraTela/2, alturaTela/2 - 10, larguraTela/2, alturaTela/2 + 10, WHITE);
                    DrawTextEx(fUniv, Config.Tr("T: Chat | TAB: Diario | ESPACO: Pular | SHIFT: Correr | ESC: Menu", "T: Chat | TAB: Logbook | SPACE: Jump | SHIFT: Sprint | ESC: Menu"), {20, 20}, 20, 1, LIGHTGRAY);
                    if (timeScale == 0.0f) DrawTextCenter(Config.Tr("TEMPO CONGELADO", "TIME FROZEN"), 100, 24, SKYBLUE);
                    if (godModeAtivo) DrawTextEx(fUniv, Config.Tr("GOD MODE (F: Voar, C: Descer)", "GOD MODE (F: Fly, C: Descend)"), {20, 50}, 20, 1, GOLD);
                }

                if (modoDiario) {
                    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { if (paginaDiario < 11) paginaDiario++; }
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { if (paginaDiario > 0) paginaDiario--; }

                    DrawRectangle(larguraTela/2 - 420, 50, 840, alturaTela - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({(float)larguraTela/2 - 420, 50, 840, (float)alturaTela - 100}, 3, RED); 
                    DrawTextCenter(TextFormat(Config.Tr("TOMO DE ZUBA (Pag %d/12)", "ZUBA TOME (Pg %d/12)"), paginaDiario + 1), 80, 24, ORANGE);
                    
                    std::vector<std::string> pags = {
                        Config.Tr("ZUBA - IA ONISCIENTE\n\n[ MUTACOES ]\n- 'Transforme-se em cubo!'\n- 'Piramide'\n- 'Esfera'\n- 'Cubo OFF' (cancela a forma)\n\n[ MASSA ]\n- 'Fique gigante'\n- 'Fique pequena'\n- 'Gigante OFF' (volta ao normal)", "ZUBA - OMNISCIENT AI\n\n[ MUTATIONS ]\n- 'Transform into a cube!'\n- 'Pyramid'\n- 'Sphere'\n- 'Cube OFF' (cancels shape)\n\n[ MASS ]\n- 'Get giant'\n- 'Get small'\n- 'Giant OFF' (back to normal)"),
                        Config.Tr("ACOES FISICAS\n\n[ NAVEGACAO ]\n- 'Fique parada ai'\n- 'Volte a me seguir'\n- 'Va la para a frente!'\n- 'Atire um laser'\n- 'Laser OFF' (desliga acao)", "PHYSICAL ACTIONS\n\n[ NAVIGATION ]\n- 'Stop right there'\n- 'Follow me again'\n- 'Go forward!'\n- 'Shoot a laser'\n- 'Laser OFF' (stops action)"),
                        Config.Tr("TERRAFORMACAO\n\n[ GOD TIER ]\n- 'Zuba, erga uma montanha!'\n- 'Achate o terreno.'\n- 'Abra um abismo.'\n- 'Inunde o vale.'\n\nTransformacao suave e gera terramotos.\n\nUse 'Terraformar OFF' para cancelar.", "TERRAFORMING\n\n[ GOD TIER ]\n- 'Zuba, raise a mountain!'\n- 'Flatten the terrain.'\n- 'Open an abyss.'\n- 'Flood the valley.'\n\nSmooth transition creates earthquakes.\n\nUse 'Terraform OFF' to cancel."),
                        Config.Tr("SINTESE DE ITENS\n\n[ FORJA INSTANTANEA ]\n- 'Crie uma espada!'\n- 'Arco e flecha'\n- 'Picareta'\n- 'Espada OFF' (para guardar a arma)", "ITEM SYNTHESIS\n\n[ INSTANT FORGE ]\n- 'Create a sword!'\n- 'Bow and arrow'\n- 'Pickaxe'\n- 'Sword OFF' (to put weapon away)"),
                        Config.Tr("ARQUITETURA\n\n[ CONSTRUTOR MASTER ]\n- 'Construa uma piramide gigante!'\n- 'Torre de vigia.'\n- 'Ponte'\n- 'Abrigo'\n- 'Muralha gigante.'\n\nAnimacao procedural de construcao ativada.", "ARCHITECTURE\n\n[ MASTER BUILDER ]\n- 'Build a giant pyramid!'\n- 'Watchtower.'\n- 'Bridge'\n- 'Shelter'\n- 'Giant wall.'\n\nProcedural building animation active."),
                        Config.Tr("CLIMA\n\n[ DOMINIO ATMOSFERICO ]\n- 'Invoque uma tempestade!'\n- 'Faca chover.'\n- 'Nevoeiro denso.'\n- 'Tempestade OFF' (limpa o tempo)", "WEATHER\n\n[ ATMOSPHERIC DOMAIN ]\n- 'Summon a storm!'\n- 'Make it rain.'\n- 'Dense fog.'\n- 'Storm OFF' (clears weather)"),
                        Config.Tr("ESPACO-TEMPO\n\n[ DOBRADORA DE REALIDADE ]\n- 'Congele o tempo!'\n- 'Acelere o tempo.'\n- 'Abra um portal.'\n- 'Altere a gravidade'\n- 'Gravidade OFF' (restaura normalidade)", "SPACE-TIME\n\n[ REALITY BENDER ]\n- 'Freeze time!'\n- 'Accelerate time.'\n- 'Open a portal.'\n- 'Alter gravity'\n- 'Gravity OFF' (restores normality)"),
                        Config.Tr("BIOMAS E PROTOCOLOS\n\n[ CRIADOR DE MUNDOS ]\n- 'Bioma deserto.'\n- 'Ative o PROTOCOLO MAGMA!'\n- 'Floresta densa.'\n- 'Gelo eterno.'\n- 'Protocolo Magma OFF' (reverte transicao suavemente)", "BIOMES AND PROTOCOLS\n\n[ WORLD CREATOR ]\n- 'Desert biome.'\n- 'Activate MAGMA PROTOCOL!'\n- 'Dense forest.'\n- 'Eternal ice.'\n- 'Magma Protocol OFF' (reverts transition smoothly)"),
                        Config.Tr("MAGIA OFENSIVA\n\n[ DESTRUICAO DIVINA ]\n- 'Chuva de meteoros!'\n- 'Buraco negro.'\n- 'Onda de choque.'\n\nA destruicao deforma o solo e emite particulas.", "OFFENSIVE MAGIC\n\n[ DIVINE DESTRUCTION ]\n- 'Meteor shower!'\n- 'Black hole.'\n- 'Shockwave.'\n\nDestruction deforms ground and emits particles."),
                        Config.Tr("DEFESA ABSOLUTA\n\n[ BARREIRAS ]\n- 'Gere um escudo magico.'\n- 'Parede de diamante.'\n- 'Camuflagem optica.'\n- 'Escudo OFF' (Desativa defesas)", "ABSOLUTE DEFENSE\n\n[ BARRIERS ]\n- 'Generate magic shield.'\n- 'Diamond wall.'\n- 'Optical camo.'\n- 'Shield OFF' (Deactivates defenses)"),
                        Config.Tr("ENTIDADES\n\n[ SINTESE DE VIDA ]\n- 'Invoque um passaro.'\n- 'Clone holografico.'\n- 'Golem de pedra.'\n\nAs entidades possuem tempo de vida limitado.", "ENTITIES\n\n[ SYNTHESIS OF LIFE ]\n- 'Summon a bird.'\n- 'Holographic clone.'\n- 'Stone golem.'\n\nEntities have limited lifespans."),
                        Config.Tr("HACKING DO MOTOR\n\n[ ACESSO CORE ]\n- 'Modo Wireframe.' \n- 'Luzes bioluminescentes.'\n- 'Ative GOD MODE!'\n- 'Wireframe OFF'\n- 'RESET MUNDIAL' (Apaga tudo definitivamente!)", "ENGINE HACKING\n\n[ CORE ACCESS ]\n- 'Wireframe Mode.' \n- 'Bioluminescent lights.'\n- 'Activate GOD MODE!'\n- 'Wireframe OFF'\n- 'WORLD RESET' (Erases everything permanently!)")
                    };
                    DrawTextEx(fUniv, pags[paginaDiario].c_str(), {larguraTela/2.0f - 380.0f, 140}, 20, 1, LIGHTGRAY);
                    DrawTextCenter(Config.Tr("TAB para fechar | SETAS para mudar de pagina", "TAB to close | ARROWS to switch pages"), alturaTela - 60.0f, 18, GRAY);
                }

                if (!modoDiario) {
                    std::lock_guard<std::mutex> lock(companheiro.chatMutex);
                    int larguraCaixaChat = larguraTela / 2 + 100; int alturaCaixa = chatMinimizado ? 40 : 300; int startY = alturaTela - alturaCaixa; 
                    DrawRectangle(20, startY - 10, larguraCaixaChat, alturaCaixa, Fade(BLACK, 0.7f));
                    
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
                            std::vector<std::string> linhasTexto = QuebrarTextoEx(msg.texto, larguraCaixaChat - MeasureTextEx(fUniv, prefixo.c_str(), 20, 1).x - 60, fUniv, 20);
                            if (linhasTexto.empty()) continue;
                            todasLinhasVisuais.push_back({prefixo + linhasTexto[0], 30.0f, msg.cor}); float mTxt = 30.0f + MeasureTextEx(fUniv, prefixo.c_str(), 20, 1).x;
                            for (size_t l = 1; l < linhasTexto.size(); l++) todasLinhasVisuais.push_back({linhasTexto[l], mTxt, msg.cor});
                            todasLinhasVisuais.push_back({"", 30.0f, BLANK});
                        }
                        int maxLinhasNaTela = 10; int maxScroll = todasLinhasVisuais.size() - maxLinhasNaTela; if (maxScroll < 0) maxScroll = 0;
                        if (companheiro.forcadoScroll) { chatScrollIndex = maxScroll; companheiro.forcadoScroll = false; }
                        if (chatScrollIndex < 0) chatScrollIndex = 0; if (chatScrollIndex > maxScroll) chatScrollIndex = maxScroll;
                        if (maxScroll > 0) {
                            float tamBarra = (alturaCaixa - 20) * ((float)maxLinhasNaTela / todasLinhasVisuais.size());
                            DrawRectangle(20 + larguraCaixaChat - 10, startY - 10 + ((alturaCaixa - 20) - tamBarra) * ((float)chatScrollIndex / maxScroll), 6, tamBarra, GRAY);
                        }
                        int desenhadas = 0;
                        for (size_t i = chatScrollIndex; i < todasLinhasVisuais.size() && desenhadas < maxLinhasNaTela; i++) {
                            DrawTextEx(fUniv, todasLinhasVisuais[i].txt.c_str(), {todasLinhasVisuais[i].x, (float)startY}, 20, 1, todasLinhasVisuais[i].c); startY += 25; desenhadas++;
                        }
                    } else DrawTextEx(fUniv, companheiro.pensando ? Config.Tr("Zuba processando...", "Zuba processing...") : Config.Tr("Zuba aguarda. (Chat Oculto)", "Zuba awaits. (Hidden Chat)"), {30.0f, (float)startY}, 20, 1, GRAY);

                    if (companheiro.pensando && !chatMinimizado) DrawTextEx(fUniv, Config.Tr("Zuba esta processando algoritmos de NLP...", "Zuba is processing NLP algorithms..."), {30.0f, (float)alturaTela - 30}, 18, 1, GRAY);

                    if (modoChat) {
                        DrawRectangle(20, alturaTela - 50, larguraCaixaChat, 30, Fade(DARKBLUE, 0.9f));
                        DrawTextEx(fUniv, TextFormat(Config.Tr("Voce: %s_", "You: %s_"), inputAtual.c_str()), {30.0f, (float)alturaTela - 45}, 20, 1, WHITE);
                        DrawTextEx(fUniv, Config.Tr("[ENTER] enviar | [ESC] cancelar | Roda Rato: Scroll | Click [-] Esconder", "[ENTER] send | [ESC] cancel | Scroll Wheel | Click [-] Hide"), {20.0f, (float)alturaTela - 70}, 15, 1, GRAY);
                    }
                }
            EndDrawing();
        }
    }
    companheiro.DescarregarModelo();
    Audio.Close();
    UnloadFont(fUniv); CloseWindow(); return 0;
}