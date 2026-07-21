/* The street: platforms, props, enemies and everything shooting at you.
   Art: Warped City / Warped City 2 by ansimuz (CC0). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "world.h"
#include "fx.h"

#define Z_PROP_BACK  -8.0f
#define Z_PLAY        0.0f
#define Z_PROP_FRONT  3.0f

/* ---------------- level layout ---------------- */

typedef struct { float x, y, w; } Platform;

/* A street that climbs to the right: ground level, then fire escapes and
   walkway spans you have to work your way up. */
/* A single jump clears 3.8 units, a double jump about twice that, so the
   first step up is always within one jump and the height is earned in stages.
   The street runs unbroken except where a collapsed span is drawn as a hole. */
static const Platform platforms[] = {
    { -20.0f,  0.0f,  88.0f },  /* the street, unbroken to the first hole */
    {  22.0f,  3.2f,   7.0f },  /* fire escape up to the ledges */
    {  33.0f,  6.2f,   7.0f },
    {  45.0f,  9.0f,   8.0f },
    {  58.0f,  6.0f,   7.0f },  /* back down the far side */
    {  70.0f,  3.0f,   7.0f },
    {  96.0f,  0.0f,  48.0f },  /* street resumes past the hole */
    {  76.0f,  4.0f,   6.0f },  /* the crossing over the hole */
    {  86.0f,  6.5f,   6.0f },
    { 112.0f,  3.4f,   8.0f },
    { 124.0f,  6.6f,   8.0f },
    { 136.0f,  9.8f,   9.0f },
    { 160.0f,  0.0f,  60.0f },  /* final stretch */
    { 150.0f,  6.0f,   7.0f },  /* the crossing over the second hole */
};
#define NUM_PLATFORMS ((int)(sizeof platforms / sizeof platforms[0]))

typedef struct {
    int   kind;        /* index into prop_art */
    float x, y, z;
    float scale;
} Prop;

enum { PR_HOTEL, PR_NEON, PR_SUSHI, PR_BIG, PR_LIGHT, PR_BANNER_C,
       PR_BOX1, PR_BOX2, PR_ANTENNA, PR_ARROW, PR_COUNT };

static const struct { const char *file; int frames; float fps; } prop_art[PR_COUNT] = {
    { "props/hotel-sign.png",   1,  0.0f },
    { "props/banner-neon.png",  4,  6.0f },
    { "props/banner-sushi.png", 3,  4.0f },
    { "props/banner-big.png",   4,  5.0f },
    { "props/street-light.png", 4,  8.0f },
    { "props/banner-c.png",     4,  6.0f },
    { "props/control-box-1.png",1,  0.0f },
    { "props/control-box-2.png",1,  0.0f },
    { "props/antenna.png",      1,  0.0f },
    { "props/banner-arrow.png", 1,  0.0f },
};

static const Prop props[] = {
    { PR_LIGHT,     -14.0f,  0.0f, Z_PROP_BACK, 1.0f },
    { PR_NEON,       -6.0f,  6.5f, Z_PROP_BACK, 1.3f },
    { PR_HOTEL,       4.0f,  8.0f, Z_PROP_BACK, 1.2f },
    { PR_BOX1,        9.0f,  0.0f, Z_PROP_FRONT, 1.0f },
    { PR_SUSHI,      14.0f,  5.5f, Z_PROP_BACK, 1.6f },
    { PR_LIGHT,      18.0f,  0.0f, Z_PROP_BACK, 1.0f },
    { PR_BIG,        26.0f,  9.0f, Z_PROP_BACK, 1.2f },
    { PR_ANTENNA,    40.0f, 11.0f, Z_PROP_BACK, 1.0f },
    { PR_BANNER_C,   48.0f,  7.5f, Z_PROP_BACK, 1.1f },
    { PR_LIGHT,      62.0f,  0.0f, Z_PROP_BACK, 1.0f },
    { PR_NEON,       68.0f,  6.0f, Z_PROP_BACK, 1.3f },
    { PR_BOX2,       72.0f,  0.0f, Z_PROP_FRONT, 1.0f },
    { PR_ARROW,      80.0f,  9.0f, Z_PROP_BACK, 1.0f },
    { PR_SUSHI,      92.0f, 12.5f, Z_PROP_BACK, 1.5f },
    { PR_LIGHT,     104.0f,  0.0f, Z_PROP_BACK, 1.0f },
    { PR_HOTEL,     114.0f,  8.5f, Z_PROP_BACK, 1.2f },
    { PR_BANNER_C,  124.0f,  6.0f, Z_PROP_BACK, 1.1f },
    { PR_BOX1,      128.0f,  0.0f, Z_PROP_FRONT, 1.0f },
    { PR_BIG,       140.0f, 11.0f, Z_PROP_BACK, 1.2f },
    { PR_LIGHT,     150.0f,  0.0f, Z_PROP_BACK, 1.0f },
    { PR_NEON,      160.0f,  7.0f, Z_PROP_BACK, 1.3f },
    { PR_ANTENNA,   174.0f, 10.0f, Z_PROP_BACK, 1.0f },
};
#define NUM_PROPS ((int)(sizeof props / sizeof props[0]))

