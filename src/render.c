/* Renderer: perspective camera over a 2D gameplay plane, procedural textures.
   Depth comes from real Z placement, so parallax is a property of the camera
   rather than a per-layer scroll hack. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl.h"
#include "neonfall.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#define CAM_DIST  CAM_Z

void batch_push(float x0, float y0, float x1, float y1,
                float x2, float y2, float x3, float y3,
                float z, float u0, float v0, float u1, float v1, Color c);

static float cam_x, cam_y;
static int   ui_mode;

uint32_t nf_rand(uint32_t *s) {
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return *s;
}
float nf_randf(uint32_t *s) { return (nf_rand(s) >> 8) / 16777216.0f; }

/* ---------------- textures ---------------- */

Tex tex_from_rgba(const unsigned char *px, int w, int h) {
    Tex t;
    t.w = w; t.h = h;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return t;
}

/* Pixel art: nearest filtering, no smoothing between texels. */
Tex tex_load(const char *path, int tile) {
    Tex t = { 0, 0, 0 };
    int w, h, comp;
    unsigned char *px = stbi_load(path, &w, &h, &comp, 4);
    if (!px) {
        fprintf(stderr, "missing asset: %s\n", path);
        return t;
    }
    t.w = w; t.h = h;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLint wrap = tile ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    return t;
}

void tex_set_repeat(Tex t) {
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

/* Radial falloff sprite - the workhorse behind every neon halo. */
Tex tex_glow(int size, float falloff) {
    unsigned char *px = (unsigned char *)malloc(size * size * 4);
    float c = (size - 1) * 0.5f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - c) / c, dy = (y - c) / c;
            float d = sqrtf(dx * dx + dy * dy);
            float a = d >= 1.0f ? 0.0f : powf(1.0f - d, falloff);
            unsigned char *p = px + (y * size + x) * 4;
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(clampf(a, 0.0f, 1.0f) * 255.0f);
        }
    }
    Tex t = tex_from_rgba(px, size, size);
    free(px);
    return t;
}

/* Soft-edged rectangle: neon tubes, light shafts, ground reflections. */
Tex tex_soft_bar(int w, int h) {
    unsigned char *px = (unsigned char *)malloc(w * h * 4);
    for (int y = 0; y < h; y++) {
        float fy = fabsf((y + 0.5f) / h * 2.0f - 1.0f);
        float ay = powf(1.0f - fy, 1.6f);
        for (int x = 0; x < w; x++) {
            float fx = fabsf((x + 0.5f) / w * 2.0f - 1.0f);
            float ax = powf(1.0f - fx, 0.8f);
            unsigned char *p = px + (y * w + x) * 4;
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(clampf(ax * ay, 0.0f, 1.0f) * 255.0f);
        }
    }
    Tex t = tex_from_rgba(px, w, h);
    free(px);
    return t;
}

Tex tex_blob(int size) { return tex_glow(size, 2.4f); }

/* Rain streak: bright core fading along its length. */
Tex tex_streak(int w, int h) {
    unsigned char *px = (unsigned char *)malloc(w * h * 4);
    for (int y = 0; y < h; y++) {
        float t = (y + 0.5f) / h;                 /* 0 = head, 1 = tail */
        float along = powf(1.0f - t, 1.3f);
        for (int x = 0; x < w; x++) {
            float fx = fabsf((x + 0.5f) / w * 2.0f - 1.0f);
            float across = powf(1.0f - fx, 1.2f);
            unsigned char *p = px + (y * w + x) * 4;
            p[0] = 220; p[1] = 236; p[2] = 255;
            p[3] = (unsigned char)(clampf(along * across, 0.0f, 1.0f) * 255.0f);
        }
    }
    Tex t = tex_from_rgba(px, w, h);
    free(px);
    return t;
}

