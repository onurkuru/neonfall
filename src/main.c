/* Neonfall entry point: fixed-timestep update, uncapped draw. */
#include <stdlib.h>
#include <string.h>
#include "neonfall.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    const char *shot = getenv("NEONFALL_SHOT");
    const char *shot_at = getenv("NEONFALL_SHOT_FRAME");
    long shot_frame = shot_at ? atol(shot_at) : 120;
    long frame_no = 0;

    if (!plat_init("Neonfall")) return 1;
    rnd_init();
    game_init();

    Input in;
    memset(&in, 0, sizeof in);

    double prev = plat_time();
    float  acc = 0.0f;

    while (!in.quit) {
        double now = plat_time();
        float frame = (float)(now - prev);
        prev = now;
        if (frame > 0.25f) frame = 0.25f;
        acc += frame;

        plat_poll(&in);

        while (acc >= TICK_DT) {
            game_update(&in, TICK_DT);
            acc -= TICK_DT;
            /* edge-triggered inputs are consumed by the first tick */
            in.jump = in.dash = in.attack = in.parry = 0;
        }

        rnd_begin_frame();
        game_draw();
        rnd_end_frame();

        if (shot && ++frame_no >= shot_frame) {
            plat_screenshot(shot);
            in.quit = 1;
        }
        plat_swap();
    }

    plat_shutdown();
    return 0;
}