/* ---------------- cover ---------------- */

/* Waist-high street furniture. A bullet crossing one dies there, so what
   decides a firefight is whether your chest is above the lip or below it. */
typedef struct { float x, y, w, h; int art; } Cover;

static const Cover covers[] = {
    {   6.0f, 0.0f, 2.4f, 1.5f, PR_BOX1 },
    {  18.0f, 0.0f, 2.4f, 1.5f, PR_BOX2 },
    {  28.0f, 0.0f, 2.4f, 1.5f, PR_BOX1 },
    {  52.0f, 0.0f, 2.4f, 1.5f, PR_BOX2 },
    {  64.0f, 0.0f, 2.4f, 1.5f, PR_BOX1 },
    {  99.0f, 0.0f, 2.4f, 1.5f, PR_BOX2 },
    { 108.0f, 0.0f, 2.4f, 1.5f, PR_BOX1 },
    { 118.0f, 0.0f, 2.4f, 1.5f, PR_BOX2 },
    { 166.0f, 0.0f, 2.4f, 1.5f, PR_BOX1 },
    { 178.0f, 0.0f, 2.4f, 1.5f, PR_BOX2 },
    /* a couple up on the ledges, so the climb has cover too */
    {  33.0f, 6.2f, 2.2f, 1.4f, PR_BOX1 },
    { 124.0f, 6.6f, 2.2f, 1.4f, PR_BOX2 },
};
#define NUM_COVERS ((int)(sizeof covers / sizeof covers[0]))

int world_behind_cover(float x, float chest_y) {
    for (int i = 0; i < NUM_COVERS; i++) {
        const Cover *c = &covers[i];
        if (x < c->x - 1.6f || x > c->x + c->w + 1.6f) continue;
        if (chest_y < c->y + c->h) return 1;
    }
    return 0;
}

static int bullet_hits_cover(float x, float y) {
    for (int i = 0; i < NUM_COVERS; i++) {
        const Cover *c = &covers[i];
        if (x < c->x || x > c->x + c->w) continue;
        if (y >= c->y && y <= c->y + c->h) return 1;
    }
    return 0;
}

/* ---------------- enemies ---------------- */

enum { EN_DRONE, EN_TURRET, EN_COP };

typedef struct {
    int   kind, alive;
    float x, y;           /* feet / centre */
    float home_x, home_y;
    float vx;
    int   hp;
    float fire_t, anim_t, hurt_t, death_t;
    int   facing;
} Enemy;

/* Every enemy sits on the route: on the street, on a ledge you have to
   cross, or hovering over the gap you are about to jump. */
static const struct { int kind; float x, y; } spawns[] = {
    { EN_COP,     14.0f,  0.0f },
    { EN_TURRET,  33.0f,  6.2f },   /* guarding the climb */
    { EN_DRONE,   45.0f, 13.0f },
    { EN_TURRET,  58.0f,  6.0f },
    { EN_COP,     70.0f,  3.0f },
    { EN_DRONE,   82.0f,  9.0f },   /* over the first hole */
    { EN_COP,    104.0f,  0.0f },
    { EN_TURRET, 124.0f,  6.6f },
    { EN_DRONE,  136.0f, 14.0f },
    { EN_COP,    136.0f,  9.8f },
    { EN_DRONE,  152.0f, 10.0f },   /* over the second hole */
    { EN_COP,    176.0f,  0.0f },
};
#define NUM_ENEMIES ((int)(sizeof spawns / sizeof spawns[0]))
static Enemy enemies[NUM_ENEMIES];