/* Building facade: dark slab, grid of windows, a few lit warm or cyan. */
Tex tex_facade(int w, int h, uint32_t seed, int lit_ratio) {
    unsigned char *px = (unsigned char *)malloc(w * h * 4);
    uint32_t s = seed ? seed : 1u;

    for (int i = 0; i < w * h; i++) {
        unsigned char *p = px + i * 4;
        p[0] = 10; p[1] = 13; p[2] = 32; p[3] = 255;
    }
    int cw = 5, ch = 7, gx = 4, gy = 6;
    int cols = (w - gx) / (cw + gx);
    int rows = (h - gy) / (ch + gy);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int lit = (int)(nf_randf(&s) * 100.0f) < lit_ratio;
            if (!lit) continue;
            float warm = nf_randf(&s);
            unsigned char cr, cg, cb;
            if (warm < 0.62f)      { cr = 255; cg = 186; cb = 96;  }
            else if (warm < 0.88f) { cr = 96;  cg = 220; cb = 255; }
            else                   { cr = 255; cg = 110; cb = 200; }
            float b = 0.35f + nf_randf(&s) * 0.65f;
            int x0 = gx + c * (cw + gx), y0 = gy + r * (ch + gy);
            for (int y = y0; y < y0 + ch && y < h; y++) {
                for (int x = x0; x < x0 + cw && x < w; x++) {
                    unsigned char *p = px + (y * w + x) * 4;
                    p[0] = (unsigned char)(cr * b);
                    p[1] = (unsigned char)(cg * b);
                    p[2] = (unsigned char)(cb * b);
                }
            }
        }
    }
    /* vertical grime so flat slabs are not perfectly even (tileable) */
    for (int x = 0; x < w; x++) {
        float g = 0.86f + 0.14f * sinf((float)x / (float)w * 6.2831853f * 3.0f
                                       + (float)(seed & 255) * 0.02f);
        for (int y = 0; y < h; y++) {
            unsigned char *p = px + (y * w + x) * 4;
            p[0] = (unsigned char)(p[0] * g);
            p[1] = (unsigned char)(p[1] * g);
            p[2] = (unsigned char)(p[2] * g);
        }
    }
    Tex t = tex_from_rgba(px, w, h);
    free(px);
    return t;
}

/* Distant skyline silhouette with a soft glow bleeding off the rooftops. */
Tex tex_skyline(int w, int h, uint32_t seed) {
    unsigned char *px = (unsigned char *)calloc(w * h * 4, 1);
    uint32_t s = seed ? seed : 3u;
    int *top = (int *)malloc(w * sizeof(int));

    int x = 0;
    while (x < w) {
        int bw = 14 + (int)(nf_randf(&s) * 46.0f);
        int bh = (int)(h * (0.28f + nf_randf(&s) * 0.55f));
        for (int i = 0; i < bw && x + i < w; i++) top[x + i] = h - bh;
        x += bw;
    }
    for (int px_x = 0; px_x < w; px_x++) {
        for (int y = 0; y < h; y++) {
            unsigned char *p = px + (y * w + px_x) * 4;
            if (y >= top[px_x]) {
                float depth = (float)(y - top[px_x]) / (float)h;
                p[0] = (unsigned char)(12 + depth * 6);
                p[1] = (unsigned char)(16 + depth * 8);
                p[2] = (unsigned char)(44 + depth * 14);
                p[3] = 255;
                /* sparse window pinpricks */
                if (nf_randf(&s) < 0.004f) {
                    p[0] = 200; p[1] = 150; p[2] = 90;
                }
            } else {
                int d = top[px_x] - y;
                float a = d < 26 ? (1.0f - d / 26.0f) * 0.5f : 0.0f;
                p[0] = 90; p[1] = 60; p[2] = 130;
                p[3] = (unsigned char)(a * 255.0f);
            }
        }
    }
    free(top);
    Tex t = tex_from_rgba(px, w, h);
    free(px);
    return t;
}

/* ---------------- camera & frame ---------------- */

static void perspective(float fovy_deg, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy_deg * 0.5f * 3.14159265f / 180.0f);
    float m[16] = {0};
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m);
}

/* Fit the 960x544 game frame inside whatever the window actually is, keeping
   its aspect and bordering the rest, so nothing stretches or falls off. */
void rnd_resize(int win_w, int win_h) {
    if (win_w <= 0 || win_h <= 0) return;
    float want = (float)SCREEN_W / (float)SCREEN_H;
    float have = (float)win_w / (float)win_h;
    int vw = win_w, vh = win_h, vx = 0, vy = 0;
    if (have > want) {
        vw = (int)(win_h * want + 0.5f);
        vx = (win_w - vw) / 2;
    } else {
        vh = (int)(win_w / want + 0.5f);
        vy = (win_h - vh) / 2;
    }
    glViewport(vx, vy, vw, vh);
}

