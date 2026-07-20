/* Neonfall - Zone 1 "Undercity" vertical slice.
   Art: Warped City / Warped City 2 by ansimuz (CC0). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neonfall.h"
#include "skel.h"

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

typedef struct { float x, y, z, len, speed; } Drop;

static Player pl;
static float  cam_x, cam_y, shake, clock_t_;
static uint32_t rng_state = 0x1337c0deu;

static Tex tx_white, tx_glow, tx_soft, tx_streak;
static Tex tx_layer[6];
static Tex tx_deck;
static Rig rig;

#define NUM_DROPS 200
static Drop drops[NUM_DROPS];

/* neon accents layered on top of the pixel art */
typedef struct { float x, y, z, w, h; Color c; float freq, phase; } Lamp;
#define NUM_LAMPS 16
static Lamp lamps[NUM_LAMPS];

static const char *asset_root(void) {
    static char root[512];
    if (root[0]) return root;
#ifdef VITA
    const char *fallback = "app0:assets";
    const char *env = NULL;
#else
    const char *fallback = "assets";
    const char *env = getenv("NEONFALL_ASSETS");
#endif
    snprintf(root, sizeof root, "%s", env ? env : fallback);
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

    tx_layer[0] = load("bg/layer1-sky.png", 1);
    tx_layer[1] = load("bg/layer2-skyline.png", 1);
    tx_layer[2] = load("bg/layer3-mid.png", 1);
    tx_layer[3] = load("bg/layer4-near2.png", 1);
    tx_layer[4] = load("bg/layer4-near.png", 1);
    tx_layer[5] = load("bg/layer5-fore.png", 1);
    tx_deck     = load("tiles/walkway.png", 1);

    skel_load_player(&rig);

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

    /* the run cycle is driven by distance travelled, not by wall time */
    if (pl.on_ground) pl.anim_t += dt * fabsf(pl.vx) * 0.62f;
    else              pl.anim_t += dt;
}

void game_update(const Input *in, float dt) {
    clock_t_ += dt;

    int was_air = !pl.on_ground;
    update_player(in, dt);
    if (was_air != !pl.on_ground) pl.anim_t = 0.0f;
    if (pl.on_ground && fabsf(pl.vx) < 0.6f) pl.anim_t = clock_t_;

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
static int layer_filter = -2;   /* NEONFALL_LAYER isolates one band for debugging */
static int layer_seq;

static void draw_layer(Tex t, float z, float height_units, float y_base, Color c) {
    if (!t.id) return;
    if (layer_filter == -2) {
        const char *e = getenv("NEONFALL_LAYER");
        layer_filter = e ? atoi(e) : -1;
    }
    int me = layer_seq++;
    if (layer_filter >= 0 && me != layer_filter) return;
    float s = depth_scale(z);
    float h = height_units * s;
    float w = h * (float)t.w / (float)t.h;

    /* the frustum stays centred on the camera at every depth; it just gets
       wider the further back you go */
    float half_view = VIEW_H * ((float)SCREEN_W / (float)SCREEN_H) * 0.5f * s;
    float start = floorf((cam_x - half_view - w) / w) * w;
    /* place the layer so it lands where we want it on screen, whatever its
       depth: convert the desired play-plane position back out to depth z */
    float y = cam_y + (y_base + height_units * 0.5f - cam_y) * s;

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(t);
    for (float x = start; x < cam_x + half_view + w; x += w)
        rnd_quad(x + w * 0.5f, y, z, w, h, c);
}

static void draw_sky(void) {
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    /* the city's light dome, painted straight onto the sky layer */
    rnd_quad(cam_x - 30.0f, 18.0f, Z_SKY + 20.0f, 700.0f, 260.0f,
             rgba(0.34f, 0.10f, 0.42f, 0.22f));
    rnd_quad(cam_x + 140.0f, 12.0f, Z_SKY + 20.0f, 560.0f, 210.0f,
             rgba(0.08f, 0.28f, 0.52f, 0.20f));
}

/* Aerial perspective wash sitting in front of a depth band. */
static void draw_haze(float z, float density) {
    float s = depth_scale(z);
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);
    rnd_quad(cam_x, cam_y, z,
             VIEW_H * 2.2f * s, VIEW_H * 1.6f * s,
             rgba(0.13f, 0.07f, 0.22f, density));
}