typedef struct { float x, y, vx, vy, life; int from_player; } Bullet;
#define MAX_BULLETS 48
static Bullet bullets[MAX_BULLETS];

typedef struct { float x, y, t; } Burst;
#define MAX_BURSTS 12
static Burst bursts[MAX_BURSTS];
static int next_burst;

/* ---------------- assets ---------------- */

typedef struct { Tex tex; int frames; float fps; } Sheet;
static Sheet sh_prop[PR_COUNT];
static Sheet sh_drone, sh_turret, sh_cop_run, sh_cop_idle;
static Sheet sh_shot, sh_hit, sh_boom;
static Tex tx_slab, tx_span, tx_support;
static float world_t;
static Color world_tint = { 1.0f, 1.0f, 1.0f, 1.0f };

static Sheet load_sheet(const char *root, const char *rel, int frames, float fps) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    Sheet s = { tex_load(path, 0), frames, fps };
    return s;
}

void world_init(const char *root) {
    for (int i = 0; i < PR_COUNT; i++)
        sh_prop[i] = load_sheet(root, prop_art[i].file, prop_art[i].frames,
                                prop_art[i].fps);
    sh_drone    = load_sheet(root, "enemy/drone.png", 4, 12.0f);
    sh_turret   = load_sheet(root, "enemy/turret.png", 6, 10.0f);
    sh_cop_run  = load_sheet(root, "enemy/cop-run.png", 10, 12.0f);
    sh_cop_idle = load_sheet(root, "enemy/cop-idle.png", 3, 6.0f);
    sh_shot     = load_sheet(root, "fx2/shot.png", 3, 14.0f);
    sh_hit      = load_sheet(root, "fx2/shot-hit.png", 3, 18.0f);
    sh_boom     = load_sheet(root, "fx2/explosion.png", 6, 16.0f);

    char path[512];
    snprintf(path, sizeof path, "%s/tiles/slab.png", root);
    tx_slab = tex_load(path, 0);
    snprintf(path, sizeof path, "%s/tiles/span.png", root);
    tx_span = tex_load(path, 1);
    snprintf(path, sizeof path, "%s/tiles/support.png", root);
    tx_support = tex_load(path, 0);

    for (int i = 0; i < NUM_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        e->kind = spawns[i].kind;
        e->x = e->home_x = spawns[i].x;
        e->y = e->home_y = spawns[i].y;
        e->alive = 1;
        e->facing = -1;
        e->hp = (e->kind == EN_TURRET) ? 3 : (e->kind == EN_COP ? 4 : 2);
        e->fire_t = 0.7f + i * 0.23f;
        e->vx = (e->kind == EN_COP) ? 3.4f : 0.0f;
    }
    memset(bullets, 0, sizeof bullets);
    memset(bursts, 0, sizeof bursts);
}

int world_ground_segments(const float **out) {
    static float segs[NUM_PLATFORMS * 3];
    int n = 0;
    for (int i = 0; i < NUM_PLATFORMS; i++) {
        if (platforms[i].y > 0.01f) continue;
        segs[n * 3 + 0] = platforms[i].x;
        segs[n * 3 + 1] = platforms[i].y;
        segs[n * 3 + 2] = platforms[i].w;
        n++;
    }
    *out = segs;
    return n;
}

float world_left_bound(void)  { return platforms[0].x + 1.0f; }
float world_right_bound(void) {
    float right = -1e9f;
    for (int i = 0; i < NUM_PLATFORMS; i++) {
        float e = platforms[i].x + platforms[i].w;
        if (e > right) right = e;
    }
    return right - 1.0f;
}

/* ---------------- collision ---------------- */

int world_land(float x, float y_from, float y_to, float *out_y) {
    int hit = 0;
    float best = -1e9f;
    for (int i = 0; i < NUM_PLATFORMS; i++) {
        const Platform *p = &platforms[i];
        if (x < p->x || x > p->x + p->w) continue;
        /* one-way: only catch the player on the way down through the surface */
        if (y_from >= p->y - 0.01f && y_to <= p->y && p->y > best) {
            best = p->y;
            hit = 1;
        }
    }
    if (hit) *out_y = best;
    return hit;
}

