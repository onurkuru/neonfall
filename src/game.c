/* Neonfall - Zone 1 "Undercity" vertical slice.
   Art: Warped City / Warped City 2 by ansimuz (CC0). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neonfall.h"
#include "fx.h"
#include "world.h"

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
    float respawn_x, respawn_y, hurt_t;
    float anim_t;
    int   anim_frame;
    float attack_t;
    int   hp, hp_max;
    float energy;
} Player;


static Player pl;
static float  cam_x, cam_y, shake, clock_t_;
static uint32_t rng_state = 0x1337c0deu;

static Tex tx_white;
static Tex tx_layer[6];
static Tex tx_deck;
typedef struct { Tex tex; int frames; float fps; } Anim;
static Anim an_idle, an_run, an_jump, an_shoot;


#define NUM_LIGHTS 18
static Light lights[NUM_LIGHTS];

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

    tx_layer[0] = load("bg/l1-far.png", 1);
    tx_layer[1] = load("bg/l2-skyline.png", 1);
    tx_layer[2] = load("bg/l3-mid.png", 1);
    tx_layer[3] = load("bg/l4-blocks.png", 1);
    tx_layer[4] = load("bg/l5-near.png", 1);
    tx_layer[5] = load("bg/l6-fore.png", 1);
    tx_deck     = load("tiles/deck.png", 1);

    an_idle  = (Anim){ load("player/idle.png", 0),  4, 8.0f  };
    an_run   = (Anim){ load("player/run.png", 0),   8, 14.0f };
    an_jump  = (Anim){ load("player/jump.png", 0),  5, 10.0f };
    an_shoot = (Anim){ load("player/shoot.png", 0), 1, 12.0f };
    fx_init(asset_root());
    world_init(asset_root());

    for (int i = 0; i < NUM_LIGHTS; i++) {
        Light *l = &lights[i];
        l->x = -120.0f + i * 15.0f + nf_randf(&rng_state) * 7.0f;
        l->y = 2.0f + nf_randf(&rng_state) * 10.0f;
        l->z = (i % 3 == 0) ? Z_L3 : ((i % 3 == 1) ? Z_L4 : Z_L5);
        l->radius = 1.4f + nf_randf(&rng_state) * 2.2f;
        switch (i % 3) {
            case 0:  l->color = rgba(0.30f, 0.91f, 1.00f, 1.0f); break;
            case 1:  l->color = rgba(1.00f, 0.28f, 0.72f, 1.0f); break;
            default: l->color = rgba(1.00f, 0.66f, 0.26f, 1.0f); break;
        }
        l->freq  = 0.7f + nf_randf(&rng_state) * 2.4f;
        l->phase = nf_randf(&rng_state) * 6.28f;
        l->flare = (i % 4) == 0;
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
        world_player_shoot(pl.x, pl.y + 1.05f, pl.facing);
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

    float y_prev = pl.y;
    pl.x += pl.vx * dt;
    pl.y += pl.vy * dt;

    float surface;
    if (pl.vy <= 0.0f && world_land(pl.x, y_prev, pl.y, &surface)) {
        if (!pl.on_ground && pl.vy < -18.0f) shake = 0.10f;
        pl.y = surface;
        pl.vy = 0.0f;
        pl.on_ground = 1;
        pl.jumps_left = 2;
        pl.coyote = COYOTE_TIME;
    } else {
        pl.on_ground = 0;
    }

    /* fall off the street and you go back to the last safe ledge */
    if (pl.y < -14.0f) {
        pl.x = pl.respawn_x;
        pl.y = pl.respawn_y;
        pl.vx = pl.vy = 0.0f;
        pl.hp--;
        pl.hurt_t = 0.9f;
        shake = 0.3f;
    } else if (pl.on_ground) {
        pl.respawn_x = pl.x;
        pl.respawn_y = pl.y;
    }

    if (pl.hurt_t > 0.0f) pl.hurt_t -= dt;
    else {
        int dmg = world_take_player_damage(pl.x, pl.y);
        if (dmg) {
            pl.hp -= dmg;
            pl.hurt_t = 0.8f;
            shake = 0.22f;
            pl.vx -= pl.facing * 6.0f;
        }
    }
    if (pl.hp <= 0) {
        pl.hp = pl.hp_max;
        pl.x = pl.respawn_x; pl.y = pl.respawn_y;
        pl.vx = pl.vy = 0.0f;
        pl.hurt_t = 1.2f;
    }

    if (pl.x < world_left_bound())  pl.x = world_left_bound();
    if (pl.x > world_right_bound()) pl.x = world_right_bound();

    const Anim *a = !pl.on_ground ? &an_jump
                  : (pl.attack_t > 0.0f ? &an_shoot
                  : (fabsf(pl.vx) > 0.6f ? &an_run : &an_idle));
    pl.anim_t += dt * a->fps;
    if (!pl.on_ground) {
        int f = (int)pl.anim_t;                     /* hold the apex frame */
        pl.anim_frame = f < a->frames ? f : a->frames - 1;
    } else {
        pl.anim_frame = ((int)pl.anim_t) % (a->frames > 0 ? a->frames : 1);
    }
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

    world_update(dt, pl.x, pl.y, pl.facing);
    fx_update(dt, cam_x, cam_y, GROUND_Y);
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

