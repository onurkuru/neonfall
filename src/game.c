/* Neonfall - Zone 1 "Undercity" vertical slice.
   Art: Warped City / Warped City 2 by ansimuz (CC0). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neonfall.h"

#define GROUND_Y      0.0f
#define GRAVITY      -58.0f
#define RUN_SPEED     11.0f
#define ACCEL         90.0f
#define FRICTION      70.0f
#define JUMP_VEL      21.0f
#define COYOTE_TIME   0.10f
#define JUMP_BUFFER   0.12f
#define DASH_SPEED    26.0f
#define DASH_TIME     0.16f
#define DASH_COOLDOWN 0.42f

/* depth planes: negative is away from the camera */
#define Z_SKY      -260.0f
#define Z_L1       -150.0f
#define Z_L2        -90.0f
#define Z_L3        -52.0f
#define Z_L4        -26.0f
#define Z_L5        -12.0f
#define Z_DECK       -1.0f
#define Z_PLAY        0.0f
#define Z_FORE        7.0f

typedef struct {
    float x, y, vx, vy;
    int   facing, on_ground, jumps_left;
    float coyote, buffer;
    float dash_t, dash_cd;
    float anim_t;
    int   anim_frame;
    float attack_t;
    int   hp, hp_max;
    float energy;
} Player;

typedef struct { Tex tex; int frames; float fps; } Anim;
typedef struct { float x, y, z, len, speed; } Drop;

static Player pl;
static float  cam_x, cam_y, shake, clock_t_;
static uint32_t rng_state = 0x1337c0deu;

static Tex tx_white, tx_glow, tx_soft, tx_streak, tx_sky;
static Tex tx_layer[5];
static Tex tx_deck, tx_slab;
static Anim an_idle, an_run, an_jump, an_shoot;

#define NUM_DROPS 200
static Drop drops[NUM_DROPS];

/* neon accents layered on top of the pixel art */
typedef struct { float x, y, z, w, h; Color c; float freq, phase; } Lamp;
#define NUM_LAMPS 16
static Lamp lamps[NUM_LAMPS];

static const char *asset_root(void) {
    static char root[512];
    if (root[0]) return root;
    const char *env = getenv("NEONFALL_ASSETS");
    snprintf(root, sizeof root, "%s", env ? env : "assets");
    return root;
}

static Tex load(const char *rel, int tile) {
    char path[640];
    snprintf(path, sizeof path, "%s/%s", asset_root(), rel);
    return tex_load(path, tile);
}

static Tex make_white(void) {
    unsigned char px[4] = { 255, 255, 255, 255 };
    return tex_from_rgba(px, 1, 1);
}

static Tex make_sky(int n) {
    unsigned char *px = (unsigned char *)malloc(n * 4);
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);          /* 0 top, 1 horizon */
        float r = lerpf(0.055f, 0.230f, powf(t, 1.9f));
        float g = lerpf(0.030f, 0.070f, powf(t, 2.6f));
        float b = lerpf(0.115f, 0.235f, powf(t, 1.4f));
        px[i * 4 + 0] = (unsigned char)(clampf(r, 0, 1) * 255.0f);
        px[i * 4 + 1] = (unsigned char)(clampf(g, 0, 1) * 255.0f);
        px[i * 4 + 2] = (unsigned char)(clampf(b, 0, 1) * 255.0f);
        px[i * 4 + 3] = 255;
    }
    Tex t = tex_from_rgba(px, 1, n);
    free(px);
    return t;
}