float world_surface_under(float x, float y) {
    float best = -1e9f;
    for (int i = 0; i < NUM_PLATFORMS; i++) {
        const Platform *p = &platforms[i];
        if (x < p->x || x > p->x + p->w) continue;
        if (p->y <= y + 0.2f && p->y > best) best = p->y;
    }
    return best;
}

/* ---------------- bullets ---------------- */

static void spawn_bullet(float x, float y, float vx, float vy, int from_player) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].life > 0.0f) continue;
        bullets[i] = (Bullet){ x, y, vx, vy, 2.2f, from_player };
        return;
    }
}

static void spawn_burst(float x, float y) {
    bursts[next_burst] = (Burst){ x, y, 0.0f };
    next_burst = (next_burst + 1) % MAX_BURSTS;
}

void world_player_shoot(float x, float y, int facing) {
    spawn_bullet(x + facing * 0.8f, y, facing * 34.0f, 0.0f, 1);
}

/* ---------------- update ---------------- */

static void enemy_fire(Enemy *e, float px, float p_chest) {
    float dx = px - e->x, dy = p_chest - (e->y + 0.8f);
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.01f) return;
    float speed = (e->kind == EN_TURRET) ? 20.0f : 17.0f;
    spawn_bullet(e->x, e->y + 0.8f, dx / d * speed, dy / d * speed, 0);
}

void world_update(float dt, float px, float py, int p_facing, float p_chest) {
    (void)p_facing;
    world_t += dt;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (e->death_t > 0.0f) e->death_t += dt;
        if (!e->alive) continue;
        if (e->hurt_t > 0.0f) e->hurt_t -= dt;
        e->anim_t += dt;

        float dx = px - e->x, dy = py - e->y;
        float dist = sqrtf(dx * dx + dy * dy);
        e->facing = dx < 0.0f ? -1 : 1;

        if (e->kind == EN_DRONE) {
            /* hover toward the player but keep its distance and altitude */
            float want_x = px - (dx > 0 ? -5.0f : 5.0f);
            e->x += clampf((want_x - e->x), -1.0f, 1.0f) * 3.2f * dt;
            e->y = e->home_y + sinf(world_t * 1.6f + i) * 0.9f;
        } else if (e->kind == EN_COP) {
            /* patrol its ledge, turn at the edge */
            e->x += e->vx * dt;
            float ahead = world_surface_under(e->x + (e->vx > 0 ? 1.2f : -1.2f),
                                              e->y + 0.5f);
            if (ahead < e->y - 0.5f || fabsf(e->x - e->home_x) > 9.0f)
                e->vx = -e->vx;
            e->facing = e->vx > 0 ? 1 : -1;
        }

        if (dist < 26.0f) {
            e->fire_t -= dt;
            if (e->fire_t <= 0.0f) {
                enemy_fire(e, px, p_chest);
                e->fire_t = (e->kind == EN_TURRET) ? 1.5f
                          : (e->kind == EN_DRONE ? 1.9f : 2.3f);
            }
        }
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->life <= 0.0f) continue;
        b->life -= dt;
        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (bullet_hits_cover(b->x, b->y)) {
            b->life = 0.0f;
            spawn_burst(b->x, b->y);
            continue;
        }

        if (!b->from_player) continue;
        for (int j = 0; j < NUM_ENEMIES; j++) {
            Enemy *e = &enemies[j];
            if (!e->alive) continue;
            if (fabsf(b->x - e->x) > 1.1f) continue;
            if (fabsf(b->y - (e->y + 0.8f)) > 1.1f) continue;
            b->life = 0.0f;
            e->hp--;
            e->hurt_t = 0.16f;
            spawn_burst(b->x, b->y);
            if (e->hp <= 0) {
                e->alive = 0;
                e->death_t = 0.001f;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_BURSTS; i++)
        if (bursts[i].t >= 0.0f) bursts[i].t += dt;
}

int world_take_player_damage(float px, float py) {
    int dmg = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->life <= 0.0f || b->from_player) continue;
        if (fabsf(b->x - px) > 0.9f) continue;
        if (b->y < py || b->y > py + 2.2f) continue;
        b->life = 0.0f;
        spawn_burst(b->x, b->y);
        dmg++;
    }
    return dmg;
}