int rnd_init(void) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    int dw = SCREEN_W, dh = SCREEN_H;
    plat_drawable(&dw, &dh);
    rnd_resize(dw, dh);
    return 1;
}

void rnd_camera(float cx, float cy, float shake) {
    rnd_flush();
    cam_x = cx + shake;
    cam_y = cy + shake * 0.6f;
    ui_mode = 0;
    perspective(FOV_Y, (float)SCREEN_W / (float)SCREEN_H, 1.0f, 600.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-cam_x, -cam_y, -CAM_DIST);
}

void rnd_begin_frame(void) {
    glClearColor(0.016f, 0.020f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void rnd_end_frame(void) { rnd_flush(); }

/* ---------------- quads ---------------- */

void rnd_quad_uv(float x, float y, float z, float w, float h,
                 float u0, float v0, float u1, float v1, Color c) {
    float hw = w * 0.5f, hh = h * 0.5f;
    if (ui_mode) {
        /* screen space: y grows downward, origin top-left */
        float x0 = x - hw, x1 = x + hw, y0 = y - hh, y1 = y + hh;
        batch_push(x0, y1, x1, y1, x1, y0, x0, y0, 0.0f, u0, v0, u1, v1, c);
        return;
    }
    batch_push(x - hw, y - hh, x + hw, y - hh, x + hw, y + hh, x - hw, y + hh,
               z, u0, v0, u1, v1, c);
}

void rnd_quad(float x, float y, float z, float w, float h, Color c) {
    rnd_quad_uv(x, y, z, w, h, 0.0f, 0.0f, 1.0f, 1.0f, c);
}

void rnd_quad_rot(float x, float y, float z, float w, float h, float rot, Color c) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float s = sinf(rot), co = cosf(rot);
    float cx[4] = { -hw,  hw,  hw, -hw };
    float cy[4] = { -hh, -hh,  hh,  hh };
    float px[4], py[4];
    for (int i = 0; i < 4; i++) {
        px[i] = x + cx[i] * co - cy[i] * s;
        py[i] = y + cx[i] * s + cy[i] * co;
    }
    batch_push(px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3],
               z, 0.0f, 0.0f, 1.0f, 1.0f, c);
}

/* Quad rotated around an arbitrary pivot given in 0..1 texture space, with
   optional horizontal flip. This is what the bone hierarchy draws through. */
void rnd_sprite(float x, float y, float z, float w, float h,
                float pivot_x, float pivot_y, float rot, int flip, Color c) {
    float lx = -pivot_x * w, rx = (1.0f - pivot_x) * w;
    /* pivot_y is measured from the top of the texture */
    float ty = pivot_y * h, by = ty - h;
    float cx[4] = { lx, rx, rx, lx };
    float cy[4] = { by, by, ty, ty };
    if (flip) {
        cx[0] = -lx; cx[1] = -rx; cx[2] = -rx; cx[3] = -lx;
    }
    float s = sinf(rot), co = cosf(rot);
    float px[4], py[4];
    for (int i = 0; i < 4; i++) {
        px[i] = x + cx[i] * co - cy[i] * s;
        py[i] = y + cx[i] * s + cy[i] * co;
    }
    batch_push(px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3],
               z, 0.0f, 0.0f, 1.0f, 1.0f, c);
}

void rnd_ui_begin(void) {
    rnd_flush();
    ui_mode = 1;
    float m[16] = {0};
    /* orthographic, pixel coordinates, origin top-left */
    m[0]  =  2.0f / SCREEN_W;
    m[5]  = -2.0f / SCREEN_H;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] =  1.0f;
    m[15] =  1.0f;
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void rnd_ui_quad(float x, float y, float w, float h, Color c) {
    rnd_quad_uv(x + w * 0.5f, y + h * 0.5f, 0.0f, w, h, 0.0f, 0.0f, 1.0f, 1.0f, c);
}

void rnd_ui_end(void) {
    rnd_flush();
    ui_mode = 0;
}