static void draw_sky_dome(void) {
    /* the glow the unseen city throws up into the rain */
    Light dome = { cam_x - 30.0f, 16.0f, Z_SKY + 20.0f,
                   rgba(0.42f, 0.14f, 0.50f, 0.55f), 60.0f, 0.21f, 0.4f, 0 };
    Light dome2 = { cam_x + 140.0f, 10.0f, Z_SKY + 20.0f,
                    rgba(0.10f, 0.34f, 0.60f, 0.50f), 52.0f, 0.17f, 2.2f, 0 };
    fx_light(&dome, clock_t_);
    fx_light(&dome2, clock_t_);
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
    draw_layer(tx_layer[0], Z_SKY, 30.0f, -4.0f, rgba(0.34f, 0.30f, 0.52f, 1.0f));
    draw_sky_dome();
    draw_layer(tx_layer[1], Z_L1, 24.0f, -3.0f, rgba(0.30f, 0.27f, 0.46f, 1.0f));
    draw_haze(Z_L1 + 8.0f, 0.26f);
    draw_layer(tx_layer[2], Z_L2, 20.0f, -3.0f, rgba(0.36f, 0.33f, 0.54f, 1.0f));
    draw_haze(Z_L2 + 6.0f, 0.18f);
    draw_layer(tx_layer[3], Z_L3, 17.0f, -3.0f, rgba(0.44f, 0.41f, 0.62f, 1.0f));
    draw_haze(Z_L3 + 5.0f, 0.10f);
    draw_layer(tx_layer[4], Z_L4, 14.0f, -3.0f, rgba(0.52f, 0.49f, 0.70f, 1.0f));
    draw_layer(tx_layer[5], Z_L5, 11.0f, -3.0f, rgba(0.16f, 0.15f, 0.30f, 1.0f));

    /* close the bottom of the frame: below the walkway is unlit depth */
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);
    rnd_quad(cam_x, GROUND_Y - 22.0f, Z_L5 + 2.0f, 300.0f, 40.0f,
             rgba(0.020f, 0.022f, 0.050f, 1.0f));}

static void draw_lights(void) {
    for (int i = 0; i < NUM_LIGHTS; i++)
        fx_light(&lights[i], clock_t_);
}


/* Street level is wet asphalt: a dark plane that exists to hold reflections.
   Almost nothing is drawn here - the light lying on it is the art. */
static void draw_street(void) {
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);

    /* the road itself, darkening as it runs away from the kerb */
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        rnd_quad(cam_x, GROUND_Y - 0.6f - i * 1.5f, Z_DECK, 260.0f, 1.6f,
                 rgba(lerpf(0.055f, 0.015f, t),
                      lerpf(0.060f, 0.018f, t),
                      lerpf(0.110f, 0.038f, t), 1.0f));
    }
    /* kerb: the one hard line that tells you where the ground is */
    rnd_quad(cam_x, GROUND_Y + 0.03f, Z_DECK, 260.0f, 0.07f,
             rgba(0.16f, 0.24f, 0.42f, 0.45f));

    /* every neon source lays a pool and a long streak on the wet road */
    for (int i = 0; i < NUM_LIGHTS; i++)
        fx_light_on_ground(&lights[i], clock_t_, GROUND_Y, Z_DECK + 0.4f);

    /* standing water catching the sky */
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_white);
    rnd_quad(cam_x, GROUND_Y - 0.9f, Z_DECK + 0.3f, 260.0f, 1.8f,
             rgba(0.07f, 0.14f, 0.30f, 0.35f));
}

