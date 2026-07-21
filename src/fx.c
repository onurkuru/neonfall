/* Weather, light and particle layer.
   Sprites are Kenney's Particle Pack (CC0). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fx.h"

#define NUM_DROPS_FAR   260
#define NUM_DROPS_NEAR  130
#define NUM_SPLASH       48
#define NUM_STEAM        18

/* rain sits in two bands so the near band can be big, fast and blurred */
#define Z_RAIN_FAR  -34.0f
#define Z_RAIN_NEAR   4.0f

typedef struct { float x, y, z, len, speed, alpha; } Drop;
typedef struct { float x, y, z, life, size; } Splash;
typedef struct { float x, y, z, life, size, drift; } Steam;

static Tex tx_glow, tx_lamp, tx_flare, tx_rain, tx_spark;
static Tex tx_star, tx_smoke_a, tx_smoke_b, tx_ring, tx_muzzle, tx_impact;

static Drop   drops_far[NUM_DROPS_FAR];
static Drop   drops_near[NUM_DROPS_NEAR];
static Splash splashes[NUM_SPLASH];
static Steam  steam[NUM_STEAM];
static int    next_splash;
static uint32_t rng = 0xC0FFEEu;

static float frand(float lo, float hi) { return lo + nf_randf(&rng) * (hi - lo); }

static Tex load_fx(const char *root, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/fx/%s", root, name);
    return tex_load(path, 0);
}

void fx_init(const char *root) {
    tx_glow    = load_fx(root, "glow.png");
    tx_lamp    = load_fx(root, "lamp.png");
    tx_flare   = load_fx(root, "flare.png");
    tx_rain    = load_fx(root, "rain.png");
    tx_spark   = load_fx(root, "spark.png");
    tx_star    = load_fx(root, "star.png");
    tx_smoke_a = load_fx(root, "smoke-a.png");
    tx_smoke_b = load_fx(root, "smoke-b.png");
    tx_ring    = load_fx(root, "ring.png");
    tx_muzzle  = load_fx(root, "muzzle.png");
    tx_impact  = load_fx(root, "impact.png");

    for (int i = 0; i < NUM_DROPS_FAR; i++) {
        Drop *d = &drops_far[i];
        d->x = frand(-60.0f, 60.0f); d->y = frand(-6.0f, 34.0f);
        d->z = Z_RAIN_FAR + frand(-12.0f, 12.0f);
        d->len = frand(1.1f, 2.4f); d->speed = frand(30.0f, 44.0f);
        d->alpha = frand(0.22f, 0.42f);
    }
    for (int i = 0; i < NUM_DROPS_NEAR; i++) {
        Drop *d = &drops_near[i];
        d->x = frand(-30.0f, 30.0f); d->y = frand(-6.0f, 26.0f);
        d->z = Z_RAIN_NEAR + frand(-2.0f, 5.0f);
        d->len = frand(2.2f, 4.5f); d->speed = frand(52.0f, 78.0f);
        d->alpha = frand(0.34f, 0.62f);
    }
    for (int i = 0; i < NUM_STEAM; i++) {
        Steam *s = &steam[i];
        s->x = frand(-40.0f, 40.0f); s->y = frand(-1.0f, 3.0f);
        s->z = frand(-14.0f, 2.0f);
        s->life = frand(0.0f, 1.0f); s->size = frand(2.5f, 6.0f);
        s->drift = frand(-0.5f, 0.9f);
    }
}

static void spawn_splash(float x, float y, float z) {
    Splash *s = &splashes[next_splash];
    next_splash = (next_splash + 1) % NUM_SPLASH;
    s->x = x; s->y = y; s->z = z;
    s->life = 1.0f;
    s->size = frand(0.35f, 0.85f);
}

static void step_rain(Drop *d, int n, float dt, float cam_x, float cam_y,
                      float ground_y, float spread, int splash) {
    for (int i = 0; i < n; i++) {
        d[i].y -= d[i].speed * dt;
        d[i].x -= d[i].speed * 0.16f * dt;      /* wind shear */
        if (d[i].y < ground_y) {
            if (splash && nf_randf(&rng) < 0.5f)
                spawn_splash(d[i].x, ground_y, d[i].z);
            d[i].y = cam_y + 22.0f;
            d[i].x = cam_x + frand(-spread, spread);
        }
        /* keep the column centred on the camera as it travels */
        if (d[i].x < cam_x - spread) d[i].x += spread * 2.0f;
        if (d[i].x > cam_x + spread) d[i].x -= spread * 2.0f;
    }
}

