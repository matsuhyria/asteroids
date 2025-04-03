#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MILLIS 16

#define SHIP_MAX_SPEED 10
#define SHIP_SIDE_SIZE 10
#define SHIP_SIDE_COUNT 3

typedef struct InputState {
    bool UP;
    bool RIGHT;
    bool LEFT;
} InputState;

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
    InputState *is;
} AppState;

Shape* createPolygon(int nsides, float sidelen);
void transformPolygon(SDL_FPoint *vert, const Shape *poly, int cx, int cy, float rad);
void drawPolygon(SDL_Renderer *r, SDL_FPoint *vert, int nsides);


bool initShip(Ship *player);
void drawShip(SDL_Renderer *r, Ship *player);
void updateShip(InputState *is, Ship *player);


void accelerate(float *speed, float val);
void turn(float *angle, float deg);
void wrapAround(float *x, float *y);
float degToRad(float angle);


SDL_AppResult handleKeyPress(InputState *is, SDL_Scancode keycode, bool isPressed);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

SDL_AppResult handleKeyPress(InputState *is, SDL_Scancode keycode, bool isPressed) {
    switch(keycode) {
        case SDL_SCANCODE_Q: 
            return SDL_APP_SUCCESS;
        case SDL_SCANCODE_RIGHT:
            is->RIGHT = isPressed;
            break;
        case SDL_SCANCODE_LEFT:
            is->LEFT = isPressed;
            break;
        case SDL_SCANCODE_UP:
            is->UP = isPressed;
            break;
        default:
            return SDL_APP_CONTINUE;
    }
    return SDL_APP_CONTINUE;
}

Shape* createPolygon(int nsides, float radius) {
    const int size = nsides + 1;
    const float angleStep = 2 * SDL_PI_F / nsides;

    Shape *shape = SDL_malloc(sizeof(Shape));
    if(!shape) {
        return NULL;
    }

    shape->nsides = size;
    shape->x = SDL_calloc(size, sizeof(float));
    shape->y = SDL_calloc(size, sizeof(float));
    if(!shape->x || !shape->y) {
        SDL_free(shape->x);
        SDL_free(shape->y);
        return NULL;
    }

    // ship like nose
    shape->x[0] = radius * 2;
    shape->y[0] = 0;
    for(int i = 1; i < nsides; i++) {
        shape->x[i] = radius * SDL_cosf(i * angleStep);
        shape->y[i] = radius * (-SDL_sinf(i * angleStep));
    }
    // close the shape
    shape->x[nsides] = shape->x[0];
    shape->y[nsides] = shape->y[0];
    return shape;
}

void transformPolygon(SDL_FPoint *vert, const Shape *poly, int cx, int cy, float rad) {
    float cos = SDL_cosf(rad);
    float sin = SDL_sinf(rad);

    for(int i = 0; i < poly->nsides; i++) {
        float x = poly->x[i];
        float y = poly->y[i];

        // rotate
        vert[i].x = x * cos - y * (-sin);
        vert[i].y = x * (-sin) + y * cos;

        // translate
        vert[i].x += cx;
        vert[i].y += cy;
    }
    vert[poly->nsides] = vert[0];
}

void drawPolygon(SDL_Renderer *r, SDL_FPoint *vert, int nsides) {
    SDL_RenderLines(r, vert, nsides);
}

bool initShip(Ship *player) {
    player->shape = createPolygon(SHIP_SIDE_COUNT, SHIP_SIDE_SIZE);
    if(!player->shape) {
        return false;
    }
    player->x = W_WIDTH / 2.0f;
    player->y = W_HEIGHT / 2.0f;
    player->dx = 0.0f;
    player->dy = 0.0f;
    player->angle = 0.0f;
    player->speed = 0.0f;
    return true;
}

void drawShip(SDL_Renderer *r, Ship *player) {
    const int nsides = player->shape->nsides;
    SDL_FPoint vert[nsides];
    transformPolygon(vert, player->shape, player->x, player->y, degToRad(player->angle));
    drawPolygon(r, vert, nsides);
}

void updateShip(InputState *is, Ship *player) {
    float acceleration = 0.1f;
    float rotationSpeed = 10.0f;

    if(is->LEFT) {
        turn(&player->angle, rotationSpeed);
    }
    if(is->RIGHT) {
        turn(&player->angle, -rotationSpeed);
    }
    if(is->UP) {
        accelerate(&player->speed, acceleration);
    }

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
    *angle += deg;
    if(*angle >= 360) {
        *angle -= 360;
    } else if(*angle < 0){
        *angle += 360;
    }
}

void wrapAround(float *x, float *y) {
    if(*x >= W_WIDTH) *x = 0;
    else if(*x < 0) *x = W_WIDTH;
    if(*y >= W_HEIGHT) *y = 0;
    else if(*y < 0) *y = W_HEIGHT;
}

float degToRad(float angle) {
    return (angle * SDL_PI_F) / 180.0f;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = SDL_malloc(sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("Asteroids", W_WIDTH, W_HEIGHT, 0, &as->w, &as->r)) {
        return SDL_APP_FAILURE;
    }

    as->s = SDL_malloc(sizeof(Ship));
    if(!as->s) {
        return SDL_APP_FAILURE;
    }
    if(!initShip(as->s)) {
        return SDL_APP_FAILURE;
    }

    as->is = SDL_malloc(sizeof(InputState));
    if(!as->is) {
        return SDL_APP_FAILURE;
    }
    *as->is = (InputState){ 0 };

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
        updateShip(as->is, as->s);
        SDL_RenderPresent(as->r);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *as = (AppState *)appstate;
    
    if(event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    
    if(event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        bool isPressed = (event->type == SDL_EVENT_KEY_DOWN);
        return handleKeyPress(as->is, event->key.scancode, isPressed);
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
        SDL_free(as->is);
        SDL_DestroyRenderer(as->r);
        SDL_DestroyWindow(as->w);
        SDL_free(as);
    }
}