void game_init(void) {
    memset(&pl, 0, sizeof pl);
    pl.facing = 1;
    pl.hp = pl.hp_max = 10;
    pl.energy = 0.62f;
    pl.jumps_left = 2;

    tx_white  = make_white();
    tx_glow   = tex_glow(64, 2.0f);
    tx_soft   = tex_soft_bar(32, 32);
    tx_streak = tex_streak(8, 48);
    tx_sky    = make_sky(256);

    tx_layer[0] = load("bg/layer1-far.png", 1);
    tx_layer[1] = load("bg/layer2-skyline.png", 1);
    tx_layer[2] = load("bg/layer3-mid.png", 1);
    tx_layer[3] = load("bg/layer4-near.png", 1);
    tx_layer[4] = load("bg/layer5-front.png", 1);
    tx_deck     = load("tiles/deck.png", 1);
    tx_slab     = load("tiles/slab.png", 0);

    an_idle  = (Anim){ load("player/idle.png", 0),  4, 8.0f  };
    an_run   = (Anim){ load("player/run.png", 0),   8, 14.0f };
    an_jump  = (Anim){ load("player/jump.png", 0),  5, 10.0f };
    an_shoot = (Anim){ load("player/shoot.png", 0), 1, 10.0f };

    for (int i = 0; i < NUM_DROPS; i++) {
        Drop *d = &drops[i];
        d->z = -30.0f + nf_randf(&rng_state) * 45.0f;
        d->x = -40.0f + nf_randf(&rng_state) * 80.0f;
        d->y = -6.0f + nf_randf(&rng_state) * 34.0f;
        d->len = 0.7f + nf_randf(&rng_state) * 1.5f;
        d->speed = 26.0f + nf_randf(&rng_state) * 20.0f;
    }

    for (int i = 0; i < NUM_LAMPS; i++) {
        Lamp *l = &lamps[i];
        l->x = -120.0f + i * 17.0f + nf_randf(&rng_state) * 6.0f;
        l->y = 3.0f + nf_randf(&rng_state) * 9.0f;
        l->z = (i % 2) ? Z_L4 : Z_L5;
        l->w = 0.5f + nf_randf(&rng_state) * 0.4f;
        l->h = 1.4f + nf_randf(&rng_state) * 2.6f;
        switch (i % 3) {
            case 0:  l->c = rgba(0.30f, 0.91f, 1.00f, 1.0f); break;
            case 1:  l->c = rgba(1.00f, 0.28f, 0.72f, 1.0f); break;
            default: l->c = rgba(1.00f, 0.66f, 0.26f, 1.0f); break;
        }
        l->freq  = 0.7f + nf_randf(&rng_state) * 2.4f;
        l->phase = nf_randf(&rng_state) * 6.28f;
    }
}

/* ---------------- update ---------------- */

static void update_player(const Input *in, float dt) {
    if (pl.dash_cd > 0.0f)  pl.dash_cd -= dt;
    if (pl.attack_t > 0.0f) pl.attack_t -= dt;

    if (in->dash && pl.dash_cd <= 0.0f) {
        pl.dash_t = DASH_TIME;
        pl.dash_cd = DASH_COOLDOWN;
        if (in->move != 0.0f) pl.facing = in->move > 0 ? 1 : -1;
        shake = 0.14f;
    }
    if ((in->attack || in->fire) && pl.attack_t <= 0.0f) {
        pl.attack_t = 0.24f;
        pl.energy = clampf(pl.energy + 0.05f, 0.0f, 1.0f);
    }

    if (pl.dash_t > 0.0f) {
        pl.dash_t -= dt;
        pl.vx = DASH_SPEED * pl.facing;
        pl.vy = 0.0f;
    } else {
        if (in->move != 0.0f) {
            float target = in->move * RUN_SPEED;
            pl.vx += (target - pl.vx) * clampf(ACCEL * dt / RUN_SPEED, 0.0f, 1.0f);
            pl.facing = in->move > 0 ? 1 : -1;
        } else {
            float drop = FRICTION * dt;
            if (fabsf(pl.vx) <= drop) pl.vx = 0.0f;
            else pl.vx -= drop * (pl.vx > 0 ? 1.0f : -1.0f);
        }
        pl.vy += GRAVITY * dt;
    }

    if (in->jump) pl.buffer = JUMP_BUFFER;
    if (pl.buffer > 0.0f) pl.buffer -= dt;
    if (pl.coyote > 0.0f) pl.coyote -= dt;

    int can_jump = (pl.coyote > 0.0f) || (pl.jumps_left > 0 && !pl.on_ground);
    if (pl.buffer > 0.0f && can_jump) {
        pl.vy = JUMP_VEL;
        pl.buffer = 0.0f;
        pl.coyote = 0.0f;
        if (!pl.on_ground) pl.jumps_left--;
        pl.on_ground = 0;
    }
    if (!in->jump_held && pl.vy > 0.0f) pl.vy -= pl.vy * 6.0f * dt;

    pl.x += pl.vx * dt;
    pl.y += pl.vy * dt;

    if (pl.y <= GROUND_Y) {
        if (!pl.on_ground && pl.vy < -18.0f) shake = 0.10f;
        pl.y = GROUND_Y;
        pl.vy = 0.0f;
        pl.on_ground = 1;
        pl.jumps_left = 2;
        pl.coyote = COYOTE_TIME;
    } else {
        pl.on_ground = 0;
    }

    if (pl.x < -140.0f) pl.x = -140.0f;
    if (pl.x >  160.0f) pl.x =  160.0f;

    /* animation clock */
    const Anim *a = !pl.on_ground ? &an_jump
                  : (pl.attack_t > 0.0f ? &an_shoot
                  : (fabsf(pl.vx) > 0.6f ? &an_run : &an_idle));
    pl.anim_t += dt * a->fps;
    if (!pl.on_ground) {
        /* hold the apex frame instead of looping the jump */
        int f = (int)pl.anim_t;
        pl.anim_frame = f < a->frames ? f : a->frames - 1;
    } else {
        pl.anim_frame = ((int)pl.anim_t) % a->frames;
    }
}

