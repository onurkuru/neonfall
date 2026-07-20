/* Cut-out skeletal animation for the painted character parts. */
#include <math.h>
#include <stdio.h>
#include "skel.h"

/* character art resolution: painted pixels per world unit.
   Attach offsets below were authored at 620, so they scale with it. */
#define RIG_PPU 330.0f
#define RIG_SCALE (620.0f / RIG_PPU)
#define DEG (3.14159265f / 180.0f)

static Tex load_part(int index) {
    char path[256];
    snprintf(path, sizeof path, "assets/player/player-%02d.png", index);
    return tex_load(path, 0);
}

/* Part indices as they come out of tools/ingest.py. */
enum {
    P_HEAD = 0, P_TORSO = 1, P_COAT_FRONT = 2, P_COAT_BACK = 3,
    P_UPPER_B = 4, P_UPPER_F = 5, P_FORE_F = 6, P_FORE_B = 7,
    P_HAND_B = 8, P_HAND_F = 9, P_THIGH_F = 10, P_THIGH_B = 11,
    P_SHIN_F = 12, P_KATANA = 13, P_SHEATH = 14,
    P_SHIN_B = 16, P_FOOT_F = 17, P_FOOT_B = 18
};

static float part_h(Tex t) { return t.h / RIG_PPU; }

/* ax/ay are fractions of the PARENT's height, so the rig stays coherent
   whatever RIG_PPU is set to and whatever the generator handed us. */
static void set(Rig *r, int b, int parent, int part,
                float ax_units, float ay_frac, float px, float py,
                float rest, float shade) {
    Bone *bo = &r->bones[b];
    bo->parent = parent;
    bo->tex = load_part(part);
    float ph = (parent < 0) ? 1.0f : part_h(r->bones[parent].tex);
    bo->ax = ax_units;
    bo->ay = ay_frac * ph;
    bo->px = px; bo->py = py;
    bo->rest = rest * DEG;
    bo->shade = shade;
}

void skel_load_player(Rig *r) {
    for (int i = 0; i < BONE_COUNT; i++) r->rot[i] = 0.0f;
    r->facing = 1;
    r->alpha = 1.0f;

    /* Torso is the root; its pivot is the hip, so it stands up from there. */
    set(r, B_TORSO,       -1,            P_TORSO,      0.00f,  0.00f, 0.50f, 0.97f,   0, 1.00f);

    /* coats hang from the shoulders down past the hip */
    set(r, B_COAT_BACK,   B_TORSO,       P_COAT_BACK, -0.06f,  0.93f, 0.50f, 0.06f,   0, 0.78f);
    set(r, B_COAT_FRONT,  B_TORSO,       P_COAT_FRONT, 0.04f,  0.93f, 0.50f, 0.06f,   0, 1.00f);
    set(r, B_SHEATH,      B_TORSO,       P_SHEATH,    -0.14f,  0.72f, 0.50f, 0.22f, -20, 0.72f);
    set(r, B_HEAD,        B_TORSO,       P_HEAD,       0.02f,  0.95f, 0.46f, 0.88f,   0, 1.00f);

    /* far-side limbs read as shadow behind the coat */
    set(r, B_ARM_B_UPPER, B_TORSO,       P_UPPER_B,   -0.05f,  0.82f, 0.50f, 0.10f,  10, 0.58f);
    set(r, B_ARM_B_FORE,  B_ARM_B_UPPER, P_FORE_B,     0.00f, -0.86f, 0.50f, 0.10f,   6, 0.58f);
    set(r, B_ARM_B_HAND,  B_ARM_B_FORE,  P_HAND_B,     0.00f, -0.84f, 0.45f, 0.12f,   0, 0.58f);

    set(r, B_ARM_F_UPPER, B_TORSO,       P_UPPER_F,    0.09f,  0.82f, 0.50f, 0.10f, -10, 1.00f);
    set(r, B_ARM_F_FORE,  B_ARM_F_UPPER, P_FORE_F,     0.00f, -0.86f, 0.50f, 0.10f,  -6, 1.00f);
    set(r, B_ARM_F_HAND,  B_ARM_F_FORE,  P_HAND_F,     0.00f, -0.84f, 0.45f, 0.12f,   0, 1.00f);

    set(r, B_LEG_B_THIGH, B_TORSO,       P_THIGH_B,   -0.07f,  0.03f, 0.50f, 0.06f,   0, 0.58f);
    set(r, B_LEG_B_SHIN,  B_LEG_B_THIGH, P_SHIN_B,     0.00f, -0.88f, 0.50f, 0.06f,   0, 0.58f);
    set(r, B_LEG_B_FOOT,  B_LEG_B_SHIN,  P_FOOT_B,     0.00f, -0.90f, 0.32f, 0.26f,   0, 0.58f);

    set(r, B_LEG_F_THIGH, B_TORSO,       P_THIGH_F,    0.07f,  0.03f, 0.50f, 0.06f,   0, 1.00f);
    set(r, B_LEG_F_SHIN,  B_LEG_F_THIGH, P_SHIN_F,     0.00f, -0.88f, 0.50f, 0.06f,   0, 1.00f);
    set(r, B_LEG_F_FOOT,  B_LEG_F_SHIN,  P_FOOT_F,     0.00f, -0.90f, 0.32f, 0.26f,   0, 1.00f);

    /* katana rides on the back until an attack takes it off the sheath */
    set(r, B_KATANA,      B_TORSO,       P_KATANA,    -0.18f,  0.68f, 0.50f, 0.18f, -22, 0.90f);
}

