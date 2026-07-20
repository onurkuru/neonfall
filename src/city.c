/* Ambient city life: traffic crossing the skyline and people in the street.
   None of it is interactive - it exists so the place looks inhabited.
   Art: Warped City / Warped Vehicles by ansimuz (CC0). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "city.h"
#include "fx.h"

typedef struct { Tex tex; int frames; float fps; } Sheet;

#define NUM_FLYERS  6
#define NUM_WALKERS 9

typedef struct {
    int   sheet;          /* which vehicle */
    float x, y, z, speed;
    float wait;           /* seconds until it enters again */
    float t;
    int   dir;
} Flyer;

typedef struct {
    float x, y, z, speed, t;
    int   dir, sheet;
} Walker;

static Sheet sh_flyer[2], sh_thrust;
static Sheet sh_walk[2];
static Flyer flyers[NUM_FLYERS];
static Walker walkers[NUM_WALKERS];
static uint32_t rng = 0x51DE51DEu;
static float city_t;

static float frand(float lo, float hi) { return lo + nf_randf(&rng) * (hi - lo); }

static Sheet load_sheet(const char *root, const char *rel, int frames, float fps) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    Sheet s = { tex_load(path, 0), frames, fps };
    return s;
}

void city_init(const char *root) {
    sh_flyer[0] = load_sheet(root, "city/flyer-a.png", 3, 10.0f);
    sh_flyer[1] = load_sheet(root, "city/flyer-b.png", 3, 10.0f);
    sh_thrust   = load_sheet(root, "city/thrust.png", 3, 16.0f);
    /* the crowd is the same art as the cops, worn as silhouette */
    sh_walk[0]  = load_sheet(root, "enemy/cop-run.png", 10, 9.0f);
    sh_walk[1]  = load_sheet(root, "enemy/cop-idle.png", 3, 5.0f);

    for (int i = 0; i < NUM_FLYERS; i++) {
        Flyer *f = &flyers[i];
        f->sheet = i & 1;
        f->z = -140.0f + (i % 3) * 42.0f;
        f->y = 12.0f + frand(0.0f, 22.0f);
        f->dir = (i & 1) ? 1 : -1;
        f->speed = frand(9.0f, 20.0f);
        f->wait = frand(0.0f, 14.0f);
        f->x = 0.0f;
    }
    for (int i = 0; i < NUM_WALKERS; i++) {
        Walker *w = &walkers[i];
        w->z = -30.0f - (i % 3) * 14.0f;
        w->y = -1.5f - (i % 3) * 0.6f;
        w->dir = (i & 1) ? 1 : -1;
        w->speed = frand(1.4f, 3.0f);
        w->x = frand(-60.0f, 60.0f);
        w->t = frand(0.0f, 4.0f);
        w->sheet = (i % 4 == 0) ? 1 : 0;    /* one in four just stands there */
    }
}

void city_update(float dt, float cam_x) {
    city_t += dt;

    for (int i = 0; i < NUM_FLYERS; i++) {
        Flyer *f = &flyers[i];
        if (f->wait > 0.0f) {
            f->wait -= dt;
            if (f->wait <= 0.0f) {
                /* enter from whichever side it is heading away from */
                float span = 130.0f;
                f->x = cam_x - f->dir * span;
                f->y = 12.0f + frand(0.0f, 22.0f);
                f->speed = frand(9.0f, 20.0f);
            }
            continue;
        }
        f->x += f->dir * f->speed * dt;
        f->t += dt;
        if (fabsf(f->x - cam_x) > 150.0f) {
            f->wait = frand(4.0f, 16.0f);
            f->dir = -f->dir;
        }
    }

    for (int i = 0; i < NUM_WALKERS; i++) {
        Walker *w = &walkers[i];
        if (w->sheet == 0) w->x += w->dir * w->speed * dt;
        w->t += dt;
        if (w->x < cam_x - 70.0f) w->x = cam_x + 70.0f;
        if (w->x > cam_x + 70.0f) w->x = cam_x - 70.0f;
    }
}

static void draw_sheet(Sheet s, float x, float y, float z, float scale,
                       float t, int flip, Color c) {
    if (!s.tex.id) return;
    int fw = s.tex.w / s.frames;
    int f = (int)(t * s.fps) % s.frames;
    float w = fw / PPU * scale, h = s.tex.h / PPU * scale;
    float u0 = (float)(f * fw) / (float)s.tex.w;
    float u1 = (float)((f + 1) * fw) / (float)s.tex.w;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }
    rnd_set_blend(BLEND_ALPHA);
    rnd_set_tex(s.tex);
    rnd_quad_uv(x, y + h * 0.5f, z, w, h, u0, 0.0f, u1, 1.0f, c);
}

void city_draw_traffic(void) {
    for (int i = 0; i < NUM_FLYERS; i++) {
        const Flyer *f = &flyers[i];
        if (f->wait > 0.0f) continue;
        /* distance drains the colour out of them, same as the buildings */
        float dim = clampf(1.0f + f->z / 200.0f, 0.25f, 1.0f);
        draw_sheet(sh_flyer[f->sheet], f->x, f->y, f->z, 1.6f, f->t,
                   f->dir > 0, rgba(0.45f * dim, 0.42f * dim, 0.62f * dim, 1.0f));
        draw_sheet(sh_thrust, f->x - f->dir * 2.2f, f->y + 0.3f, f->z + 0.2f,
                   1.2f, f->t, f->dir > 0, rgba(0.8f, 0.6f, 0.35f, 0.8f));

        Light head = { f->x + f->dir * 2.4f, f->y + 0.8f, f->z + 0.3f,
                       rgba(0.55f, 0.85f, 1.0f, 0.7f), 1.6f, 0.0f, 0.0f, 1 };
        Light tail = { f->x - f->dir * 2.4f, f->y + 0.6f, f->z + 0.3f,
                       rgba(1.0f, 0.35f, 0.25f, 0.6f), 1.2f, 0.0f, 0.0f, 0 };
        fx_light(&head, city_t);
        fx_light(&tail, city_t);
    }
}

void city_draw_crowd(void) {
    for (int i = 0; i < NUM_WALKERS; i++) {
        const Walker *w = &walkers[i];
        /* people are shapes, not characters: flat dark against the neon */
        draw_sheet(sh_walk[w->sheet], w->x, w->y, w->z, 0.9f, w->t,
                   w->dir > 0, rgba(0.055f, 0.060f, 0.115f, 0.92f));
    }
}