void game_update(const Input *in, float dt) {
    clock_t_ += dt;

    int was_air = !pl.on_ground;
    update_player(in, dt);
    if (was_air != !pl.on_ground) pl.anim_t = 0.0f;

    float target_x = pl.x + pl.vx * 0.22f;
    float target_y = 4.2f + pl.y * 0.40f;
    cam_x = lerpf(cam_x, target_x, clampf(dt * 5.0f, 0.0f, 1.0f));
    cam_y = lerpf(cam_y, target_y, clampf(dt * 3.4f, 0.0f, 1.0f));

    if (shake > 0.0f) shake = clampf(shake - dt * 0.9f, 0.0f, 1.0f);

    for (int i = 0; i < NUM_DROPS; i++) {
        Drop *d = &drops[i];
        d->y -= d->speed * dt;
        d->x -= d->speed * 0.14f * dt;
        if (d->y < GROUND_Y - 4.0f) {
            d->y = cam_y + 22.0f;
            d->x = cam_x - 45.0f + nf_randf(&rng_state) * 90.0f;
        }
    }
}

/* ---------------- draw ---------------- */

/* Depth scale: how much bigger a quad must be at depth z to cover the same
   screen area as it would on the play plane. */
static float depth_scale(float z) { return (CAM_Z - z) / CAM_Z; }

/* Tiled parallax layer. Height is given in play-plane units so art stays
   legible no matter how far back it sits; parallax then comes from z alone. */
static void draw_layer(Tex t, float z, float height_units, float y_base, Color c) {
    if (!t.id) return;
    float s = depth_scale(z);
    float h = height_units * s;
    float w = h * (float)t.w / (float)t.h;
    float view_w = VIEW_H * ((float)SCREEN_W / (float)SCREEN_H) * s;

    float left = cam_x * s - view_w * 0.5f - w;
    float start = floorf(left / w) * w;
    float y = (y_base + height_units * 0.5f) * s + cam_y * (s - 1.0f) * 0.5f;

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(t);
    for (float x = start; x < cam_x * s + view_w * 0.5f + w; x += w)
        rnd_quad(x, y, z, w, h, c);
}

static void draw_sky(void) {
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_sky);
    rnd_quad(cam_x * 0.02f, 40.0f, Z_SKY, 1600.0f, 300.0f, rgba(1, 1, 1, 1));

    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    rnd_quad(-30.0f, 18.0f, Z_SKY + 20.0f, 700.0f, 260.0f,
             rgba(0.40f, 0.12f, 0.46f, 0.30f));
    rnd_quad(140.0f, 12.0f, Z_SKY + 20.0f, 560.0f, 210.0f,
             rgba(0.10f, 0.34f, 0.58f, 0.26f));
}

