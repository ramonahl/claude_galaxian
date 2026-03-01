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
static constexpr float EBULLET_SPEED = 240.f;

// Particles
static constexpr float PARTICLE_LIFE = 0.4f;
static constexpr int   PARTICLE_COUNT= 8;

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
struct Particle {
    float x, y, vx, vy;
    float life;        // remaining life 0..PARTICLE_LIFE
    float maxLife;
    float size;
    bool  active = false;
};

static std::vector<Particle> gParticles;

void spawnExplosion(float cx, float cy) {
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        float angle = (float)i / PARTICLE_COUNT * 2.f * PI;
        float speed = (float)GetRandomValue(60, 160);
        Particle p;
        p.x    = cx;  p.y = cy;
        p.vx   = cosf(angle) * speed;
        p.vy   = sinf(angle) * speed;
        p.life = PARTICLE_LIFE;
        p.maxLife = PARTICLE_LIFE;
        p.size = (float)GetRandomValue(3, 5);
        p.active = true;
        gParticles.push_back(p);
    }
}

void updateParticles(float dt) {
    for (auto& p : gParticles) {
        if (!p.active) continue;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        if (p.life <= 0.f) p.active = false;
    }
    gParticles.erase(std::remove_if(gParticles.begin(), gParticles.end(),
        [](const Particle& p){ return !p.active; }), gParticles.end());
}

void drawParticles() {
    for (const auto& p : gParticles) {
        if (!p.active) continue;
        float t = p.life / p.maxLife;
        unsigned char alpha = (unsigned char)(t * 255);
        float sz = p.size * t;
        DrawCircle((int)p.x, (int)p.y, sz, {255, 160, 0, alpha});
    }
}

// ─────────────────────────────────────────────────────────────
//  IMAGE SPRITES
// ─────────────────────────────────────────────────────────────
static constexpr float PLAYER_DRAW_SIZE = 48.f;
static constexpr float ENEMY_DRAW_SIZE  = 38.f;
static constexpr float LIFE_ICON_SIZE   = 16.f;

struct SpriteAssets {
    Texture2D player = {};
    Texture2D playerLife = {};
    Texture2D enemy1 = {};
    Texture2D enemy2 = {};
    Texture2D enemy3 = {};
    bool loaded = false;

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
        enemy1 = loadTrimmedTexture("sprites_new/enemy1.png", (int)ENEMY_DRAW_SIZE);
        enemy2 = loadTrimmedTexture("sprites_new/enemy2.png", (int)ENEMY_DRAW_SIZE);
        enemy3 = loadTrimmedTexture("sprites_new/enemy3.png", (int)ENEMY_DRAW_SIZE);
        loaded = player.id != 0 && playerLife.id != 0 &&
                 enemy1.id != 0 && enemy2.id != 0 && enemy3.id != 0;
    }

    void unload() {
        if (player.id != 0) UnloadTexture(player);
        if (playerLife.id != 0) UnloadTexture(playerLife);
        if (enemy1.id != 0) UnloadTexture(enemy1);
        if (enemy2.id != 0) UnloadTexture(enemy2);
        if (enemy3.id != 0) UnloadTexture(enemy3);
        loaded = false;
    }
};

static SpriteAssets gSprites;

static void drawTextureCentered(const Texture2D& tex, float cx, float cy, float size, float rotationDeg = 0.f, bool pixelSnap = true) {
    if (tex.id == 0) return;
    Rectangle src = {0.f, 0.f, (float)tex.width, (float)tex.height};
    float drawX = pixelSnap ? std::roundf(cx) : cx;
    float drawY = pixelSnap ? std::roundf(cy) : cy;
    Rectangle dst = {drawX, drawY, size, size};
    Vector2 origin = {size * 0.5f, size * 0.5f};
    DrawTexturePro(tex, src, dst, origin, rotationDeg, WHITE);
}

void drawPlayerShip(float cx, float cy, float size = PLAYER_DRAW_SIZE) {
    drawTextureCentered(gSprites.player, cx, cy, size);
}

static float enemyBaseRotation(EnemyType type) {
    switch (type) {
        case EnemyType::FLAGSHIP:
        case EnemyType::ESCORT:
            return 0.f;
        case EnemyType::ZAKO_BLUE:
        case EnemyType::ZAKO_BLUE2:
        case EnemyType::ZAKO_GREEN:
            return 180.f;
    }
    return 0.f;
}

