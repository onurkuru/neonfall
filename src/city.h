/* Ambient city life: traffic and crowds. Purely decorative. */
#ifndef NEONFALL_CITY_H
#define NEONFALL_CITY_H

#include "neonfall.h"

void city_init(const char *asset_root);
void city_update(float dt, float cam_x);
void city_draw_traffic(void);   /* between the far layers */
void city_draw_crowd(void);     /* behind the play plane */

#endif
