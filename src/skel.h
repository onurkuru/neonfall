/* Cut-out skeletal animation: painted body parts driven by a bone hierarchy.
   Poses are generated procedurally rather than authored frame by frame. */
#ifndef NEONFALL_SKEL_H
#define NEONFALL_SKEL_H

#include "neonfall.h"

enum {
    B_COAT_BACK, B_ARM_B_UPPER, B_ARM_B_FORE, B_ARM_B_HAND,
    B_LEG_B_THIGH, B_LEG_B_SHIN, B_LEG_B_FOOT,
    B_SHEATH, B_KATANA, B_TORSO,
    B_LEG_F_THIGH, B_LEG_F_SHIN, B_LEG_F_FOOT,
    B_COAT_FRONT, B_HEAD,
    B_ARM_F_UPPER, B_ARM_F_FORE, B_ARM_F_HAND,
    BONE_COUNT
};

typedef struct {
    int   parent;        /* index of parent bone, or -1 for the root */
    Tex   tex;
    float ax, ay;        /* attach point in the parent's local space, world units */
    float px, py;        /* pivot inside this part, 0..1 of the texture */
    float rest;          /* rest rotation, radians */
    float shade;         /* 1.0 = lit, lower for limbs on the far side */
} Bone;

typedef struct {
    Bone  bones[BONE_COUNT];
    float rot[BONE_COUNT];    /* animated rotation, added to rest */
    float root_x, root_y;     /* hip position in world space */
    int   facing;
    float alpha;
    Color tint;               /* multiplied into every part */
} Rig;

/* Load the player rig from assets/player/player-NN.png. */
void skel_load_player(Rig *r);
float skel_hip_height(const Rig *r);

/* Pose generators - each writes rot[] for the whole rig. */
void skel_pose_idle(Rig *r, float t);
void skel_pose_run(Rig *r, float phase, float speed);
void skel_pose_air(Rig *r, float vy);
void skel_pose_attack(Rig *r, float t01);
void skel_pose_dash(Rig *r);

void skel_draw(const Rig *r, float z);
/* Silhouette pass: flat dark body plus a rim of light along the lit edge. */
void skel_draw_silhouette(const Rig *r, float z, Color body, Color rim, float rim_width);

#endif