static const Anim *player_anim(void) {
    return !pl.on_ground ? &an_jump
         : (pl.attack_t > 0.0f ? &an_shoot
         : (fabsf(pl.vx) > 0.6f ? &an_run : &an_idle));
}

/* Draw the current frame; `tint` lets the reflection pass reuse this. */
static void draw_player_frame(float z, Color tint) {
    const Anim *a = player_anim();
    if (!a->tex.id) return;
    int fw = a->tex.w / a->frames;
    float w = fw / PPU, h = a->tex.h / PPU;
    float u0 = (float)(pl.anim_frame * fw) / (float)a->tex.w;
    float u1 = (float)((pl.anim_frame + 1) * fw) / (float)a->tex.w;
    if (pl.facing < 0) { float t = u0; u0 = u1; u1 = t; }

    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(a->tex);
    rnd_quad_uv(pl.x, pl.y + h * 0.5f, z, w, h, u0, 0.0f, u1, 1.0f, tint);
}

static void draw_player(void) {
    /* backlight so the character separates from the city behind it */
    Light back = { pl.x, pl.y + 1.0f, Z_PLAY - 0.6f,
                   rgba(0.18f, 0.52f, 0.85f, 0.5f), 1.8f, 0.0f, 0.0f, 0 };
    fx_light(&back, clock_t_);

    if (pl.dash_t > 0.0f) {
        for (int i = 1; i <= 3; i++) {
            float save = pl.x;
            pl.x = save - pl.facing * i * 0.7f;
            draw_player_frame(Z_PLAY - 0.3f, rgba(0.30f, 0.70f, 1.0f, 0.22f / i));
            pl.x = save;
        }
    }

    Color body = rgba(1, 1, 1, 1);
    if (pl.hurt_t > 0.0f && fmodf(pl.hurt_t, 0.16f) > 0.08f)
        body = rgba(1.0f, 0.45f, 0.45f, 0.75f);
    draw_player_frame(Z_PLAY, body);

    if (pl.attack_t > 0.0f) {
        float t = 1.0f - (pl.attack_t / 0.24f);
        fx_muzzle(pl.x + pl.facing * 1.1f, pl.y + 1.1f, Z_PLAY + 0.4f, pl.facing, t);
    }
}

/* The walkway is wet. Draw the world again, flipped about the surface line
   and squashed so it lies inside the deck rather than hanging below it, then
   composite it additively as light sitting on wet metal. One mirror flag, and
   anything that can be drawn reflects for free. */
static void draw_reflections(void) {
    rnd_set_mirror(1, GROUND_Y, 0.95f);

    /* signs, platforms and enemies stand above the road, so they mirror into
       it cleanly - the city layers do not, they straddle the waterline */
    world_set_tint(rgba(0.42f, 0.46f, 0.70f, 0.55f));
    world_draw_back();
    world_draw_front();
    world_set_tint(rgba(1, 1, 1, 1));

    for (int i = 0; i < NUM_LIGHTS; i++) {
        Light l = lights[i];
        l.color.a *= 0.85f;
        l.z = Z_DECK + 0.5f;
        l.flare = 0;
        fx_light(&l, clock_t_);
    }

    draw_player_frame(Z_DECK + 0.6f, rgba(0.40f, 0.62f, 0.95f, 0.42f));

    fx_flush_lights();          /* reflected light, still mirrored */
    rnd_set_mirror(0, 0.0f, 1.0f);

    /* the reflection dies out with depth into the water */
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(tx_white);
    for (int i = 0; i < 8; i++)
        rnd_quad(cam_x, GROUND_Y - 0.9f - i * 1.4f, Z_DECK + 0.7f, 260.0f, 1.5f,
                 rgba(0.015f, 0.018f, 0.040f, 0.10f + i * 0.11f));
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
    draw_lights();
    fx_flush_lights();          /* city neon lands before the rain in front of it */
    fx_draw_rain_far();
    draw_street();
    world_draw_back();
    fx_flush_pools(clock_t_);
    fx_flush_lights();          /* sign glow sits on the street, under the steam */
    fx_draw_steam();
    world_draw_front();
    draw_player();
    fx_flush_lights();
    draw_reflections();
    fx_draw_splashes();
    fx_draw_rain_near();
    draw_hud();
}