void fx_update(float dt, float cam_x, float cam_y, float ground_y) {
    step_rain(drops_far, NUM_DROPS_FAR, dt, cam_x, cam_y, ground_y - 6.0f, 60.0f, 0);
    step_rain(drops_near, NUM_DROPS_NEAR, dt, cam_x, cam_y, ground_y, 26.0f, 1);

    for (int i = 0; i < NUM_SPLASH; i++)
        if (splashes[i].life > 0.0f) splashes[i].life -= dt * 3.4f;

    for (int i = 0; i < NUM_STEAM; i++) {
        Steam *s = &steam[i];
        s->life -= dt * 0.16f;
        s->y += dt * 0.9f;
        s->x += s->drift * dt;
        if (s->life <= 0.0f) {
            s->life = 1.0f;
            s->x = cam_x + frand(-24.0f, 24.0f);
            s->y = ground_y + frand(-0.5f, 1.0f);
            s->size = frand(2.5f, 6.5f);
            s->drift = frand(-0.5f, 0.9f);
        }
    }
}

static void draw_drops(const Drop *d, int n, float w_scale) {
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_rain);
    for (int i = 0; i < n; i++)
        rnd_quad_rot(d[i].x, d[i].y, d[i].z,
                     0.20f * w_scale, d[i].len, 0.13f,
                     rgba(0.70f, 0.84f, 1.0f, d[i].alpha));
}

void fx_draw_rain_far(void)  { draw_drops(drops_far, NUM_DROPS_FAR, 1.0f); }
void fx_draw_rain_near(void) { draw_drops(drops_near, NUM_DROPS_NEAR, 1.8f); }

void fx_draw_splashes(void) {
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_ring);
    for (int i = 0; i < NUM_SPLASH; i++) {
        const Splash *s = &splashes[i];
        if (s->life <= 0.0f) continue;
        float t = 1.0f - s->life;                 /* 0 at impact, 1 when gone */
        float r = s->size * (0.4f + t * 1.8f);
        rnd_quad(s->x, s->y + 0.05f, s->z, r * 2.2f, r * 0.5f,
                 rgba(0.55f, 0.80f, 1.0f, s->life * 0.45f));
    }
}

void fx_draw_steam(void) {
    rnd_set_blend(BLEND_ALPHA);
    for (int pass = 0; pass < 2; pass++) {
        rnd_set_tex(pass ? tx_smoke_a : tx_smoke_b);
        for (int i = pass; i < NUM_STEAM; i += 2) {
            const Steam *s = &steam[i];
            float fade = s->life * (1.0f - s->life) * 4.0f;   /* in and out */
            rnd_quad(s->x, s->y, s->z, s->size, s->size * 0.75f,
                     rgba(0.42f, 0.40f, 0.62f, clampf(fade, 0.0f, 1.0f) * 0.22f));
        }
    }
}

/* ---------------- volumetric shafts ---------------- */

#define MAX_SHAFTS 48
static Light shafts[MAX_SHAFTS];
static float shaft_len[MAX_SHAFTS], shaft_a[MAX_SHAFTS];
static int   shaft_n;

/* ---------------- bokeh ---------------- */

#define NUM_BOKEH 14
typedef struct { float x, y, z, r, drift, phase; Color c; } Bokeh;
static Bokeh bokeh[NUM_BOKEH];
static int   bokeh_ready;

static float flicker(const Light *l, float t) {
    float a = 0.86f + 0.14f * sinf(t * l->freq + l->phase);
    if (sinf(t * l->freq * 3.7f + l->phase * 5.0f) > 0.965f) a *= 0.25f;
    return a;
}

#define MAX_QUEUED_LIGHTS 128
static Light queue[MAX_QUEUED_LIGHTS];
static float queue_a[MAX_QUEUED_LIGHTS];
static int   queue_n;

void fx_light(const Light *l, float t) {
    if (queue_n >= MAX_QUEUED_LIGHTS) {
        fx_flush_lights();                  /* rare, but never drop a light */
    }
    queue[queue_n] = *l;
    queue_a[queue_n] = flicker(l, t);
    queue_n++;
}