static void draw_background(void) {
    /* every layer is anchored by its base near the walkway line and rises
       past the top of the frame, so the city towers over the player */
    draw_layer(tx_layer[0], Z_SKY, 34.0f, -8.0f, rgba(1.0f, 1.0f, 1.0f, 1.0f));
    draw_sky();
    draw_layer(tx_layer[1], Z_L1, 21.0f, -1.5f, rgba(0.60f, 0.56f, 0.80f, 1.0f));
    draw_haze(Z_L1 + 8.0f, 0.30f);
    draw_layer(tx_layer[2], Z_L2, 19.0f, -2.5f, rgba(0.76f, 0.73f, 0.93f, 1.0f));
    draw_haze(Z_L2 + 6.0f, 0.20f);
    /* layer4-near came back as an opaque haze plate rather than a keyed band,
       so it serves as an extra depth wash instead of a city layer */
    draw_layer(tx_layer[4], Z_L3 - 6.0f, 20.0f, -3.0f, rgba(0.7f, 0.66f, 0.9f, 0.35f));
    draw_layer(tx_layer[3], Z_L4, 15.0f, -3.5f, rgba(1.0f, 1.0f, 1.0f, 1.0f));
    draw_haze(Z_L4 + 4.0f, 0.06f);
    /* nearest band reads as a dark silhouette rather than another city block */
    draw_layer(tx_layer[5], Z_L5, 13.0f, -4.5f, rgba(0.30f, 0.27f, 0.48f, 1.0f));
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
    /* the painted walkway is sized in world units, not by pixel scale;
       WALK_SURFACE is where the deck's top edge sits inside the texture */
    const float tile_w = 8.0f;
    const float tile_h = tile_w * (float)tx_deck.h / (float)tx_deck.w;
    const float WALK_SURFACE = 0.30f;        /* 0 = texture top, 1 = bottom */
    float y = GROUND_Y - tile_h * (0.5f - WALK_SURFACE);

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
    rig.facing = pl.facing;
    rig.root_x = pl.x;
    rig.root_y = pl.y + skel_hip_height(&rig);

    if (pl.dash_t > 0.0f)          skel_pose_dash(&rig);
    else if (pl.attack_t > 0.0f)   skel_pose_attack(&rig, 1.0f - pl.attack_t / 0.24f);
    else if (!pl.on_ground)        skel_pose_air(&rig, pl.vy);
    else if (fabsf(pl.vx) > 0.6f)  skel_pose_run(&rig, pl.anim_t * 6.2831853f,
                                                 fabsf(pl.vx) / RUN_SPEED);
    else                           skel_pose_idle(&rig, pl.anim_t);

    /* backlight so the silhouette separates from the walkway */
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    rnd_quad(pl.x, pl.y + 1.3f, Z_PLAY - 0.6f, 6.0f, 5.0f,
             rgba(0.16f, 0.48f, 0.80f, 0.22f));

    /* dash leaves a fading trail of the previous pose */
    if (pl.dash_t > 0.0f) {
        Rig ghost = rig;
        for (int i = 1; i <= 3; i++) {
            ghost.root_x = pl.x - pl.facing * i * 0.75f;
            ghost.alpha = 0.20f / i;
            skel_draw(&ghost, Z_PLAY - 0.3f);
        }
        rig.alpha = 1.0f;
    }

    /* Silhouette-first: the character reads by shape against the neon, and
       the painted interior detail never has to survive being 100px tall.
       NEONFALL_LIT=1 shows the fully painted version for comparison. */
    if (getenv("NEONFALL_LIT"))
        skel_draw(&rig, Z_PLAY);
    else
        skel_draw_silhouette(&rig, Z_PLAY,
                             rgba(0.055f, 0.065f, 0.115f, 1.0f),
                             rgba(0.30f, 0.85f, 1.0f, 0.30f), 0.045f);

    if (pl.attack_t > 0.0f) {
        float t = 1.0f - (pl.attack_t / 0.24f);
        rnd_set_blend(BLEND_ADD);
        rnd_set_tex(tx_glow);
        rnd_quad(pl.x + pl.facing * (1.3f + t * 1.1f), pl.y + 1.5f,
                 Z_PLAY + 0.4f, 3.4f, 2.0f,
                 rgba(0.45f, 0.85f, 1.0f, (1.0f - t) * 0.45f));
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
    layer_seq = 0;
    float sh = shake > 0.0f ? sinf(clock_t_ * 90.0f) * shake * 0.5f : 0.0f;
    rnd_camera(cam_x, cam_y, sh);

    draw_background();
    draw_lamps();
    draw_deck();
    draw_player();
    draw_rain();
    draw_hud();
}