int world_enemies_alive(void) {
    int n = 0;
    for (int i = 0; i < NUM_ENEMIES; i++) n += enemies[i].alive;
    return n;
}

/* ---------------- draw ---------------- */

void world_set_tint(Color c) { world_tint = c; }

static void draw_frame(Sheet s, float x, float y_bottom, float z, float scale,
                       float t, int loop, int flip, Color c) {
    c = rgba(c.r * world_tint.r, c.g * world_tint.g,
             c.b * world_tint.b, c.a * world_tint.a);
    if (!s.tex.id) return;
    int fw = s.tex.w / s.frames;
    int f = s.fps > 0.0f ? (int)(t * s.fps) : 0;
    if (loop) f %= s.frames;
    else if (f >= s.frames) f = s.frames - 1;

    float w = fw / PPU * scale, h = s.tex.h / PPU * scale;
    float u0 = (float)(f * fw) / (float)s.tex.w;
    float u1 = (float)((f + 1) * fw) / (float)s.tex.w;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(s.tex);
    rnd_quad_uv(x, y_bottom + h * 0.5f, z, w, h, u0, 0.0f, u1, 1.0f, c);
}

static void draw_props(float z_filter) {
    for (int i = 0; i < NUM_PROPS; i++) {
        const Prop *p = &props[i];
        if (p->z != z_filter) continue;
        draw_frame(sh_prop[p->kind], p->x, p->y, p->z, p->scale,
                   world_t + i * 0.37f, 1, 0, rgba(0.85f, 0.83f, 1.0f, 1.0f));
        /* signs light their own patch of street */
        if (p->kind == PR_NEON || p->kind == PR_SUSHI || p->kind == PR_BIG ||
            p->kind == PR_LIGHT || p->kind == PR_BANNER_C || p->kind == PR_HOTEL) {
            Color c = (i % 3 == 0) ? rgba(0.30f, 0.91f, 1.0f, 0.9f)
                    : (i % 3 == 1) ? rgba(1.0f, 0.32f, 0.72f, 0.9f)
                                   : rgba(1.0f, 0.68f, 0.28f, 0.9f);
            Light l = { p->x, p->y + 1.2f, p->z + 0.5f, c, 2.0f,
                        1.1f + (i % 5) * 0.4f, (float)i, (i % 4) == 0 };
            fx_light(&l, world_t);
            float road = world_surface_under(p->x, p->y);
            if (road > -100.0f)
                fx_light_on_ground(&l, world_t, road, Z_PLAY - 0.2f);
        }
    }
}

/* Every platform above street level gets built out of walkway pieces, so the
   thing you land on is the thing you can see. */
static void draw_platforms(void) {
    if (!tx_span.id) return;
    float tw = tx_span.w / PPU, th = tx_span.h / PPU;

    rnd_set_blend(BLEND_ALPHA);
    for (int i = 0; i < NUM_PLATFORMS; i++) {
        const Platform *p = &platforms[i];
        if (p->y <= 0.01f) continue;           /* street level is the deck */
        rnd_set_tex(tx_span);
        int n = (int)ceilf(p->w / tw);
        for (int k = 0; k < n; k++) {
            float x = p->x + k * tw;
            float w = (k == n - 1) ? (p->x + p->w - x) : tw;
            if (w <= 0.05f) break;
            rnd_quad_uv(x + w * 0.5f, p->y - th * 0.5f, Z_PLAY - 0.4f, w, th,
                        0.0f, 0.0f, w / tw, 1.0f,
                        rgba(0.78f * world_tint.r, 0.75f * world_tint.g,
                             0.98f * world_tint.b, world_tint.a));
        }
        /* Legs down to whatever is under it. A ledge that visibly carries
           its own weight reads as part of the street rather than a slab
           someone left floating in the air. */
        if (tx_support.id) {
            float below = -8.0f;
            for (int k = 0; k < NUM_PLATFORMS; k++) {
                const Platform *q = &platforms[k];
                if (q == p || q->y >= p->y - 0.5f) continue;
                if (p->x + p->w < q->x || p->x > q->x + q->w) continue;
                if (q->y > below) below = q->y;
            }
            float leg_w = tx_support.w / PPU;
            int legs = (int)(p->w / 5.0f) + 1;
            rnd_set_blend(BLEND_ALPHA);
            rnd_set_tex(tx_support);
            for (int k = 0; k < legs; k++) {
                float lx = p->x + (p->w - leg_w) * (legs == 1 ? 0.5f
                                                   : (float)k / (legs - 1))
                         + leg_w * 0.5f;
                float top = p->y - 0.45f, bot = below;
                float h = top - bot;
                if (h <= 0.2f) continue;
                rnd_quad(lx, bot + h * 0.5f, Z_PLAY - 0.5f, leg_w * 0.7f, h,
                         rgba(0.34f * world_tint.r, 0.33f * world_tint.g,
                              0.52f * world_tint.b, world_tint.a));
            }
        }

        /* a lit edge so the landing line reads at a glance */
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(tx_slab);
        rnd_quad(p->x + p->w * 0.5f, p->y + 0.07f, Z_PLAY - 0.3f, p->w, 0.5f,
                 rgba(0.35f, 0.90f, 1.0f, 0.75f));
        rnd_set_blend(BLEND_ALPHA);
    }
}

