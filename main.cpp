// ============================================================
//  GALAXIAN CLONE  –  C++ / Raylib 5.5
//  Single-file implementation following the full specification
// ============================================================
#include "raylib.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>

// ─────────────────────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────────────────────
static constexpr int   SW            = 480;
static constexpr int   SH            = 720;
static constexpr int   WINDOW_SCALE  = 2;
static constexpr int   FPS_TARGET    = 60;

// Player
static constexpr float PLAYER_MAX_SPEED = 320.f;
static constexpr float PLAYER_ACCEL     = 2400.f;
static constexpr float PLAYER_DECEL     = 5200.f;
static constexpr float PLAYER_Y      = SH - 80.f;
static constexpr float BULLET_SPEED  = 520.f;
static constexpr float BULLET_W      = 3.f;
static constexpr float BULLET_H      = 12.f;

// Enemies
static constexpr int   COLS          = 10;
static constexpr int   ROWS          = 4;
static constexpr float CELL_W        = 44.f;
static constexpr float CELL_H        = 50.f;
static constexpr float FORM_START_X  = (SW - COLS * CELL_W) / 2.f;
static constexpr float FORM_START_Y  = 80.f;
static constexpr float FORM_BOB_AMP  = 7.f;
static constexpr float FORM_BOB_FREQ = 2.2f;
static constexpr float FORM_ROW_AMP  = 1.0f;
static constexpr float FORM_ROW_PHASE= 0.55f;
static constexpr float FORM_COL_AMP  = 3.2f;
static constexpr float FORM_COL_PHASE= 0.60f;

// Enemy bullet
static constexpr float EBULLET_W     = 4.f;
static constexpr float EBULLET_H     = 10.f;
static constexpr float EBULLET_SPEED_BASE = 260.f;

// Particles
static constexpr float PARTICLE_LIFE = 0.55f;
static constexpr int   PARTICLE_COUNT= 18;

// ─────────────────────────────────────────────────────────────
//  ENUMS & TYPES
// ─────────────────────────────────────────────────────────────
enum class GameState { ATTRACT, PLAYING, PLAYER_DEAD, GAME_OVER, STAGE_CLEAR };

enum class EnemyType  { FLAGSHIP, ESCORT, ZAKO_BLUE, ZAKO_BLUE2, ZAKO_GREEN };
enum class EnemyState { IN_FORMATION, DIVING, RETURNING };
enum class PowerUpType { FIRE_RATE, DOUBLE_SHOT, TRIPLE_SHOT };

// ─────────────────────────────────────────────────────────────
//  STAR FIELD
// ─────────────────────────────────────────────────────────────
struct Star {
    float x, y, speed;
    float size;
    unsigned char brightness;
};

struct StarField {
    static constexpr int COUNT = 80;
    Star stars[COUNT];

    void init() {
        for (int i = 0; i < COUNT; ++i) {
            stars[i].x = (float)(GetRandomValue(0, SW));
            stars[i].y = (float)(GetRandomValue(0, SH));
            int layer = i % 3;
            if (layer == 0)      { stars[i].speed = 20.f;  stars[i].size = 1.f; stars[i].brightness = 120; }
            else if (layer == 1) { stars[i].speed = 50.f;  stars[i].size = 1.f; stars[i].brightness = 180; }
            else                 { stars[i].speed = 100.f; stars[i].size = 2.f; stars[i].brightness = 255; }
        }
    }

    void update(float dt) {
        for (auto& s : stars) {
            s.y += s.speed * dt;
            if (s.y > SH) { s.y = 0.f; s.x = (float)GetRandomValue(0, SW); }
        }
    }

    void draw() const {
        for (const auto& s : stars) {
            unsigned char b = s.brightness;
            DrawRectangle((int)s.x, (int)s.y, (int)s.size, (int)s.size, {b, b, b, 255});
        }
    }
};

// ─────────────────────────────────────────────────────────────
//  BULLETS
// ─────────────────────────────────────────────────────────────
struct Bullet {
    float x, y;
    float vx, vy;
    bool  active = false;
    bool  enemy  = false;   // true = enemy bullet

    Rectangle rect() const {
        float w = enemy ? EBULLET_W : BULLET_W;
        float h = enemy ? EBULLET_H : BULLET_H;
        return { x - w/2, y - h/2, w, h };
    }
};

struct PowerUp {
    PowerUpType type;
    float x, y;
    float vy = 100.f;
    bool  active = true;

    Rectangle rect() const { return {x - 8.f, y - 8.f, 16.f, 16.f}; }
};

// ─────────────────────────────────────────────────────────────
//  PARTICLES
// ─────────────────────────────────────────────────────────────
enum class ParticleType { DOT, SPARK };

struct Particle {
    float x, y, vx, vy;
    float life, maxLife;
    float size;
    Color color;
    ParticleType type = ParticleType::DOT;
    bool  active = false;
};

struct Flash {
    float x, y;
    float life, maxLife;
    float radius;
    bool  active = false;
};

struct Debris {
    float x, y, vx, vy;
    float rot, rotSpeed;   // degrees
    float life, maxLife;
    float w, h;
    Color color;
    bool  active = false;
};

static std::vector<Particle> gParticles;
static std::vector<Flash>    gFlashes;
static std::vector<Debris>   gDebris;
static float                 gShake = 0.f;

void spawnExplosion(float cx, float cy, bool big = false, EnemyType etype = EnemyType::ZAKO_BLUE, bool isPlayer = false) {
    // Screen shake
    gShake = std::max(gShake, big ? 7.f : 4.f);

    // Debris fragments (rotando, se desvanecen lento)
    int dcount = big ? 5 : 2;
    // Paleta de debris según tipo de enemigo
    Color debrisColors[4];
    if (isPlayer) {
        // Blanco / plateado (jugador)
        debrisColors[0] = {255, 255, 255, 255};
        debrisColors[1] = {200, 220, 255, 255};
        debrisColors[2] = {160, 200, 255, 255};
        debrisColors[3] = {255, 240, 180, 255};
    } else if (etype == EnemyType::FLAGSHIP || etype == EnemyType::ESCORT || etype == EnemyType::ZAKO_BLUE2) {
        // Rojo / naranja (enemy1)
        debrisColors[0] = {255,  60,  30, 255};
        debrisColors[1] = {255, 140,  20, 255};
        debrisColors[2] = {220,  30,  30, 255};
        debrisColors[3] = {255, 200,  80, 255};
    } else if (etype == EnemyType::ZAKO_BLUE) {
        // Verde (enemy2)
        debrisColors[0] = { 60, 220,  80, 255};
        debrisColors[1] = {120, 255, 100, 255};
        debrisColors[2] = { 30, 180,  60, 255};
        debrisColors[3] = {200, 255, 150, 255};
    } else {
        // Lila / violeta (enemy3 ZAKO_GREEN)
        debrisColors[0] = {180,  80, 255, 255};
        debrisColors[1] = {220, 140, 255, 255};
        debrisColors[2] = {130,  50, 200, 255};
        debrisColors[3] = {255, 180, 255, 255};
    }
    for (int i = 0; i < dcount; ++i) {
        float angle = GetRandomValue(0, 359) * DEG2RAD;
        float spd   = (float)GetRandomValue(70, big ? 200 : 140);
        Debris d;
        d.x = cx + GetRandomValue(-5, 5);
        d.y = cy + GetRandomValue(-5, 5);
        d.vx = cosf(angle) * spd;
        d.vy = sinf(angle) * spd;
        d.rot      = (float)GetRandomValue(0, 359);
        d.rotSpeed = (float)GetRandomValue(-480, 480);
        d.life = d.maxLife = 0.45f + GetRandomValue(0, 35) * 0.01f;
        d.w = big ? (float)GetRandomValue(6, 11) : (float)GetRandomValue(3, 7);
        d.h = d.w * 0.45f;
        d.color = debrisColors[GetRandomValue(0, 3)];
        d.active = true;
        gDebris.push_back(d);
    }

    // Initial bright flash + expanding ring
    Flash fl;
    fl.x = cx; fl.y = cy;
    fl.life = fl.maxLife = 0.18f;
    fl.radius = big ? 32.f : 22.f;
    fl.active = true;
    gFlashes.push_back(fl);

    // Debris particles
    int count = big ? PARTICLE_COUNT + 8 : PARTICLE_COUNT;
    for (int i = 0; i < count; ++i) {
        float angle = (float)i / count * 2.f * PI + GetRandomValue(-8, 8) * 0.06f;
        float speed = (float)GetRandomValue(55, big ? 210 : 170);

        Particle p;
        p.x = cx; p.y = cy;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life = p.maxLife = PARTICLE_LIFE * (0.75f + GetRandomValue(0, 50) * 0.005f);
        p.size = (float)GetRandomValue(2, big ? 6 : 5);
        p.active = true;
        p.type = (GetRandomValue(0, 2) == 0) ? ParticleType::SPARK : ParticleType::DOT;

        int roll = GetRandomValue(0, 3);
        if (isPlayer) {
            // Blanco / plateado / azul hielo
            if      (roll == 0) p.color = {255, 255, 255, 255};
            else if (roll == 1) p.color = {200, 230, 255, 255};
            else if (roll == 2) p.color = {150, 200, 255, 255};
            else                p.color = {255, 240, 160, 255};
        } else if (etype == EnemyType::ZAKO_BLUE) {
            // Verde
            if      (roll == 0) p.color = {200, 255, 200, 255};
            else if (roll == 1) p.color = { 80, 255,  80, 255};
            else if (roll == 2) p.color = { 30, 200,  60, 255};
            else                p.color = {160, 255, 100, 255};
        } else if (etype == EnemyType::ZAKO_GREEN) {
            // Lila
            if      (roll == 0) p.color = {240, 200, 255, 255};
            else if (roll == 1) p.color = {200,  80, 255, 255};
            else if (roll == 2) p.color = {160,  50, 220, 255};
            else                p.color = {255, 160, 255, 255};
        } else {
            // Rojo / naranja (enemy1, flagship, escort)
            if      (roll == 0) p.color = {255, 255, 220, 255};
            else if (roll == 1) p.color = {255, 200,  30, 255};
            else if (roll == 2) p.color = {255, 100,   0, 255};
            else                p.color = {255,  40,   0, 255};
        }

        gParticles.push_back(p);
    }
}

