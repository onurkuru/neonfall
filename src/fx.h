/* Weather, light and particle layer.
   Sprites are Kenney's Particle Pack (CC0). */
#ifndef NEONFALL_FX_H
#define NEONFALL_FX_H

#include "neonfall.h"

typedef struct {
    float x, y, z;
    Color color;
    float radius;      /* halo size in world units */
    float freq, phase; /* flicker */
    int   flare;       /* draw an anamorphic streak through the source */
} Light;

void fx_init(const char *asset_root);

/* Rain, splashes and drifting steam follow the camera. */
void fx_update(float dt, float cam_x, float cam_y, float ground_y);
void fx_draw_rain_far(void);
void fx_draw_rain_near(void);
void fx_draw_steam(void);
void fx_draw_splashes(void);

/* A neon source: halo, hot core, and optional lens streak. */
void fx_light(const Light *l, float t);
/* The pool of light a source throws onto the wet ground below it. */
void fx_light_on_ground(const Light *l, float t, float ground_y, float z);

void fx_impact(float x, float y, float z, Color c, float t01);
void fx_muzzle(float x, float y, float z, int facing, float t01);

#endif