void drawEnemy(EnemyType type, float cx, float cy, float rotationDeg = 0.f) {
    switch (type) {
        case EnemyType::FLAGSHIP:
        case EnemyType::ESCORT:
            drawTextureCentered(gSprites.enemy1, cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
        case EnemyType::ZAKO_BLUE:
        case EnemyType::ZAKO_BLUE2:
            drawTextureCentered(gSprites.enemy2, cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
        case EnemyType::ZAKO_GREEN:
            drawTextureCentered(gSprites.enemy3, cx, cy, ENEMY_DRAW_SIZE, rotationDeg, false);
            break;
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
    int    lives = 3;
    int    shotLevel = 1;      // 1=single, 2=double, 3=triple
    float  shotCooldown = 0.22f;
    float  shotTimer    = 0.f;
    bool   invincible  = false;
    float  invTimer    = 0.f;
    bool   alive       = true;

    Rectangle hitbox() const { return { x-5, y-16, 10, 32 }; }
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
    float  diveInterval= 3.f;   // seconds between dive groups

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
            boss.vx = 120.f + (round / 3) * 8.f;
            boss.size = 96.f;
            boss.shotInterval = std::max(0.45f, 1.15f - (round / 3) * 0.05f);
            boss.shotTimer = 0.5f;
            boss.maxHp = 10 + (round / 3) * 2;
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
            // Random 1-2 Zakos
            std::shuffle(candidates.begin(), candidates.end(),
                std::default_random_engine(GetRandomValue(0,99999)));
            int cnt = GetRandomValue(1, 2);
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

        // Bullets setup
        if (e.type == EnemyType::FLAGSHIP) {
            e.bulletsLeft  = 3;
            e.shootInterval = 0.4f;
        } else if (e.type == EnemyType::ESCORT) {
            e.bulletsLeft  = 2;
            e.shootInterval = 0.5f;
        } else {
            e.bulletsLeft  = GetRandomValue(1, 2);
            e.shootInterval = 0.6f;
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
        if (GetRandomValue(0, 99) > 22) return; // ~23% drop chance
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
        switch (type) {
            case PowerUpType::FIRE_RATE:
                player.shotCooldown = std::max(0.08f, player.shotCooldown - 0.03f);
                break;
            case PowerUpType::DOUBLE_SHOT:
                player.shotLevel = std::max(player.shotLevel, 2);
                break;
            case PowerUpType::TRIPLE_SHOT:
                player.shotLevel = 3;
                break;
        }
    }

    // ── update ────────────────────────────────────────────────
    void update(float dt) {
        stars.update(dt);
        updateParticles(dt);

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
            diveInterval = std::max(1.5f, 3.f - (round-1)*0.2f);
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

        // Player shoot (hold to fire)
        if (IsKeyDown(KEY_SPACE) && player.shotTimer <= 0.f) {
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
                    spawnExplosion(boss.x, boss.y);
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
                    spawnExplosion(e.x, e.y);
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
            for (auto& b : eBullets) {
                if (!b.active) continue;
                if (CheckCollisionRecs(b.rect(), player.hitbox())) {
                    b.active = false;
                    killPlayer();
                    return;
                }
            }

            // Collision: diving enemy body vs player
            for (auto& e : enemies) {
                if (!e.alive || e.state != EnemyState::DIVING) continue;
                if (CheckCollisionRecs(e.hitbox(), player.hitbox())) {
                    e.alive = false;
                    spawnExplosion(e.x, e.y);
                    killPlayer();
                    return;
                }
            }

            if (boss.active && CheckCollisionRecs(boss.hitbox(), player.hitbox())) {
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
        const float emitter[3] = {-0.32f, 0.f, 0.32f};
        for (float rel : emitter) {
            Bullet b;
            b.active = true;
            b.enemy = true;
            b.x = boss.x + rel * boss.size;
            b.y = boss.y + boss.size * 0.2f;

            float dx = player.x - b.x;
            float dy = std::max(24.f, player.y - b.y);
            float dist = std::sqrt(dx * dx + dy * dy);
            float spd = EBULLET_SPEED * 1.05f;
            b.vx = (dx / dist) * spd * 0.75f;
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
                b.vy     = EBULLET_SPEED;
                // Slight aim toward player X
                float dx = player.x - e.x;
                float dist = std::abs(dx) + 200.f;
                b.vx = (dx / dist) * EBULLET_SPEED * 0.3f;
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
        spawnExplosion(player.x, player.y);
        player.lives--;
        player.alive = false;
        player.shotLevel = 1;
        player.shotCooldown = 0.22f;
        player.shotTimer = 0.f;
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
        DrawText(TextFormat("SHOT x%d", player.shotLevel), 10, 34, 14, {120, 255, 120, 255});

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
            drawTextureCentered(
                (boss.type == EnemyType::FLAGSHIP) ? gSprites.enemy1 :
                (boss.type == EnemyType::ZAKO_BLUE ? gSprites.enemy2 : gSprites.enemy3),
                boss.x, boss.y, boss.size);

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
            drawEnemy(e.type, e.x, e.y, rot);
        }
    }

    void drawBullets() {
        // Player bullets
        for (const auto& b : pBullets) {
            if (!b.active) continue;
            DrawRectangle(
                (int)(b.x - BULLET_W/2),
                (int)(b.y - BULLET_H/2),
                (int)BULLET_W, (int)BULLET_H,
                {255, 255, 100, 255});
        }
        // Enemy bullets
        for (const auto& b : eBullets) {
            if (!b.active) continue;
            DrawRectangle(
                (int)(b.x - EBULLET_W/2),
                (int)(b.y - EBULLET_H/2),
                (int)EBULLET_W, (int)EBULLET_H,
                {255, 60, 60, 255});
        }
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
            drawPlayerShip(player.x, player.y);

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
        int tw = MeasureText("GALAXIAN", 48);
        DrawText("GALAXIAN", SW/2 - tw/2, 40, 48, {255, 220, 50, 255});

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
