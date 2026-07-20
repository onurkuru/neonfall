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

/* A neon source: halo, hot core, and optional lens streak.
   Lights are queued rather than drawn, then emitted texture by texture, so a
   scene full of neon costs a handful of draw calls instead of four per light.
   Additive blending is order-independent, so batching is free; call
   fx_flush_lights() at each point in the painter's order where the lights
   queued so far must land before whatever is drawn next. */
void fx_light(const Light *l, float t);
void fx_flush_lights(void);
/* The pool of light a source throws onto the wet ground below it. */
void fx_light_on_ground(const Light *l, float t, float ground_y, float z);
void fx_flush_pools(float t);

void fx_impact(float x, float y, float z, Color c, float t01);
void fx_muzzle(float x, float y, float z, int facing, float t01);

#endif
