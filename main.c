#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MILLIS 16

#define SHIP_MAX_SPEED 10
#define SHIP_SIDE_SIZE 15
#define SHIP_VERT_CNT 3
#define DEG_TURN 10
#define SPEED_INC 0.1

typedef struct Ship {
    int x, y;
    float dx, dy;
    float angle;
    float speed;
} Ship;

typedef struct AppState {
    SDL_Window *w;
    SDL_Renderer *r;
    Uint64 last;
    Ship *s;
} AppState;

SDL_AppResult handleKeyPress(Ship *s, SDL_Scancode keycode);
void drawPolygon(SDL_Renderer *r, int nsides, int sidelen, float cx, float cy, float rad);
void initShip(Ship *player);
void drawShip(SDL_Renderer *r, Ship *player);
void updateShip(SDL_Renderer *r, Ship *player);
void accelerate(float *speed, float val);
void turn(float *angle, int deg);
void wrapAround(int *x, int *y);
float degToRad(float angle);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

SDL_AppResult handleKeyPress(Ship *s, SDL_Scancode keycode) {
    switch(keycode) {
        case SDL_SCANCODE_Q: 
            return SDL_APP_SUCCESS;
        case SDL_SCANCODE_RIGHT:
            turn(&s->angle, -DEG_TURN);
            break;
        case SDL_SCANCODE_LEFT:
            turn(&s->angle, DEG_TURN);
            break;
        case SDL_SCANCODE_UP:
            accelerate(&s->speed, SPEED_INC);
            break;
        default:
            return SDL_APP_CONTINUE;
    }
    return SDL_APP_CONTINUE;
}

void drawPolygon(SDL_Renderer *r, int nsides, int sidelen, float cx, float cy, float rad) {
    const float angleStep = 2 * SDL_PI_F / nsides;

    float sx[nsides];
    float sy[nsides];

    // define polygon
    for(int i = 0; i < nsides; i++) {
        sx[i] = sidelen * SDL_cosf(i * angleStep);
        sy[i] = sidelen * -SDL_sinf(i * angleStep);
    }

    // rotate
    SDL_Vertex vert[nsides];
    for(int i = 0; i < nsides; i++) {
        vert[i].position.x = sx[i] * SDL_cosf(rad) - sy[i] * (-SDL_sinf(rad));
        vert[i].position.y = sx[i] * (-SDL_sinf(rad)) + sy[i] * SDL_cosf(rad);
    }

    // translate
    SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
    for(int i = 0; i < 3; i++) {
        vert[i].position.x = cx + vert[i].position.x;
        vert[i].position.y = cy + vert[i].position.y;
        vert[i].color = color;
    }

    SDL_RenderGeometry(r, NULL, vert, 3, NULL, 0);
}

void initShip(Ship *player) {
    player->x = W_WIDTH / 2;
    player->y = W_HEIGHT / 2;
    player->dx = 0.0f;
    player->dy = 0.0f;
    player->angle = 0.0f;
    player->speed = 0.0f;
}

void drawShip(SDL_Renderer *r, Ship *player) {
    float rad = degToRad(player->angle);
    drawPolygon(r, SHIP_VERT_CNT, SHIP_SIDE_SIZE, player->x, player->y, rad);
}

void updateShip(SDL_Renderer *r, Ship *player) {
    player->dx = player->speed * SDL_cosf(degToRad(player->angle));
    player->dy = player->speed * -SDL_sinf(degToRad(player->angle));

    player->x += player->dx;
    player->y += player->dy;
    wrapAround(&player->x, &player->y);
}

void accelerate(float *speed, float val) {
    *speed += val;
    if(*speed > SHIP_MAX_SPEED) {
        *speed = SHIP_MAX_SPEED;
    } else if(*speed < 0) {
        *speed = 0;
    }
}

void turn(float *angle, int deg) {
    *angle = *angle + deg;
    if(*angle >= 360) {
        *angle -= 360;
    } else if(*angle < 0){
        *angle += 360;
    }
}

void wrapAround(int *x, int *y) {
    if(*x >= W_WIDTH) {
        *x = 0;
    } else if(*x < 0) {
        *x = W_WIDTH;
    } else if(*y >= W_HEIGHT) {
        *y = 0;
    } else if(*y < 0){
        *y = W_HEIGHT;
    }
}

float degToRad(float angle) {
    return (angle * SDL_PI_F) / 180.0f;
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

        SDL_SetRenderDrawColor(as->r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(as->r);
        drawShip(as->r, as->s);
        updateShip(as->r, as->s);
        SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_SetRenderScale(as->r, 2.0f, 2.0f);
        SDL_RenderDebugTextFormat(as->r, 0, 0, "Angle: %.2f", as->s->angle);
        SDL_SetRenderScale(as->r, 1.0f, 1.0f);
        SDL_RenderPresent(as->r);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *as = (AppState *)appstate;
    
    if(event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    
    if(event->type == SDL_EVENT_KEY_DOWN) {
        return handleKeyPress(as->s, event->key.scancode);
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