/* Aerial perspective wash sitting in front of a depth band. */
static void draw_haze(float z, float density) {
    float s = depth_scale(z);
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);
    rnd_quad(cam_x * s, cam_y * s, z,
             VIEW_H * 2.2f * s, VIEW_H * 1.6f * s,
             rgba(0.13f, 0.07f, 0.22f, density));
}

static void draw_background(void) {
    draw_sky();
    draw_layer(tx_layer[0], Z_L1, 15.0f, -1.0f, rgba(0.60f, 0.54f, 0.80f, 1.0f));
    draw_haze(Z_L1 + 8.0f, 0.36f);
    draw_layer(tx_layer[1], Z_L2, 13.0f, -2.0f, rgba(0.74f, 0.70f, 0.92f, 1.0f));
    draw_haze(Z_L2 + 6.0f, 0.24f);
    draw_layer(tx_layer[2], Z_L3, 11.0f, -2.5f, rgba(0.88f, 0.86f, 1.00f, 1.0f));
    draw_haze(Z_L3 + 5.0f, 0.15f);
    draw_layer(tx_layer[3], Z_L4, 9.5f, -3.0f, rgba(1.0f, 1.0f, 1.0f, 1.0f));
    draw_haze(Z_L4 + 4.0f, 0.08f);
    /* nearest band reads as a dark silhouette rather than another city block */
    draw_layer(tx_layer[4], Z_L5, 7.0f, -3.5f, rgba(0.20f, 0.17f, 0.38f, 1.0f));
}

static float flicker(const Lamp *l) {
    float a = 0.84f + 0.16f * sinf(clock_t_ * l->freq + l->phase);
    if (sinf(clock_t_ * l->freq * 3.7f + l->phase * 5.0f) > 0.965f) a *= 0.25f;
    return a;
}

static void draw_lamps(void) {
    for (int i = 0; i < NUM_LAMPS; i++) {
        Lamp *l = &lamps[i];
        float a = flicker(l);
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(tx_glow);
        rnd_quad(l->x, l->y, l->z, l->w * 7.0f, l->h * 3.2f,
                 rgba(l->c.r, l->c.g, l->c.b, a * 0.34f));
        rnd_set_tex(tx_soft);
        rnd_quad(l->x, l->y, l->z, l->w, l->h,
                 rgba(l->c.r, l->c.g, l->c.b, a * 0.85f));
    }
}

static void draw_deck(void) {
    if (!tx_deck.id) return;
    float tile_w = tx_deck.w / PPU;          /* world units per tile */
    float tile_h = tx_deck.h / PPU;
    /* the walking surface is the texture's top row */
    float y = GROUND_Y - tile_h * 0.5f;

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_deck);
    float start = floorf((cam_x - 32.0f) / tile_w) * tile_w;
    for (float x = start; x < cam_x + 32.0f; x += tile_w)
        rnd_quad(x + tile_w * 0.5f, y, Z_DECK, tile_w, tile_h, rgba(1, 1, 1, 1));

    /* wet sheen and neon bleeding across the walkway */
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_soft);
    for (int i = 0; i < NUM_LAMPS; i++) {
        Lamp *l = &lamps[i];
        float a = flicker(l) * 0.16f;
        rnd_quad(l->x + sinf(clock_t_ * 2.2f + l->phase) * 0.2f,
                 GROUND_Y - 0.35f, Z_DECK + 0.4f, l->w * 2.2f, 0.7f,
                 rgba(l->c.r, l->c.g, l->c.b, a));
    }
    rnd_set_tex(tx_glow);
    rnd_quad(pl.x, GROUND_Y - 0.25f, Z_DECK + 0.5f, 4.4f, 1.0f,
             rgba(0.24f, 0.70f, 0.95f, 0.16f));
}

