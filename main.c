#include <SDL3/SDL_init.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define STEP_MILLIS 100

typedef struct AppState {
    SDL_Window *w;
    SDL_Renderer *r;
    Uint64 last;
} AppState;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("Asteroids", W_WIDTH, W_HEIGHT, 0, &as->w, &as->r)) {
        return SDL_APP_FAILURE;
    }

    as->last = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *as = (AppState *)appstate;
    const Uint64 now = SDL_GetTicks();
    const Uint64 dt = now - as->last;
    if(dt >= STEP_MILLIS) {
        SDL_Log("Game updated\n");
        // update pos
        as->last = now;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch(event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if(appstate != NULL) {
        AppState *as = (AppState *)appstate;
        SDL_DestroyRenderer(as->r);
        SDL_DestroyWindow(as->w);
        SDL_free(as);
    }
}
