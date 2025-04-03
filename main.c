#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MILLIS 16

#define SHIP_MAX_SPEED 10
#define SHIP_SIDE_SIZE 16
#define SHIP_VERT_CNT 4
#define DEG_TURN 10
#define SPEED_INC 1

typedef struct Shape {
    int nsides;
    float *x;
    float *y;
} Shape;

typedef struct Ship {
    float x, y;
    float dx, dy;
    float angle;
    float speed;
    Shape *shape;
} Ship;

typedef struct AppState {
    SDL_Window *w;
    SDL_Renderer *r;
    Uint64 last;
    Ship *s;
} AppState;

Shape* createPolygon(int nsides, float sidelen);
void drawPolygon(SDL_Renderer *r, Shape *poly, int cx, int cy, float rad);


bool initShip(Ship *player);
void drawShip(SDL_Renderer *r, Ship *player);
void updateShip(SDL_Renderer *r, Ship *player);


void accelerate(float *speed, float val);
void turn(float *angle, float deg);
void wrapAround(float *x, float *y);
float degToRad(float angle);


SDL_AppResult handleKeyPress(Ship *s, SDL_Scancode keycode);
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

Shape* createPolygon(int nsides, float sidelen) {
    const int size = nsides + 1;
    const float angleStep = 2 * SDL_PI_F / nsides;

    Shape *shape = SDL_malloc(sizeof(Shape));
    if(!shape) {
        return NULL;
    }

    shape->nsides = size;
    shape->x = SDL_calloc(size, sizeof(float));
    if(!shape->x) {
        return NULL;
    }

    shape->y = SDL_calloc(size, sizeof(float));
    if(!shape->y) {
        return NULL;
    }

    for(int i = 0; i < nsides; i++) {
        shape->x[i] = sidelen * SDL_cosf(i * angleStep);
        shape->y[i] = sidelen * (-SDL_sinf(i * angleStep));
    }
    shape->x[nsides] = shape->x[0];
    shape->y[nsides] = shape->y[0];
    return shape;
}

void drawPolygon(SDL_Renderer *r, Shape *poly, int cx, int cy, float rad) {
    const int nsides = poly->nsides;

    SDL_FPoint points[nsides];

    // rotate
    for(int i = 0; i < nsides; i++) {
        points[i].x = poly->x[i] * SDL_cosf(rad) - poly->y[i] * (-SDL_sinf(rad));
        points[i].y = poly->x[i] * (-SDL_sinf(rad)) + poly->y[i] * SDL_cosf(rad);
    }

    // translate
    for(int i = 0; i < nsides; i++) {
        points[i].x = cx + points[i].x;
        points[i].y = cy + points[i].y;
    }
    points[nsides].x = points[0].x;
    points[nsides].y = points[0].y;

    /*for(int i = 0; i < size - 1; i++) {*/
    /*    SDL_RenderLine(r, vert[i].x, vert[i].y, vert[i+1].x, vert[i+1].y);*/
    /*}*/
    SDL_RenderLines(r, points, nsides);
}

bool initShip(Ship *player) {
    player->x = W_WIDTH / 2.0f;
    player->y = W_HEIGHT / 2.0f;
    player->dx = 0.0f;
    player->dy = 0.0f;
    player->angle = 0.0f;
    player->speed = 0.0f;
    player->shape = createPolygon(3, 10.0f);
    if(!player->shape) {
        return false;
    }
    return true;
}

void drawShip(SDL_Renderer *r, Ship *player) {
    drawPolygon(r, player->shape, player->x, player->y, degToRad(player->angle));
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

void turn(float *angle, float deg) {
    *angle = *angle + deg;
    if(*angle >= 360) {
        *angle -= 360;
    } else if(*angle < 0){
        *angle += 360;
    }
}

void wrapAround(float *x, float *y) {
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

    AppState *as = (AppState *)SDL_malloc(sizeof(AppState));
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
    if(!initShip(as->s)) {
        return SDL_APP_FAILURE;
    }

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
        SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_SetRenderScale(as->r, 2.0f, 2.0f);
        SDL_RenderDebugTextFormat(as->r, 0, 0, "Angle: %.2f", as->s->angle);
        SDL_SetRenderScale(as->r, 1.0f, 1.0f);
        drawShip(as->r, as->s);
        updateShip(as->r, as->s);
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
        SDL_free(as->s->shape->x);
        SDL_free(as->s->shape->y);
        SDL_free(as->s->shape);
        SDL_free(as->s);
        SDL_DestroyRenderer(as->r);
        SDL_DestroyWindow(as->w);
        SDL_free(as);
    }
}
