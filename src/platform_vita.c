/* PS Vita platform layer: vitaGL context, SceCtrl input. */
#include <vitaGL.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#include "neonfall.h"

#define GL_POOL_SIZE (8 * 1024 * 1024)

static uint32_t tick_res;

int plat_init(const char *title) {
    (void)title;
    vglInit(GL_POOL_SIZE);
    vglWaitVblankStart(GL_TRUE);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    tick_res = sceRtcGetTickResolution();
    return 1;
}

static float stick(unsigned char v) {
    float f = ((float)v - 128.0f) / 127.0f;
    return (f > -0.22f && f < 0.22f) ? 0.0f : clampf(f, -1.0f, 1.0f);
}

void plat_poll(Input *in) {
    static unsigned int prev;
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);

    float mv = stick(pad.lx);
    if (pad.buttons & SCE_CTRL_LEFT)  mv = -1.0f;
    if (pad.buttons & SCE_CTRL_RIGHT) mv =  1.0f;
    in->move = mv;
    in->down = (pad.buttons & SCE_CTRL_DOWN) != 0 || stick(pad.ly) > 0.45f;

    in->aim_x = stick(pad.rx);
    in->aim_y = stick(pad.ry);

    unsigned int b = pad.buttons;
    unsigned int pressed = b & ~prev;

    in->jump_held = (b & SCE_CTRL_CROSS)    != 0;
    in->jump      = (pressed & SCE_CTRL_CROSS)  != 0;
    in->dash      = (pressed & SCE_CTRL_CIRCLE) != 0;
    in->attack    = (pressed & SCE_CTRL_SQUARE) != 0;
    in->parry     = (pressed & SCE_CTRL_LTRIGGER) != 0;
    in->fire      = (b & SCE_CTRL_RTRIGGER) != 0;
    in->quit      = (b & SCE_CTRL_START) && (b & SCE_CTRL_SELECT);

    prev = b;
}

void plat_swap(void) { vglSwapBuffers(GL_FALSE); }

void plat_screenshot(const char *path) { (void)path; }

void plat_drawable(int *w, int *h) { *w = SCREEN_W; *h = SCREEN_H; }

double plat_time(void) {
    SceRtcTick t;
    sceRtcGetCurrentTick(&t);
    return (double)t.tick / (double)tick_res;
}

/* This vitaGL exposes no teardown entry point; the process exit releases GXM. */
void plat_shutdown(void) {
    sceKernelExitProcess(0);
}
