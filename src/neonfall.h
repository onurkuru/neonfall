/* Neonfall - shared declarations. */
#ifndef NEONFALL_H
#define NEONFALL_H

#include <stdint.h>

#define SCREEN_W 960
#define SCREEN_H 544
#define TICK_RATE 60.0f
#define TICK_DT (1.0f / TICK_RATE)

/* camera: 40 degree vertical FOV, VIEW_H world units tall at the play plane */
#define FOV_Y   40.0f
#define VIEW_H  18.0f
#define CAM_Z   (VIEW_H * 0.5f / 0.363970f)

/* ---------- platform layer ---------- */

typedef struct {
    float move;        /* -1..1 horizontal */
    float aim_x, aim_y;
    int jump, jump_held;
    int down;          /* crouch / take cover */
    int dash;
    int attack;
    int fire;
    int parry;
    int quit;
} Input;

int  plat_init(const char *title);
void plat_poll(Input *in);
void plat_swap(void);
void plat_shutdown(void);
double plat_time(void);   /* seconds */
void plat_screenshot(const char *path);
/* Size of the actual GL drawable, which is not the game's 960x544 when the
   window is scaled, resized or on a high-DPI display. */
void plat_drawable(int *w, int *h);

/* ---------- textures ---------- */

typedef struct {
    unsigned int id;
    int w, h;
} Tex;

Tex tex_from_rgba(const unsigned char *px, int w, int h);
Tex tex_load(const char *path, int tile);   /* PNG, nearest filtered */
void tex_set_repeat(Tex t);

/* pixel art scale: texture pixels per world unit */
#define PPU 16.0f
Tex tex_glow(int size, float falloff);
Tex tex_soft_bar(int w, int h);
Tex tex_facade(int w, int h, uint32_t seed, int lit_ratio);
Tex tex_skyline(int w, int h, uint32_t seed);
Tex tex_streak(int w, int h);
Tex tex_blob(int size);

/* ---------- renderer ---------- */

typedef struct { float r, g, b, a; } Color;

int  rnd_init(void);
void rnd_resize(int win_w, int win_h);
void rnd_begin_frame(void);
void rnd_end_frame(void);

/* camera lives on the gameplay plane; z grows toward the viewer */
void rnd_camera(float cx, float cy, float shake);

typedef enum { BLEND_ALPHA, BLEND_ADD } BlendMode;

void rnd_set_tex(Tex t);
void rnd_set_blend(BlendMode m);
/* Axis-aligned quad centred at (x,y) on depth plane z. */
void rnd_quad(float x, float y, float z, float w, float h, Color c);
void rnd_quad_uv(float x, float y, float z, float w, float h,
                 float u0, float v0, float u1, float v1, Color c);
/* Quad rotated around its centre (radians). */
void rnd_quad_rot(float x, float y, float z, float w, float h, float rot, Color c);
/* Quad rotated around a pivot in 0..1 texture space, optionally flipped. */
void rnd_sprite(float x, float y, float z, float w, float h,
                float pivot_x, float pivot_y, float rot, int flip, Color c);
/* Screen-space quad, pixel coordinates, origin top-left. */
void rnd_ui_begin(void);
void rnd_ui_quad(float x, float y, float w, float h, Color c);
void rnd_ui_end(void);
void rnd_flush(void);
/* Reflect every subsequent draw about a horizontal world line (wet ground).
   `squash` compresses the reflection toward the line so it stays inside the
   surface it is supposed to be lying on. */
void rnd_set_mirror(int on, float y_line, float squash);

/* ---------- game ---------- */

void game_init(void);
void game_update(const Input *in, float dt);
void game_draw(void);

/* ---------- helpers ---------- */

static inline Color rgba(float r, float g, float b, float a) {
    Color c = { r, g, b, a };
    return c;
}
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

uint32_t nf_rand(uint32_t *state);
float    nf_randf(uint32_t *state);

#endif
