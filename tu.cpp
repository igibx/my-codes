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
#include <cstring>

using namespace std;

// =====================================================================
// DINÂMICA WINSOCK2: MODO MULTIPLAYER UDP SEM CONFLITOS COM O WINDOWS.H
// =====================================================================
#ifdef _WIN32
#include <stdint.h>
extern "C" {
    void* __stdcall LoadLibraryA(const char* lpLibFileName);
    void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
}
typedef uintptr_t OS_SOCKET;
#define OS_INVALID_SOCKET (OS_SOCKET)(~0)
#define OS_AF_INET 2
#define OS_SOCK_DGRAM 2
#define OS_IPPROTO_UDP 17
const unsigned long OS_FIONBIO = 0x8004667e;

struct os_sockaddr_in { short sin_family; unsigned short sin_port; unsigned long sin_addr; char sin_zero[8]; };
struct os_WSAData { unsigned short wVersion; unsigned short wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets; unsigned short iMaxUdpDg; char* lpVendorInfo; };

typedef int (__stdcall *FN_WSAStartup)(unsigned short, os_WSAData*);
typedef int (__stdcall *FN_WSACleanup)(void);
typedef OS_SOCKET (__stdcall *FN_socket)(int, int, int);
typedef int (__stdcall *FN_closesocket)(OS_SOCKET);
typedef int (__stdcall *FN_bind)(OS_SOCKET, const void*, int);
typedef int (__stdcall *FN_recvfrom)(OS_SOCKET, char*, int, int, void*, int*);
typedef int (__stdcall *FN_sendto)(OS_SOCKET, const char*, int, int, const void*, int);
typedef int (__stdcall *FN_ioctlsocket)(OS_SOCKET, long, unsigned long*);
typedef unsigned short (__stdcall *FN_htons)(unsigned short);
typedef unsigned long (__stdcall *FN_inet_addr)(const char*);

void* hWs2 = nullptr;
FN_WSAStartup wsaStartup = nullptr;
FN_WSACleanup wsaCleanup = nullptr;
FN_socket osSocket = nullptr;
FN_closesocket osCloseSocket = nullptr;
FN_bind osBind = nullptr;
FN_recvfrom osRecvFrom = nullptr;
FN_sendto osSendTo = nullptr;
FN_ioctlsocket osIoctlSocket = nullptr;
FN_htons osHtons = nullptr;
FN_inet_addr osInetAddr = nullptr;

bool InitWs2() {
    if (hWs2) return true;
    hWs2 = LoadLibraryA("ws2_32.dll");
    if (!hWs2) return false;
    wsaStartup = (FN_WSAStartup)GetProcAddress(hWs2, "WSAStartup");
    wsaCleanup = (FN_WSACleanup)GetProcAddress(hWs2, "WSACleanup");
    osSocket = (FN_socket)GetProcAddress(hWs2, "socket");
    osCloseSocket = (FN_closesocket)GetProcAddress(hWs2, "closesocket");
    osBind = (FN_bind)GetProcAddress(hWs2, "bind");
    osRecvFrom = (FN_recvfrom)GetProcAddress(hWs2, "recvfrom");
    osSendTo = (FN_sendto)GetProcAddress(hWs2, "sendto");
    osIoctlSocket = (FN_ioctlsocket)GetProcAddress(hWs2, "ioctlsocket");
    osHtons = (FN_htons)GetProcAddress(hWs2, "htons");
    osInetAddr = (FN_inet_addr)GetProcAddress(hWs2, "inet_addr");
    os_WSAData d; if(wsaStartup) wsaStartup(0x0202, &d);
    return true;
}

OS_SOCKET netSocket = OS_INVALID_SOCKET;
os_sockaddr_in otherAddr;
int otherAddrLen = sizeof(otherAddr);
#endif

#pragma pack(push, 1)
struct NetPlayerState { int x, y, color, pSize; int piece[4][4]; int boardState[20][14]; int score, bombs; bool isDead; };
#pragma pack(pop)

// =====================================================================
// SISTEMA DE CONTROLE TOUCH (ANDROID / MOBILE)
// =====================================================================
struct TouchBtn {
    Rectangle rec;
    string text;
    Color color;
    bool isDown;
    bool isPressed;
    bool wasDown;
};

// =====================================================================
// CONSTANTES DO MUNDO 3D E RESOLUÇÃO BASE MOBILE
// =====================================================================
const int MAX_BOARD_WIDTH = 1000; const int BOARD_HEIGHT = 20;    
const int SCREEN_WIDTH = 2400; const int SCREEN_HEIGHT = 1080; const float CUBE_SIZE = 0.85f; 
int globalGraphicsQuality = 3; 

const Color C_CYAN = {0,255,255,255}, C_BLUE = {0,100,255,255}, C_ORANGE = {255,120,0,255};
const Color C_YELLOW = {255,255,0,255}, C_GREEN = {0,255,0,255}, C_PURPLE = {180,0,255,255};
const Color C_RED = {255,10,50,255}, C_GOLD = {255,215,0,255}, C_BG = {8,6,12,255};    
Color pieceColors[10] = {BLANK, C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED, DARKGRAY, C_GOLD};

enum GameState { INTRO, MENU, SETTINGS, CREDITS, PLAYING, NET_SETUP }; 
float globalMusicAmplitude = 0.0f;
void AudioInputCallback(void *bufferData, unsigned int frames) {
    float *samples = (float *)bufferData; float sum = 0.0f;
    for(unsigned int i=0; i<frames; i++) sum += fabs(samples[i]);
    globalMusicAmplitude = sum / (float)frames;
}

Color LerpColor(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return {(unsigned char)(a.r+(b.r-a.r)*t), (unsigned char)(a.g+(b.g-a.g)*t), (unsigned char)(a.b+(b.b-a.b)*t), (unsigned char)(a.a+(b.a-a.a)*t)};
}
float ElasticEaseOut(float t) { return sin(-13.0f * (t + 1.0f) * PI / 2.0f) * pow(2.0f, -10.0f * t) + 1.0f; }

// =====================================================================
// PARTÍCULAS AVANÇADAS
// =====================================================================
struct Particle3D { Vector3 position, velocity; Color color; float life, maxLife, size; bool isSpark, isRing; int shapeType; Vector3 rotation, rotVelocity; deque<Vector3> trail; };
vector<Particle3D> particles;
struct FloatingText { Vector3 pos; string text; float life; Color color; float scale; };
vector<FloatingText> floatingTexts;

float GetRandomFloat(float min, float max) { return min + (max - min) * ((float)rand() / RAND_MAX); }
void SpawnFloatingText(Vector3 pos, string text, Color color, float scale = 1.0f) { floatingTexts.push_back({pos, text, 1.5f, color, scale}); }
void SpawnParticles3D(Vector3 pos, Color color, int amount, float force, int shapeType = 0) {
    for (int i = 0; i < amount; i++) {
        Particle3D p; p.position = pos; p.velocity = {GetRandomFloat(-force, force), GetRandomFloat(force*0.2f, force*1.5f), GetRandomFloat(-force, force)};
        p.color = color; p.maxLife = GetRandomFloat(0.5f, 1.5f); p.life = p.maxLife; p.isSpark = (GetRandomFloat(0, 1) > 0.4f); p.isRing = false;
        p.size = p.isSpark ? GetRandomFloat(0.1f, 0.4f) : GetRandomFloat(0.3f, 0.6f); p.shapeType = shapeType;
        p.rotation = {GetRandomFloat(0,360), GetRandomFloat(0,360), GetRandomFloat(0,360)}; p.rotVelocity = {GetRandomFloat(-400,400), GetRandomFloat(-400,400), GetRandomFloat(-400,400)};
        particles.push_back(p);
    }
}
void SpawnShockwave(Vector3 pos, Color color) {
    Particle3D p; p.position = pos; p.velocity = {0,0,0}; p.color = color; p.maxLife = 0.6f; p.life = p.maxLife; p.isSpark = false; p.isRing = true;
    p.size = 0.1f; p.rotation = {90, 0, 0}; p.rotVelocity = {0,0,0}; particles.push_back(p);
}

void UpdateAndDrawParticles3D(float dt) {
    for (int i = particles.size() - 1; i >= 0; i--) {
        if (!particles[i].isRing) {
            particles[i].trail.push_front(particles[i].position); if (particles[i].trail.size() > 8) particles[i].trail.pop_back();
            particles[i].position.x += particles[i].velocity.x * dt; particles[i].position.y += particles[i].velocity.y * dt; particles[i].position.z += particles[i].velocity.z * dt;
            particles[i].velocity.y -= 25.0f * dt; particles[i].velocity.x *= 0.95f; particles[i].velocity.z *= 0.95f;
            particles[i].rotation.x += particles[i].rotVelocity.x * dt; particles[i].rotation.y += particles[i].rotVelocity.y * dt; particles[i].rotation.z += particles[i].rotVelocity.z * dt;
        } else { particles[i].size += dt * 30.0f; }
        particles[i].life -= dt;
        if (particles[i].life <= 0 || particles[i].position.y < -5.0f) particles.erase(particles.begin() + i);
        else {
            float alpha = particles[i].life / particles[i].maxLife; Color fadeColor = particles[i].color; fadeColor.a = (unsigned char)(255 * (alpha * alpha));
            if (particles[i].isRing) {
                rlPushMatrix(); rlTranslatef(particles[i].position.x, particles[i].position.y, particles[i].position.z); rlRotatef(particles[i].rotation.x, 1, 0, 0);
                rlBegin(RL_LINES); rlColor4ub(fadeColor.r, fadeColor.g, fadeColor.b, fadeColor.a);
                int segments = 36;
                for(int s=0; s<segments; s++) {
                    float a1 = (float)s/segments*PI*2.0f; float a2 = (float)(s+1)/segments*PI*2.0f;
                    rlVertex3f(cos(a1)*particles[i].size, sin(a1)*particles[i].size, 0); rlVertex3f(cos(a2)*particles[i].size, sin(a2)*particles[i].size, 0);
                    rlVertex3f(cos(a1)*(particles[i].size*0.9f), sin(a1)*(particles[i].size*0.9f), 0); rlVertex3f(cos(a2)*(particles[i].size*0.9f), sin(a2)*(particles[i].size*0.9f), 0);
                }
                rlEnd(); rlPopMatrix();
            } else if (particles[i].isSpark) {
                rlBegin(RL_LINES);
                for(size_t j=0; j<particles[i].trail.size()-1; j++) {
                    float trailAlpha = fadeColor.a * (1.0f - ((float)j / particles[i].trail.size()));
                    rlColor4ub(fadeColor.r, fadeColor.g, fadeColor.b, (unsigned char)trailAlpha);
                    rlVertex3f(particles[i].trail[j].x, particles[i].trail[j].y, particles[i].trail[j].z); rlVertex3f(particles[i].trail[j+1].x, particles[i].trail[j+1].y, particles[i].trail[j+1].z);
                }
                rlEnd();
            } else {
                rlPushMatrix(); rlTranslatef(particles[i].position.x, particles[i].position.y, particles[i].position.z);
                rlRotatef(particles[i].rotation.x, 1,0,0); rlRotatef(particles[i].rotation.y, 0,1,0); rlRotatef(particles[i].rotation.z, 0,0,1);
                float s = particles[i].size * ElasticEaseOut(1.0f - alpha); 
                DrawCubeWiresV({0,0,0}, {s, s, s}, fadeColor); DrawCubeV({0,0,0}, {s*0.9f, s*0.9f, s*0.9f}, ColorAlpha(fadeColor, alpha * 0.6f));
                rlPopMatrix();
            }
        }
    }
}