/* Distance from the soles to the hip, derived from the art we actually loaded. */
float skel_hip_height(const Rig *r) {
    return part_h(r->bones[B_LEG_F_THIGH].tex) * 0.88f
         + part_h(r->bones[B_LEG_F_SHIN].tex)  * 0.90f
         + part_h(r->bones[B_LEG_F_FOOT].tex)  * 0.30f;
}

/* ---------------- poses ---------------- */

static void clear(Rig *r) {
    for (int i = 0; i < BONE_COUNT; i++) r->rot[i] = 0.0f;
}

void skel_pose_idle(Rig *r, float t) {
    clear(r);
    float breathe = sinf(t * 1.9f);
    r->rot[B_TORSO]       = breathe * 1.2f * DEG;
    r->rot[B_HEAD]        = breathe * -1.6f * DEG;
    r->rot[B_ARM_F_UPPER] = (4.0f + breathe * 2.5f) * DEG;
    r->rot[B_ARM_F_FORE]  = (10.0f + breathe * 3.0f) * DEG;
    r->rot[B_ARM_B_UPPER] = (-3.0f - breathe * 2.0f) * DEG;
    r->rot[B_ARM_B_FORE]  = (12.0f + breathe * 2.0f) * DEG;
    r->rot[B_COAT_BACK]   = (2.0f + breathe * 2.2f) * DEG;
    r->rot[B_COAT_FRONT]  = (-1.0f - breathe * 1.4f) * DEG;
}

void skel_pose_run(Rig *r, float phase, float speed) {
    clear(r);
    float s = clampf(speed, 0.35f, 1.25f);
    float a = sinf(phase), b = cosf(phase);

    r->rot[B_TORSO] = (-8.0f - 3.0f * s) * DEG;
    r->rot[B_HEAD]  = (6.0f + a * 2.0f) * DEG;

    /* thighs drive, shins lag a quarter cycle - that lag is the whole illusion */
    r->rot[B_LEG_F_THIGH] = ( 40.0f * a) * s * DEG;
    r->rot[B_LEG_F_SHIN]  = (-38.0f + 34.0f * sinf(phase - 1.5f)) * s * DEG;
    r->rot[B_LEG_F_FOOT]  = ( 12.0f * sinf(phase - 2.0f)) * s * DEG;

    r->rot[B_LEG_B_THIGH] = (-40.0f * a) * s * DEG;
    r->rot[B_LEG_B_SHIN]  = (-38.0f - 34.0f * sinf(phase - 1.5f)) * s * DEG;
    r->rot[B_LEG_B_FOOT]  = (-12.0f * sinf(phase - 2.0f)) * s * DEG;

    /* arms counter-swing */
    r->rot[B_ARM_F_UPPER] = (-46.0f * a) * s * DEG;
    r->rot[B_ARM_F_FORE]  = ( 34.0f + 22.0f * b) * s * DEG;
    r->rot[B_ARM_B_UPPER] = ( 46.0f * a) * s * DEG;
    r->rot[B_ARM_B_FORE]  = ( 34.0f - 22.0f * b) * s * DEG;

    /* the coat trails the run */
    r->rot[B_COAT_BACK]  = (16.0f + 9.0f * b) * s * DEG;
    r->rot[B_COAT_FRONT] = (-7.0f - 4.0f * b) * s * DEG;
    r->rot[B_SHEATH]     = (6.0f * b) * s * DEG;
}