void fx_flush_lights(void) {
    if (!queue_n) return;
    rnd_set_blend(BLEND_ADD);

    /* wide halo */
    rnd_set_tex(tx_glow);
    for (int i = 0; i < queue_n; i++) {
        const Light *l = &queue[i];
        rnd_quad(l->x, l->y, l->z, l->radius * 4.0f, l->radius * 4.0f,
                 rgba(l->color.r, l->color.g, l->color.b,
                      queue_a[i] * 0.26f * l->color.a));
    }
    /* tighter falloff so the source has a hot centre */
    rnd_set_tex(tx_lamp);
    for (int i = 0; i < queue_n; i++) {
        const Light *l = &queue[i];
        rnd_quad(l->x, l->y, l->z, l->radius * 1.8f, l->radius * 1.8f,
                 rgba(l->color.r, l->color.g, l->color.b,
                      queue_a[i] * 0.34f * l->color.a));
    }
    /* core */
    rnd_set_tex(tx_star);
    for (int i = 0; i < queue_n; i++) {
        const Light *l = &queue[i];
        rnd_quad(l->x, l->y, l->z, l->radius * 1.1f, l->radius * 1.1f,
                 rgba(1.0f, 1.0f, 1.0f, queue_a[i] * 0.30f * l->color.a));
    }
    rnd_set_tex(tx_flare);
    for (int i = 0; i < queue_n; i++) {
        const Light *l = &queue[i];
        if (!l->flare) continue;
        rnd_quad(l->x, l->y, l->z, l->radius * 9.0f, l->radius * 1.2f,
                 rgba(l->color.r, l->color.g, l->color.b,
                      queue_a[i] * 0.20f * l->color.a));
    }
    queue_n = 0;
}

#define MAX_POOLS 64
static Light pools[MAX_POOLS];
static float pool_a[MAX_POOLS], pool_ground[MAX_POOLS], pool_z[MAX_POOLS];
static int   pool_n;

void fx_light_on_ground(const Light *l, float t, float ground_y, float z) {
    if (l->y - ground_y <= 0.0f || pool_n >= MAX_POOLS) return;
    pools[pool_n] = *l;
    pool_a[pool_n] = flicker(l, t);
    pool_ground[pool_n] = ground_y;
    pool_z[pool_n] = z;
    pool_n++;
}

/* Emit every queued pool: the disc of light on the road, then the chain of
   vertical smears that makes it read as water rather than a mirror. */
void fx_flush_pools(float t) {
    if (!pool_n) return;
    rnd_set_blend(BLEND_ADD);

    rnd_set_tex(tx_glow);
    for (int i = 0; i < pool_n; i++) {
        const Light *l = &pools[i];
        float drop = l->y - pool_ground[i];
        float fade = clampf(1.0f - drop / 26.0f, 0.15f, 1.0f);
        float wobble = sinf(t * 2.3f + l->phase) * 0.18f;
        rnd_quad(l->x + wobble, pool_ground[i] - 0.2f, pool_z[i],
                 l->radius * (3.0f + drop * 0.30f), 2.2f,
                 rgba(l->color.r, l->color.g, l->color.b, pool_a[i] * fade * 0.34f));
    }

    rnd_set_tex(tx_flare);
    for (int i = 0; i < pool_n; i++) {
        const Light *l = &pools[i];
        float drop = l->y - pool_ground[i];
        float fade = clampf(1.0f - drop / 26.0f, 0.15f, 1.0f);
        float wobble = sinf(t * 2.3f + l->phase) * 0.18f;
        for (int k = 0; k < 5; k++) {
            float depth = 0.5f + k * 1.15f;
            float jitter = sinf(t * (2.0f + k * 0.7f) + l->phase + k)
                         * (0.10f + k * 0.06f);
            float w = l->radius * (1.10f + k * 0.28f);
            float alpha = pool_a[i] * fade * 0.42f / (1.0f + k * 0.62f);
            rnd_quad_rot(l->x + wobble + jitter, pool_ground[i] - depth, pool_z[i],
                         1.5f + k * 0.5f, w, 1.5707963f,
                         rgba(l->color.r, l->color.g, l->color.b, alpha));
        }
    }
    pool_n = 0;
}

void fx_shaft(const Light *l, float t, float length) {
    if (shaft_n >= MAX_SHAFTS) return;
    shafts[shaft_n] = *l;
    shaft_len[shaft_n] = length;
    shaft_a[shaft_n] = flicker(l, t);
    shaft_n++;
}

/* A cone of lit rain under each source, built from stacked quads that widen
   and fade as they fall. Cheap, and it is what makes the air look wet. */