// =====================================================================
// SISTEMA DE PEÇAS
// =====================================================================
struct Tetromino { vector<vector<int>> shape; int colorID; };
vector<Tetromino> pieces = {
    { {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, 1 }, { {{0,0,0},{1,1,1},{0,0,0}}, 2 }, { {{0,1,0},{1,1,1},{0,0,0}}, 3 },
    { {{1,1},{1,1}}, 4 }, { {{1,0},{1,1}}, 5 }, { {{0,1},{1,1}}, 6 }, { {{0,0,1},{1,1,1},{0,0,0}}, 2 }, { {{1,1,0},{0,1,1},{0,0,0}}, 1 },
    { {{1,1,1},{1,1,1},{0,0,0}}, 9 }, { {{1,1,1},{1,1,0},{0,0,0}}, 6 }
};
vector<vector<int>> RotateMatrix(const vector<vector<int>>& mat) {
    int n = mat.size(); vector<vector<int>> res(n, vector<int>(n, 0));
    for(int i=0; i<n; ++i) for(int j=0; j<n; ++j) res[j][n-1-i] = mat[i][j];
    return res;
}

struct BgPiece { Vector3 position, velocity, rotation, rotVelocity; int pieceType; Color color; float scale; deque<Vector3> trail; };
vector<BgPiece> bgPieces;
Vector3 GetSafeBgPos() {
    float x, y, z;
    do { x = GetRandomFloat(-150, 150); y = GetRandomFloat(-80, 150); } while (x > -40.0f && x < 40.0f && y > -20.0f && y < 60.0f);
    z = GetRandomFloat(-500, 50); return {x, y, z};
}
void InitBackgroundPieces() {
    bgPieces.clear();
    for (int i=0; i<35; i++) {
        BgPiece p; p.position = GetSafeBgPos(); p.velocity = {0,0,GetRandomFloat(40,100)}; p.rotation = {GetRandomFloat(0,360),GetRandomFloat(0,360),GetRandomFloat(0,360)};
        p.rotVelocity = {GetRandomFloat(-60,60),GetRandomFloat(-60,60),GetRandomFloat(-60,60)}; p.pieceType = GetRandomValue(0, pieces.size()-1);
        p.color = pieceColors[pieces[p.pieceType].colorID]; p.scale = GetRandomFloat(1.5f, 4.5f); bgPieces.push_back(p);
    }
}

// =====================================================================
// ENGINE PRINCIPAL DO JOGO AAA
// =====================================================================
class JogoTetris3D {
private:
    GameState currentState = INTRO; int menuSelection = 0, settingsSelection = 0;
    
    int netSelection = 0; bool isDuelNet = false; int netRole = 0; bool netConnected = false; string targetIP = "127.0.0.1"; bool isTypingIP = false;
    vector<string> qualities = {"LOW", "MEDIUM", "HIGH", "ULTRA"}; int currentQualityIdx = 3;

    int board[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int score=0, level=1, continues=3, bombs=2, stars=0, currentGridWidth=14, linesClearedTotal=0;
    int comboCount = 0; float comboTimer = 0.0f; bool isContinuing = false; float continueTimer = 0.0f;
    bool gameOver = false, isPaused = false; 
    
    bool isClassicMode=false, isExpansiveMode=true, isBossMode=false, isTimeAttackMode=false, isHardcoreMode=false, isDuelMode=false, isDuelOnline=false;     
    float timeAttackTimer = 180.0f, hardcoreJunkTimer = 0.0f; 
    
    int aiBoard[BOARD_HEIGHT][MAX_BOARD_WIDTH] = {0};
    int aiScore=0, aiBombs=2; bool aiDead=false;
    vector<vector<int>> aiCurrentPiece, aiNextPiece; int aiCurrentX, aiCurrentY, aiCurrentColor, aiNextColor;
    float aiRenderFallY, aiRenderX, aiCurrentRotAngle, aiMoveTimer = 0.0f; int aiTargetX = 0, aiTargetRot = 0; bool aiIsBrilliant = false;
    
    float p2FallTimer = 0.0f, p2MoveLeftTimer = 0.0f, p2MoveRightTimer = 0.0f;
    vector<vector<int>> p2HoldPiece; int p2HoldColor = 0; bool p2CanHold = true;
    int aiComboCount = 0; float aiComboTimer = 0.0f; string aiMensagemEspecial = ""; float aiTimerMensagem = 0.0f;

    struct PieceTrail { vector<vector<int>> shape; int colorID; float x, y, rot, life, maxLife; };
    vector<PieceTrail> pieceTrails, aiPieceTrails; float trailSpawnTimer = 0.0f;

    float manualZoomOffset = 0.0f, manualCamAngleX = 0.0f, manualCamAngleY = 0.0f; Vector3 manualCamPan = {0,0,0}; 
    vector<vector<int>> currentPiece, nextPiece, holdPiece;
    int currentX, currentY, currentColor, nextColor, holdColor=0;
    float renderFallY, renderX, currentRotAngle = 0.0f; 
    bool currentIsBrilliant=false, nextIsBrilliant=false, canHold=true; 
    float pieceSpawnAnimTimer=0.0f, aiPieceSpawnAnimTimer=0.0f, fallTimer=0.0f, gridExpansionTimer=0.0f, currentGridElevation=2.0f; 
    string mensagemEspecial=""; float timerMensagem=0.0f;
    float hitStopTimer=0.0f, damageVignette=0.0f, goldTint=0.0f, musicPulse=0.0f;
    
    Camera3D camera = { 0 }; Vector3 defaultCamPos = {0,2,22}, defaultCamTarget = {0,10,0}; float cameraFovTarget = 45.0f, cameraBankAngle = 0.0f; 
    float cameraShakeTimer = 0.0f, cameraShakeIntensity = 0.0f, nukeSpinAngle = 0.0f, bossOrbitAngle = 0.0f; Vector3 currentBossPos = {0,0,0}; 
    float moveLeftTimer = 0.0f, moveRightTimer = 0.0f; const float DAS_DELAY = 0.12f, ARR_RATE = 0.02f;  

    bool bossActive=false; int bossHp=0, linesUntilBoss=15, bossEncounterCount=0; 
    float bossAttackTimer=0.0f, bossEntryAnim=0.0f, currentBossAttackDelay=16.0f, bossCinematicSpinTimer=0.0f, bossCinematicCooldown=15.0f;

    bool showExitPrompt=false, confirmExit=false, sfxEnabled=true, musicEnabled=true; 
    Sound sndMove, sndRotate, sndDrop, sndClear1, sndClear2, sndClear3, sndClear4, sndGameOver;
    Music sndMusic = { 0 }; int currentMusicTrack = 1; mt19937 rng;

    // Listas de botões touch
    vector<TouchBtn> menuBtns;
    vector<TouchBtn> gameBtns;
    vector<TouchBtn> netAutoFillBtn;
    vector<TouchBtn> sysBtns;

    void InitTouchButtons() {
        // Menu Navigation
        menuBtns.push_back({{SCREEN_WIDTH - 300.0f, 300, 250, 150}, "UP", C_CYAN, false, false, false});
        menuBtns.push_back({{SCREEN_WIDTH - 300.0f, 500, 250, 150}, "DOWN", C_CYAN, false, false, false});
        menuBtns.push_back({{SCREEN_WIDTH - 300.0f, 700, 250, 150}, "SELECT", C_GOLD, false, false, false});
        menuBtns.push_back({{50, 50, 250, 150}, "BACK", C_RED, false, false, false});

        // Gameplay (Mobile Layout para proporção estreita)
        gameBtns.push_back({{50, SCREEN_HEIGHT - 280.0f, 200, 200}, "< L", C_CYAN, false, false, false});       
        gameBtns.push_back({{300, SCREEN_HEIGHT - 280.0f, 200, 200}, "R >", C_CYAN, false, false, false});      
        gameBtns.push_back({{550, SCREEN_HEIGHT - 280.0f, 200, 200}, "DROP", C_ORANGE, false, false, false});   
        gameBtns.push_back({{SCREEN_WIDTH - 250.0f, SCREEN_HEIGHT - 280.0f, 200, 200}, "HARD", C_RED, false, false, false});     
        gameBtns.push_back({{SCREEN_WIDTH - 500.0f, SCREEN_HEIGHT - 280.0f, 200, 200}, "ROT", C_GOLD, false, false, false});     
        gameBtns.push_back({{SCREEN_WIDTH - 250.0f, SCREEN_HEIGHT - 530.0f, 200, 200}, "HOLD", C_GREEN, false, false, false});   
        gameBtns.push_back({{50, 50, 200, 150}, "NUKE", C_PURPLE, false, false, false});     
        gameBtns.push_back({{SCREEN_WIDTH - 250.0f, 50, 200, 150}, "PAUSE", GRAY, false, false, false});      

        // Auto preenchimento 
        netAutoFillBtn.push_back({{SCREEN_WIDTH/2.0f - 250, 750, 500, 150}, "AUTO-FILL", C_GOLD, false, false, false});

        // Yes / No
        sysBtns.push_back({{SCREEN_WIDTH/2.0f - 400.0f, 700, 350, 150}, "YES [Y]", C_GREEN, false, false, false});
        sysBtns.push_back({{SCREEN_WIDTH/2.0f + 50.0f, 700, 350, 150}, "NO [N]", C_RED, false, false, false});
    }

    void UpdateTouchLogic(vector<TouchBtn>& btns, float scaleX, float scaleY) {
        for (auto& b : btns) {
            b.wasDown = b.isDown;
            b.isDown = false;
        }

        int touchCount = GetTouchPointCount();
        if (touchCount == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) touchCount = 1;

        for (int i = 0; i < touchCount; i++) {
            Vector2 rawPos = (GetTouchPointCount() > 0) ? GetTouchPosition(i) : GetMousePosition();
            Vector2 vPos = { rawPos.x / scaleX, rawPos.y / scaleY };

            for (auto& b : btns) {
                if (CheckCollisionPointRec(vPos, b.rec)) {
                    b.isDown = true;
                }
            }
        }

        for (auto& b : btns) {
            b.isPressed = (b.isDown && !b.wasDown);
        }
    }

    void DrawTouchGamepad(vector<TouchBtn>& btns) {
        for (const auto& b : btns) {
            DrawRectangleRec(b.rec, b.isDown ? ColorAlpha(b.color, 0.6f) : ColorAlpha(b.color, 0.2f));
            DrawRectangleLinesEx(b.rec, b.isDown ? 6.0f : 3.0f, b.color);
            float txtW = MeasureTetrisText(b.text, 5.0f);
            DrawTetrisText(b.text, b.rec.x + (b.rec.width - txtW)/2.0f, b.rec.y + b.rec.height/2.0f - 20.0f, 5.0f, 0, false, b.isDown ? WHITE : b.color);
        }
    }

    float MeasureTetrisText(string text, float blockSize) {
        float width = 0.0f; for(char c:text) width += (c==' ') ? 3.0f*blockSize : 4.0f*blockSize; return width>0 ? width-blockSize : 0; 
    }

    void DrawTetrisText(string text, float startX, float startY, float blockSize, float pulse, bool useRainbow, Color flatColor) {
        float currentX = startX, time = (float)GetTime();
        for(int k=0; k<text.length(); k++) {
            char c = toupper(text[k]); if(c==' ') { currentX += 3.0f*blockSize; continue; }
            vector<string> letter;
            if(c=='A') letter={"010","101","111","101","101"}; else if(c=='B') letter={"110","101","110","101","110"};
            else if(c=='C') letter={"011","100","100","100","011"}; else if(c=='D') letter={"110","101","101","101","110"};
            else if(c=='E') letter={"111","100","111","100","111"}; else if(c=='F') letter={"111","100","110","100","100"};
            else if(c=='G') letter={"011","100","101","101","011"}; else if(c=='H') letter={"101","101","111","101","101"};
            else if(c=='I') letter={"111","010","010","010","111"}; else if(c=='J') letter={"001","001","001","101","111"};
            else if(c=='K') letter={"101","110","100","110","101"}; else if(c=='L') letter={"100","100","100","100","111"};
            else if(c=='M') letter={"101","111","101","101","101"}; else if(c=='N') letter={"111","101","101","101","101"};
            else if(c=='O') letter={"010","101","101","101","010"}; else if(c=='P') letter={"110","101","110","100","100"};
            else if(c=='Q') letter={"010","101","101","010","001"}; else if(c=='R') letter={"110","101","110","101","101"};
            else if(c=='S') letter={"011","100","010","001","110"}; else if(c=='T') letter={"111","010","010","010","010"};
            else if(c=='U') letter={"101","101","101","101","011"}; else if(c=='V') letter={"101","101","101","101","010"};
            else if(c=='W') letter={"101","101","101","111","101"}; else if(c=='X') letter={"101","101","010","101","101"};
            else if(c=='Y') letter={"101","101","010","010","010"}; else if(c=='Z') letter={"111","001","010","100","111"};
            else if(c=='0') letter={"111","101","101","101","111"}; else if(c=='1') letter={"010","110","010","010","111"};
            else if(c=='2') letter={"111","001","111","100","111"}; else if(c=='3') letter={"111","001","111","001","111"};
            else if(c=='4') letter={"101","101","111","001","001"}; else if(c=='5') letter={"111","100","111","001","111"};
            else if(c=='6') letter={"111","100","111","101","111"}; else if(c=='7') letter={"111","001","001","010","010"};
            else if(c=='8') letter={"111","101","111","101","111"}; else if(c=='9') letter={"111","101","111","001","111"};
            else if(c==':') letter={"000","010","000","010","000"}; else if(c=='-') letter={"000","000","111","000","000"};
            else if(c=='>') letter={"100","010","001","010","100"}; else if(c=='/') letter={"001","001","010","100","100"};
            else if(c=='[') letter={"110","100","100","100","110"}; else if(c==']') letter={"011","001","001","001","011"};
            else if(c=='.') letter={"000","000","000","000","010"}; else if(c=='+') letter={"000","010","111","010","000"};
            else if(c=='?') letter={"010","101","001","000","010"}; else if(c=='(') letter={"010","100","100","100","010"};
            else if(c==')') letter={"010","001","001","001","010"}; else if(c=='!') letter={"010","010","010","000","010"};
            else if(c=='=') letter={"000","111","000","111","000"}; else letter={"111","111","111","111","111"};
            
            Color baseColor = useRainbow ? pieceColors[(k%8)+1] : flatColor; 
            float jumpY = (useRainbow || c=='>') ? sin(time*8.0f+k*0.5f)*(pulse*3.0f) : 0.0f; 
            float glitchX = (useRainbow && GetRandomValue(0,100)>95) ? GetRandomFloat(-pulse,pulse)*3.0f : 0.0f;

            for(int i=0; i<5; i++) {
                for(int j=0; j<3; j++) {
                    if(letter[i][j]=='1') {
                        float bx = currentX + j*blockSize + glitchX, by = startY + i*blockSize + jumpY;
                        float innerSize = blockSize*0.85f, beatScale = 1.0f+(pulse*0.06f); 
                        DrawRectangle(bx, by, innerSize*beatScale, innerSize*beatScale, baseColor);
                        DrawRectangle(bx+(blockSize*0.1f), by+(blockSize*0.1f), innerSize*beatScale-(blockSize*0.25f), innerSize*beatScale-(blockSize*0.25f), ColorAlpha(WHITE,0.4f));
                        DrawRectangleLines(bx, by, innerSize*beatScale, innerSize*beatScale, ColorAlpha(WHITE,0.8f));
                    }
                }
            }
            currentX += 4.0f * blockSize; 
        }
    }
    
    void DrawTetrisTextGlowing(string text, float startX, float startY, float blockSize, float pulse) {
        float t = (float)GetTime()*150.0f; Color glow1=ColorFromHSV(fmod(t,360.0f),1,1); Color glow2=ColorFromHSV(fmod(t+180.0f,360.0f),1,1);
        glow1.a=200; glow2.a=200; float off=blockSize*0.25f;
        DrawTetrisText(text, startX-off, startY, blockSize, pulse, false, glow1); DrawTetrisText(text, startX+off, startY, blockSize, pulse, false, glow2);
        DrawTetrisText(text, startX, startY-off, blockSize, pulse, false, glow2); DrawTetrisText(text, startX, startY+off, blockSize, pulse, false, glow1);
        DrawTetrisText(text, startX, startY, blockSize, pulse, false, WHITE);
    }

    void DrawCube2D(Vector2 pos, float size, Color col) {
        DrawRectangle(pos.x, pos.y, size, size, col); DrawRectangle(pos.x+size*0.1f, pos.y+size*0.1f, size*0.8f, size*0.8f, ColorAlpha(WHITE,0.3f));
        DrawRectangleLines(pos.x, pos.y, size, size, ColorAlpha(WHITE,0.6f));
    }

    void DrawFrameWithCubes2D(Rectangle rect, float cubeSize, Color color, Color bgColor) {
        DrawRectangle(rect.x+cubeSize, rect.y+cubeSize, rect.width-cubeSize*2, rect.height-cubeSize*2, bgColor);
        for(float x=rect.x; x<rect.x+rect.width; x+=cubeSize) { DrawCube2D({x, rect.y}, cubeSize, color); DrawCube2D({x, rect.y+rect.height-cubeSize}, cubeSize, color); }
        for(float y=rect.y+cubeSize; y<rect.y+rect.height-cubeSize; y+=cubeSize) { DrawCube2D({rect.x, y}, cubeSize, color); DrawCube2D({rect.x+rect.width-cubeSize, y}, cubeSize, color); }
    }

    void DrawPiece2D(const vector<vector<int>>& piece, int colorID, float cx, float cy, float blockSize, bool isHoldCheck) {
        if(piece.empty()) return;
        float oX = piece[0].size() / 2.0f;
        float oY = piece.size() / 2.0f;
        Color col = (isHoldCheck && !canHold) ? GRAY : pieceColors[colorID];
        
        for(int i = 0; i < piece.size(); ++i) {
            for(int j = 0; j < piece[i].size(); ++j) {
                if(piece[i][j] != 0) {
                    float px = cx + (j - oX) * blockSize;
                    float py = cy + (i - oY) * blockSize;
                    DrawCube2D({px, py}, blockSize, col);
                }
            }
        }
    }

    float introTimer = 5.0f; 
    void DrawHexagonWire(Vector2 center, float radius, float rotation, float thick, Color color) {
        for(int i=0; i<6; i++) {
            float a1=rotation+(i*PI/3.0f), a2=rotation+((i+1)*PI/3.0f);
            DrawLineEx({center.x+cos(a1)*radius, center.y+sin(a1)*radius}, {center.x+cos(a2)*radius, center.y+sin(a2)*radius}, thick, color);
        }
    }

    void DrawIntro2D(float dt) {
        float progress = 5.0f - introTimer; string targetText1 = "BETTARELLO", targetText2 = "CODE", renderText1 = "", renderText2 = "";
        int charsToReveal1 = (int)((progress/2.0f)*targetText1.length()), charsToReveal2 = (int)(((progress-1.5f)/1.5f)*targetText2.length());
        for(int i=0; i<targetText1.length(); i++) renderText1 += (i<charsToReveal1) ? targetText1[i] : (char)GetRandomValue(33,126);
        for(int i=0; i<targetText2.length(); i++) { if(charsToReveal2<0) { renderText2=""; break; } renderText2 += (i<charsToReveal2) ? targetText2[i] : (char)GetRandomValue(33,126); }
        float alpha = 1.0f; if(introTimer<0.5f) alpha=introTimer/0.5f; if(progress<0.5f) alpha=progress/0.5f; 
        Color mainCol = ColorAlpha(C_GOLD, alpha), secCol = ColorAlpha(C_ORANGE, alpha), whiteCol = ColorAlpha(WHITE, alpha), glitchCol = ColorAlpha(C_RED, alpha*0.5f);
        float cx = SCREEN_WIDTH/2.0f, cy = SCREEN_HEIGHT/2.0f, logoY = cy-120.0f, rotation = progress*2.0f, radius = Lerp(0.0f, 80.0f, Clamp(progress*2.0f, 0.0f, 1.0f)); 
        DrawHexagonWire({cx, logoY}, radius, rotation, 4.0f, mainCol); DrawHexagonWire({cx, logoY}, radius*0.8f, -rotation*1.5f, 2.0f, secCol);
        if(progress>1.0f) DrawTetrisText("B", cx-MeasureTetrisText("B",12.0f)/2, logoY-30.0f, 12.0f, 0, false, ColorAlpha(WHITE, Clamp((progress-1.0f)*2.0f,0.0f,1.0f)*alpha));
        if(GetRandomValue(0,100)>90) DrawTetrisText(renderText1, cx-MeasureTetrisText(renderText1,10.0f)/2+GetRandomValue(-8,8), cy+10, 10.0f, 0, false, glitchCol);
        DrawTetrisText(renderText1, cx-MeasureTetrisText(renderText1,10.0f)/2, cy+10, 10.0f, 0, false, whiteCol);
        if(progress>1.5f) { string spacedCode=""; for(char c:renderText2){spacedCode+=c; spacedCode+=" ";} DrawTetrisText(spacedCode, cx-MeasureTetrisText(spacedCode,6.0f)/2, cy+80, 6.0f, 0, false, secCol); }
        float barWidth = Lerp(0.0f, 400.0f, Clamp(progress,0.0f,1.0f)); DrawRectangle(cx-barWidth/2, cy, barWidth, 2, ColorAlpha(C_GOLD,alpha*0.5f)); DrawRectangle(cx-barWidth/2, cy+140, barWidth, 2, ColorAlpha(C_GOLD,alpha*0.5f));
    }

    void DrawFloatingTexts(float dt, float scaleX, float scaleY) {
        rlDisableDepthMask(); 
        for(int i=floatingTexts.size()-1; i>=0; i--) {
            floatingTexts[i].pos.y += dt*3.0f; floatingTexts[i].life -= dt;
            if(floatingTexts[i].life<=0) floatingTexts.erase(floatingTexts.begin()+i);
            else {
                Vector2 sPos = GetWorldToScreen(floatingTexts[i].pos, camera);
                // Inverse scale for custom full screen mapping
                sPos.x /= scaleX; sPos.y /= scaleY;
                Color c = floatingTexts[i].color; c.a = (unsigned char)(255 * Clamp(floatingTexts[i].life,0,1));
                DrawTetrisText(floatingTexts[i].text, sPos.x-MeasureTetrisText(floatingTexts[i].text, 4.0f*floatingTexts[i].scale)/2, sPos.y, 4.0f*floatingTexts[i].scale, 0, false, c);
            }
        }
        rlEnableDepthMask();
    }

    int GetRandomPiece() { uniform_int_distribution<int> dist(0, pieces.size()-1); return dist(rng); }

    Vector3 GetWorldPos(int logicalX, int logicalY) { return {(float)logicalX-(currentGridWidth/2.0f)+0.5f+(isDuelMode?-7.5f:0.0f), (float)(BOARD_HEIGHT-logicalY)-0.5f+currentGridElevation, 0.0f}; }
    Vector3 GetAIWorldPos(int logicalX, int logicalY) { return {(float)logicalX-(currentGridWidth/2.0f)+0.5f+7.5f, (float)(BOARD_HEIGHT-logicalY)-0.5f+currentGridElevation, 0.0f}; }
    void TocarSom(Sound snd) { if(sfxEnabled) PlaySound(snd); }

    void LoadNextMusic() {
        if(sndMusic.stream.buffer) { DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback); UnloadMusicStream(sndMusic); sndMusic.stream.buffer=NULL; }
        string fn = "src/music" + to_string(currentMusicTrack) + ".mp3";
        if(FileExists(fn.c_str())) { sndMusic=LoadMusicStream(fn.c_str()); if(sndMusic.stream.buffer) { PlayMusicStream(sndMusic); AttachAudioStreamProcessor(sndMusic.stream, AudioInputCallback); } } 
        else { if(currentMusicTrack>1) { currentMusicTrack=1; LoadNextMusic(); } else musicEnabled=false; }
    }
    void ShuffleMusic() {
        int prev=currentMusicTrack; for(int i=0; i<30; i++) { currentMusicTrack=GetRandomValue(1,30); if(currentMusicTrack!=prev && FileExists(("src/music"+to_string(currentMusicTrack)+".mp3").c_str())) break; }
        LoadNextMusic();
    }

    void SpawnPiece() {
        if(nextPiece.empty()) { int p1=GetRandomPiece(); nextPiece=pieces[p1].shape; nextColor=pieces[p1].colorID; }
        currentPiece=nextPiece; currentColor=nextColor; currentIsBrilliant=nextIsBrilliant; nextIsBrilliant=false; currentRotAngle=0.0f;
        currentX=currentGridWidth/2-currentPiece[0].size()/2; currentY=-5; renderFallY=-5.0f; renderX=currentX; pieceSpawnAnimTimer=1.0f; 
        float spawnWorldX=currentX+(currentPiece[0].size()/2.0f)-(currentGridWidth/2.0f)+0.5f; if(isDuelMode) spawnWorldX-=7.5f; 
        float spawnWorldY=(float)BOARD_HEIGHT-(currentY+currentPiece.size()/2.0f)-0.5f+currentGridElevation;
        SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, WHITE, 8, 15.0f); SpawnParticles3D({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[currentColor], 15, 25.0f); SpawnShockwave({spawnWorldX, spawnWorldY, 0.0f}, pieceColors[currentColor]);
        int p2=GetRandomPiece(); nextPiece=pieces[p2].shape; nextColor=pieces[p2].colorID; canHold=true; 
        if(!IsValidMove(currentPiece, currentX, currentY)) {
            if(continues>0 && !isDuelMode) {
                continues--; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) if(board[i][j]!=0) { SpawnParticles3D(GetWorldPos(j,i), pieceColors[board[i][j]], 10, 10.0f); board[i][j]=0; }
                TocarSom(sndGameOver); cameraShakeTimer=0.5f; cameraShakeIntensity=2.5f; damageVignette=1.0f; currentX=currentGridWidth/2-currentPiece[0].size()/2; currentY=-5; renderFallY=-5.0f; renderX=currentX;
            } else { if(isDuelMode) { gameOver=true; TocarSom(sndGameOver); } else { isContinuing=true; continueTimer=9.99f; TocarSom(sndGameOver); } }
        }
    }

    bool IsValidMove(const vector<vector<int>>& piece, int x, int y) {
        for(int i=0; i<piece.size(); ++i) for(int j=0; j<piece[i].size(); ++j) if(piece[i][j]!=0) { int bX=x+j, bY=y+i; if(bX<0 || bX>=currentGridWidth || bY>=BOARD_HEIGHT || (bY>=0 && board[bY][bX]!=0)) return false; } return true;
    }

    void LockPiece() {
        TocarSom(sndDrop); cameraShakeTimer=0.15f; cameraShakeIntensity=0.3f; bool blockPlacedOut=false;
        for(int i=0; i<currentPiece.size(); ++i) for(int j=0; j<currentPiece[i].size(); ++j) if(currentPiece[i][j]!=0) { if(currentY+i<0) blockPlacedOut=true; else if(currentY+i>=0 && currentY+i<BOARD_HEIGHT) { board[currentY+i][currentX+j]=currentColor; SpawnParticles3D(GetWorldPos(currentX+j, currentY+i), pieceColors[currentColor], 8, 5.0f); } }
        if(blockPlacedOut) {
            if(continues>0 && !isDuelMode) {
                continues--; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) if(board[i][j]!=0) { SpawnParticles3D(GetWorldPos(j,i), pieceColors[board[i][j]], 10, 10.0f); board[i][j]=0; }
                TocarSom(sndGameOver); cameraShakeTimer=0.5f; cameraShakeIntensity=2.5f; damageVignette=1.0f; SpawnPiece();
            } else { if(isDuelMode) { gameOver=true; TocarSom(sndGameOver); } else { isContinuing=true; continueTimer=9.99f; TocarSom(sndGameOver); } } return; 
        }
        if(currentIsBrilliant && isExpansiveMode) {
            if(currentGridWidth<MAX_BOARD_WIDTH) {
                int exp=2, sh=exp/2; for(int i=0; i<BOARD_HEIGHT; i++) { for(int j=currentGridWidth-1; j>=0; j--) board[i][j+sh]=board[i][j]; for(int j=0; j<sh; j++) board[i][j]=0; }
                currentGridWidth+=exp; gridExpansionTimer=2.0f; mensagemEspecial="SYSTEM EXPANDED"; timerMensagem=3.0f; cameraShakeTimer=1.0f; cameraShakeIntensity=3.5f; hitStopTimer=0.2f; TocarSom(sndClear4); 
            } else { mensagemEspecial="MAX POWER OVERDRIVE!"; score+=2000*level; timerMensagem=3.0f; SpawnParticles3D({0,BOARD_HEIGHT/2.0f,0}, C_GOLD, 150, 25.0f, 3); TocarSom(sndClear3); }
        }
        ClearLines(); SpawnPiece();
    }

    void BossAddJunkLine() {
        for(int i=0; i<BOARD_HEIGHT-1; i++) for(int j=0; j<currentGridWidth; j++) board[i][j] = board[i+1][j];
        int hole = GetRandomValue(0, currentGridWidth-1); for(int j=0; j<currentGridWidth; j++) board[BOARD_HEIGHT-1][j] = (j==hole)?0:8;
        cameraShakeTimer=0.6f; cameraShakeIntensity=3.0f; damageVignette=1.0f; TocarSom(sndDrop);
    }

    void ClearLines() {
        int linesClearedNow=0; Vector3 avgClearPos={0,0,0};
        for(int i=BOARD_HEIGHT-1; i>=0; --i) {
            bool isFull=true; for(int j=0; j<currentGridWidth; ++j) if(board[i][j]==0) { isFull=false; break; }
            if(isFull) {
                linesClearedNow++; avgClearPos.y+=GetWorldPos(0,i).y; 
                for(int j=0; j<currentGridWidth; j++) SpawnParticles3D(GetWorldPos(j,i), pieceColors[board[i][j]], 25, 20.0f, board[i][j]);
                for(int k=i; k>0; --k) for(int j=0; j<currentGridWidth; ++j) board[k][j]=board[k-1][j];
                for(int j=0; j<currentGridWidth; ++j) board[0][j]=0; i++; 
            }
        }
        if(linesClearedNow>0) {
            comboCount++; comboTimer=6.0f; avgClearPos.y/=linesClearedNow; linesClearedTotal+=linesClearedNow; if(isDuelMode) avgClearPos.x-=7.5f;
            if(isExpansiveMode) { stars+=linesClearedNow; if(stars>=10) { stars-=10; nextIsBrilliant=true; mensagemEspecial="MAGIC PIECE READY"; timerMensagem=3.0f; TocarSom(sndClear2); } }
            cameraShakeTimer=0.5f+(linesClearedNow*0.15f); cameraShakeIntensity=linesClearedNow*2.5f; cameraFovTarget=45.0f-(linesClearedNow*3.0f); hitStopTimer=linesClearedNow*0.05f; 
            if(linesClearedNow>=4) { goldTint=1.0f; SpawnShockwave({isDuelMode?-7.5f:0.0f, avgClearPos.y, 0}, C_GOLD); }
            int ptsGained=0; if(linesClearedNow==1){ptsGained=100*level; TocarSom(sndClear1);} else if(linesClearedNow==2){ptsGained=300*level; TocarSom(sndClear2);} else if(linesClearedNow==3){ptsGained=500*level; TocarSom(sndClear3);} else if(linesClearedNow>=4){ptsGained=800*level; cameraShakeIntensity=6.0f; TocarSom(sndClear4);}
            ptsGained*=comboCount; score+=ptsGained;
            if(comboCount==2){mensagemEspecial="NICE!"; timerMensagem=2.0f;} else if(comboCount==3){mensagemEspecial="VERY NICE!"; timerMensagem=2.0f;} else if(comboCount==4){mensagemEspecial="INCREDIBLE!"; timerMensagem=3.0f;} else if(comboCount>=5){mensagemEspecial="GOD MODE!"; timerMensagem=3.0f; goldTint=1.0f;} else if(linesClearedNow>=4){mensagemEspecial="TETRABETTA!"; timerMensagem=3.0f;} else if(linesClearedNow==3){mensagemEspecial="IMPRESSIVE"; timerMensagem=2.0f;} else if(linesClearedNow==2){mensagemEspecial="GOOD"; timerMensagem=2.0f;}
            SpawnFloatingText(avgClearPos, "+"+to_string(ptsGained), C_GOLD, 1.5f+(linesClearedNow*0.5f));
            if(comboCount>1) SpawnFloatingText({avgClearPos.x, avgClearPos.y+2.0f, avgClearPos.z}, "COMBO X"+to_string(comboCount), C_GOLD, 2.0f);
            if(isTimeAttackMode) {
                int bTime = (linesClearedNow==1)?5:(linesClearedNow==2)?8:(linesClearedNow==3)?11:15; timeAttackTimer+=bTime;
                floatingTexts.push_back({{0,currentGridElevation+(BOARD_HEIGHT/2.0f),0}, "+"+to_string(bTime)+" SEC", 0.8f, C_CYAN, 3.5f});
            }
            if(bossActive && isBossMode) {
                bossHp-=linesClearedNow; SpawnFloatingText({0, avgClearPos.y+4.0f, 0}, "BOSS DMG -"+to_string(linesClearedNow), C_RED, 2.5f);
                if(bossHp<=0) {
                    bossActive=false; bossCinematicSpinTimer=0.0f; linesUntilBoss=(bossEncounterCount<=4)?(bossEncounterCount==1?45:bossEncounterCount==2?80:bossEncounterCount==3?110:150):(150+(bossEncounterCount-4)*50); 
                    score+=5000*level; SpawnParticles3D({0,currentGridElevation+20.0f,-5.0f}, C_GOLD, 400, 60.0f, 8); cameraShakeTimer=2.0f; cameraShakeIntensity=3.0f; hitStopTimer=0.5f; mensagemEspecial="VIRUS DELETED!"; timerMensagem=4.0f; TocarSom(sndClear4);
                }
            }
            if(!isHardcoreMode) level=(linesClearedTotal/10)+1;
        } 
    }

    void NukeBoard() {
        int bd=0; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) if(board[i][j]!=0) { bd++; SpawnParticles3D(GetWorldPos(j,i), pieceColors[board[i][j]], 10, 25.0f, board[i][j]); board[i][j]=0; }
        if(bd>0) {
            score+=bd*50*level; mensagemEspecial="SYSTEM PURGE!!!"; timerMensagem=3.0f; cameraShakeTimer=1.5f; cameraShakeIntensity=4.0f; cameraFovTarget=35.0f; nukeSpinAngle=PI*4.0f; hitStopTimer=0.3f; goldTint=1.0f; SpawnShockwave({isDuelMode?-7.5f:0.0f, currentGridElevation, 0}, C_GOLD); TocarSom(sndClear4); 
            if(bossActive && isBossMode) { bossActive=false; bossCinematicSpinTimer=0.0f; linesUntilBoss=(bossEncounterCount<=4)?(bossEncounterCount==1?45:bossEncounterCount==2?80:bossEncounterCount==3?110:150):(150+(bossEncounterCount-4)*50); score+=10000; SpawnParticles3D({0,currentGridElevation+20.0f,-5.0f}, C_RED, 800, 80.0f); cameraShakeTimer=2.0f; cameraShakeIntensity=3.0f; hitStopTimer=0.5f; mensagemEspecial="BOSS PURGED!"; timerMensagem=4.0f; }
        }
    }

    void SetupNetwork(string ip, bool isHost) {
        #ifdef _WIN32
        if(!InitWs2()) return;
        if(netSocket!=OS_INVALID_SOCKET) { osCloseSocket(netSocket); netSocket=OS_INVALID_SOCKET; }
        netSocket = osSocket(OS_AF_INET, OS_SOCK_DGRAM, OS_IPPROTO_UDP);
        unsigned long mode = 1; osIoctlSocket(netSocket, OS_FIONBIO, &mode);
        if(isHost) { os_sockaddr_in localAddr; memset(&localAddr,0,sizeof(localAddr)); localAddr.sin_family=OS_AF_INET; localAddr.sin_port=osHtons(27015); localAddr.sin_addr=0; osBind(netSocket, &localAddr, sizeof(localAddr)); } 
        else { memset(&otherAddr,0,sizeof(otherAddr)); otherAddr.sin_family=OS_AF_INET; otherAddr.sin_port=osHtons(27015); otherAddr.sin_addr=osInetAddr(ip.c_str()); otherAddrLen=sizeof(otherAddr); }
        #endif
    }
    void CloseNetwork() {
        #ifdef _WIN32
        if(netSocket!=OS_INVALID_SOCKET) { osCloseSocket(netSocket); netSocket=OS_INVALID_SOCKET; }
        if(wsaCleanup) wsaCleanup();
        #endif
    }

    bool IsValidAIMove(const vector<vector<int>>& piece, int x, int y) {
        for(int i=0; i<piece.size(); ++i) for(int j=0; j<piece[i].size(); ++j) if(piece[i][j]!=0) { int bX=x+j, bY=y+i; if(bX<0 || bX>=currentGridWidth || bY>=BOARD_HEIGHT || (bY>=0 && aiBoard[bY][bX]!=0)) return false; } return true;
    }
    int GetAIHoles(int tb[BOARD_HEIGHT][MAX_BOARD_WIDTH]) { int h=0; for(int x=0; x<currentGridWidth; x++) { bool f=false; for(int y=0; y<BOARD_HEIGHT; y++) { if(tb[y][x]!=0) f=true; else if(f) h++; } } return h; }
    int GetAIAggregateHeight(int tb[BOARD_HEIGHT][MAX_BOARD_WIDTH]) { int h=0; for(int x=0; x<currentGridWidth; x++) for(int y=0; y<BOARD_HEIGHT; y++) if(tb[y][x]!=0) { h+=(BOARD_HEIGHT-y); break; } return h; }
    int GetAIBumpiness(int tb[BOARD_HEIGHT][MAX_BOARD_WIDTH]) { int b=0, hs[MAX_BOARD_WIDTH]={0}; for(int x=0; x<currentGridWidth; x++) for(int y=0; y<BOARD_HEIGHT; y++) if(tb[y][x]!=0) { hs[x]=BOARD_HEIGHT-y; break; } for(int x=0; x<currentGridWidth-1; x++) b+=abs(hs[x]-hs[x+1]); return b; }

    void EvaluateBestAIMove() {
        int bestScore = -2000000000; aiTargetX = aiCurrentX; aiTargetRot = 0; vector<vector<int>> testPiece = aiCurrentPiece;
        for(int rot=0; rot<4; rot++) {
            for(int x=-4; x<currentGridWidth+4; x++) {
                if(IsValidAIMove(testPiece, x, aiCurrentY)) {
                    int dropY=aiCurrentY; while(IsValidAIMove(testPiece, x, dropY+1)) dropY++;
                    int tb[BOARD_HEIGHT][MAX_BOARD_WIDTH]; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) tb[i][j]=aiBoard[i][j];
                    for(int i=0; i<testPiece.size(); ++i) for(int j=0; j<testPiece[i].size(); ++j) if(testPiece[i][j]!=0) if(dropY+i>=0 && dropY+i<BOARD_HEIGHT && x+j>=0 && x+j<currentGridWidth) tb[dropY+i][x+j]=1;
                    int lines=0; for(int y=BOARD_HEIGHT-1; y>=0; --y) { bool full=true; for(int cx=0; cx<currentGridWidth; cx++) if(tb[y][cx]==0){full=false;break;} if(full){lines++; for(int k=y; k>0; --k) for(int cx=0; cx<currentGridWidth; cx++) tb[k][cx]=tb[k-1][cx]; for(int cx=0; cx<currentGridWidth; cx++) tb[0][cx]=0; y++;} }
                    int score = (lines*3400) - (GetAIAggregateHeight(tb)*510) - (GetAIHoles(tb)*3560) - (GetAIBumpiness(tb)*180) + GetRandomValue(-800,800);
                    if(score>bestScore) { bestScore=score; aiTargetX=x; aiTargetRot=rot; }
                }
            }
            testPiece = RotateMatrix(testPiece);
        }
    }

    void SpawnAIPiece() {
        if(aiNextPiece.empty()) { int p1=GetRandomPiece(); aiNextPiece=pieces[p1].shape; aiNextColor=pieces[p1].colorID; }
        aiCurrentPiece=aiNextPiece; aiCurrentColor=aiNextColor; aiCurrentRotAngle=0.0f; aiCurrentX=currentGridWidth/2-aiCurrentPiece[0].size()/2; aiCurrentY=-5; aiRenderFallY=-5.0f; aiRenderX=aiCurrentX; aiPieceSpawnAnimTimer=1.0f; 
        float sWX=aiCurrentX+(aiCurrentPiece[0].size()/2.0f)-(currentGridWidth/2.0f)+0.5f+7.5f, sWY=(float)BOARD_HEIGHT-(aiCurrentY+aiCurrentPiece.size()/2.0f)-0.5f+currentGridElevation;
        SpawnParticles3D({sWX,sWY,0}, WHITE, 8, 15.0f); SpawnParticles3D({sWX,sWY,0}, pieceColors[aiCurrentColor], 15, 25.0f); SpawnShockwave({sWX,sWY,0}, pieceColors[aiCurrentColor]);
        int p2=GetRandomPiece(); aiNextPiece=pieces[p2].shape; aiNextColor=pieces[p2].colorID; p2CanHold=true; EvaluateBestAIMove(); 
    }

    void AINukeBoard() {
        int bd=0; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) if(aiBoard[i][j]!=0) { bd++; SpawnParticles3D(GetAIWorldPos(j,i), pieceColors[aiBoard[i][j]], 10, 25.0f, aiBoard[i][j]); aiBoard[i][j]=0; }
        if(bd>0) { aiScore+=bd*50*level; aiMensagemEspecial=(isDuelOnline||isDuelNet)?"P2 SYSTEM PURGE!!!":"AI SYSTEM PURGE!!!"; aiTimerMensagem=3.0f; cameraShakeTimer=1.5f; cameraShakeIntensity=4.0f; hitStopTimer=0.3f; SpawnShockwave({7.5f, currentGridElevation, 0}, C_RED); TocarSom(sndClear4); }
    }

    void ClearAILines() {
        int linesClearedNow=0; Vector3 avgClearPos={0,0,0};
        for(int i=BOARD_HEIGHT-1; i>=0; --i) {
            bool isFull=true; for(int j=0; j<currentGridWidth; ++j) if(aiBoard[i][j]==0) { isFull=false; break; }
            if(isFull) {
                linesClearedNow++; avgClearPos.y+=GetAIWorldPos(0,i).y;
                for(int j=0; j<currentGridWidth; j++) SpawnParticles3D(GetAIWorldPos(j,i), pieceColors[aiBoard[i][j]], 25, 20.0f, aiBoard[i][j]);
                for(int k=i; k>0; --k) for(int j=0; j<currentGridWidth; ++j) aiBoard[k][j]=aiBoard[k-1][j];
                for(int j=0; j<currentGridWidth; ++j) aiBoard[0][j]=0; i++; 
            }
        }
        if(linesClearedNow>0) {
            aiComboCount++; aiComboTimer=6.0f; avgClearPos.y/=linesClearedNow; avgClearPos.x+=7.5f; cameraShakeTimer=0.5f+(linesClearedNow*0.15f); cameraShakeIntensity=linesClearedNow*2.5f; hitStopTimer=linesClearedNow*0.05f; 
            if(linesClearedNow>=4) SpawnShockwave({7.5f, avgClearPos.y, 0}, C_RED);
            int ptsGained=0; if(linesClearedNow==1){ptsGained=100*level; TocarSom(sndClear1);} else if(linesClearedNow==2){ptsGained=300*level; TocarSom(sndClear2);} else if(linesClearedNow==3){ptsGained=500*level; TocarSom(sndClear3);} else if(linesClearedNow>=4){ptsGained=800*level; cameraShakeIntensity=6.0f; TocarSom(sndClear4);} 
            ptsGained*=aiComboCount; aiScore+=ptsGained;
            if(aiComboCount==2){aiMensagemEspecial="NICE!"; aiTimerMensagem=2.0f;} else if(aiComboCount==3){aiMensagemEspecial="VERY NICE!"; aiTimerMensagem=2.0f;} else if(aiComboCount==4){aiMensagemEspecial="INCREDIBLE!"; aiTimerMensagem=3.0f;} else if(aiComboCount>=5){aiMensagemEspecial="GOD MODE!"; aiTimerMensagem=3.0f;} else if(linesClearedNow>=4){aiMensagemEspecial="TETRABETTA!"; aiTimerMensagem=3.0f;} else if(linesClearedNow==3){aiMensagemEspecial="IMPRESSIVE"; aiTimerMensagem=2.0f;} else if(linesClearedNow==2){aiMensagemEspecial="GOOD"; aiTimerMensagem=2.0f;}
            SpawnFloatingText(avgClearPos, "+"+to_string(ptsGained), C_RED, 1.5f+(linesClearedNow*0.5f));
            if(aiComboCount>1) SpawnFloatingText({avgClearPos.x, avgClearPos.y+2.0f, avgClearPos.z}, "COMBO X"+to_string(aiComboCount), C_RED, 2.0f);
        }
    }

    void LockAIPiece() {
        TocarSom(sndDrop);
        for(int i=0; i<aiCurrentPiece.size(); ++i) for(int j=0; j<aiCurrentPiece[i].size(); ++j) if(aiCurrentPiece[i][j]!=0) {
            if(aiCurrentY+i>=0 && aiCurrentY+i<BOARD_HEIGHT) { aiBoard[aiCurrentY+i][aiCurrentX+j]=aiCurrentColor; SpawnParticles3D(GetAIWorldPos(aiCurrentX+j, aiCurrentY+i), pieceColors[aiCurrentColor], 8, 5.0f); } 
            else if(aiCurrentY+i<0) { if(aiBombs>0) { aiBombs--; AINukeBoard(); SpawnAIPiece(); return; } else { aiDead=true; gameOver=true; TocarSom(sndGameOver); return; } }
        }
        ClearAILines(); SpawnAIPiece();
    }

    void DrawSciFiBlock3D(Vector3 pos, Color baseCol, bool isReflection, bool isGhost = false, bool isBrilliant = false, float scale = 1.0f) {
        float s = CUBE_SIZE*scale, coreSize = s*0.70f; Color coreCol = isReflection?ColorAlpha(baseCol,0.2f):baseCol, shellCol = isReflection?ColorAlpha(WHITE,0.02f):ColorAlpha(WHITE,0.5f); 
        if(isBrilliant && !isGhost) { coreCol=ColorFromHSV(fmod((float)GetTime()*200.0f+40.0f,60.0f),1,1); shellCol=C_GOLD; }
        if(isGhost) { float p=(sin((float)GetTime()*10.0f)+1.0f)*0.5f; DrawCube(pos,coreSize,coreSize,coreSize,ColorAlpha(baseCol,0.1f+p*0.1f)); DrawCubeWires(pos,s*0.95f,s*0.95f,s*0.95f,ColorAlpha(baseCol,0.4f+p*0.4f)); } 
        else { DrawCube(pos,coreSize,coreSize,coreSize,coreCol); if(!isReflection) DrawCube({pos.x, pos.y+coreSize*0.42f, pos.z+0.05f*scale}, coreSize*0.9f, s*0.05f, coreSize*0.9f, ColorAlpha(WHITE,0.7f)); DrawCubeWires(pos,s*0.95f,s*0.95f,s*0.95f,shellCol); }
    }

    void DrawProceduralEnvironment() {
        rlDisableDepthMask();
        for(auto& bp : bgPieces) {
            if(bp.trail.size()>1) { rlBegin(RL_LINES); for(size_t j=0; j<bp.trail.size()-1; j++) { float a=(1.0f-((float)j/bp.trail.size()))*(0.08f+musicPulse*0.04f); rlColor4ub(bp.color.r,bp.color.g,bp.color.b,(unsigned char)(bp.color.a*fmin(1.0f,fmax(0.0f,a)))); rlVertex3f(bp.trail[j].x,bp.trail[j].y,bp.trail[j].z); rlVertex3f(bp.trail[j+1].x,bp.trail[j+1].y,bp.trail[j+1].z); } rlEnd(); }
            rlPushMatrix(); rlTranslatef(bp.position.x,bp.position.y,bp.position.z); rlRotatef(bp.rotation.x,1,0,0); rlRotatef(bp.rotation.y,0,1,0); rlRotatef(bp.rotation.z,0,0,1); float bs=bp.scale*(1.0f+musicPulse*0.25f); rlScalef(bs,bs,bs);
            auto sh=pieces[bp.pieceType].shape; float oX=sh[0].size()/2.0f, oY=sh.size()/2.0f;
            for(int i=0; i<sh.size(); ++i) for(int j=0; j<sh[i].size(); ++j) if(sh[i][j]!=0) { Vector3 bP={(j-oX+0.5f)*CUBE_SIZE,(-i+oY-0.5f)*CUBE_SIZE,0}; DrawCubeWires(bP,CUBE_SIZE,CUBE_SIZE,CUBE_SIZE,ColorAlpha(bp.color,fmin(1.0f,fmax(0.0f,0.06f+musicPulse*0.08f)))); DrawCube(bP,CUBE_SIZE*0.9f,CUBE_SIZE*0.9f,CUBE_SIZE*0.9f,ColorAlpha(bp.color,fmin(1.0f,fmax(0.0f,0.005f+musicPulse*0.03f)))); }
            rlPopMatrix();
        }
        rlEnableDepthMask();
        Color bgC=bossActive?Color{40,0,0,200}:Color{15,10,5,180}; if(bossCinematicSpinTimer>0) bgC.a=(unsigned char)(bgC.a*(1.0f-sin((1.0f-bossCinematicSpinTimer/2.0f)*PI)));
        DrawCube({0.0f, currentGridElevation+(BOARD_HEIGHT+3.0f)/2.0f, -0.2f}, currentGridWidth, BOARD_HEIGHT+3.0f, 0.05f, bgC);
    }

    void DrawPlayfieldAndPieces() {
        float tBY=currentGridElevation, tTY=currentGridElevation+BOARD_HEIGHT+3.0f, hW=currentGridWidth/2.0f, wS=0.25f; Color fC=bossActive?C_RED:C_GOLD; 
        for(float y=tBY; y<tTY; y+=wS) { DrawSciFiBlock3D({-hW-wS/2.0f, y+wS/2.0f, 0}, fC, false, false, bossActive, wS); DrawSciFiBlock3D({hW+wS/2.0f, y+wS/2.0f, 0}, fC, false, false, bossActive, wS); }
        for(float x=-hW-wS; x<hW+wS; x+=wS) DrawSciFiBlock3D({x+wS/2.0f, tBY-wS/2.0f, 0}, fC, false, false, bossActive, wS);
        if(gridExpansionTimer>0.0f) { Color gC=ColorAlpha(C_GOLD,abs(sin((2.0f-gridExpansionTimer)*2.0f*PI))*0.7f); for(float y=0; y<BOARD_HEIGHT+3.0f; y+=1.0f) { DrawSciFiBlock3D({-hW+0.5f, tBY+y+0.5f, 0}, gC, false, false, true, 1.0f); DrawSciFiBlock3D({hW-0.5f, tBY+y+0.5f, 0}, gC, false, false, true, 1.0f); } }
        rlBegin(RL_LINES); Color gCC=ColorAlpha(WHITE,0.1f), bGC=ColorAlpha(C_GOLD,0.25f);
        for(int x=0; x<=currentGridWidth; x++) { float lX=-hW+x; rlColor4ub(gCC.r,gCC.g,gCC.b,gCC.a); rlVertex3f(lX,tBY,0); rlVertex3f(lX,tTY,0); rlColor4ub(bGC.r,bGC.g,bGC.b,bGC.a); rlVertex3f(lX,tBY,-0.25f); rlVertex3f(lX,tTY,-0.25f); }
        for(int y=0; y<=BOARD_HEIGHT+3; y++) { float lY=tBY+y; rlColor4ub(gCC.r,gCC.g,gCC.b,gCC.a); rlVertex3f(-hW,lY,0); rlVertex3f(hW,lY,0); rlColor4ub(bGC.r,bGC.g,bGC.b,bGC.a); rlVertex3f(-hW,lY,-0.25f); rlVertex3f(hW,lY,-0.25f); } rlEnd();
        for(int i=0; i<BOARD_HEIGHT; ++i) for(int j=0; j<currentGridWidth; ++j) if(board[i][j]!=0) DrawSciFiBlock3D({(float)j-hW+0.5f, (float)(BOARD_HEIGHT-i)-0.5f+currentGridElevation, 0}, pieceColors[board[i][j]], false);
        
        if(currentState==PLAYING && !isContinuing) {
            int tI=0; for(auto& pt:pieceTrails) {
                float a=pt.life/pt.maxLife, zO=-0.05f-(tI*0.005f), cx=pt.x+pt.shape[0].size()/2.0f-hW, cy=(float)BOARD_HEIGHT-pt.y-pt.shape.size()/2.0f+currentGridElevation;
                rlPushMatrix(); rlTranslatef(cx,cy,zO); rlRotatef(pt.rot,0,0,1); rlTranslatef(-cx,-cy,-zO);
                for(int i=0; i<pt.shape.size(); ++i) for(int j=0; j<pt.shape[i].size(); ++j) if(pt.shape[i][j]!=0) { Vector3 tP={pt.x+j-hW+0.5f, (float)BOARD_HEIGHT-(pt.y+i)-0.5f+currentGridElevation, zO}; float s=CUBE_SIZE*(0.6f+0.4f*a); DrawCube(tP, s*0.8f, s*0.8f, s*0.8f, ColorAlpha(pieceColors[pt.colorID], 0.15f*a)); DrawCubeWires(tP, s, s, s, ColorAlpha(pieceColors[pt.colorID], 0.5f*a)); }
                rlPopMatrix(); tI++;
            }
            int gY=currentY; while(IsValidMove(currentPiece, currentX, gY+1)) gY++;
            if(gY>currentY) for(int i=0; i<currentPiece.size(); ++i) for(int j=0; j<currentPiece[i].size(); ++j) if(currentPiece[i][j]!=0) DrawSciFiBlock3D({renderX+j-hW+0.5f, (float)BOARD_HEIGHT-(gY+i)-0.5f+currentGridElevation, 0}, pieceColors[currentColor], false, true);
            rlPushMatrix(); float cx=renderX+currentPiece[0].size()/2.0f-hW, cy=(float)BOARD_HEIGHT-renderFallY-currentPiece.size()/2.0f+currentGridElevation; rlTranslatef(cx,cy,0); rlRotatef(currentRotAngle,0,0,1); rlTranslatef(-cx,-cy,0);
            for(int i=0; i<currentPiece.size(); ++i) for(int j=0; j<currentPiece[i].size(); ++j) if(currentPiece[i][j]!=0) DrawSciFiBlock3D({renderX+j-hW+0.5f, (float)BOARD_HEIGHT-(renderFallY+i)-0.5f+currentGridElevation, 0}, pieceColors[currentColor], false, false, currentIsBrilliant||pieceSpawnAnimTimer>0.6f);
            rlPopMatrix();
        }
    }

    void DrawBossEncounter() {
        if(bossEntryAnim<0.01f) return; float t=(float)GetTime();
        rlPushMatrix(); rlTranslatef(currentBossPos.x, currentBossPos.y, currentBossPos.z); rlRotatef(t*150.0f, 0,1,0); rlRotatef(sin(t*5.0f)*30.0f, 1,0,1);
        auto sh=pieces[(bossEncounterCount-1)%pieces.size()].shape; float oX=sh[0].size()/2.0f, oY=sh.size()/2.0f;
        for(int i=0; i<sh.size(); ++i) for(int j=0; j<sh[i].size(); ++j) if(sh[i][j]!=0) { float gX=(GetRandomValue(0,10)>8)?GetRandomFloat(-0.4f,0.4f):0, gY=(GetRandomValue(0,10)>8)?GetRandomFloat(-0.4f,0.4f):0, gZ=(GetRandomValue(0,10)>8)?GetRandomFloat(-0.4f,0.4f):0; Color vC=(GetRandomValue(0,10)>8)?WHITE:C_RED; Vector3 bP={((float)j-oX+0.5f)*2.5f+gX, ((float)-i+oY-0.5f)*2.5f+gY, gZ}; DrawCube(bP, 2.2f, 2.2f, 2.2f, ColorAlpha(vC,0.8f)); DrawCubeWires(bP, 2.3f, 2.3f, 2.3f, C_ORANGE); }
        for(int i=0; i<4; i++) { float a=t*5.0f+(i*PI/2.0f); Vector3 sat={cos(a)*5.0f, sin(t*10.0f)*3.0f, sin(a)*5.0f}; DrawCube(sat,1,1,1,WHITE); DrawCubeWires(sat,1.1f,1.1f,1.1f,C_RED); }
        rlPopMatrix();
    }