void updateParticles(float dt) {
    gShake = std::max(0.f, gShake - dt * 35.f);

    for (auto& p : gParticles) {
        if (!p.active) continue;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        if (p.life <= 0.f) p.active = false;
    }
    gParticles.erase(std::remove_if(gParticles.begin(), gParticles.end(),
        [](const Particle& p){ return !p.active; }), gParticles.end());

    for (auto& f : gFlashes) {
        if (!f.active) continue;
        f.life -= dt;
        if (f.life <= 0.f) f.active = false;
    }
    gFlashes.erase(std::remove_if(gFlashes.begin(), gFlashes.end(),
        [](const Flash& f){ return !f.active; }), gFlashes.end());

    for (auto& d : gDebris) {
        if (!d.active) continue;
        d.x   += d.vx * dt;
        d.y   += d.vy * dt;
        d.vy  += 90.f * dt;   // gravedad suave
        d.rot += d.rotSpeed * dt;
        d.life -= dt;
        if (d.life <= 0.f) d.active = false;
    }
    gDebris.erase(std::remove_if(gDebris.begin(), gDebris.end(),
        [](const Debris& d){ return !d.active; }), gDebris.end());
}

void drawParticles() {
    BeginBlendMode(BLEND_ADDITIVE);

    // Flash + shockwave ring
    for (const auto& f : gFlashes) {
        if (!f.active) continue;
        float t = f.life / f.maxLife;
        // Core flash (shrinks slightly)
        float r = f.radius * (0.9f + t * 0.4f);
        unsigned char fa = (unsigned char)(t * 230);
        DrawCircleGradient((int)f.x, (int)f.y, r,
            {255, 255, 255, fa}, {255, 180, 20, 0});
        // Expanding ring (grows outward as flash fades)
        float ringR = f.radius * (1.0f + (1.0f - t) * 2.2f);
        unsigned char ra = (unsigned char)(t * 160);
        DrawRing({f.x, f.y}, ringR - 1.5f, ringR + 1.5f, 0, 360, 24,
            {255, 200, 60, ra});
    }

    // Debris
    for (const auto& p : gParticles) {
        if (!p.active) continue;
        float t = p.life / p.maxLife;
        unsigned char alpha = (unsigned char)(t * 255);
        Color c = {p.color.r, p.color.g, p.color.b, alpha};

        if (p.type == ParticleType::SPARK) {
            float len = p.size * 5.f * t;
            float mag = sqrtf(p.vx * p.vx + p.vy * p.vy);
            if (mag > 0.f) {
                float nx = p.vx / mag, ny = p.vy / mag;
                Vector2 tail = {p.x - nx * len, p.y - ny * len};
                DrawLineEx(tail, {p.x, p.y}, 1.5f, c);
            }
        } else {
            float sz = p.size * (0.4f + 0.6f * t);
            DrawCircleGradient((int)p.x, (int)p.y, sz, c,
                {p.color.r, p.color.g, p.color.b, 0});
        }
    }

    EndBlendMode();

    // Debris fragments – blend normal, sólidos
    for (const auto& d : gDebris) {
        if (!d.active) continue;
        float t = d.life / d.maxLife;
        unsigned char alpha = (unsigned char)(t * 230);
        Color c = {d.color.r, d.color.g, d.color.b, alpha};
        Rectangle rect = {d.x, d.y, d.w, d.h};
        Vector2 origin = {d.w * 0.5f, d.h * 0.5f};
        DrawRectanglePro(rect, origin, d.rot, c);
    }
}

// ─────────────────────────────────────────────────────────────
//  IMAGE SPRITES
// ─────────────────────────────────────────────────────────────
static constexpr float PLAYER_DRAW_SIZE = 64.f;
static constexpr float ENEMY_DRAW_SIZE  = 38.f;
static constexpr float LIFE_ICON_SIZE   = 16.f;

struct SpriteAssets {
    Texture2D player = {};
    Texture2D playerLife = {};
    Texture2D playerBody = {};
    Texture2D playerThrusters = {};
    Texture2D enemy1Anim[3] = {};   // 3 frames de animación del cangrejo
    Texture2D enemy2Anim[6] = {};   // 6 frames ping-pong del Alien1
    Texture2D enemy3Anim[3] = {};   // 3 frames ping-pong de la Nave1
    bool loaded = false;

    // Carga sin recortar canvas (para sprites compuestos que deben alinearse)
    // Elimina píxeles marcadores rojo/verde y escala a outputSize x outputSize
    static Texture2D loadNoTrimTexture(const char* path, int outputSize) {
        Image img = LoadImage(path);
        if (img.data == nullptr) {
            TraceLog(LOG_ERROR, "No se pudo cargar sprite: %s", path);
            return {};
        }
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color* pixels = (Color*)img.data;
        int total = img.width * img.height;
        for (int i = 0; i < total; ++i) {
            Color& c = pixels[i];
            if (c.r == 255 && c.g == 0 && c.b == 0 && c.a > 128) c = BLANK;
            if (c.r == 0   && c.g == 255 && c.b == 0 && c.a > 128) c = BLANK;
        }
        if (outputSize > 0) {
            float fit = std::min((float)outputSize / img.width, (float)outputSize / img.height);
            int scaledW = std::max(1, (int)std::round((float)img.width * fit));
            int scaledH = std::max(1, (int)std::round((float)img.height * fit));
            if (img.width != scaledW || img.height != scaledH)
                ImageResize(&img, scaledW, scaledH);
            Image canvas = GenImageColor(outputSize, outputSize, BLANK);
            Rectangle srcRect = {0.f, 0.f, (float)img.width, (float)img.height};
            Rectangle dstRect = {
                (float)((outputSize - img.width) / 2),
                (float)((outputSize - img.height) / 2),
                (float)img.width, (float)img.height
            };
            ImageDraw(&canvas, img, srcRect, dstRect, WHITE);
            UnloadImage(img);
            img = canvas;
        }
        Texture2D tex = LoadTextureFromImage(img);
        if (tex.id != 0) SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        UnloadImage(img);
        return tex;
    }