static void draw_cover(void) {
    for (int i = 0; i < NUM_COVERS; i++) {
        const Cover *c = &covers[i];
        draw_frame(sh_prop[c->art], c->x + c->w * 0.5f, c->y, Z_PLAY + 1.0f,
                   c->h / (sh_prop[c->art].tex.h / PPU),
                   world_t, 0, 0, rgba(0.62f, 0.60f, 0.85f, 1.0f));
        /* the lip is the line that matters, so it gets a light of its own */
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(sh_prop[c->art].tex);
        rnd_set_blend(BLEND_ALPHA);
    }
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_slab);
    for (int i = 0; i < NUM_COVERS; i++) {
        const Cover *c = &covers[i];
        rnd_quad(c->x + c->w * 0.5f, c->y + c->h, Z_PLAY + 1.1f, c->w * 1.1f, 0.30f,
                 rgba(0.35f, 0.90f, 1.0f, 0.55f));
    }
    rnd_set_blend(BLEND_ALPHA);
}

void world_draw_back(void) {
    draw_props(Z_PROP_BACK);
    draw_platforms();
}

void world_draw_front(void) {
    for (int i = 0; i < NUM_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) {
            if (e->death_t > 0.0f && e->death_t < 0.5f)
                draw_frame(sh_boom, e->x, e->y, Z_PLAY, 1.0f, e->death_t, 0, 0,
                           rgba(1, 1, 1, 1));
            continue;
        }
        Color tint = e->hurt_t > 0.0f ? rgba(1.0f, 0.55f, 0.55f, 1.0f)
                                      : rgba(1, 1, 1, 1);
        Sheet s = (e->kind == EN_DRONE)  ? sh_drone
                : (e->kind == EN_TURRET) ? sh_turret
                : (fabsf(e->vx) > 0.1f ? sh_cop_run : sh_cop_idle);
        draw_frame(s, e->x, e->y, Z_PLAY, 1.0f, e->anim_t, 1,
                   e->facing > 0, tint);

        /* their sensors glow, which is also how you spot them in the dark */
        Light eye = { e->x, e->y + 1.0f, Z_PLAY + 0.2f,
                      rgba(1.0f, 0.25f, 0.28f, 0.8f), 0.7f, 2.4f, (float)i, 0 };
        fx_light(&eye, world_t);
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->life <= 0.0f) continue;
        float rot = atan2f(b->vy, b->vx);
        Color c = b->from_player ? rgba(0.45f, 0.95f, 1.0f, 1.0f)
                                 : rgba(1.0f, 0.45f, 0.35f, 1.0f);
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(sh_shot.tex);
        rnd_quad_rot(b->x, b->y, Z_PLAY + 0.3f, 1.1f, 0.5f, rot, c);
        Light l = { b->x, b->y, Z_PLAY + 0.2f, c, 0.6f, 0.0f, 0.0f, 0 };
        fx_light(&l, world_t);
    }

    for (int i = 0; i < MAX_BURSTS; i++) {
        if (bursts[i].t < 0.0f || bursts[i].t > 0.25f) continue;
        draw_frame(sh_hit, bursts[i].x, bursts[i].y - 0.3f, Z_PLAY + 0.4f, 1.4f,
                   bursts[i].t, 0, 0, rgba(1, 1, 1, 1));
    }

    draw_props(Z_PROP_FRONT);
    draw_cover();
}
