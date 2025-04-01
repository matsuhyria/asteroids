#include <SDL3/SDL_render.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MILLIS 100

typedef struct Ship {
    float x, y;
    float dx, dy;
    float angle;
} Ship;

typedef struct AppState {
    SDL_Window *w;
    SDL_Renderer *r;
    Uint64 last;
    Ship *s;
} AppState;

void initShip(Ship *player);
void drawShip(SDL_Renderer *r, Ship *player);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

void initShip(Ship *player) {
    player->x = (float)W_WIDTH / 2;
    player->y = (float)W_HEIGHT / 2;
    player->dx = 0.0f;
    player->dy = 0.0f;
    player->angle = 0.0f;
}

void drawShip(SDL_Renderer *r, Ship *player) {
    float mx[3] = { +0.0f, -12.5f, +12.5f };
    float my[3] = { -15.0f, +12.5f, +12.5f };

    SDL_Vertex vert[3];
    SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };

    for(int i = 0; i < 3; i++) {
        vert[i].position.x = player->x + mx[i];
        vert[i].position.y = player->y + my[i];
        vert[i].color = color;
    }

    SDL_RenderGeometry(r, NULL, vert, 3, NULL, 0);

}

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

    as->s = (Ship *)SDL_malloc(sizeof(Ship));
    if(!as->s) {
        return SDL_APP_FAILURE;
    }
    initShip(as->s);

    as->last = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *as = (AppState *)appstate;
    const Uint64 now = SDL_GetTicks();
    const Uint64 dt = now - as->last;
    if(dt >= UPDATE_STEP_MILLIS) {
        as->last = now;
        SDL_Log("Game updated\n");

        SDL_SetRenderDrawColor(as->r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(as->r);
        drawShip(as->r, as->s);
        /*SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);*/
        /*SDL_SetRenderScale(as->r, 2.0f, 2.0f);*/
        /*SDL_RenderDebugText(as->r, 0, 0, "Hello World");*/
        SDL_RenderPresent(as->r);
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
        SDL_free(as->s);
        SDL_DestroyRenderer(as->r);
        SDL_DestroyWindow(as->w);
        SDL_free(as);
    }
}
