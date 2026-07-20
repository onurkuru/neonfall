/* Neonfall entry point: fixed-timestep update, uncapped draw. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "neonfall.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    const char *shot = getenv("NEONFALL_SHOT");
    const char *shot_at = getenv("NEONFALL_SHOT_FRAME");
    long shot_frame = shot_at ? atol(shot_at) : 120;
    long frame_no = 0;
    int demo = getenv("NEONFALL_DEMO") != NULL;

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
        if (demo) {
            /* scripted run-and-jump so screenshots exercise the animations */
            long t = frame_no % 240;
            in.move = (t < 200) ? 1.0f : -1.0f;
            in.jump_held = (t % 80) > 40 && (t % 80) < 70;
            in.jump = (t % 80) == 41 || (t % 80) == 58;
            in.dash = (t % 240) == 150;
            in.attack = (t % 45) == 0;
        }

        while (acc >= TICK_DT) {
            game_update(&in, TICK_DT);
            acc -= TICK_DT;
            /* edge-triggered inputs are consumed by the first tick */
            in.jump = in.dash = in.attack = in.parry = 0;
        }

        extern int nf_draw_calls, nf_verts_peak;
        nf_draw_calls = 0;
        rnd_begin_frame();
        game_draw();
        rnd_end_frame();

        if (getenv("NEONFALL_STATS") && (frame_no % 60) == 0)
            fprintf(stderr, "frame %ld: %d draw calls, peak %d verts/batch\n",
                    frame_no, nf_draw_calls, nf_verts_peak);
        if (shot && ++frame_no >= shot_frame) {
            plat_screenshot(shot);
            in.quit = 1;
        }
        plat_swap();
    }

    plat_shutdown();
    return 0;
}