    static Texture2D loadTrimmedTexture(const char* path, int outputSize) {
        Image img = LoadImage(path);
        if (img.data == nullptr) {
            TraceLog(LOG_ERROR, "No se pudo cargar sprite: %s", path);
            return {};
        }
        ImageAlphaCrop(&img, 0.01f);
        if (outputSize > 0) {
            float fit = std::min((float)outputSize / img.width, (float)outputSize / img.height);
            int scaledW = std::max(1, (int)std::round((float)img.width * fit));
            int scaledH = std::max(1, (int)std::round((float)img.height * fit));
            if (img.width != scaledW || img.height != scaledH) {
                ImageResize(&img, scaledW, scaledH);
            }

            Image canvas = GenImageColor(outputSize, outputSize, BLANK);
            Rectangle srcRect = {0.f, 0.f, (float)img.width, (float)img.height};
            Rectangle dstRect = {
                (float)((outputSize - img.width) / 2),
                (float)((outputSize - img.height) / 2),
                (float)img.width,
                (float)img.height
            };
            ImageDraw(&canvas, img, srcRect, dstRect, WHITE);
            UnloadImage(img);
            img = canvas;
        }
        Texture2D tex = LoadTextureFromImage(img);
        if (tex.id != 0) SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        UnloadImage(img);
        return tex;
    }