public:
    JogoTetris3D() : currentState(INTRO), rng(random_device{}()) { 
        camera.position=defaultCamPos; camera.target=defaultCamTarget; camera.up={0,1,0}; camera.fovy=45.0f; camera.projection=CAMERA_PERSPECTIVE;
        sndMove=LoadSound("src/move.mp3"); sndRotate=LoadSound("src/rotate.mp3"); sndDrop=LoadSound("src/drop.mp3"); sndClear1=LoadSound("src/clear1.mp3"); sndClear2=LoadSound("src/clear2.mp3"); sndClear3=LoadSound("src/clear3.mp3"); sndClear4=LoadSound("src/clear4.mp3"); sndGameOver=LoadSound("src/gameover.mp3");
        InitBackgroundPieces(); ShuffleMusic(); SpawnPiece(); 
        InitTouchButtons();
    }
    ~JogoTetris3D() {
        CloseNetwork(); if(sndMusic.stream.buffer) DetachAudioStreamProcessor(sndMusic.stream, AudioInputCallback);
        UnloadSound(sndMove); UnloadSound(sndRotate); UnloadSound(sndDrop); UnloadSound(sndClear1); UnloadSound(sndClear2); UnloadSound(sndClear3); UnloadSound(sndClear4); UnloadSound(sndGameOver);
        if(sndMusic.stream.buffer) UnloadMusicStream(sndMusic);
    }

    void Update(float dt) {
        float scaleX = (float)GetScreenWidth() / SCREEN_WIDTH;
        float scaleY = (float)GetScreenHeight() / SCREEN_HEIGHT;

        if(currentState == MENU || currentState == SETTINGS || currentState == NET_SETUP) UpdateTouchLogic(menuBtns, scaleX, scaleY);
        if(currentState == PLAYING) { UpdateTouchLogic(gameBtns, scaleX, scaleY); UpdateTouchLogic(sysBtns, scaleX, scaleY); }
        if(currentState == NET_SETUP) UpdateTouchLogic(netAutoFillBtn, scaleX, scaleY);

        if(hitStopTimer>0.0f) { hitStopTimer-=GetFrameTime(); dt*=0.1f; }
        for(int i=pieceTrails.size()-1; i>=0; i--) { pieceTrails[i].life-=dt; if(pieceTrails[i].life<=0) pieceTrails.erase(pieceTrails.begin()+i); }
        for(auto& bp:bgPieces) {
            bp.trail.push_front(bp.position); if(bp.trail.size()>15) bp.trail.pop_back();
            bp.position.x+=bp.velocity.x*dt; bp.position.y+=bp.velocity.y*dt; bp.position.z+=bp.velocity.z*dt;
            bp.rotation.x+=bp.rotVelocity.x*dt; bp.rotation.y+=bp.rotVelocity.y*dt; bp.rotation.z+=bp.rotVelocity.z*dt;
            if(bp.position.z>100.0f) { bp.position=GetSafeBgPos(); bp.position.z=-400.0f; bp.pieceType=GetRandomValue(0,pieces.size()-1); bp.color=pieceColors[pieces[bp.pieceType].colorID]; bp.trail.clear(); }
        }
        if(musicEnabled && sndMusic.stream.buffer) { UpdateMusicStream(sndMusic); if(GetMusicTimePlayed(sndMusic)>=GetMusicTimeLength(sndMusic)-0.1f) ShuffleMusic(); musicPulse=Lerp(musicPulse, globalMusicAmplitude*15.0f, dt*20.0f); } else musicPulse=Lerp(musicPulse, 0.0f, dt*5.0f);
        if(damageVignette>0.0f) damageVignette=Lerp(damageVignette,0.0f,dt*2.0f); if(goldTint>0.0f) goldTint=Lerp(goldTint,0.0f,dt*1.5f); 
        if(currentRotAngle!=0.0f) currentRotAngle=Lerp(currentRotAngle,0.0f,dt*15.0f); 
        if(pieceSpawnAnimTimer>0.0f) { pieceSpawnAnimTimer-=dt*2.0f; if(pieceSpawnAnimTimer<0.0f) pieceSpawnAnimTimer=0.0f; }
        if(aiPieceSpawnAnimTimer>0.0f) { aiPieceSpawnAnimTimer-=dt*2.0f; if(aiPieceSpawnAnimTimer<0.0f) aiPieceSpawnAnimTimer=0.0f; }

        if(currentState==INTRO) {
            introTimer-=dt; 
            if(introTimer<=0.0f) { currentState=MENU; introTimer=0.0f; damageVignette=0.0f; musicPulse=0.0f; }
            else { damageVignette=(GetRandomValue(0,100)>90)?GetRandomFloat(0.1f,0.3f):Lerp(damageVignette,0.0f,dt*5.0f); musicPulse=abs(sin(introTimer*10.0f))*2.0f; }
        } else if(currentState==NET_SETUP) {
            if(isTypingIP) {
                if(netAutoFillBtn[0].isPressed) { targetIP = "127.0.0.1"; }

                int key=GetCharPressed(); while(key>0){ if((key>=48&&key<=57)||key==46){ if(targetIP.length()<15){targetIP+=(char)key;TocarSom(sndMove);} } key=GetCharPressed(); }
                if(IsKeyPressed(KEY_BACKSPACE) && targetIP.length()>0){targetIP.pop_back();TocarSom(sndMove);}
                if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_ENTER)||netAutoFillBtn[0].isPressed){isTypingIP=false; netRole=2; SetupNetwork(targetIP,false); netConnected=false; Restart(); currentState=PLAYING; TocarSom(sndDrop);}
                if(IsKeyPressed(KEY_ESCAPE) || menuBtns[3].isPressed){isTypingIP=false;TocarSom(sndMove);}
            } else {
                if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W)||menuBtns[0].isPressed){netSelection--;TocarSom(sndMove);} 
                if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S)||menuBtns[1].isPressed){netSelection++;TocarSom(sndMove);}
                if(netSelection<0)netSelection=2; if(netSelection>2)netSelection=0;
                if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||menuBtns[2].isPressed){
                    TocarSom(sndDrop);
                    if(netSelection==0){netRole=1;targetIP="";SetupNetwork("",true);netConnected=false;Restart();currentState=PLAYING;}
                    else if(netSelection==1){isTypingIP=true;targetIP="";} else if(netSelection==2){currentState=MENU;}
                }
                if(IsKeyPressed(KEY_ESCAPE) || menuBtns[3].isPressed) currentState=MENU;
            }
        } else if(currentState==MENU || currentState==SETTINGS) {
            int* sel = (currentState==MENU)?&menuSelection:&settingsSelection; int maxSel = (currentState==MENU)?10:3; 
            if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W)||menuBtns[0].isPressed) { (*sel)--; TocarSom(sndMove); } 
            if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S)||menuBtns[1].isPressed) { (*sel)++; TocarSom(sndMove); }
            if(*sel<0) *sel=maxSel; if(*sel>maxSel) *sel=0;
            if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||menuBtns[2].isPressed) {
                TocarSom(sndDrop);
                if(currentState==MENU) {
                    if(*sel==0){isClassicMode=true;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=false;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==1){isClassicMode=false;isExpansiveMode=true;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=false;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==2){isClassicMode=false;isExpansiveMode=false;isBossMode=true;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=false;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==3){isClassicMode=false;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=true;isHardcoreMode=false;isDuelMode=false;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==4){isClassicMode=false;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=true;isDuelMode=false;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==5){isClassicMode=false;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=true;isDuelOnline=false;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==6){isClassicMode=false;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=true;isDuelOnline=true;isDuelNet=false;Restart();currentState=PLAYING;}
                    else if(*sel==7){isClassicMode=false;isExpansiveMode=false;isBossMode=false;isTimeAttackMode=false;isHardcoreMode=false;isDuelMode=true;isDuelOnline=false;isDuelNet=true;currentState=NET_SETUP;}
                    else if(*sel==8)currentState=SETTINGS; else if(*sel==9)currentState=CREDITS; else if(*sel==10)confirmExit=true; 
                } else {
                    if(*sel==0){currentQualityIdx=(currentQualityIdx+1)%4;globalGraphicsQuality=currentQualityIdx;InitBackgroundPieces();}
                    else if(*sel==1)sfxEnabled=!sfxEnabled;
                    else if(*sel==2){musicEnabled=!musicEnabled;if(!musicEnabled)PauseMusicStream(sndMusic);else ResumeMusicStream(sndMusic);}
                    else if(*sel==3)currentState=MENU;
                }
            }
            if((IsKeyPressed(KEY_ESCAPE) || menuBtns[3].isPressed) && currentState==SETTINGS) currentState=MENU;
        } else if(currentState==CREDITS) { if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_ENTER)||menuBtns[3].isPressed||menuBtns[2].isPressed) { TocarSom(sndDrop); currentState=MENU; } }
        else if(currentState==PLAYING) {
            if(isContinuing) {
                continueTimer-=dt;
                if(IsKeyPressed(KEY_Y) || sysBtns[0].isPressed) { isContinuing=false; continues=3; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j]=0; SpawnPiece(); TocarSom(sndClear2); } 
                else if(IsKeyPressed(KEY_N) || continueTimer<=0.0f || sysBtns[1].isPressed) { isContinuing=false; gameOver=true; TocarSom(sndGameOver); } return; 
            }
            if(IsKeyPressed(KEY_P) || gameBtns[7].isPressed) { isPaused=!isPaused; TocarSom(sndMove); } 
            if(IsKeyPressed(KEY_ESCAPE)) showExitPrompt=!showExitPrompt; 
            if(showExitPrompt) { if(IsKeyPressed(KEY_S)||IsKeyPressed(KEY_Y)||sysBtns[0].isPressed) { showExitPrompt=false; currentState=MENU; CloseNetwork(); } if(IsKeyPressed(KEY_N)||sysBtns[1].isPressed) showExitPrompt=false; return; }
            if(gameOver) { if(IsKeyPressed(KEY_ENTER)||gameBtns[4].isPressed||gameBtns[3].isPressed) { Restart(); currentState=MENU; CloseNetwork(); } return; }

            if(bossActive && !isPaused) {
                bossEntryAnim=Lerp(bossEntryAnim, 1.0f, dt*2.0f); bossAttackTimer-=dt; bossOrbitAngle+=dt*1.5f; bossCinematicCooldown-=dt;
                if(bossCinematicCooldown<=0.0f && bossCinematicSpinTimer<=0.0f) { bossCinematicSpinTimer=2.0f; bossCinematicCooldown=GetRandomFloat(20,30); mensagemEspecial="WARNING!"; timerMensagem=2.0f; }
                if(bossCinematicSpinTimer>0.0f) bossCinematicSpinTimer-=dt;
                if(GetRandomValue(0,100)>92) { cameraShakeTimer=0.1f; cameraShakeIntensity=1.5f; damageVignette=GetRandomFloat(0.1f,0.4f); }
                if(GetRandomValue(0,100)>90) { 
                    Particle3D fire; fire.position=currentBossPos; Vector3 target={GetRandomFloat(-10,10), currentGridElevation+GetRandomFloat(0,15), 0};
                    Vector3 dir = {target.x-currentBossPos.x, target.y-currentBossPos.y, target.z-currentBossPos.z}; float len=sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z); if(len>0) { dir.x/=len; dir.y/=len; dir.z/=len; } 
                    float force=GetRandomFloat(30,60); fire.velocity={dir.x*force, dir.y*force, dir.z*force}; fire.color=C_ORANGE; fire.maxLife=GetRandomFloat(0.3f,0.8f); fire.life=fire.maxLife; fire.size=GetRandomFloat(0.4f,1.0f); fire.isSpark=true; particles.push_back(fire);
                }
                if(bossAttackTimer<=0.0f) { BossAddJunkLine(); bossAttackTimer=currentBossAttackDelay; }
                if(musicEnabled && sndMusic.stream.buffer) SetMusicPitch(sndMusic, 0.8f+(musicPulse*0.1f));
            } else { bossEntryAnim=Lerp(bossEntryAnim, 0.0f, dt*2.0f); if(musicEnabled && sndMusic.stream.buffer) SetMusicPitch(sndMusic, 1.0f); }

            if(!isPaused) {
                if(isDuelMode && !gameOver && !isContinuing) {
                    for(int i=aiPieceTrails.size()-1; i>=0; i--) { aiPieceTrails[i].life-=dt; if(aiPieceTrails[i].life<=0) aiPieceTrails.erase(aiPieceTrails.begin()+i); }
                    if(isDuelNet) {
                        #ifdef _WIN32
                        static float syncTimer=0.0f; syncTimer+=dt;
                        if(syncTimer>0.05f) { 
                            syncTimer=0.0f;
                            if(netRole==1 && !netConnected) { } else if(netSocket!=OS_INVALID_SOCKET) {
                                NetPlayerState ps; memset(&ps,0,sizeof(ps)); ps.x=currentX; ps.y=currentY; ps.color=currentColor; ps.pSize=currentPiece.size();
                                for(int i=0; i<ps.pSize; i++) for(int j=0; j<ps.pSize; j++) ps.piece[i][j]=currentPiece[i][j];
                                for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) ps.boardState[i][j]=board[i][j];
                                ps.score=score; ps.bombs=bombs; ps.isDead=gameOver; osSendTo(netSocket, (const char*)&ps, sizeof(NetPlayerState), 0, &otherAddr, otherAddrLen);
                            }
                        }
                        if(netSocket!=OS_INVALID_SOCKET) {
                            NetPlayerState rs; os_sockaddr_in fromA; int fromL=sizeof(fromA);
                            while(true) {
                                int b=osRecvFrom(netSocket, (char*)&rs, sizeof(NetPlayerState), 0, &fromA, &fromL);
                                if(b==sizeof(NetPlayerState)) {
                                    if(netRole==1 && !netConnected) { otherAddr=fromA; otherAddrLen=fromL; netConnected=true; TocarSom(sndClear2); }
                                    if(netRole==2 && !netConnected) { netConnected=true; TocarSom(sndClear2); }
                                    aiCurrentX=rs.x; aiCurrentY=rs.y; aiCurrentColor=rs.color; aiCurrentPiece.clear(); aiCurrentPiece.resize(rs.pSize, vector<int>(rs.pSize,0));
                                    for(int i=0; i<rs.pSize; i++) for(int j=0; j<rs.pSize; j++) aiCurrentPiece[i][j]=rs.piece[i][j];
                                    for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<currentGridWidth; j++) { if(aiBoard[i][j]!=0 && rs.boardState[i][j]==0) SpawnParticles3D(GetAIWorldPos(j,i), pieceColors[aiBoard[i][j]], 10, 20.0f); aiBoard[i][j]=rs.boardState[i][j]; }
                                    aiScore=rs.score; aiBombs=rs.bombs; if(rs.isDead && !aiDead) { aiDead=true; TocarSom(sndGameOver); }
                                } else break; 
                            }
                        }
                        #endif
                    } else if(!isDuelOnline) {
                        aiMoveTimer-=dt;
                        if(aiMoveTimer<=0.0f) {
                            aiMoveTimer=fmax(0.12f, 0.40f-(level*0.015f)); 
                            if(aiTargetRot>0) { aiCurrentPiece=RotateMatrix(aiCurrentPiece); aiTargetRot--; aiCurrentRotAngle=90.0f; TocarSom(sndRotate); }
                            else if(aiCurrentX<aiTargetX) { if(IsValidAIMove(aiCurrentPiece, aiCurrentX+1, aiCurrentY)) { aiCurrentX++; TocarSom(sndMove); } else aiTargetX=aiCurrentX; }
                            else if(aiCurrentX>aiTargetX) { if(IsValidAIMove(aiCurrentPiece, aiCurrentX-1, aiCurrentY)) { aiCurrentX--; TocarSom(sndMove); } else aiTargetX=aiCurrentX; }
                            else { int dD=0, sY=aiCurrentY; while(IsValidAIMove(aiCurrentPiece, aiCurrentX, aiCurrentY+1)) { aiCurrentY++; dD++; } for(int k=1; k<=dD; k++) aiPieceTrails.push_back({aiCurrentPiece, aiCurrentColor, (float)aiCurrentX, (float)(sY+k), aiCurrentRotAngle, 0.4f-(k*0.01f), 0.4f}); aiScore+=dD*2; SpawnShockwave(GetAIWorldPos(aiCurrentX+aiCurrentPiece[0].size()/2.0f, aiCurrentY+aiCurrentPiece.size()/2.0f), WHITE); LockAIPiece(); }
                        }
                    } else {
                        float p2Spd=fmax(0.08f, 0.8f-(level*0.03f)); p2FallTimer+=dt;
                        if(IsKeyPressed(KEY_RIGHT_CONTROL) && aiBombs>0) { aiBombs--; AINukeBoard(); }
                        if(IsKeyPressed(KEY_RIGHT_SHIFT) && p2CanHold) { if(p2HoldPiece.empty()) { p2HoldPiece=aiCurrentPiece; p2HoldColor=aiCurrentColor; SpawnAIPiece(); } else { auto tP=aiCurrentPiece; int tC=aiCurrentColor; aiCurrentPiece=p2HoldPiece; aiCurrentColor=p2HoldColor; p2HoldPiece=tP; p2HoldColor=tC; aiCurrentX=currentGridWidth/2-aiCurrentPiece[0].size()/2; aiCurrentY=-5; aiRenderFallY=-5.0f; aiRenderX=aiCurrentX; } p2CanHold=false; TocarSom(sndMove); SpawnParticles3D(GetAIWorldPos(aiCurrentX, aiCurrentY), WHITE, 10, 5.0f); }
                        if(IsKeyPressed(KEY_LEFT)) { if(IsValidAIMove(aiCurrentPiece, aiCurrentX-1, aiCurrentY)) { aiCurrentX--; TocarSom(sndMove); SpawnParticles3D(GetAIWorldPos(aiCurrentX, aiCurrentY), WHITE, 2, 2.0f); } p2MoveLeftTimer=0.0f; } else if(IsKeyDown(KEY_LEFT)) { p2MoveLeftTimer+=dt; if(p2MoveLeftTimer>=DAS_DELAY) { p2MoveLeftTimer-=ARR_RATE; if(IsValidAIMove(aiCurrentPiece, aiCurrentX-1, aiCurrentY)) { aiCurrentX--; TocarSom(sndMove); } } } else p2MoveLeftTimer=0.0f;
                        if(IsKeyPressed(KEY_RIGHT)) { if(IsValidAIMove(aiCurrentPiece, aiCurrentX+1, aiCurrentY)) { aiCurrentX++; TocarSom(sndMove); SpawnParticles3D(GetAIWorldPos(aiCurrentX, aiCurrentY), WHITE, 2, 2.0f); } p2MoveRightTimer=0.0f; } else if(IsKeyDown(KEY_RIGHT)) { p2MoveRightTimer+=dt; if(p2MoveRightTimer>=DAS_DELAY) { p2MoveRightTimer-=ARR_RATE; if(IsValidAIMove(aiCurrentPiece, aiCurrentX+1, aiCurrentY)) { aiCurrentX++; TocarSom(sndMove); } } } else p2MoveRightTimer=0.0f;
                        if(IsKeyPressed(KEY_UP)) { auto r=RotateMatrix(aiCurrentPiece); if(IsValidAIMove(r, aiCurrentX, aiCurrentY)) { aiCurrentPiece=r; TocarSom(sndRotate); aiCurrentRotAngle=90.0f; SpawnParticles3D(GetAIWorldPos(aiCurrentX, aiCurrentY), pieceColors[aiCurrentColor], 5, 5.0f); } else if(IsValidAIMove(r, aiCurrentX-1, aiCurrentY)) { aiCurrentPiece=r; aiCurrentX--; TocarSom(sndRotate); aiCurrentRotAngle=90.0f; } else if(IsValidAIMove(r, aiCurrentX+1, aiCurrentY)) { aiCurrentPiece=r; aiCurrentX++; TocarSom(sndRotate); aiCurrentRotAngle=90.0f; } }
                        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_0)) { int dD=0, sY=aiCurrentY; while(IsValidAIMove(aiCurrentPiece, aiCurrentX, aiCurrentY+1)) { aiCurrentY++; dD++; } for(int k=1; k<=dD; k++) aiPieceTrails.push_back({aiCurrentPiece, aiCurrentColor, (float)aiCurrentX, (float)(sY+k), aiCurrentRotAngle, 0.4f-(k*0.01f), 0.4f}); aiScore+=dD*2; SpawnShockwave(GetAIWorldPos(aiCurrentX+aiCurrentPiece[0].size()/2.0f, aiCurrentY+aiCurrentPiece.size()/2.0f), WHITE); LockAIPiece(); p2FallTimer=0; cameraShakeTimer=0.3f; cameraShakeIntensity=fmax(2.0f, dD*0.2f); } else if(IsKeyDown(KEY_DOWN)) { p2FallTimer+=dt*30.0f; if(p2FallTimer>=p2Spd && IsValidAIMove(aiCurrentPiece, aiCurrentX, aiCurrentY+1)) aiScore+=1; }
                        if(p2FallTimer>=p2Spd) { p2FallTimer=0; if(IsValidAIMove(aiCurrentPiece, aiCurrentX, aiCurrentY+1)) aiCurrentY++; else LockAIPiece(); }
                    }
                    aiCurrentRotAngle=Lerp(aiCurrentRotAngle,0.0f,dt*15.0f); aiRenderFallY=Lerp(aiRenderFallY,(float)aiCurrentY,dt*30.0f); aiRenderX=Lerp(aiRenderX,(float)aiCurrentX,dt*30.0f);
                }

                if(aiComboTimer>0.0f) {
                    aiComboTimer-=dt; if(aiComboTimer<=0.0f) { aiComboTimer=0.0f; if(aiComboCount>0) { Vector3 cF=Vector3Normalize(Vector3Subtract(camera.target,camera.position)), cR=Vector3Normalize(Vector3CrossProduct(cF,camera.up)), cU=Vector3CrossProduct(cR,cF), sP=camera.position; sP=Vector3Add(sP,Vector3Scale(cF,25.0f)); sP=Vector3Add(sP,Vector3Scale(cR,13.0f)); sP=Vector3Add(sP,Vector3Scale(cU,1.0f)); SpawnParticles3D(sP, C_RED, 80, 50.0f, 3); SpawnParticles3D(sP, C_ORANGE, 60, 80.0f, 1); TocarSom(sndDrop); cameraShakeTimer=0.5f; cameraShakeIntensity=3.5f; } aiComboCount=0; }
                }

                if(isTimeAttackMode && !bossActive) { timeAttackTimer-=dt; if(timeAttackTimer<=0.0f) { timeAttackTimer=0.0f; gameOver=true; TocarSom(sndGameOver); } }
                if(isHardcoreMode && !bossActive && !gameOver) { hardcoreJunkTimer-=dt; if(hardcoreJunkTimer<=0.0f) { hardcoreJunkTimer=GetRandomFloat(8,15); BossAddJunkLine(); mensagemEspecial="SYSTEM CORRUPTION!"; timerMensagem=1.5f; TocarSom(sndClear4); } }
                trailSpawnTimer+=dt; if(trailSpawnTimer>0.04f) { pieceTrails.push_back({currentPiece, currentColor, renderX, renderFallY, currentRotAngle, 0.25f, 0.25f}); trailSpawnTimer=0.0f; }

                if(comboTimer>0.0f) {
                    comboTimer-=dt; if(comboTimer<=0.0f) { comboTimer=0.0f; if(comboCount>0) { Vector3 cF=Vector3Normalize(Vector3Subtract(camera.target,camera.position)), cR=Vector3Normalize(Vector3CrossProduct(cF,camera.up)), cU=Vector3CrossProduct(cR,cF), sP=camera.position; sP=Vector3Add(sP,Vector3Scale(cF,25.0f)); sP=Vector3Add(sP,Vector3Scale(cR,13.0f)); sP=Vector3Add(sP,Vector3Scale(cU,isDuelMode?-5.0f:-1.0f)); SpawnParticles3D(sP, C_GOLD, 80, 50.0f, 3); SpawnParticles3D(sP, C_RED, 60, 80.0f, 1); TocarSom(sndDrop); cameraShakeTimer=0.5f; cameraShakeIntensity=3.5f; } comboCount=0; }
                }

                float speed=fmax(0.08f, 0.8f-(level*0.03f)); fallTimer+=dt;
                bool p1Nuke = (isDuelOnline||isDuelNet)?IsKeyPressed(KEY_LEFT_CONTROL):(IsKeyPressed(KEY_LEFT_CONTROL)||IsKeyPressed(KEY_RIGHT_CONTROL)) || gameBtns[6].isPressed;
                if(p1Nuke && bombs>0) { bombs--; NukeBoard(); }
                bool p1Hold = (isDuelOnline||isDuelNet)?(IsKeyPressed(KEY_LEFT_SHIFT)||IsKeyPressed(KEY_C)):(IsKeyPressed(KEY_C)||IsKeyPressed(KEY_LEFT_SHIFT)) || gameBtns[5].isPressed;
                if(p1Hold && canHold) { if(holdPiece.empty()) { holdPiece=currentPiece; holdColor=currentColor; SpawnPiece(); } else { auto tP=currentPiece; int tC=currentColor; currentPiece=holdPiece; currentColor=holdColor; holdPiece=tP; holdColor=tC; currentX=currentGridWidth/2-currentPiece[0].size()/2; currentY=-5; renderFallY=-5.0f; renderX=currentX; } canHold=false; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 10, 5.0f); }

                cameraBankAngle=Lerp(cameraBankAngle,0.0f,dt*10.0f); 
                bool p1L = (isDuelOnline||isDuelNet)?IsKeyPressed(KEY_A):(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A)) || gameBtns[0].isPressed; 
                bool p1LD = (isDuelOnline||isDuelNet)?IsKeyDown(KEY_A):(IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A)) || gameBtns[0].isDown;
                if(p1L) { if(IsValidMove(currentPiece, currentX-1, currentY)) { currentX--; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); } moveLeftTimer=0.0f; } else if(p1LD) { moveLeftTimer+=dt; if(moveLeftTimer>=DAS_DELAY) { moveLeftTimer-=ARR_RATE; if(IsValidMove(currentPiece, currentX-1, currentY)) { currentX--; TocarSom(sndMove); } } } else moveLeftTimer=0.0f;
                
                bool p1R = (isDuelOnline||isDuelNet)?IsKeyPressed(KEY_D):(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D)) || gameBtns[1].isPressed; 
                bool p1RD = (isDuelOnline||isDuelNet)?IsKeyDown(KEY_D):(IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D)) || gameBtns[1].isDown;
                if(p1R) { if(IsValidMove(currentPiece, currentX+1, currentY)) { currentX++; TocarSom(sndMove); SpawnParticles3D(GetWorldPos(currentX, currentY), WHITE, 2, 2.0f); } moveRightTimer=0.0f; } else if(p1RD) { moveRightTimer+=dt; if(moveRightTimer>=DAS_DELAY) { moveRightTimer-=ARR_RATE; if(IsValidMove(currentPiece, currentX+1, currentY)) { currentX++; TocarSom(sndMove); } } } else moveRightTimer=0.0f;
                
                bool p1U = (isDuelOnline||isDuelNet)?IsKeyPressed(KEY_W):(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W)) || gameBtns[4].isPressed;
                if(p1U) { auto r=RotateMatrix(currentPiece); if(IsValidMove(r, currentX, currentY)) { currentPiece=r; TocarSom(sndRotate); currentRotAngle=90.0f; SpawnParticles3D(GetWorldPos(currentX, currentY), pieceColors[currentColor], 5, 5.0f); } else if(IsValidMove(r, currentX-1, currentY)) { currentPiece=r; currentX--; TocarSom(sndRotate); currentRotAngle=90.0f; } else if(IsValidMove(r, currentX+1, currentY)) { currentPiece=r; currentX++; TocarSom(sndRotate); currentRotAngle=90.0f; } }
                
                if(IsKeyPressed(KEY_SPACE) || gameBtns[3].isPressed) { int dD=0, sY=currentY; while(IsValidMove(currentPiece, currentX, currentY+1)) { currentY++; dD++; } for(int k=1; k<=dD; k++) pieceTrails.push_back({currentPiece, currentColor, (float)currentX, (float)(sY+k), currentRotAngle, 0.4f-(k*0.01f), 0.4f}); score+=dD*2; SpawnShockwave(GetWorldPos(currentX+currentPiece[0].size()/2.0f, currentY+currentPiece.size()/2.0f), WHITE); LockPiece(); fallTimer=0; cameraShakeTimer=0.3f; cameraShakeIntensity=fmax(2.0f, dD*0.2f); } 
                else if((isDuelOnline||isDuelNet)?IsKeyDown(KEY_S):(IsKeyDown(KEY_DOWN)||IsKeyDown(KEY_S)) || gameBtns[2].isDown) { fallTimer+=dt*30.0f; if(fallTimer>=speed && IsValidMove(currentPiece, currentX, currentY+1)) score+=1; }
                if(fallTimer>=speed) { fallTimer=0; if(IsValidMove(currentPiece, currentX, currentY+1)) currentY++; else LockPiece(); }
                renderFallY=Lerp(renderFallY, (float)currentY, dt*30.0f); renderX=Lerp(renderX, (float)currentX, dt*30.0f);
            } 
        }
        if(gridExpansionTimer>0.0f) gridExpansionTimer-=dt;
        if(IsKeyPressed(KEY_Y)) { manualZoomOffset=0.0f; manualCamAngleX=0.0f; manualCamAngleY=0.0f; manualCamPan.x=0.0f; manualCamPan.y=0.0f; manualCamPan.z=0.0f; TocarSom(sndMove); }
        float wheel = GetMouseWheelMove(); if(wheel!=0.0f) manualZoomOffset-=wheel*6.0f; 
        
        if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { Vector2 d=GetMouseDelta(); manualCamAngleX-=d.x*0.005f; manualCamAngleY+=d.y*0.005f; if(manualCamAngleY>1.4f) manualCamAngleY=1.4f; if(manualCamAngleY<-1.4f) manualCamAngleY=-1.4f; }
        if(IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) { Vector2 d=GetMouseDelta(); manualCamPan.x-=d.x*0.05f; manualCamPan.y+=d.y*0.05f; }

        float zoomMult = 0.6f+((currentGridWidth-10)*0.08f), gridZoom = (currentGridWidth-10)*zoomMult+manualZoomOffset, finalDist = 34.0f+gridZoom; 
        if(isDuelMode) finalDist+=5.5f; 
        cameraFovTarget=Lerp(cameraFovTarget, 45.0f, dt*2.0f); currentGridElevation=Lerp(currentGridElevation, (bossActive||bossEntryAnim>0.01f)?Lerp(2.0f,7.0f,bossEntryAnim):2.0f, dt*3.0f);
        float bOA=(float)GetTime()*0.2f+nukeSpinAngle, cX=manualCamPan.x-1.0f+(float)sin(bOA)*2.0f+(float)sin(manualCamAngleX)*finalDist, cZ=manualCamPan.z+(float)cos(manualCamAngleX)*finalDist, cY=manualCamPan.y+currentGridElevation+2.5f+(float)sin(manualCamAngleY)*finalDist;
        Vector3 tPN={cX,cY,cZ}, tTN={manualCamPan.x, currentGridElevation+13.0f+manualCamPan.y, manualCamPan.z};
        float bD=20.0f+sin(bossOrbitAngle*0.45f)*15.0f+(gridZoom*0.5f); currentBossPos.x=manualCamPan.x+(float)sin(bossOrbitAngle)*bD; currentBossPos.y=manualCamPan.y+currentGridElevation+Lerp(45.0f, 12.0f+sin(bossOrbitAngle*0.7f)*14.0f, bossEntryAnim); currentBossPos.z=manualCamPan.z+(float)cos(bossOrbitAngle)*bD;
        Vector3 tPB={tPN.x,tPN.y,tPN.z+5.0f}, tTB={manualCamPan.x, currentGridElevation+13.0f+manualCamPan.y, manualCamPan.z};
        if(bossActive && bossCinematicSpinTimer>0.0f) { float sP=1.0f-(bossCinematicSpinTimer/2.0f), eS=sP*sP*(3.0f-2.0f*sP), sA=eS*PI*2.0f+manualCamAngleX, r=finalDist+5.0f; tPB.x=manualCamPan.x+(float)sin(sA)*r; tPB.y=manualCamPan.y+currentGridElevation+5.0f+sin(sP*PI)*10.0f+(float)sin(manualCamAngleY)*r; tPB.z=manualCamPan.z+(float)cos(sA)*r; tTB.x=manualCamPan.x; tTB.y=currentGridElevation+13.0f+manualCamPan.y; tTB.z=manualCamPan.z; cameraFovTarget=Lerp(45.0f, 65.0f, sin(sP*PI)); }
        Vector3 fPT=Vector3Lerp(tPN,tPB,bossEntryAnim), fLT=Vector3Lerp(tTN,tTB,bossEntryAnim);
        if(cameraShakeTimer>0 && !isPaused) { float rx=GetRandomFloat(-cameraShakeIntensity,cameraShakeIntensity), ry=GetRandomFloat(-cameraShakeIntensity,cameraShakeIntensity); fPT.x+=rx; fPT.y+=ry; fLT.x+=rx*0.5f; fLT.y+=ry*0.5f; cameraShakeTimer-=dt; cameraShakeIntensity=Lerp(cameraShakeIntensity,0.0f,dt*5.0f); }
        camera.position=Vector3Lerp(camera.position, fPT, dt*15.0f); camera.target=Vector3Lerp(camera.target, fLT, dt*15.0f); camera.fovy=Lerp(camera.fovy, cameraFovTarget, dt*10.0f); camera.up.x=sin(cameraBankAngle*DEG2RAD); camera.up.y=cos(cameraBankAngle*DEG2RAD); camera.up.z=0.0f;
        if(timerMensagem>0 && !isPaused) { timerMensagem-=dt; if(timerMensagem<=0.0f) mensagemEspecial=""; }
    }

    void Draw() {
        ClearBackground(C_BG);

        float scaleX = (float)GetScreenWidth() / SCREEN_WIDTH;
        float scaleY = (float)GetScreenHeight() / SCREEN_HEIGHT;

        if (currentState == INTRO) {
            rlPushMatrix();
            rlScalef(scaleX, scaleY, 1.0f);
            DrawIntro2D(GetFrameTime());
            rlPopMatrix();
        } else {
            // ---- DRAW 3D SCENE DIRECTLY TO SCREEN ----
            BeginMode3D(camera); 
            DrawProceduralEnvironment(); 
            if(isDuelMode) {
                rlPushMatrix(); rlTranslatef(-7.5f,0,0); DrawPlayfieldAndPieces(); rlPopMatrix();
                swap(board,aiBoard); swap(currentPiece,aiCurrentPiece); swap(currentX,aiCurrentX); swap(currentY,aiCurrentY); swap(currentColor,aiCurrentColor); swap(renderFallY,aiRenderFallY); swap(renderX,aiRenderX); swap(currentRotAngle,aiCurrentRotAngle); swap(pieceTrails,aiPieceTrails); swap(currentIsBrilliant,aiIsBrilliant); swap(pieceSpawnAnimTimer,aiPieceSpawnAnimTimer); 
                rlPushMatrix(); rlTranslatef(7.5f,0,0); DrawPlayfieldAndPieces(); rlPopMatrix();
                swap(board,aiBoard); swap(currentPiece,aiCurrentPiece); swap(currentX,aiCurrentX); swap(currentY,aiCurrentY); swap(currentColor,aiCurrentColor); swap(renderFallY,aiRenderFallY); swap(renderX,aiRenderX); swap(currentRotAngle,aiCurrentRotAngle); swap(pieceTrails,aiPieceTrails); swap(currentIsBrilliant,aiIsBrilliant); swap(pieceSpawnAnimTimer,aiPieceSpawnAnimTimer); 
            } else {
                DrawPlayfieldAndPieces();
            }
            DrawBossEncounter(); 
            UpdateAndDrawParticles3D(GetFrameTime()); 
            EndMode3D(); 
            
            // ---- EFEITOS DE TELA CHEIA COMPENSATÓRIOS SEM SHADER ----
            if (damageVignette > 0.0f) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(RED, damageVignette * 0.3f));
            }
            if (goldTint > 0.0f) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(GOLD, goldTint * 0.15f));
            }

            // ---- UI ESTICADA PRA COBRIR A TELA ----
            rlPushMatrix();
            rlScalef(scaleX, scaleY, 1.0f);
            DrawFloatingTexts(GetFrameTime(), scaleX, scaleY);
            DrawUI(); 
            rlPopMatrix();
        }
    }

    void DrawUI() {
        if(currentState==INTRO) return; 
        if(currentState==NET_SETUP) {
            DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.8f));
            DrawTetrisText("NETWORK PROTOCOL", SCREEN_WIDTH/2-MeasureTetrisText("NETWORK PROTOCOL",10.0f)/2, 150, 10.0f, musicPulse*0.2f, false, C_CYAN);
            if(isTypingIP) {
                DrawTetrisText("ENTER HOST IP ADDRESS:", SCREEN_WIDTH/2-MeasureTetrisText("ENTER HOST IP ADDRESS:",6.0f)/2, 350, 6.0f, 0, false, C_ORANGE);
                DrawFrameWithCubes2D({(float)SCREEN_WIDTH/2-300, 420, 600, 80}, 8.0f, C_CYAN, ColorAlpha(C_BG,0.9f));
                DrawTetrisText(targetIP+"_", SCREEN_WIDTH/2-MeasureTetrisText(targetIP+"_",8.0f)/2, 435, 8.0f, 0, false, WHITE);
                DrawTetrisText("PRESS [ENTER] TO CONNECT", SCREEN_WIDTH/2-MeasureTetrisText("PRESS [ENTER] TO CONNECT",4.0f)/2, 550, 4.0f, musicPulse*0.1f, false, C_GOLD);
                DrawTouchGamepad(netAutoFillBtn); 
                DrawTouchGamepad(menuBtns);
            } else {
                const char* mI[]={"HOST GAME", "JOIN GAME", "RETURN"};
                for(int i=0; i<3; i++) {
                    Color c=(i==netSelection)?C_ORANGE:GRAY; float iS=7.0f, bW=MeasureTetrisText(mI[i],iS), iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f), yP=350.0f+i*70.0f;
                    if(i==netSelection) { float aS=iS*(1.0f+musicPulse*0.05f); bW=MeasureTetrisText(mI[i],aS); iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f); DrawTetrisText("> ", iX-8*aS, yP, aS, musicPulse, false, C_ORANGE); DrawTetrisText(mI[i], iX, yP, aS, musicPulse*0.3f, false, C_ORANGE); }
                    else DrawTetrisText(mI[i], iX, yP, iS, 0, false, c);
                }
                DrawTouchGamepad(menuBtns);
            }
            DrawTetrisText("(C) BETTARELLO CODE.", SCREEN_WIDTH/2-MeasureTetrisText("(C) BETTARELLO CODE.",2.5f)/2, SCREEN_HEIGHT-30, 2.5f, 0, false, ColorAlpha(GRAY,0.7f));
        } else if(currentState==MENU) {
            DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.7f));
            DrawTetrisText("TETRABETTA", (SCREEN_WIDTH/2.0f)-(MeasureTetrisText("TETRABETTA",16.0f)/2.0f), 120.0f, 16.0f, musicPulse, true, WHITE);
            DrawTetrisText("GOLD EDITION", (SCREEN_WIDTH/2.0f)-(MeasureTetrisText("GOLD EDITION",6.0f)/2.0f), 220.0f, 6.0f, 0, false, C_GOLD);
            const char* mI[]={"CLASSIC RUN", "EXPANSIVE RUN", "BOSS RUSH", "TIME ATTACK", "HARDCORE RUN", "DUEL - AI", "DUEL - ONLINE", "DUEL - NET", "SYSTEM CONFIG", "CREDITS", "LOGOUT"};
            for(int i=0; i<11; i++) {
                Color c=(i==menuSelection)?C_GOLD:GRAY; float iS=8.0f, bW=MeasureTetrisText(mI[i],iS), iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f), yP=340.0f+i*50.0f; 
                if(i==menuSelection) { float aS=iS*(1.0f+musicPulse*0.05f); bW=MeasureTetrisText(mI[i],aS); iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f); DrawTetrisText("> ", iX-8*aS, yP, aS, musicPulse, false, C_GOLD); DrawTetrisText(mI[i], iX, yP, aS, musicPulse*0.5f, false, C_GOLD); }
                else DrawTetrisText(mI[i], iX, yP, iS, 0, false, c);
            }
            DrawTetrisText("(C) BETTARELLO CODE.", SCREEN_WIDTH/2-MeasureTetrisText("(C) BETTARELLO CODE.",2.5f)/2, SCREEN_HEIGHT-30, 2.5f, 0, false, ColorAlpha(GRAY,0.7f));
            DrawTouchGamepad(menuBtns);
        } else if(currentState==SETTINGS) {
            DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.8f));
            DrawTetrisText("SYSTEM CONFIG", (SCREEN_WIDTH/2.0f)-(MeasureTetrisText("SYSTEM CONFIG",10.0f)/2.0f), 150.0f, 10.0f, musicPulse*0.2f, false, C_CYAN);
            string o0="GRAPHICS: "+qualities[currentQualityIdx], o1="HAPTIC SFX: "+string(sfxEnabled?"ON":"OFF"), o2="SYNTHWAVE: "+string(musicEnabled?"ON":"OFF"), o3="RETURN";
            const char* sI[]={o0.c_str(), o1.c_str(), o2.c_str(), o3.c_str()};
            for(int i=0; i<4; i++) { 
                Color c=(i==settingsSelection)?C_ORANGE:GRAY; float iS=7.0f, bW=MeasureTetrisText(sI[i],iS), iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f), yP=350.0f+i*70.0f;
                if(i==settingsSelection) { float aS=iS*(1.0f+musicPulse*0.05f); bW=MeasureTetrisText(sI[i],aS); iX=(SCREEN_WIDTH/2.0f)-(bW/2.0f); DrawTetrisText("> ", iX-8*aS, yP, aS, musicPulse, false, C_ORANGE); DrawTetrisText(sI[i], iX, yP, aS, musicPulse*0.3f, false, C_ORANGE); }
                else DrawTetrisText(sI[i], iX, yP, iS, 0, false, c);
            }
            DrawTetrisText("(C) BETTARELLO CODE.", SCREEN_WIDTH/2-MeasureTetrisText("(C) BETTARELLO CODE.",2.5f)/2, SCREEN_HEIGHT-30, 2.5f, 0, false, ColorAlpha(GRAY,0.7f));
            DrawTouchGamepad(menuBtns);
        } else if(currentState==CREDITS) {
            DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.9f));
            DrawTetrisText("PRODUCER AND CODER", SCREEN_WIDTH/2-MeasureTetrisText("PRODUCER AND CODER",5.0f)/2, 500, 5.0f, 0, false, C_ORANGE);
            DrawTetrisText("IGOR BETTARELLO XAVIER", SCREEN_WIDTH/2-MeasureTetrisText("IGOR BETTARELLO XAVIER",8.0f)/2, 600, 8.0f, musicPulse*0.2f, true, WHITE); 
            DrawTetrisText("(C) BETTARELLO CODE.", SCREEN_WIDTH/2-MeasureTetrisText("(C) BETTARELLO CODE.",2.5f)/2, SCREEN_HEIGHT-30, 2.5f, 0, false, ColorAlpha(GRAY,0.7f));
            DrawTouchGamepad(menuBtns); 
        } else if(currentState==PLAYING) {
            DrawFrameWithCubes2D({40, 40, 480, 400}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f)); 
            DrawTetrisText("GOLD EDITION", 60, 60, 2.5f, 0, false, C_GOLD); DrawTetrisText(TextFormat("SCORE: %08d", score), 60, 100, 6.0f, 0, false, WHITE); DrawTetrisText(TextFormat("LEVEL: %02d", level), 60, 160, 5.0f, 0, false, C_ORANGE);
            if(!isDuelMode) DrawTetrisText(TextFormat("LIVES: %d", continues), 60, 210, 4.0f, 0, false, C_RED); 
            else DrawTetrisText(isDuelNet?"MODE: NET":(isDuelOnline?"MODE: PVP":"MODE: AI"), 60, 210, 4.0f, 0, false, C_CYAN);
            int tY=260; 
            if(isExpansiveMode) { DrawTetrisText("MAGIC CHARGE:", 60, 260, 3.0f, 0, false, WHITE); DrawRectangleLines(60, 290, 300, 20, C_YELLOW); DrawRectangle(60, 290, 300*(stars/10.0f), 20, ColorAlpha(C_YELLOW,0.7f+sin(GetTime()*10)*0.3f)); tY=340; }
            else if(isTimeAttackMode) { DrawTetrisText("TIME REMAINING:", 60, 260, 3.0f, 0, false, WHITE); DrawTetrisText(TextFormat("%02d:%02d", (int)timeAttackTimer/60, (int)timeAttackTimer%60), 60, 290, 6.0f, musicPulse*0.2f, false, timeAttackTimer<30.0f?C_RED:C_CYAN); tY=340; }
            else if(isHardcoreMode) { DrawTetrisText("SPEED: MAX", 60, 260, 4.0f, musicPulse*0.2f, false, C_RED); tY=310; }
            
            if(!isContinuing && !gameOver && !isPaused && !showExitPrompt) {
                DrawTouchGamepad(gameBtns);
            }

            if(bossActive && isBossMode) { float gX=(GetRandomValue(0,10)>8)?GetRandomFloat(-8,8):0, gY=(GetRandomValue(0,10)>8)?GetRandomFloat(-8,8):0; DrawFrameWithCubes2D({(float)SCREEN_WIDTH/2-400+gX, 40+gY, 800, 80}, 8.0f, C_RED, ColorAlpha(C_BG,0.7f)); string bN=TextFormat("ANOMALY: OMEGARED V.%d", bossEncounterCount); DrawTetrisText(bN, SCREEN_WIDTH/2-MeasureTetrisText(bN,5.0f)/2+gX, 50+gY, 5.0f, 0, false, C_RED); DrawRectangleRounded({SCREEN_WIDTH/2-380+gX, 85+gY, 760*((float)bossHp/(10.0f+(bossEncounterCount*5.0f))), 20}, 0.5f, 8, C_ORANGE); }
            
            DrawFrameWithCubes2D({SCREEN_WIDTH-360.0f, 250.0f, 320.0f, 264.0f}, 8.0f, ColorAlpha(C_GOLD, 0.5f), ColorAlpha(C_BG, 0.6f)); DrawTetrisText("NEXT QUEUE", SCREEN_WIDTH-200-MeasureTetrisText("NEXT QUEUE",3.5f)/2, 270, 3.5f, 0, false, C_GOLD);
            if(!gameOver && !isPaused && !isContinuing) {
                DrawPiece2D(nextPiece, nextColor, SCREEN_WIDTH - 200.0f, 390.0f, 35.0f, false);
                DrawFrameWithCubes2D({SCREEN_WIDTH-360.0f, 40.0f, 320.0f, 180.0f}, 8.0f, ColorAlpha(C_GREEN, 0.5f), ColorAlpha(C_BG, 0.6f));
                DrawTetrisText("HOLD", SCREEN_WIDTH - 200-MeasureTetrisText("HOLD", 3.0f)/2, 60.0f, 3.0f, 0, false, C_GREEN);
                DrawPiece2D(holdPiece, holdColor, SCREEN_WIDTH - 200.0f, 130.0f, 30.0f, true);
            }

            if(isDuelMode) { DrawFrameWithCubes2D({SCREEN_WIDTH-360.0f, 520.0f, 320.0f, 120.0f}, 8.0f, ColorAlpha(C_RED, 0.5f), ColorAlpha(C_BG, 0.6f)); string aT=(isDuelOnline||isDuelNet)?"P2 SCORE":"AI SCORE"; DrawTetrisText(aT, SCREEN_WIDTH-200-MeasureTetrisText(aT,3.5f)/2, 540.0f, 3.5f, 0, false, C_RED); DrawTetrisText(TextFormat("%08d", aiScore), SCREEN_WIDTH-200-MeasureTetrisText("00000000",5.0f)/2, 575.0f, 5.0f, 0, false, WHITE); string nT=(isDuelOnline||isDuelNet)?TextFormat("P2 NUKES: %d", aiBombs):TextFormat("AI NUKES: %d", aiBombs); DrawTetrisText(nT, SCREEN_WIDTH-200-MeasureTetrisText("AI NUKES: 2",3.0f)/2, 610.0f, 3.0f, 0, false, C_ORANGE); }
            if(isDuelNet && !netConnected && !gameOver) { string wM="AWAITING NETWORK..."; DrawTetrisTextGlowing(wM, SCREEN_WIDTH-200-MeasureTetrisText(wM,3.0f)/2, SCREEN_HEIGHT/2, 3.0f, musicPulse*0.2f); }
            if(comboCount>0 && comboTimer>0.0f && !isContinuing) { float pS=1.0f+(musicPulse*0.05f), cS=6.5f*pS; string cM=TextFormat("COMBO X%d", comboCount); float rPC=isDuelMode?(SCREEN_WIDTH-200.0f):(SCREEN_WIDTH-240.0f), cYP=isDuelMode?740.0f:450.0f; DrawTetrisTextGlowing(cM, rPC-MeasureTetrisText(cM,cS)/2.0f, cYP, cS, musicPulse*0.2f); string tV=TextFormat("%.2f", comboTimer); float tS=8.5f*pS, tP=(comboTimer<=2.0f)?abs(sin((float)GetTime()*15.0f))*0.8f:0.0f; if(comboTimer<=2.0f) DrawTetrisText(tV, rPC-MeasureTetrisText(tV,tS)/2.0f, cYP+65.0f, tS, tP, false, C_RED); else DrawTetrisTextGlowing(tV, rPC-MeasureTetrisText(tV,tS)/2.0f, cYP+65.0f, tS, tP); }
            if(timerMensagem>0 && !isContinuing) { float pS=ElasticEaseOut(1.0f-(timerMensagem/3.0f)), fS=9.5f*pS, rPC=isDuelMode?(SCREEN_WIDTH-200.0f):(SCREEN_WIDTH-240.0f), mYP=isDuelMode?860.0f:580.0f; DrawTetrisTextGlowing(mensagemEspecial, rPC-MeasureTetrisText(mensagemEspecial,fS)/2.0f, mYP, fS, 0.0f); }
            if(isDuelMode) { if(aiComboCount>0 && aiComboTimer>0.0f && !isContinuing) { float pS=1.0f+(musicPulse*0.05f), cS=6.5f*pS; string cM=TextFormat("COMBO X%d", aiComboCount); float pC=SCREEN_WIDTH-200.0f, cYP=490.0f; DrawTetrisTextGlowing(cM, pC-MeasureTetrisText(cM,cS)/2.0f, cYP, cS, musicPulse*0.2f); string tV=TextFormat("%.2f", aiComboTimer); float tS=8.5f*pS, tP=(aiComboTimer<=2.0f)?abs(sin((float)GetTime()*15.0f))*0.8f:0.0f; if(aiComboTimer<=2.0f) DrawTetrisText(tV, pC-MeasureTetrisText(tV,tS)/2.0f, cYP+65.0f, tS, tP, false, C_RED); else DrawTetrisTextGlowing(tV, pC-MeasureTetrisText(tV,tS)/2.0f, cYP+65.0f, tS, tP); } if(aiTimerMensagem>0 && !isContinuing) { float pS=ElasticEaseOut(1.0f-(aiTimerMensagem/3.0f)), fS=9.5f*pS, pC=SCREEN_WIDTH-200.0f, mYP=610.0f; DrawTetrisTextGlowing(aiMensagemEspecial, pC-MeasureTetrisText(aiMensagemEspecial,fS)/2.0f, mYP, fS, 0.0f); } }
            if(isPaused && !isContinuing) { DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.8f)); DrawTetrisText("SYSTEM PAUSED", SCREEN_WIDTH/2-MeasureTetrisText("SYSTEM PAUSED",10.0f)/2, SCREEN_HEIGHT/2-30, 10.0f, 0, false, C_GOLD); DrawTouchGamepad(gameBtns); }
            if(isContinuing) { DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.85f)); DrawTetrisText("CONTINUE?", SCREEN_WIDTH/2-MeasureTetrisText("CONTINUE?",12.0f)/2, SCREEN_HEIGHT/2-140, 12.0f, 0, false, C_ORANGE); int tI=fmax(0,(int)ceil(continueTimer)); DrawTetrisText(to_string(tI), SCREEN_WIDTH/2-MeasureTetrisText(to_string(tI),25.0f)/2, SCREEN_HEIGHT/2-20, 25.0f, musicPulse*0.3f, false, C_RED); DrawTouchGamepad(sysBtns); }
            if(gameOver) { DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(C_RED,0.4f)); DrawRectangle(0,SCREEN_HEIGHT/2-100,SCREEN_WIDTH,200,ColorAlpha(BLACK,0.9f)); if(isDuelMode) { string w=""; if(aiDead && continues>0) w=(isDuelOnline||isDuelNet)?"PLAYER 1 WINS!":"PLAYER WINS!"; else if(continues<=0 && !aiDead) w=(isDuelOnline||isDuelNet)?"PLAYER 2 WINS!":"AI WINS!"; else w=(score>=aiScore)?((isDuelOnline||isDuelNet)?"PLAYER 1 WINS!":"PLAYER WINS!"):((isDuelOnline||isDuelNet)?"PLAYER 2 WINS!":"AI WINS!"); DrawTetrisText(w, SCREEN_WIDTH/2-MeasureTetrisText(w,12.0f)/2, SCREEN_HEIGHT/2-80, 12.0f, musicPulse*0.2f, false, (w=="PLAYER 1 WINS!"||w=="PLAYER WINS!")?C_GOLD:C_RED); } else { DrawTetrisText("CRITICAL FAILURE", SCREEN_WIDTH/2-MeasureTetrisText("CRITICAL FAILURE",12.0f)/2, SCREEN_HEIGHT/2-80, 12.0f, 0, false, C_RED); } DrawTetrisText("TAP HARD DROP TO REBOOT", SCREEN_WIDTH/2-MeasureTetrisText("TAP HARD DROP TO REBOOT",4.0f)/2, SCREEN_HEIGHT/2+40, 4.0f, 0, false, WHITE); DrawTouchGamepad(gameBtns); }
            if(showExitPrompt) { DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,ColorAlpha(BLACK,0.95f)); DrawTetrisText("ABORT SIMULATION?", SCREEN_WIDTH/2-MeasureTetrisText("ABORT SIMULATION?",10.0f)/2, SCREEN_HEIGHT/2-60, 10.0f, 0, false, C_ORANGE); DrawTouchGamepad(sysBtns); }
            DrawTetrisText("(C) BETTARELLO CODE.", 60.0f, SCREEN_HEIGHT-40, 2.5f, 0.0f, false, ColorAlpha(GRAY,0.7f));
        }
    }

    void Restart() {
        for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) board[i][j]=0;
        score=0; level=1; continues=3; bombs=2; stars=0; currentGridWidth=isExpansiveMode?10:14; 
        if(isHardcoreMode) { level=20; hardcoreJunkTimer=12.0f; } if(isTimeAttackMode) timeAttackTimer=180.0f; 
        if(isDuelMode) { currentGridWidth=10; for(int i=0; i<BOARD_HEIGHT; i++) for(int j=0; j<MAX_BOARD_WIDTH; j++) aiBoard[i][j]=0; aiScore=0; aiDead=false; aiBombs=2; aiComboCount=0; aiComboTimer=0.0f; aiTimerMensagem=0.0f; aiMensagemEspecial=""; aiPieceTrails.clear(); aiNextPiece.clear(); p2FallTimer=0.0f; p2MoveLeftTimer=0.0f; p2MoveRightTimer=0.0f; p2HoldPiece.clear(); p2CanHold=true; SpawnAIPiece(); }
        nextIsBrilliant=false; currentIsBrilliant=false; linesClearedTotal=0; comboCount=0; comboTimer=0.0f; isContinuing=false; continueTimer=0.0f; holdPiece.clear(); canHold=true; pieceTrails.clear(); pieceSpawnAnimTimer=0.0f; aiPieceSpawnAnimTimer=0.0f;
        manualZoomOffset=0.0f; manualCamAngleX=0.0f; manualCamAngleY=0.0f; manualCamPan.x=0.0f; manualCamPan.y=0.0f; manualCamPan.z=0.0f;
        gameOver=false; isPaused=false; timerMensagem=0; mensagemEspecial=""; gridExpansionTimer=0.0f; currentGridElevation=2.0f; moveLeftTimer=0.0f; moveRightTimer=0.0f; nukeSpinAngle=0.0f; hitStopTimer=0.0f; damageVignette=0.0f; goldTint=0.0f;
        bossActive=false; bossEntryAnim=0.0f; linesUntilBoss=15; bossEncounterCount=0; currentBossAttackDelay=15.0f; bossOrbitAngle=0.0f; bossCinematicSpinTimer=0.0f; bossCinematicCooldown=15.0f;
        particles.clear(); floatingTexts.clear(); SpawnPiece(); camera.position=defaultCamPos; camera.target=defaultCamTarget; ShuffleMusic(); 
    }
    bool ShouldExit() { return confirmExit; }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE); 
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TeTRABeTTA - GOLD EDITION MOBILE"); 
    SetExitKey(KEY_NULL); ToggleFullscreen(); InitAudioDevice(); SetTargetFPS(60); HideCursor(); 
    JogoTetris3D game; while(!WindowShouldClose() && !game.ShouldExit()) { game.Update(GetFrameTime()); BeginDrawing(); game.Draw(); EndDrawing(); }
    CloseAudioDevice(); CloseWindow(); return 0;
}