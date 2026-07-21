/* The street: platforms, props, enemies and everything shooting at you. */
#ifndef NEONFALL_WORLD_H
#define NEONFALL_WORLD_H

#include "neonfall.h"

void world_init(const char *asset_root);
void world_update(float dt, float px, float py, int p_facing);
void world_draw_back(void);    /* props behind the play plane */
/* Multiplied into every prop, platform and enemy - used by the reflection pass. */
void world_set_tint(Color c);
void world_draw_front(void);   /* enemies, bullets, foreground props */

/* Landing test: given the player's vertical span this tick, report the surface
   it should stand on. Returns 1 when there is one. */
int  world_land(float x, float y_from, float y_to, float *out_y);
/* Is there a solid surface under x at (or just below) y? Keeps enemies honest. */
float world_surface_under(float x, float y);

void world_player_shoot(float x, float y, int facing);
/* Damage dealt to the player this tick, and where it came from. */
int  world_take_player_damage(float px, float py);

int  world_enemies_alive(void);
/* Street-level segments, so the road is drawn from the same data it is
   collided against and a hole you fall into is a hole you can see. */
int  world_ground_segments(const float **out);   /* triples: x, y, width */
float world_left_bound(void);
float world_right_bound(void);

#endif
