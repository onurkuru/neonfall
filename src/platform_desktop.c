/* Desktop platform layer: SDL2 window, GL 2.1 compatibility context. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "gl.h"
#include "neonfall.h"

static SDL_Window *win;
static SDL_GLContext ctx;

int plat_init(const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    int scale = 1;
    const char *s = SDL_getenv("NEONFALL_SCALE");
    if (s) scale = SDL_atoi(s);
    if (scale < 1 || scale > 4) scale = 1;

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
                   SDL_WINDOW_RESIZABLE;
    if (SDL_getenv("NEONFALL_FULLSCREEN")) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           SCREEN_W * scale, SCREEN_H * scale, flags);
    if (!win) {
        fprintf(stderr, "CreateWindow: %s\n", SDL_GetError());
        return 0;
    }
    ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "GL context: %s\n", SDL_GetError());
        return 0;
    }
    SDL_GL_SetSwapInterval(1);
    SDL_GameControllerOpen(0);
    return 1;
}

static float axis(SDL_GameController *c, SDL_GameControllerAxis a) {
    if (!c) return 0.0f;
    float v = SDL_GameControllerGetAxis(c, a) / 32767.0f;
    return (v > -0.22f && v < 0.22f) ? 0.0f : v;
}

void plat_poll(Input *in) {
    SDL_Event ev;
    static int prev_jump, prev_dash, prev_attack, prev_parry;

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) in->quit = 1;
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) in->quit = 1;
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_f) {
            Uint32 f = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(win, f ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        if (ev.type == SDL_WINDOWEVENT &&
            (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             ev.window.event == SDL_WINDOWEVENT_RESIZED)) {
            int w, h;
            SDL_GL_GetDrawableSize(win, &w, &h);
            rnd_resize(w, h);
        }
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    SDL_GameController *pad = SDL_GameControllerFromInstanceID(0);

    float mv = 0.0f;
    if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) mv -= 1.0f;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) mv += 1.0f;
    if (mv == 0.0f) mv = axis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    in->move = clampf(mv, -1.0f, 1.0f);

    in->down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
    if (pad) {
        in->down |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        if (axis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 0.45f) in->down = 1;
    }

    in->aim_x = axis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    in->aim_y = axis(pad, SDL_CONTROLLER_AXIS_RIGHTY);

    int jump   = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_Z];
    int dash   = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_C];
    int attack = k[SDL_SCANCODE_X] || k[SDL_SCANCODE_J];
    int parry  = k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_K];

    if (pad) {
        jump   |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        dash   |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
        attack |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X);
        parry  |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        in->fire = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    } else {
        in->fire = k[SDL_SCANCODE_L] || k[SDL_SCANCODE_V];
    }

    in->jump_held = jump;
    in->jump   = jump   && !prev_jump;
    in->dash   = dash   && !prev_dash;
    in->attack = attack && !prev_attack;
    in->parry  = parry  && !prev_parry;
    prev_jump = jump; prev_dash = dash; prev_attack = attack; prev_parry = parry;
}

void plat_drawable(int *w, int *h) { SDL_GL_GetDrawableSize(win, w, h); }

void plat_swap(void) { SDL_GL_SwapWindow(win); }

double plat_time(void) {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

/* Grab the framebuffer for visual regression checks. */
void plat_screenshot(const char *path) {
    int w, h;
    SDL_GL_GetDrawableSize(win, &w, &h);
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
    if (!px) return;
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);

    /* glReadPixels is bottom-up; SDL surfaces are top-down */
    size_t stride = (size_t)w * 4;
    unsigned char *row = (unsigned char *)malloc(stride);
    for (int y = 0; y < h / 2; y++) {
        memcpy(row, px + y * stride, stride);
        memcpy(px + y * stride, px + (h - 1 - y) * stride, stride);
        memcpy(px + (h - 1 - y) * stride, row, stride);
    }
    free(row);

    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(px, w, h, 32, (int)stride,
                                              0x000000ff, 0x0000ff00, 0x00ff0000, 0);
    if (s) {
        if (SDL_SaveBMP(s, path) != 0) fprintf(stderr, "SaveBMP: %s\n", SDL_GetError());
        SDL_FreeSurface(s);
    }
    free(px);
}

void plat_shutdown(void) {
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