void skel_pose_air(Rig *r, float vy) {
    clear(r);
    float rising = clampf(vy / 18.0f, -1.0f, 1.0f);

    r->rot[B_TORSO] = (-6.0f + rising * 5.0f) * DEG;
    r->rot[B_HEAD]  = (4.0f - rising * 4.0f) * DEG;

    /* tuck on the way up, reach for the ground on the way down */
    r->rot[B_LEG_F_THIGH] = (34.0f + rising * 22.0f) * DEG;
    r->rot[B_LEG_F_SHIN]  = (-58.0f - rising * 26.0f) * DEG;
    r->rot[B_LEG_F_FOOT]  = 12.0f * DEG;
    r->rot[B_LEG_B_THIGH] = (-16.0f + rising * 10.0f) * DEG;
    r->rot[B_LEG_B_SHIN]  = (-44.0f - rising * 20.0f) * DEG;
    r->rot[B_LEG_B_FOOT]  = 8.0f * DEG;

    r->rot[B_ARM_F_UPPER] = (-58.0f - rising * 26.0f) * DEG;
    r->rot[B_ARM_F_FORE]  = (26.0f) * DEG;
    r->rot[B_ARM_B_UPPER] = (36.0f + rising * 18.0f) * DEG;
    r->rot[B_ARM_B_FORE]  = (30.0f) * DEG;

    r->rot[B_COAT_BACK]  = (30.0f + rising * 14.0f) * DEG;
    r->rot[B_COAT_FRONT] = (-12.0f - rising * 6.0f) * DEG;
}

void skel_pose_attack(Rig *r, float t01) {
    clear(r);
    /* wind up fast, follow through slow */
    float t = clampf(t01, 0.0f, 1.0f);
    float swing = t < 0.25f ? (t / 0.25f) * -1.0f          /* raise behind */
                            : lerpf(-1.0f, 1.0f, (t - 0.25f) / 0.75f);

    r->rot[B_TORSO] = (swing * 14.0f) * DEG;
    r->rot[B_HEAD]  = (-swing * 8.0f) * DEG;

    r->rot[B_ARM_F_UPPER] = (-40.0f + swing * 105.0f) * DEG;
    r->rot[B_ARM_F_FORE]  = (55.0f - swing * 45.0f) * DEG;
    r->rot[B_ARM_B_UPPER] = (18.0f - swing * 30.0f) * DEG;
    r->rot[B_ARM_B_FORE]  = (34.0f) * DEG;

    r->rot[B_LEG_F_THIGH] = (10.0f + swing * 12.0f) * DEG;
    r->rot[B_LEG_F_SHIN]  = (-14.0f) * DEG;
    r->rot[B_LEG_B_THIGH] = (-14.0f - swing * 8.0f) * DEG;
    r->rot[B_LEG_B_SHIN]  = (-18.0f) * DEG;

    r->rot[B_COAT_BACK]  = (18.0f + swing * 16.0f) * DEG;
    r->rot[B_COAT_FRONT] = (-8.0f - swing * 10.0f) * DEG;
}

void skel_pose_dash(Rig *r) {
    clear(r);
    r->rot[B_TORSO]       = -26.0f * DEG;
    r->rot[B_HEAD]        =  20.0f * DEG;
    r->rot[B_LEG_F_THIGH] =  48.0f * DEG;
    r->rot[B_LEG_F_SHIN]  = -30.0f * DEG;
    r->rot[B_LEG_B_THIGH] = -42.0f * DEG;
    r->rot[B_LEG_B_SHIN]  = -52.0f * DEG;
    r->rot[B_ARM_F_UPPER] = -78.0f * DEG;
    r->rot[B_ARM_F_FORE]  =  20.0f * DEG;
    r->rot[B_ARM_B_UPPER] =  62.0f * DEG;
    r->rot[B_ARM_B_FORE]  =  40.0f * DEG;
    r->rot[B_COAT_BACK]   =  54.0f * DEG;
    r->rot[B_COAT_FRONT]  = -18.0f * DEG;
}

/* ---------------- draw ---------------- */

void skel_draw(const Rig *r, float z) {
    float wx[BONE_COUNT], wy[BONE_COUNT], wr[BONE_COUNT];
    float f = (float)r->facing;

    rnd_set_blend(BLEND_ALPHA);

    for (int i = 0; i < BONE_COUNT; i++) {
        const Bone *b = &r->bones[i];
        float rot = b->rest + r->rot[i];
        if (b->parent < 0) {
            wr[i] = rot * f;
            wx[i] = r->root_x + b->ax * f;
            wy[i] = r->root_y + b->ay;
        } else {
            int p = b->parent;
            float pr = wr[p];
            float ax = b->ax * f, ay = b->ay;
            /* the attach point rides the parent's rotation */
            wx[i] = wx[p] + ax * cosf(pr) - ay * sinf(pr);
            wy[i] = wy[p] + ax * sinf(pr) + ay * cosf(pr);
            wr[i] = pr + rot * f;
        }
    }

    for (int i = 0; i < BONE_COUNT; i++) {
        const Bone *b = &r->bones[i];
        if (!b->tex.id) continue;
        float w = b->tex.w / RIG_PPU, h = b->tex.h / RIG_PPU;
        float s = b->shade;
        rnd_set_tex(b->tex);
        rnd_sprite(wx[i], wy[i], z + i * 0.004f, w, h,
                   b->px, b->py, wr[i], r->facing < 0,
                   rgba(s, s, s, r->alpha));
    }
}