void fx_flush_shafts(void) {
    if (!shaft_n) return;
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    for (int i = 0; i < shaft_n; i++) {
        const Light *l = &shafts[i];
        float len = shaft_len[i];
        for (int k = 0; k < 6; k++) {
            float t = (k + 0.5f) / 6.0f;
            float y = l->y - len * t;
            float w = l->radius * (0.9f + t * 2.6f);
            float a = shaft_a[i] * (1.0f - t) * (1.0f - t) * 0.13f * l->color.a;
            rnd_quad(l->x, y, l->z, w, len / 5.0f,
                     rgba(l->color.r, l->color.g, l->color.b, a));
        }
    }
    shaft_n = 0;
}

void fx_update_bokeh(float dt, float cam_x, float cam_y) {
    if (!bokeh_ready) {
        for (int i = 0; i < NUM_BOKEH; i++) {
            Bokeh *b = &bokeh[i];
            b->x = cam_x + frand(-22.0f, 22.0f);
            b->y = cam_y + frand(-9.0f, 9.0f);
            b->z = 6.0f + frand(0.0f, 8.0f);
            b->r = frand(1.4f, 4.2f);
            b->drift = frand(-0.35f, 0.35f);
            b->phase = frand(0.0f, 6.28f);
            switch (i % 3) {
                case 0:  b->c = rgba(0.30f, 0.85f, 1.00f, 1.0f); break;
                case 1:  b->c = rgba(1.00f, 0.30f, 0.75f, 1.0f); break;
                default: b->c = rgba(1.00f, 0.68f, 0.30f, 1.0f); break;
            }
        }
        bokeh_ready = 1;
    }
    for (int i = 0; i < NUM_BOKEH; i++) {
        Bokeh *b = &bokeh[i];
        b->x += b->drift * dt;
        b->y -= dt * 0.25f;                       /* drift down like the rain */
        if (b->y < cam_y - 11.0f) b->y = cam_y + 11.0f;
        if (b->x < cam_x - 24.0f) b->x = cam_x + 24.0f;
        if (b->x > cam_x + 24.0f) b->x = cam_x - 24.0f;
    }
}

/* Out-of-focus highlights between the lens and the scene. Real depth of
   field needs a blur pass; this is the part of it that reads. */
void fx_draw_bokeh(void) {
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_glow);
    for (int i = 0; i < NUM_BOKEH; i++) {
        const Bokeh *b = &bokeh[i];
        float pulse = 0.75f + 0.25f * sinf(b->phase + b->x * 0.2f);
        rnd_quad(b->x, b->y, b->z, b->r * 2.0f, b->r * 2.0f,
                 rgba(b->c.r, b->c.g, b->c.b, 0.052f * pulse));
    }
    /* a couple of hard-edged rings, the way a real lens renders bright points */
    rnd_set_tex(tx_ring);
    for (int i = 0; i < NUM_BOKEH; i += 3) {
        const Bokeh *b = &bokeh[i];
        rnd_quad(b->x, b->y, b->z, b->r * 1.7f, b->r * 1.7f,
                 rgba(b->c.r, b->c.g, b->c.b, 0.045f));
    }
}

void fx_impact(float x, float y, float z, Color c, float t01) {
    float t = clampf(t01, 0.0f, 1.0f);
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_impact);
    rnd_quad(x, y, z, 2.2f + t * 2.0f, 2.2f + t * 2.0f,
             rgba(c.r, c.g, c.b, (1.0f - t) * 0.8f));
    rnd_set_tex(tx_spark);
    rnd_quad(x, y, z, 3.4f + t * 3.0f, 3.4f + t * 3.0f,
             rgba(c.r, c.g, c.b, (1.0f - t) * 0.5f));
}

void fx_muzzle(float x, float y, float z, int facing, float t01) {
    float t = clampf(t01, 0.0f, 1.0f);
    rnd_set_blend(BLEND_ADD);
    rnd_set_tex(tx_muzzle);
    rnd_quad_rot(x, y, z, 1.1f, 2.0f, facing > 0 ? -1.5707963f : 1.5707963f,
                 rgba(0.65f, 0.92f, 1.0f, (1.0f - t) * 0.9f));
    rnd_set_tex(tx_glow);
    rnd_quad(x, y, z, 5.0f, 5.0f, rgba(0.35f, 0.80f, 1.0f, (1.0f - t) * 0.35f));
}