static void draw_player(void) {
    const Anim *a = !pl.on_ground ? &an_jump
                  : (pl.attack_t > 0.0f ? &an_shoot
                  : (fabsf(pl.vx) > 0.6f ? &an_run : &an_idle));
    if (!a->tex.id) return;

    int fw = a->tex.w / a->frames, fh = a->tex.h;
    float w = fw / PPU, h = fh / PPU;
    float u0 = (float)(pl.anim_frame * fw) / (float)a->tex.w;
    float u1 = (float)((pl.anim_frame + 1) * fw) / (float)a->tex.w;
    if (pl.facing < 0) { float t = u0; u0 = u1; u1 = t; }

    /* backlight so the sprite separates from the walkway */
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    rnd_quad(pl.x, pl.y + h * 0.45f, Z_PLAY - 0.5f, w * 2.6f, h * 1.9f,
             rgba(0.16f, 0.48f, 0.80f, 0.26f));

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(a->tex);
    rnd_quad_uv(pl.x, pl.y + h * 0.5f, Z_PLAY, w, h, u0, 0.0f, u1, 1.0f,
                rgba(1, 1, 1, 1));

    if (pl.dash_t > 0.0f) {
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(a->tex);
        for (int i = 1; i <= 3; i++)
            rnd_quad_uv(pl.x - pl.facing * i * 0.9f, pl.y + h * 0.5f, Z_PLAY - 0.2f,
                        w, h, u0, 0.0f, u1, 1.0f,
                        rgba(0.25f, 0.70f, 1.0f, 0.22f / i));
    }
    if (pl.attack_t > 0.0f) {
        float t = 1.0f - (pl.attack_t / 0.24f);
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(tx_glow);
        rnd_quad(pl.x + pl.facing * (1.4f + t * 1.2f), pl.y + h * 0.55f,
                 Z_PLAY + 0.3f, 3.0f, 1.6f,
                 rgba(0.45f, 0.85f, 1.0f, (1.0f - t) * 0.5f));
    }
}

static void draw_rain(void) {
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_streak);
    for (int i = 0; i < NUM_DROPS; i++) {
        Drop *d = &drops[i];
        float near = clampf((d->z + 30.0f) / 45.0f, 0.0f, 1.0f);
        rnd_quad_rot(d->x, d->y, d->z, 0.05f + near * 0.05f, d->len, 0.12f,
                     rgba(0.60f, 0.76f, 1.0f, 0.09f + near * 0.20f));
    }
}

static void draw_hud(void) {
    rnd_ui_begin();
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);

    for (int i = 0; i < pl.hp_max; i++)
        rnd_ui_quad(30.0f + i * 17.0f, 44.0f, 13.0f, 6.0f,
                    i < pl.hp ? rgba(0.30f, 0.91f, 1.0f, 0.95f)
                              : rgba(0.30f, 0.91f, 1.0f, 0.14f));

    rnd_ui_quad(30.0f, 58.0f, 160.0f, 3.0f, rgba(1.0f, 0.30f, 0.85f, 0.16f));
    rnd_ui_quad(30.0f, 58.0f, 160.0f * pl.energy, 3.0f, rgba(1.0f, 0.30f, 0.85f, 0.95f));

    float cd = 1.0f - clampf(pl.dash_cd / DASH_COOLDOWN, 0.0f, 1.0f);
    rnd_ui_quad(SCREEN_W - 100.0f, SCREEN_H - 46.0f, 70.0f, 3.0f,
                rgba(0.30f, 0.91f, 1.0f, 0.16f));
    rnd_ui_quad(SCREEN_W - 100.0f, SCREEN_H - 46.0f, 70.0f * cd, 3.0f,
                rgba(0.30f, 0.91f, 1.0f, 0.9f));
    rnd_ui_end();
}

void game_draw(void) {
    float sh = shake > 0.0f ? sinf(clock_t_ * 90.0f) * shake * 0.5f : 0.0f;
    rnd_camera(cam_x, cam_y, sh);

    draw_background();
    draw_lamps();
    draw_deck();
    draw_player();
    draw_rain();
    draw_hud();
}
