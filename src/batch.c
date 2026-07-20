/* Interleaved vertex batching. One draw call per texture/blend run.
   Uses client arrays so the same path works on GL 2.1 and vitaGL. */
#include <string.h>
#include "gl.h"
#include "neonfall.h"

#define MAX_VERTS 12288

typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} Vtx;

static Vtx     verts[MAX_VERTS];
static int     count;
static GLuint  cur_tex;
static BlendMode cur_blend = BLEND_ALPHA;
static int     arrays_ready;
static int     mirror_on;
static float   mirror_y;
static float   mirror_squash = 1.0f;

static void apply_blend(BlendMode m) {
    if (m == BLEND_ADD) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void rnd_flush(void) {
    if (!count) return;
    if (!arrays_ready) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        arrays_ready = 1;
    }
    glBindTexture(GL_TEXTURE_2D, cur_tex);
    apply_blend(cur_blend);
    glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &verts[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), &verts[0].u);
    glColorPointer(4, GL_FLOAT, sizeof(Vtx), &verts[0].r);
    glDrawArrays(GL_TRIANGLES, 0, count);
    count = 0;
}

void rnd_set_mirror(int on, float y_line, float squash) {
    rnd_flush();
    mirror_on = on;
    mirror_y = y_line;
    mirror_squash = squash;
}

void rnd_set_tex(Tex t) {
    if (t.id != cur_tex) {
        rnd_flush();
        cur_tex = t.id;
    }
}

void rnd_set_blend(BlendMode m) {
    if (m != cur_blend) {
        rnd_flush();
        cur_blend = m;
    }
}

/* Push one quad given its four corners in world space (CCW from bottom-left). */
void batch_push(float x0, float y0, float x1, float y1,
                float x2, float y2, float x3, float y3,
                float z, float u0, float v0, float u1, float v1, Color c) {
    if (count + 6 > MAX_VERTS) rnd_flush();

    const float px[6] = { x0, x1, x2, x0, x2, x3 };
    const float py[6] = { y0, y1, y2, y0, y2, y3 };
    const float pu[6] = { u0, u1, u1, u0, u1, u0 };
    const float pv[6] = { v1, v1, v0, v1, v0, v0 };

    for (int i = 0; i < 6; i++) {
        Vtx *v = &verts[count++];
        v->x = px[i];
        v->y = mirror_on ? (mirror_y - (py[i] - mirror_y) * mirror_squash)
                         : py[i];
        v->z = z;
        v->u = pu[i]; v->v = pv[i];
        v->r = c.r; v->g = c.g; v->b = c.b; v->a = c.a;
    }
}
