#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MS 16       // run at roughly 60 fps

#define SHIP_MAX_SPEED 350      // PIXELS per SECOND
#define SHIP_SIDE_SIZE 20
#define SHIP_SIDE_COUNT 4
#define SHIP_ROTATION_SPEED 180 // DEGREES per SECOND

static const float SHIP_RADII[] = { SHIP_SIDE_SIZE * 2.0f,
                                    SHIP_SIDE_SIZE,
                                    -SHIP_SIDE_SIZE * 0.5f,
                                    SHIP_SIDE_SIZE };

typedef struct {
    int nsides;
    float *x;
    float *y;
} Shape;

typedef struct {
    float x, y;
    float vx, vy;
    float angle;
    Shape *shape;
} Ship;

typedef struct {
    bool UP;
    bool RIGHT;
    bool LEFT;
} InputState;

typedef struct {
    SDL_Window *w;
    SDL_Renderer *r;
    Ship *s;
    InputState *is;
    Uint64 lu_ms;  // last update in ms
    Uint64 lss_ms; // last second start in ms
} AppState;

Shape* createCustomPolygon(int nsides, const float *radii);
Shape* createPolygon(int nsides, float radius);
void transformPolygon(SDL_FPoint *vert, const Shape *poly, int cx, int cy, float rad);
void drawPolygon(SDL_Renderer *r, SDL_FPoint *vert, int nsides);


bool initShip(Ship *player);
void drawShip(SDL_Renderer *r, Ship *player);
void updateShip(InputState *is, Ship *player, Uint64 dt);


void accelerate(float *vx, float *vy, float rad, float dt, float val);
void turn(float *angle, float deg);
void wrapAround(float *x, float *y);
float degToRad(float angle);


SDL_AppResult handleKeyPress(InputState *is, SDL_Scancode keycode, bool isPressed);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

Shape* createCustomPolygon(int nsides, const float *radii) {
    if(nsides < 3) {
        return NULL;
    }

    const int size = nsides + 1; // extra point to close the shape
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
        SDL_free(shape);
        return NULL;
    }

    for(int i = 0; i < nsides; i++) {
        float radius = radii[i];
        float rad = i * angleStep;
        float cos = SDL_cosf(rad);
        float sin = SDL_sinf(rad);

        shape->x[i] = radius * cos;
        shape->y[i] = radius * (-sin); // Y axis inverted
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
    player->shape = createCustomPolygon(SHIP_SIDE_COUNT, SHIP_RADII);
    if(!player->shape) {
        return false;
    }
    /*player->shape = createPolygon(SHIP_SIDE_COUNT, SHIP_SIDE_SIZE);*/
    /*if(!player->shape) {*/
    /*    return false;*/
    /*}*/
    player->x = W_WIDTH / 2.0f;
    player->y = W_HEIGHT / 2.0f;
    player->vx = 0.0f;
    player->vx = 0.0f;
    return true;
}

void drawShip(SDL_Renderer *r, Ship *player) {
    const int nsides = player->shape->nsides;
    SDL_FPoint vert[nsides];
    transformPolygon(vert, player->shape, player->x, player->y, degToRad(player->angle));
    drawPolygon(r, vert, nsides);
}

void updateShip(InputState *is, Ship *player, Uint64 dt) {
    // small deltaTime rounding error
    float deltaTime = dt / 1000.0f;

    if(is->LEFT) {
        turn(&player->angle, SHIP_ROTATION_SPEED * deltaTime);
    }
    if(is->RIGHT) {
        turn(&player->angle, -SHIP_ROTATION_SPEED * deltaTime);
    }
    if(is->UP) {
        float acceleration = 100.0f;
        float rad = degToRad(player->angle);
        accelerate(&player->vx, &player->vy, rad, deltaTime, acceleration);
    }

    // UPDATE POSITION
    player->x += player->vx * deltaTime;
    player->y += player->vy * deltaTime;
    wrapAround(&player->x, &player->y);
}

void accelerate(float *vx, float *vy, float rad, float dt, float val) {
    float sin = SDL_sinf(rad);
    float cos = SDL_cosf(rad);

    // acceleration
    float ax = cos * val;
    float ay = (-sin) * val;

    // velocity
    *vx += ax * dt;
    *vy += ay * dt;

    if(*vx >= SHIP_MAX_SPEED) *vx = SHIP_MAX_SPEED;
    else if(*vx < -SHIP_MAX_SPEED) *vx = -SHIP_MAX_SPEED;
    if(*vy >= SHIP_MAX_SPEED) *vy = SHIP_MAX_SPEED;
    else if(*vy < -SHIP_MAX_SPEED) *vy = -SHIP_MAX_SPEED;
}

void turn(float *angle, float deg) {
    *angle += deg;
    if(*angle >= 360.0f) *angle -= 360.0f;
    else if(*angle < 0.0f) *angle += 360.0f;
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

    as->lu_ms = SDL_GetTicks();
    as->lss_ms = 0;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *as = (AppState *)appstate;
    const Uint64 now = SDL_GetTicks();
    const Uint64 dt = now - as->lu_ms;
    const bool secondHasPassed = (now - as->lss_ms) >= 1000;
    static int fps = 0; 
    static int frameCount = 0;
    if(dt >= UPDATE_STEP_MS) {
        frameCount++;
        as->lu_ms = now;
        updateShip(as->is, as->s, dt);
        SDL_SetRenderDrawColor(as->r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(as->r);
        SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_SetRenderScale(as->r, 2.0f, 2.0f);
        SDL_RenderDebugTextFormat(as->r, 0, 0, "fps: %d", fps);
        SDL_RenderDebugTextFormat(as->r, 0, 10, "angle: %.2f", as->s->angle);
        SDL_RenderDebugTextFormat(as->r, 0, 20, "vx: %.2f, vy: %.2f", as->s->vx, as->s->vy);
        SDL_RenderDebugTextFormat(as->r, 0, 30, "x: %.2f, y: %.2f", as->s->x, as->s->y);
        SDL_SetRenderScale(as->r, 1.0f, 1.0f);
        drawShip(as->r, as->s);
        SDL_RenderPresent(as->r);
    }

    if(secondHasPassed) {
        fps = frameCount;
        frameCount = 0;
        as->lss_ms = now;
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