    void load() {
        player = loadTrimmedTexture("sprites_new/player1.png", (int)PLAYER_DRAW_SIZE);
        playerLife = loadTrimmedTexture("sprites_new/player1.png", (int)LIFE_ICON_SIZE);
        playerBody       = loadNoTrimTexture("sprites_new/Player1SP.png",          (int)PLAYER_DRAW_SIZE);
        playerThrusters  = loadNoTrimTexture("sprites_new/Player1Propulsores.png", (int)PLAYER_DRAW_SIZE);
        enemy1Anim[0] = loadTrimmedTexture("sprites_new/enemy1_f01.png", (int)ENEMY_DRAW_SIZE);
        enemy1Anim[1] = loadTrimmedTexture("sprites_new/enemy1_f02.png", (int)ENEMY_DRAW_SIZE);
        enemy1Anim[2] = loadTrimmedTexture("sprites_new/enemy1_f03.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[0] = loadTrimmedTexture("sprites_new/enemy2_f01.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[1] = loadTrimmedTexture("sprites_new/enemy2_f02.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[2] = loadTrimmedTexture("sprites_new/enemy2_f03.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[3] = loadTrimmedTexture("sprites_new/enemy2_f04.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[4] = loadTrimmedTexture("sprites_new/enemy2_f05.png", (int)ENEMY_DRAW_SIZE);
        enemy2Anim[5] = loadTrimmedTexture("sprites_new/enemy2_f06.png", (int)ENEMY_DRAW_SIZE);
        enemy3Anim[0] = loadTrimmedTexture("sprites_new/enemy3_f01.png", (int)ENEMY_DRAW_SIZE);
        enemy3Anim[1] = loadTrimmedTexture("sprites_new/enemy3_f02.png", (int)ENEMY_DRAW_SIZE);
        enemy3Anim[2] = loadTrimmedTexture("sprites_new/enemy3_f03.png", (int)ENEMY_DRAW_SIZE);
        loaded = player.id != 0 && playerLife.id != 0 &&
                 enemy1Anim[0].id != 0 && enemy2Anim[0].id != 0 && enemy3Anim[0].id != 0;
    }

    void unload() {
        if (player.id != 0)           UnloadTexture(player);
        if (playerLife.id != 0)       UnloadTexture(playerLife);
        if (playerBody.id != 0)       UnloadTexture(playerBody);
        if (playerThrusters.id != 0)  UnloadTexture(playerThrusters);
        for (auto& t : enemy1Anim)    if (t.id != 0) UnloadTexture(t);
        for (auto& t : enemy2Anim)    if (t.id != 0) UnloadTexture(t);
        for (auto& t : enemy3Anim)    if (t.id != 0) UnloadTexture(t);
        loaded = false;
    }
};

static SpriteAssets gSprites;

// Animación enemy1: 3 frames a ~7 fps
static float gEnemyAnimTimer = 0.f;
static int   gEnemyAnimFrame = 0;
static constexpr float ENEMY_ANIM_INTERVAL = 1.f / 7.f;

// Animación enemy2: ping-pong 6 frames, velocidad y fase distintas por enemigo
static float gEnemy2Time = 0.f;
// Secuencia ping-pong: 0-1-2-3-4-5-4-3-2-1 (10 pasos)
static constexpr int ENEMY2_PINGPONG[10] = {0,1,2,3,4,5,4,3,2,1};
// Velocidades base por "slot" (0-9): 3.0..5.5 fps, distribuidas de forma irregular
static constexpr float ENEMY2_SPEEDS[10] = {4.0f,3.2f,5.0f,3.7f,4.8f,3.5f,5.3f,4.2f,3.0f,4.6f};

// Animación enemy3: ping-pong 3 frames (0-1-2-1 = 4 pasos)
static float gEnemy3Time = 0.f;
static constexpr int   ENEMY3_PINGPONG[4]  = {0,1,2,1};
static constexpr float ENEMY3_SPEEDS[10]   = {3.5f,4.2f,3.0f,4.8f,3.8f,5.0f,3.2f,4.5f,3.6f,4.1f};

static void drawTextureCentered(const Texture2D& tex, float cx, float cy, float size, float rotationDeg = 0.f, bool pixelSnap = true) {
    if (tex.id == 0) return;
    Rectangle src = {0.f, 0.f, (float)tex.width, (float)tex.height};
    float drawX = pixelSnap ? std::roundf(cx) : cx;
    float drawY = pixelSnap ? std::roundf(cy) : cy;
    Rectangle dst = {drawX, drawY, size, size};
    Vector2 origin = {size * 0.5f, size * 0.5f};
    DrawTexturePro(tex, src, dst, origin, rotationDeg, WHITE);
}

void drawPlayerShip(float cx, float cy, float vx = 0.f, float thrusterTime = 0.f, float size = PLAYER_DRAW_SIZE) {
    // ── Propulsores ──────────────────────────────────────────
    if (gSprites.playerThrusters.id != 0) {
        float pulse = (sinf(thrusterTime * 5.f) + 1.f) * 0.5f; // 0..1 a ~0.8Hz suave

        float leftAlpha, rightAlpha;
        const float threshold = 40.f;

        if (vx < -threshold) {
            // Movimiento izquierda → propulsor DERECHO activo
            leftAlpha  = 0.22f + pulse * 0.16f;   // 0.22..0.38 visible pero apagado
            rightAlpha = 0.75f + pulse * 0.20f;   // 0.75..0.95 potente
        } else if (vx > threshold) {
            // Movimiento derecha → propulsor IZQUIERDO activo
            leftAlpha  = 0.75f + pulse * 0.20f;
            rightAlpha = 0.22f + pulse * 0.16f;
        } else {
            // Reposo: ambos pulsan igual, rango estable
            float a = 0.35f + pulse * 0.30f;       // 0.35..0.65
            leftAlpha  = a;
            rightAlpha = a;
        }

        int ix    = (int)roundf(cx);
        int iy    = (int)roundf(cy);
        int half  = (int)(size * 0.5f);
        int isize = (int)size;

        Rectangle src    = {0.f, 0.f, (float)gSprites.playerThrusters.width, (float)gSprites.playerThrusters.height};
        Rectangle dst    = {cx, cy, size, size};
        Vector2   origin = {size * 0.5f, size * 0.5f};

        // Mitad izquierda
        BeginScissorMode(ix - half, iy - half, half, isize);
        DrawTexturePro(gSprites.playerThrusters, src, dst, origin, 0.f,
            {255, 255, 255, (unsigned char)(leftAlpha * 255.f)});
        EndScissorMode();

        // Mitad derecha
        BeginScissorMode(ix, iy - half, half, isize);
        DrawTexturePro(gSprites.playerThrusters, src, dst, origin, 0.f,
            {255, 255, 255, (unsigned char)(rightAlpha * 255.f)});
        EndScissorMode();

        // Brillo aditivo en el propulsor activo al moverse
        if (fabsf(vx) > threshold) {
            BeginBlendMode(BLEND_ADDITIVE);
            float glow = pulse * 0.35f;
            Color gc = {255, 255, 255, (unsigned char)(glow * 255.f)};
            if (vx > threshold) {
                BeginScissorMode(ix - half, iy - half, half, isize);
                DrawTexturePro(gSprites.playerThrusters, src, dst, origin, 0.f, gc);
                EndScissorMode();
            } else {
                BeginScissorMode(ix, iy - half, half, isize);
                DrawTexturePro(gSprites.playerThrusters, src, dst, origin, 0.f, gc);
                EndScissorMode();
            }
            EndBlendMode();
        }
    }

    // ── Cuerpo de la nave ────────────────────────────────────
    Texture2D& bodyTex = (gSprites.playerBody.id != 0) ? gSprites.playerBody : gSprites.player;
    drawTextureCentered(bodyTex, cx, cy, size);
}

static float enemyBaseRotation(EnemyType type) {
    switch (type) {
        case EnemyType::FLAGSHIP:
        case EnemyType::ESCORT:
            return 0.f;
        case EnemyType::ZAKO_BLUE:
        case EnemyType::ZAKO_BLUE2:
            return 0.f;
        case EnemyType::ZAKO_GREEN:
            return 0.f;
    }
    return 0.f;
}

void drawEnemy(EnemyType type, float cx, float cy, float rotationDeg = 0.f, int animOffset = 0) {
    switch (type) {
        case EnemyType::FLAGSHIP:
        case EnemyType::ESCORT:
            drawTextureCentered(gSprites.enemy1Anim[(gEnemyAnimFrame + animOffset) % 3], cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
        case EnemyType::ZAKO_BLUE:
        case EnemyType::ZAKO_BLUE2: {
            int slot  = animOffset % 10;
            float spd = ENEMY2_SPEEDS[slot];
            // Desfase de fase: cada slot empieza en un punto diferente del ciclo
            float phase = slot * 1.3f;
            int step  = (int)((gEnemy2Time * spd + phase)) % 10;
            drawTextureCentered(gSprites.enemy2Anim[ENEMY2_PINGPONG[step]], cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
        }
        case EnemyType::ZAKO_GREEN: {
            int slot  = animOffset % 10;
            float spd = ENEMY3_SPEEDS[slot];
            float phase = slot * 1.1f;
            int step  = (int)(gEnemy3Time * spd + phase) % 4;
            drawTextureCentered(gSprites.enemy3Anim[ENEMY3_PINGPONG[step]], cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  ENEMY
// ─────────────────────────────────────────────────────────────
struct Enemy {
    EnemyType  type;
    int        row, col;          // grid position
    float      formX, formY;      // target formation position
    float      x, y;              // current world position
    EnemyState state = EnemyState::IN_FORMATION;
    bool       alive = true;

    // Dive path (Bezier control points)
    float      t   = 0.f;         // path parameter 0..1
    float      diveSpeed = 200.f;
    Vector2    p0, p1, p2, p3;    // cubic Bezier
    float      diveTargetX = SW * 0.5f;

    // Shooting timer
    float      shootTimer  = 0.f;
    float      shootInterval = 0.f;
    int        bulletsLeft  = 0;

    // Return-to-formation arc
    float      retT  = 0.f;
    Vector2    retP0, retP1, retP2, retP3;

    // Hitbox (~80% del tamaño visual)
    Rectangle hitbox() const {
        float hw = 14.f, hh = 12.f;
        return { x - hw, y - hh, hw*2, hh*2 };
    }
};

struct Boss {
    bool      active = false;
    EnemyType type = EnemyType::FLAGSHIP;
    float     x = SW * 0.5f;
    float     y = 130.f;
    float     vx = 120.f;
    float     size = 88.f;
    int       hp = 0;
    int       maxHp = 0;
    float     shotTimer = 0.f;
    float     shotInterval = 1.2f;

    Rectangle hitbox() const {
        float hw = size * 0.36f;
        float hh = size * 0.32f;
        return { x - hw, y - hh, hw * 2.f, hh * 2.f };
    }
};

// Cubic Bezier evaluation
static Vector2 bezier(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t) {
    float u = 1.f - t;
    return {
        u*u*u*p0.x + 3*u*u*t*p1.x + 3*u*t*t*p2.x + t*t*t*p3.x,
        u*u*u*p0.y + 3*u*u*t*p1.y + 3*u*t*t*p2.y + t*t*t*p3.y
    };
}

// ─────────────────────────────────────────────────────────────
//  PLAYER
// ─────────────────────────────────────────────────────────────
struct Player {
    float  x = SW / 2.f;
    float  y = PLAYER_Y;
    float  vx = 0.f;
    float  thrusterTime = 0.f;
    int    lives = 3;
    int    shotLevel = 1;      // 1=single, 2=double, 3=triple
    float  shotCooldown = 0.22f;
    float  shotTimer    = 0.f;
    bool   invincible  = false;
    float  invTimer    = 0.f;
    bool   alive       = true;

    // Power-up temporal
    bool        hasPowerUp       = false;
    PowerUpType activePowerUp    = PowerUpType::FIRE_RATE;
    float       powerUpTimer     = 0.f;
    float       powerUpMaxDuration = 8.f;  // se recalcula al coger power-up

    // Hitbox: cuerpo central ancho + franja de alas fina
    // El sprite mide 64px; las alas ocupan la banda y-8..y+4 aprox
    static std::array<Rectangle, 2> hitboxes(float x, float y) {
        return {{
            { x - 9.f,  y - 18.f, 18.f, 26.f },   // cuerpo central
            { x - 26.f, y -  4.f, 52.f,  8.f },   // barra de alas
        }};
    }
    Rectangle hitbox() const { return { x-9, y-18, 18, 26 }; }  // cuerpo (colisión simple)
};

// ─────────────────────────────────────────────────────────────
//  GAME  (all state in one struct for clarity)
// ─────────────────────────────────────────────────────────────
struct Game {
    GameState  state      = GameState::ATTRACT;
    StarField  stars;
    Player     player;

    std::vector<Enemy>  enemies;
    std::vector<Bullet> pBullets;   // player bullets
    std::vector<Bullet> eBullets;   // enemy bullets
    std::vector<PowerUp> powerUps;
    Boss               boss;

    int    score       = 0;
    int    highScore   = 0;
    int    round       = 1;

    // Formation motion
    float  formVX      = 30.f;   // current lateral speed (px/s)
    float  formOffX    = 0.f;    // lateral offset
    float  formOffY    = 0.f;    // sine vertical offset
    float  formSineT   = 0.f;


    // Dive timer
    float  diveTimer   = 0.f;
    float  diveInterval= 2.2f;  // seconds between dive groups

    // Death / clear timers
    float  stateTimer  = 0.f;

    // Attract blink
    float  blinkTimer  = 0.f;
    bool   blinkOn     = true;

    // Stage clear flash
    float  flashTimer  = 0.f;
    bool   flashActive = false;

    // ── helpers ───────────────────────────────────────────────
    void init() {
        stars.init();
        score   = 0;
        round   = 1;
        formVX  = 30.f;
        player.x = SW / 2.f;
        player.vx = 0.f;
        player.lives = 3;
        player.shotLevel = 1;
        player.shotCooldown = 0.22f;
        player.shotTimer = 0.f;
        player.alive = true;
        player.invincible = false;
        pBullets.clear();
        eBullets.clear();
        powerUps.clear();
        gParticles.clear();
        gFlashes.clear();
        gDebris.clear();
        gShake = 0.f;
        buildFormation();
    }

    void buildFormation() {
        enemies.clear();
        pBullets.clear();
        eBullets.clear();
        powerUps.clear();
        boss = {};
        formOffX = 0.f;
        formOffY = 0.f;
        formSineT = 0.f;
        diveTimer = diveInterval;

        if (round % 3 == 0) {
            int cycle = ((round / 3) - 1) % 3;
            boss.active = true;
            boss.x = SW * 0.5f;
            boss.y = 130.f;
            int bossLevel = round / 3;  // 1, 2, 3...
            boss.vx = 110.f + bossLevel * 22.f;   // más rápido cada boss
            boss.size = 96.f;
            boss.shotInterval = std::max(0.28f, 1.2f - bossLevel * 0.10f);  // dispara más rápido
            boss.shotTimer = 0.4f;
            boss.maxHp = 10 + bossLevel * 5;       // mucho más vida
            boss.hp = boss.maxHp;
            switch (cycle) {
                case 0: boss.type = EnemyType::FLAGSHIP; break;  // enemy1
                case 1: boss.type = EnemyType::ZAKO_BLUE; break; // enemy2
                default: boss.type = EnemyType::ZAKO_GREEN; break; // enemy3
            }
            return;
        }

        static const EnemyType rowType[ROWS] = {
            EnemyType::ZAKO_GREEN, // enemy3 (arriba)
            EnemyType::ZAKO_GREEN, // enemy3 (arriba)
            EnemyType::ZAKO_BLUE,  // enemy2 (medio)
            EnemyType::ESCORT      // enemy1 (abajo)
        };

        static const int rowCount[ROWS] = {2, 6, 8, 10};

        for (int r = 0; r < ROWS; ++r) {
            int count = rowCount[r];
            int startCol = (COLS - count) / 2;
            for (int i = 0; i < count; ++i) {
                int c = startCol + i;
                Enemy e;
                e.type  = rowType[r];
                e.row   = r;
                e.col   = c;
                e.formX = FORM_START_X + c * CELL_W + CELL_W/2.f;
                e.formY = FORM_START_Y + r * CELL_H + CELL_H/2.f;
                e.x     = e.formX;
                e.y     = e.formY;
                e.alive = true;
                e.state = EnemyState::IN_FORMATION;
                enemies.push_back(e);
            }
        }
    }

    int aliveCount() const {
        int n = 0;
        for (const auto& e : enemies) if (e.alive) ++n;
        return n;
    }

    // Formation base position (accounts for lateral offset)
    float formationX(int col) const {
        return FORM_START_X + col * CELL_W + CELL_W/2.f + formOffX;
    }
    float formationY(int row, int col) const {
        // Tie wave phase to both time and lateral offset to avoid phase jumps at edge bounces.
        float t = formSineT * FORM_BOB_FREQ + formOffX * 0.08f;
        float rowBob = sinf(t + row * FORM_ROW_PHASE) * FORM_ROW_AMP;
        float colWave = sinf(t + col * FORM_COL_PHASE + row * 0.25f) * FORM_COL_AMP;
        return FORM_START_Y + row * CELL_H + CELL_H/2.f + formOffY + rowBob + colWave;
    }

    // Speed factor based on enemies killed
    float speedFactor() const {
        int total = 26;
        int killed = total - aliveCount();
        return 1.f + killed * 0.008f + (round - 1) * 0.1f;
    }

    // ── start a dive group ────────────────────────────────────
    void startDive() {
        // Find living in-formation enemies
        std::vector<Enemy*> candidates;
        for (auto& e : enemies)
            if (e.alive && e.state == EnemyState::IN_FORMATION)
                candidates.push_back(&e);

        if (candidates.empty()) return;

        // Try to launch Flagship + escorts
        Enemy* flagship = nullptr;
        for (auto* e : candidates)
            if (e->type == EnemyType::FLAGSHIP) { flagship = e; break; }

        std::vector<Enemy*> group;

        if (flagship && GetRandomValue(0,1) == 0) {
            group.push_back(flagship);
            // Find escort neighbours (row 1, same or adjacent cols)
            for (auto* e : candidates) {
                if (e->type == EnemyType::ESCORT &&
                    std::abs(e->col - flagship->col) <= 2 &&
                    (int)group.size() < 3)
                    group.push_back(e);
            }
        } else {
            // 1-3 Zakos según ronda
            std::shuffle(candidates.begin(), candidates.end(),
                std::default_random_engine(GetRandomValue(0,99999)));
            int maxCnt = (round >= 4) ? 3 : (round >= 2 ? 2 : 1);
            int cnt = GetRandomValue(1, maxCnt);
            for (int i = 0; i < cnt && i < (int)candidates.size(); ++i)
                group.push_back(candidates[i]);
        }

        for (auto* e : group) {
            launchDive(*e);
        }
    }

    void launchDive(Enemy& e) {
        e.state = EnemyState::DIVING;
        e.t     = 0.f;
        e.diveSpeed = (e.type == EnemyType::FLAGSHIP ? 190.f : 210.f) * speedFactor();

        // Bezier: start at current pos, arc up then down toward player
        float startX = e.x, startY = e.y;
        float side   = (startX < SW/2.f) ? 1.f : -1.f;

        float aimError = 0.f;
        switch (e.type) {
            case EnemyType::FLAGSHIP: aimError = (float)GetRandomValue(-36, 36); break;
            case EnemyType::ESCORT:   aimError = (float)GetRandomValue(-52, 52); break;
            default:                  aimError = (float)GetRandomValue(-70, 70); break;
        }
        e.diveTargetX = std::clamp(player.x + aimError, 24.f, SW - 24.f);

        e.p0 = {startX, startY};
        e.p1 = {startX + side*120.f, startY - 80.f};   // up and outward
        e.p2 = {e.diveTargetX + side*70.f, PLAYER_Y - 200.f};
        e.p3 = {e.diveTargetX, (float)SH + 60.f};      // fixed target (no homing)

        // Bullets setup — escalan con la ronda
        float roundMult = std::max(0.55f, 1.f - (round - 1) * 0.08f); // intervalo se reduce
        if (e.type == EnemyType::FLAGSHIP) {
            e.bulletsLeft  = 2 + std::min(round - 1, 2);  // 2-4
            e.shootInterval = 0.4f * roundMult;
        } else if (e.type == EnemyType::ESCORT) {
            e.bulletsLeft  = 1 + std::min(round / 2, 2);  // 1-3
            e.shootInterval = 0.5f * roundMult;
        } else {
            e.bulletsLeft  = GetRandomValue(1, 1 + std::min(round / 2, 2)); // 1-3
            e.shootInterval = 0.6f * roundMult;
        }
        e.shootTimer = e.shootInterval * 0.5f;
    }

    void returnToFormation(Enemy& e) {
        e.state = EnemyState::RETURNING;
        e.retT  = 0.f;
        // Fly back up from bottom, looping around edge
        float side = (e.x < SW/2.f) ? -1.f : 1.f;
        e.retP0 = {e.x,    (float)SH + 40.f};
        e.retP1 = {e.x + side*160.f, SH/2.f};
        e.retP2 = {formationX(e.col), FORM_START_Y - 80.f};
        e.retP3 = {formationX(e.col), formationY(e.row, e.col)};
    }

    void firePlayerShot(float offsetX) {
        Bullet b;
        b.active = true;
        b.enemy  = false;
        b.x = player.x + offsetX;
        b.y = player.y - 14.f;
        b.vx = 0.f;
        b.vy = -BULLET_SPEED;
        pBullets.push_back(b);
    }

    void spawnPowerUp(float x, float y) {
        if (GetRandomValue(0, 99) > 7) return; // ~8% drop chance
        PowerUp p;
        p.x = x;
        p.y = y;
        int roll = GetRandomValue(0, 99);
        if (roll < 50) p.type = PowerUpType::FIRE_RATE;
        else if (roll < 80) p.type = PowerUpType::DOUBLE_SHOT;
        else p.type = PowerUpType::TRIPLE_SHOT;
        powerUps.push_back(p);
    }

    void applyPowerUp(PowerUpType type) {
        player.hasPowerUp       = true;
        player.activePowerUp    = type;
        // Duración base 8s, se reduce 0.4s por ronda (mínimo 3s)
        float dur = std::max(3.f, 8.f - (round - 1) * 0.4f);
        player.powerUpMaxDuration = dur;
        player.powerUpTimer       = dur;
        switch (type) {
            case PowerUpType::FIRE_RATE:
                player.shotCooldown = 0.10f;
                break;
            case PowerUpType::DOUBLE_SHOT:
                player.shotLevel = 2;
                break;
            case PowerUpType::TRIPLE_SHOT:
                player.shotLevel = 3;
                break;
        }
    }

    void expirePowerUp() {
        player.hasPowerUp   = false;
        player.shotLevel    = 1;
        player.shotCooldown = 0.22f;
    }

    // ── update ────────────────────────────────────────────────
    void update(float dt) {
        stars.update(dt);
        updateParticles(dt);

        // Animación de enemigos
        gEnemyAnimTimer += dt;
        if (gEnemyAnimTimer >= ENEMY_ANIM_INTERVAL) {
            gEnemyAnimTimer -= ENEMY_ANIM_INTERVAL;
            gEnemyAnimFrame = (gEnemyAnimFrame + 1) % 3;
        }
        gEnemy2Time += dt;
        gEnemy3Time += dt;

        switch (state) {
            case GameState::ATTRACT:   updateAttract(dt);  break;
            case GameState::PLAYING:   updatePlaying(dt);  break;
            case GameState::PLAYER_DEAD: updateDead(dt);   break;
            case GameState::GAME_OVER: updateGameOver(dt); break;
            case GameState::STAGE_CLEAR: updateClear(dt);  break;
        }
    }

    void updateAttract(float dt) {
        updateFormationMotion(dt);
        blinkTimer += dt;
        if (blinkTimer >= 0.5f) { blinkTimer = 0.f; blinkOn = !blinkOn; }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            init();
            state = GameState::PLAYING;
        }
    }

    void updateDead(float dt) {
        stateTimer -= dt;
        stars.update(dt);
        if (stateTimer <= 0.f) {
            if (player.lives <= 0) {
                state = GameState::GAME_OVER;
                stateTimer = 3.f;
            } else {
                player.x = SW / 2.f;
                player.vx = 0.f;
                player.alive = true;
                pBullets.clear();
                player.invincible = true;
                player.invTimer   = 2.f;
                state = GameState::PLAYING;
            }
        }
    }

    void updateGameOver(float dt) {
        stateTimer -= dt;
        if (stateTimer <= 0.f) {
            highScore = std::max(highScore, score);
            state = GameState::ATTRACT;
        }
    }

    void updateClear(float dt) {
        stateTimer -= dt;
        flashTimer += dt;
        if (stateTimer <= 0.f) {
            round++;
            formVX = 30.f + (round-1) * 5.f;
            diveInterval = std::max(1.0f, 2.2f - (round-1)*0.15f);
            buildFormation();
            state = GameState::PLAYING;
        }
    }

    void updatePlaying(float dt) {
        // Invincibility
        if (player.invincible) {
            player.invTimer -= dt;
            if (player.invTimer <= 0.f) player.invincible = false;
        }
        if (player.shotTimer > 0.f) player.shotTimer -= dt;

        // Power-up timer
        if (player.hasPowerUp) {
            player.powerUpTimer -= dt;
            if (player.powerUpTimer <= 0.f) expirePowerUp();
        }

        player.thrusterTime += dt;

        // Player movement with acceleration/deceleration ramps
        float moveInput = 0.f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) moveInput -= 1.f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) moveInput += 1.f;

        if (moveInput != 0.f) {
            player.vx += moveInput * PLAYER_ACCEL * dt;
            player.vx = std::clamp(player.vx, -PLAYER_MAX_SPEED, PLAYER_MAX_SPEED);
        } else {
            float decel = PLAYER_DECEL * dt;
            if (std::fabs(player.vx) <= decel) player.vx = 0.f;
            else player.vx -= std::copysign(decel, player.vx);
        }

        player.x += player.vx * dt;
        if (player.x < 16.f) {
            player.x = 16.f;
            if (player.vx < 0.f) player.vx = 0.f;
        }
        if (player.x > SW - 16.f) {
            player.x = SW - 16.f;
            if (player.vx > 0.f) player.vx = 0.f;
        }

        // Player shoot (flanco positivo: solo dispara al pulsar, no al mantener)
        if (IsKeyPressed(KEY_SPACE) && player.shotTimer <= 0.f) {
            if (player.shotLevel <= 1) {
                firePlayerShot(0.f);
            } else if (player.shotLevel == 2) {
                firePlayerShot(-7.f);
                firePlayerShot(7.f);
            } else {
                firePlayerShot(-10.f);
                firePlayerShot(0.f);
                firePlayerShot(10.f);
            }
            player.shotTimer = player.shotCooldown;
        }

        // Move player bullets
        for (auto& b : pBullets) {
            if (!b.active) continue;
            b.y += b.vy * dt;
            if (b.y < -BULLET_H - 8.f) b.active = false;
        }

        // Formation motion
        updateFormationMotion(dt);

        // Enemies in formation: sync position
        for (auto& e : enemies) {
            if (!e.alive) continue;
            if (e.state == EnemyState::IN_FORMATION) {
                e.x = formationX(e.col);
                e.y = formationY(e.row, e.col);
            }
        }

        // Dive timer
        diveTimer -= dt;
        if (diveTimer <= 0.f && !boss.active) {
            startDive();
            diveTimer = (float)GetRandomValue(200, 400) / 100.f / speedFactor();
        }

        // Update diving / returning enemies
        for (auto& e : enemies) {
            if (!e.alive) continue;
            if (e.state == EnemyState::DIVING) {
                updateDiving(e, dt);
            } else if (e.state == EnemyState::RETURNING) {
                updateReturning(e, dt);
            }
        }

        // Enemy bullets
        for (auto& b : eBullets) {
            if (!b.active) continue;
            b.y += b.vy * dt;
            b.x += b.vx * dt;
            if (b.y > SH + 20.f) b.active = false;
        }

        if (boss.active) {
            updateBoss(dt);
        }

        for (auto& p : powerUps) {
            if (!p.active) continue;
            p.y += p.vy * dt;
            if (p.y > SH + 16.f) p.active = false;
            if (CheckCollisionRecs(p.rect(), player.hitbox())) {
                p.active = false;
                applyPowerUp(p.type);
            }
        }

        // Collision: player bullets vs enemies
        for (auto& pb : pBullets) {
            if (!pb.active) continue;
            Rectangle br = pb.rect();

            if (boss.active && CheckCollisionRecs(br, boss.hitbox())) {
                pb.active = false;
                boss.hp--;
                spawnExplosion(pb.x, pb.y);
                if (boss.hp <= 0) {
                    boss.active = false;
                    score += 1000 + round * 80;
                    highScore = std::max(highScore, score);
                    spawnExplosion(boss.x, boss.y, true);
                    spawnPowerUp(boss.x, boss.y);
                }
                continue;
            }

            for (auto& e : enemies) {
                if (!e.alive) continue;
                if (CheckCollisionRecs(br, e.hitbox())) {
                    e.alive = false;
                    pb.active = false;
                    int pts = pointsForEnemy(e.type, e.state == EnemyState::DIVING);
                    score += pts;
                    highScore = std::max(highScore, score);
                    spawnExplosion(e.x, e.y, false, e.type);
                    spawnPowerUp(e.x, e.y);
                    break;
                }
            }
        }

        pBullets.erase(std::remove_if(pBullets.begin(), pBullets.end(),
            [](const Bullet& b){ return !b.active; }), pBullets.end());
        eBullets.erase(std::remove_if(eBullets.begin(), eBullets.end(),
            [](const Bullet& b){ return !b.active; }), eBullets.end());
        powerUps.erase(std::remove_if(powerUps.begin(), powerUps.end(),
            [](const PowerUp& p){ return !p.active; }), powerUps.end());

        // Collision: enemy bullets vs player
        if (!player.invincible) {
            auto playerBoxes = Player::hitboxes(player.x, player.y);
            for (auto& b : eBullets) {
                if (!b.active) continue;
                Rectangle br = b.rect();
                if (CheckCollisionRecs(br, playerBoxes[0]) ||
                    CheckCollisionRecs(br, playerBoxes[1])) {
                    b.active = false;
                    killPlayer();
                    return;
                }
            }

            // Collision: diving/returning enemy body vs player
            for (auto& e : enemies) {
                if (!e.alive || (e.state != EnemyState::DIVING && e.state != EnemyState::RETURNING)) continue;
                Rectangle er = e.hitbox();
                if (CheckCollisionRecs(er, playerBoxes[0]) ||
                    CheckCollisionRecs(er, playerBoxes[1])) {
                    e.alive = false;
                    spawnExplosion(e.x, e.y, false, e.type);
                    killPlayer();
                    return;
                }
            }

            if (boss.active && (CheckCollisionRecs(boss.hitbox(), playerBoxes[0]) ||
                                CheckCollisionRecs(boss.hitbox(), playerBoxes[1]))) {
                killPlayer();
                return;
            }
        }

        // Check stage clear
        if (aliveCount() == 0 && !boss.active) {
            state      = GameState::STAGE_CLEAR;
            stateTimer = 2.f;
            flashTimer = 0.f;
        }
    }

    void updateBoss(float dt) {
        boss.x += boss.vx * dt;
        float half = boss.size * 0.5f;
        if (boss.x < half + 18.f) {
            boss.x = half + 18.f;
            boss.vx = std::abs(boss.vx);
        }
        if (boss.x > SW - half - 18.f) {
            boss.x = SW - half - 18.f;
            boss.vx = -std::abs(boss.vx);
        }

        boss.shotTimer -= dt;
        if (boss.shotTimer <= 0.f) {
            fireBossVolley();
            boss.shotTimer = boss.shotInterval;
        }
    }

    void fireBossVolley() {
        int bossLevel = round / 3;
        // 3 balas en boss 1, 4 en boss 2, 5 en boss 3+
        int count = std::min(3 + (bossLevel - 1), 5);
        // Posiciones relativas distribuidas uniformemente
        float spread = 0.65f;
        for (int i = 0; i < count; ++i) {
            float rel = (count == 1) ? 0.f
                : -spread + (2.f * spread / (count - 1)) * i;
            Bullet b;
            b.active = true;
            b.enemy = true;
            b.x = boss.x + rel * boss.size;
            b.y = boss.y + boss.size * 0.2f;

            float dx = player.x - b.x;
            float dy = std::max(24.f, player.y - b.y);
            float dist = std::sqrt(dx * dx + dy * dy);
            float spd = (EBULLET_SPEED_BASE + round * 14.f) * 1.1f;
            // A mayor bossLevel, más apuntadas al jugador
            float aimFactor = std::min(0.55f + bossLevel * 0.1f, 0.9f);
            b.vx = (dx / dist) * spd * aimFactor;
            b.vy = (dy / dist) * spd;
            eBullets.push_back(b);
        }
    }

    void updateFormationMotion(float dt) {
        float sf = (state == GameState::PLAYING) ? speedFactor() : 1.f;
        float dir = (formVX >= 0.f) ? 1.f : -1.f;
        float speed = std::abs(formVX) * sf;

        // Edges (soft turn near borders to avoid harsh direction changes)
        float maxX = (float)(SW - COLS * CELL_W) / 2.f - 10.f;
        float distToEdge = (dir > 0.f) ? (maxX - formOffX) : (formOffX + maxX);
        const float softZone = 26.f;
        float edgeFactor = 1.f;
        if (distToEdge < softZone) {
            edgeFactor = std::clamp(distToEdge / softZone, 0.35f, 1.f);
        }

        formOffX += dir * speed * edgeFactor * dt;

        if (formOffX > maxX) {
            float overshoot = formOffX - maxX;
            formOffX = maxX - overshoot;
            formVX = -std::abs(formVX);
        }
        if (formOffX < -maxX) {
            float overshoot = -maxX - formOffX;
            formOffX = -maxX + overshoot;
            formVX = std::abs(formVX);
        }

        // Vertical bobbing (Galaxian-like subtle up/down movement)
        formSineT += dt;
        formOffY  = sinf(formSineT * FORM_BOB_FREQ) * FORM_BOB_AMP;
    }

    void updateDiving(Enemy& e, float dt) {
        // Advance t based on speed / curve length approximation
        float arcLen = 600.f; // rough estimate
        e.t += (e.diveSpeed / arcLen) * dt;

        if (e.t >= 1.f) {
            e.t = 1.f;
            // Exited bottom – start return
            e.x = bezier(e.p0, e.p1, e.p2, e.p3, 1.f).x;
            e.y = (float)SH + 60.f;
            returnToFormation(e);
            return;
        }

        Vector2 pos = bezier(e.p0, e.p1, e.p2, e.p3, e.t);
        e.x = pos.x;
        e.y = pos.y;

        // Shoot
        if (e.bulletsLeft > 0) {
            e.shootTimer -= dt;
            if (e.shootTimer <= 0.f) {
                e.shootTimer = e.shootInterval;
                e.bulletsLeft--;
                Bullet b;
                b.active = true;
                b.enemy  = true;
                b.x      = e.x;
                b.y      = e.y + 8.f;
                b.vx     = 0.f;
                float eBulletSpd = EBULLET_SPEED_BASE + round * 12.f;
                b.vy     = eBulletSpd;
                // Aim mejora progresivamente con las rondas (0.25 ronda 1 → 0.55 ronda 7+)
                float aimFactor = std::min(0.25f + (round - 1) * 0.05f, 0.55f);
                float dx = player.x - e.x;
                float dist = std::abs(dx) + 200.f;
                b.vx = (dx / dist) * eBulletSpd * aimFactor;
                eBullets.push_back(b);
            }
        }
    }

    void updateReturning(Enemy& e, float dt) {
        float arcLen = 700.f;
        e.retT += (e.diveSpeed * 0.8f / arcLen) * dt;

        // Keep destination updated with current formation position
        e.retP3 = {formationX(e.col), formationY(e.row, e.col)};

        if (e.retT >= 1.f) {
            e.retT  = 1.f;
            e.state = EnemyState::IN_FORMATION;
            e.x     = e.retP3.x;
            e.y     = e.retP3.y;
            return;
        }

        Vector2 pos = bezier(e.retP0, e.retP1, e.retP2, e.retP3, e.retT);
        e.x = pos.x;
        e.y = pos.y;
    }

    void killPlayer() {
        if (player.invincible) return;
        spawnExplosion(player.x, player.y, true, EnemyType::ZAKO_BLUE, true);
        player.lives--;
        player.alive = false;
        player.shotLevel = 1;
        player.shotCooldown = 0.22f;
        player.shotTimer = 0.f;
        player.hasPowerUp = false;
        player.powerUpTimer = 0.f;
        pBullets.clear();
        powerUps.clear();
        for (auto& b : eBullets) b.active = false;
        // Return all diving enemies to formation
        for (auto& e : enemies)
            if (e.alive && (e.state == EnemyState::DIVING || e.state == EnemyState::RETURNING))
                e.state = EnemyState::IN_FORMATION;
        state      = GameState::PLAYER_DEAD;
        stateTimer = 2.f;
    }

    int pointsForEnemy(EnemyType t, bool diving) {
        int base = 0;
        switch (t) {
            case EnemyType::FLAGSHIP:   base = diving ? 400 : 150; break;
            case EnemyType::ESCORT:     base = diving ? 160 :  40; break;
            case EnemyType::ZAKO_BLUE:  base = diving ? 100 :  30; break;
            case EnemyType::ZAKO_BLUE2: base = diving ?  80 :  20; break;
            case EnemyType::ZAKO_GREEN: base = diving ?  60 :  10; break;
        }
        return base;
    }

    // ── draw ──────────────────────────────────────────────────
    void draw() {
        ClearBackground(BLACK);
        stars.draw();

        switch (state) {
            case GameState::ATTRACT:       drawAttract();     break;
            case GameState::PLAYING:       drawPlaying();     break;
            case GameState::PLAYER_DEAD:   drawDead();        break;
            case GameState::GAME_OVER:     drawGameOver();    break;
            case GameState::STAGE_CLEAR:   drawClear();       break;
        }
        drawParticles();
    }

    void drawHUD() {
        // Score top left
        DrawText(TextFormat("%06d", score), 10, 10, 20, WHITE);

        // High score centered
        DrawText("HIGH SCORE", SW/2 - 50, 8, 14, WHITE);
        DrawText(TextFormat("%06d", highScore), SW/2 - 30, 22, 14, WHITE);
        if (player.hasPowerUp) {
            // Barra de tiempo restante
            float ratio = (player.powerUpMaxDuration > 0.f)
                ? player.powerUpTimer / player.powerUpMaxDuration : 0.f;
            Color barCol = (ratio > 0.35f) ? Color{120, 255, 120, 255} : Color{255, 160, 40, 255};
            DrawText(TextFormat("SHOT x%d", player.shotLevel), 10, 34, 14, barCol);
            DrawRectangle(10, 52, 60, 5, {60, 60, 60, 200});
            DrawRectangle(10, 52, (int)(60.f * ratio), 5, barCol);
        } else {
            DrawText("SHOT x1", 10, 34, 14, {160, 160, 160, 200});
        }

        // Lives (bottom left as ship icons)
        for (int i = 0; i < player.lives; ++i) {
            drawTextureCentered(gSprites.playerLife, 20.f + i * 28.f, SH - 18.f, LIFE_ICON_SIZE);
        }

        // Round flags (bottom right)
        for (int i = 0; i < round && i < 8; ++i) {
            Color fc = {(unsigned char)(100 + i*20), 80, 200, 255};
            DrawRectangle(SW - 20 - i*16, SH - 26, 12, 16, fc);
        }
    }

    void drawEnemies() {
        if (boss.active) {
            {
                Texture2D& bossTex = (boss.type == EnemyType::FLAGSHIP)
                    ? gSprites.enemy1Anim[(gEnemyAnimFrame) % 3]
                    : (boss.type == EnemyType::ZAKO_BLUE
                        ? gSprites.enemy2Anim[ENEMY2_PINGPONG[(int)(gEnemy2Time * 4.0f) % 10]]
                        : gSprites.enemy3Anim[ENEMY3_PINGPONG[(int)(gEnemy3Time * 4.0f) % 4]]);
                drawTextureCentered(bossTex, boss.x, boss.y, boss.size);
            }

            float bw = 180.f;
            float bh = 8.f;
            float bx = SW * 0.5f - bw * 0.5f;
            float by = 52.f;
            float pct = (boss.maxHp > 0) ? (float)boss.hp / (float)boss.maxHp : 0.f;
            DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, {70, 70, 70, 220});
            DrawRectangle((int)bx, (int)by, (int)(bw * std::clamp(pct, 0.f, 1.f)), (int)bh, {255, 90, 90, 255});
            DrawText("BOSS", (int)bx, (int)by - 14, 12, {255, 180, 180, 255});
        }

        for (const auto& e : enemies) {
            if (!e.alive) continue;
            float rot = enemyBaseRotation(e.type);
            if (e.state == EnemyState::DIVING) {
                float dx = player.x - e.x;
                float dy = player.y - e.y;
                // 0 deg points "down" in this sprite set; add base per enemy art orientation.
                float aimDeg = std::atan2(dy, dx) * RAD2DEG - 90.f;
                rot += aimDeg;
            }
            drawEnemy(e.type, e.x, e.y, rot, e.col);
        }
    }

    void drawBullets() {
        BeginBlendMode(BLEND_ADDITIVE);

        // Player bullets – bright yellow/white core with soft glow
        for (const auto& b : pBullets) {
            if (!b.active) continue;
            // Outer glow
            DrawCircleGradient((int)b.x, (int)(b.y - BULLET_H * 0.3f),
                BULLET_W * 3.f, {255, 255, 180, 70}, {255, 255, 80, 0});
            // Core gradient (bright white tip → yellow base)
            DrawRectangleGradientV(
                (int)(b.x - BULLET_W/2), (int)(b.y - BULLET_H/2),
                (int)BULLET_W, (int)BULLET_H,
                {255, 255, 255, 255}, {255, 210, 30, 200});
        }

        // Enemy bullets – red/orange glow
        for (const auto& b : eBullets) {
            if (!b.active) continue;
            // Outer glow
            DrawCircleGradient((int)b.x, (int)(b.y + EBULLET_H * 0.3f),
                EBULLET_W * 3.f, {255, 60, 0, 80}, {255, 30, 0, 0});
            // Core gradient (orange tip → red base)
            DrawRectangleGradientV(
                (int)(b.x - EBULLET_W/2), (int)(b.y - EBULLET_H/2),
                (int)EBULLET_W, (int)EBULLET_H,
                {255, 180, 40, 200}, {255, 30, 0, 255});
        }

        EndBlendMode();
    }

    void drawPowerUps() {
        for (const auto& p : powerUps) {
            if (!p.active) continue;
            Color c = {120, 220, 255, 255};
            const char* label = "F";
            if (p.type == PowerUpType::DOUBLE_SHOT) { c = {255, 220, 120, 255}; label = "2"; }
            if (p.type == PowerUpType::TRIPLE_SHOT) { c = {255, 140, 120, 255}; label = "3"; }
            DrawCircle((int)p.x, (int)p.y, 8.f, c);
            DrawText(label, (int)p.x - 4, (int)p.y - 6, 12, BLACK);
        }
    }

    void drawPlaying() {
        drawEnemies();
        drawBullets();
        drawPowerUps();

        // Player (blinks when invincible)
        bool showPlayer = player.alive &&
            (!player.invincible || (int)(player.invTimer * 10) % 2 == 0);
        if (showPlayer)
            drawPlayerShip(player.x, player.y, player.vx, player.thrusterTime);

        drawHUD();
    }

    void drawDead() {
        drawEnemies();
        drawHUD();
    }

    void drawGameOver() {
        drawEnemies();
        drawHUD();
        int tw = MeasureText("GAME OVER", 40);
        DrawText("GAME OVER", SW/2 - tw/2, SH/2 - 20, 40, RED);
    }

    void drawAttract() {
        // Big title
        int tw = MeasureText("GALAX IA", 48);
        DrawText("GALAX IA", SW/2 - tw/2, 40, 48, {255, 220, 50, 255});

        drawEnemies();

        // High score
        DrawText("HIGH SCORE", SW/2 - 50, SH/2 - 30, 16, WHITE);
        DrawText(TextFormat("%06d", highScore), SW/2 - 36, SH/2 - 10, 20, WHITE);

        // Blink "INSERT COIN"
        if (blinkOn) {
            int iw = MeasureText("PRESS ENTER TO PLAY", 18);
            DrawText("PRESS ENTER TO PLAY", SW/2 - iw/2, SH*3/4, 18, {200, 200, 200, 255});
        }

        // Controls hint
        DrawText("MOVE: ARROWS / A-D    FIRE: SPACE", 30, SH - 36, 12, {150,150,150,255});
    }

    void drawClear() {
        // Flash effect
        float t = flashTimer;
        if ((int)(t * 8) % 2 == 0) {
            DrawRectangle(0, 0, SW, SH, {255,255,255, 60});
        }
        int tw = MeasureText("STAGE CLEAR!", 36);
        DrawText("STAGE CLEAR!", SW/2 - tw/2, SH/2 - 18, 36, {100, 255, 100, 255});
        drawHUD();
    }
};

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────
int main() {
    srand((unsigned)time(nullptr));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SW, SH, "GALAXIAN");
    SetTargetFPS(FPS_TARGET);
    SetRandomSeed((unsigned)time(nullptr));

    // Auto-scale initial window to fit ~85% of monitor height
    int mon = GetCurrentMonitor();
    int monH = GetMonitorHeight(mon);
    int initScale = std::max(1, (int)std::floor((float)monH * 0.85f / SH));
    int windowedW = SW * initScale;
    int windowedH = SH * initScale;
    SetWindowSize(windowedW, windowedH);

    gSprites.load();
    RenderTexture2D scene = LoadRenderTexture(SW, SH);
    SetTextureFilter(scene.texture, TEXTURE_FILTER_POINT);

    Game game;
    game.stars.init();
    // Build attract-mode formation
    game.buildFormation();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) {
            if (IsWindowFullscreen()) {
                ToggleFullscreen();
                SetWindowSize(windowedW, windowedH);
            } else {
                windowedW = GetScreenWidth();
                windowedH = GetScreenHeight();
                ToggleFullscreen();
            }
        }

        float dt = GetFrameTime();
        game.update(dt);

        BeginTextureMode(scene);
        game.draw();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        int renderW = GetScreenWidth();
        int renderH = GetScreenHeight();
        float baseScale = std::min((float)renderW / SW, (float)renderH / SH);
        float scale = std::max(0.01f, baseScale);
        float drawW = std::round(SW * scale);
        float drawH = std::round(SH * scale);
        float drawX = std::floor(((float)renderW - drawW) * 0.5f);
        float drawY = std::floor(((float)renderH - drawH) * 0.5f);
        if (gShake > 0.5f) {
            int s = (int)(gShake * scale);
            drawX += (float)GetRandomValue(-s, s);
            drawY += (float)GetRandomValue(-s, s);
        }
        Rectangle src = {0.f, 0.f, (float)SW, -(float)SH};
        Rectangle dst = {drawX, drawY, drawW, drawH};
        DrawTexturePro(scene.texture, src, dst, {0.f, 0.f}, 0.f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(scene);
    gSprites.unload();
    CloseWindow();
    return 0;
}
