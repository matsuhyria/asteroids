#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MS 16 // run at roughly 60 fps

#define PHOTON_COUNT 16
#define PHOTON_SPEED 50
#define PHOTON_BASE_RADIUS 2

#define SHIP_SIDE_COUNT 4
#define SHIP_MAX_SPEED 350 // PIXELS per SECOND
#define SHIP_BASE_RADIUS 10
#define SHIP_ROTATION_SPEED 270 // DEGREES per SECOND

#define ASTEROID_COUNT 8
#define ASTEROID_MAX_SPEED 50 // PIXELS per SECOND
#define ASTEROID_SIDE_COUNT_MIN 6
#define ASTEROID_SIDE_COUNT_MAX 12
#define ASTEROID_BASE_RADIUS 30
#define ASTEROID_ROTATION_SPEED 90 // DEGREES per SECOND

#define RAND_SIGN() (SDL_rand(2) == 0 ? 1 : -1)

static const float SHIP_RADII[SHIP_SIDE_COUNT] = {
    SHIP_BASE_RADIUS * 2.0f, SHIP_BASE_RADIUS, 
    -SHIP_BASE_RADIUS * 0.5f,SHIP_BASE_RADIUS};

#define NUM_POINTS 360
static float sin_table[NUM_POINTS];
static float cos_table[NUM_POINTS];

typedef struct {
  float x;
  float y;
  float dx;
  float dy;
  bool active;
} Photon;

typedef struct {
  int nsides;
  float *x;
  float *y;
} Shape;

typedef struct {
  float x, y;
  float vx, vy;
  int angle;
  Shape *shape;
} Object;

typedef struct {
  bool UP;
  bool RIGHT;
  bool LEFT;
} InputState;

typedef struct {
  SDL_Window *w;
  SDL_Renderer *r;
  InputState *is;
  Object *ship;
  Object **ast;
  int active_asts;
  Photon photons[PHOTON_COUNT];
  int active_photons;
  Uint64 lu_ms;  // last update in ms
  Uint64 lss_ms; // last second start in ms
} AppState;

Shape *createCustomPolygon(const float *radii, int nsides);
void transformPolygon(SDL_FPoint *vert, const Shape *poly, int cx, int cy, int deg);
void drawPolygon(SDL_Renderer *r, SDL_FPoint *vert, int nsides);

void drawObject(SDL_Renderer *r, Object *obj);
void updateObject(Object *obj, Uint64 dt);

bool initShip(Object *player);
void drawShip(SDL_Renderer *r, Object *player);
void updateShip(InputState *is, Object *player, Uint64 dt);
void freeShip(Object *player);

void createAsteroidRadii(float *radii, int nsides);
bool initAsteroids(Object **asteroids, int size);
void drawAsteroids(SDL_Renderer *r, Object **asteroids, int size);
void updateAsteroids(Object **asteroids, int size, Uint64 dt);
void freeAsteroids(Object **asteroids, int size);

void initPhotons(Photon photons[], int size);
void firePhoton(Photon photons[], int size, float x, float y, int deg, float speed, int *activeCount);
void drawPhotons(SDL_Renderer *r, Photon photons[], int size);
void updatePhotons(Photon photons[], int size, Uint64 dt, int *activeCount);

void accelerate(float *vx, float *vy, int deg, float dt, float val);
void turn(int *angle, float deg);
// TO-D0: ADD SMOOTH WRAP AROUND
void wrapAround(float *x, float *y);
float degToRad(float angle);
void initTrigTables(int npoints);

SDL_AppResult handleKeyPress(InputState *is, SDL_Scancode keycode, bool isPressed);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

Shape *createCustomPolygon(const float *radii, int nsides) {
  if (nsides < 3) {
    return NULL;
  }

  const int size = nsides + 1; // extra point to close the shape
  const int angleStep = 360 / nsides;

  Shape *shape = SDL_malloc(sizeof(Shape));
  if (!shape) {
    return NULL;
  }

  shape->nsides = size;
  shape->x = SDL_calloc(size, sizeof(float));
  shape->y = SDL_calloc(size, sizeof(float));
  if (!shape->x || !shape->y) {
    SDL_free(shape->x);
    SDL_free(shape->y);
    SDL_free(shape);
    return NULL;
  }

  for (int i = 0; i < nsides; i++) {
    float radius = radii[i];
    int tableIndex = (i * angleStep) % 360;

    shape->x[i] = radius * cos_table[tableIndex];
    shape->y[i] = radius * (-sin_table[tableIndex]); // Y axis inverted
  }
  // close the shape
  shape->x[nsides] = shape->x[0];
  shape->y[nsides] = shape->y[0];
  return shape;
}

void transformPolygon(SDL_FPoint *vert, const Shape *poly, int cx, int cy, int deg) {
  const int tableIndex = deg % 360;
  float cos = cos_table[tableIndex];
  float sin = sin_table[tableIndex];

  for (int i = 0; i < poly->nsides; i++) {
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

void drawObject(SDL_Renderer *r, Object *obj) {
  const float x = obj->x;
  const float y = obj->y;
  const int deg = obj->angle;
  const int nsides = obj->shape->nsides;
  const Shape* shape = obj->shape;
  SDL_FPoint vert[nsides];

  transformPolygon(vert, shape, x, y, deg);

  drawPolygon(r, vert, nsides);

  // NEEDED FOR DEBUGGING
  const int size = 20 + 1; // extra point to close the shape
  const int angleStep = 360 / 20;
  SDL_FPoint circle[size];
  const float radius = ASTEROID_BASE_RADIUS + (ASTEROID_BASE_RADIUS * 0.2f);
  for(int i = 0; i < size - 1; i++) {
    int tableIndex = (i * angleStep) % 360; 

    circle[i].x = radius * cos_table[tableIndex];
    circle[i].y = radius * (-sin_table[tableIndex]);
    circle[i].x += x;
    circle[i].y += y;
  }
  circle[size - 1].x = circle[0].x;
  circle[size - 1].y = circle[0].y;
  drawPolygon(r, circle, size);
}

void updateObject(Object *obj, Uint64 dt) {
  // small deltaTime rounding error
  // TO-DO: calc the same thing over and over
  float deltaTime = dt / 1000.0f;

  obj->x += obj->vx * deltaTime;
  obj->y += obj->vy * deltaTime;
  wrapAround(&obj->x, &obj->y);
}

bool initShip(Object *player) {
  player->shape = createCustomPolygon(SHIP_RADII, SHIP_SIDE_COUNT);
  if (!player->shape) {
    return false;
  }
  player->x = W_WIDTH / 2.0f;
  player->y = W_HEIGHT / 2.0f;
  player->vx = 0.0f;
  player->vx = 0.0f;
  player->angle = 0.0f;
  return true;
}

void drawShip(SDL_Renderer *r, Object *player) {
  drawObject(r, player);
}

void updateShip(InputState *is, Object *player, Uint64 dt) {
  float deltaTime = dt / 1000.0f;

  if (is->LEFT) {
    turn(&player->angle, SHIP_ROTATION_SPEED * deltaTime);
  }
  if (is->RIGHT) {
    turn(&player->angle, -SHIP_ROTATION_SPEED * deltaTime);
  }
  if (is->UP) {
    float acceleration = 100.0f;
    accelerate(&player->vx, &player->vy, player->angle, deltaTime, acceleration);
  }
  updateObject(player, dt);
}

void freeShip(Object *player) {
  SDL_free(player->shape->x);
  SDL_free(player->shape->y);
  SDL_free(player->shape);
  SDL_free(player);
}

void createAsteroidRadii(float *radii, int nsides) {
  float jitter = ASTEROID_BASE_RADIUS * 0.2f;
  for(int i = 0; i < nsides; i++) {
    radii[i] = ASTEROID_BASE_RADIUS + (RAND_SIGN() * jitter);
  }
}

bool initAsteroids(Object **asteroids, int size) {
  for (int i = 0; i < size; i++) {
    asteroids[i] = SDL_malloc(sizeof(Object));
    if(!asteroids[i]) {
      return false;
    }
    const int SIDE_COUNT = ASTEROID_SIDE_COUNT_MAX - SDL_rand(7);
    float RADII[SIDE_COUNT];
    createAsteroidRadii(RADII, SIDE_COUNT);
    asteroids[i]->shape = createCustomPolygon(RADII, SIDE_COUNT);
    asteroids[i]->x = SDL_rand(W_WIDTH) + ASTEROID_BASE_RADIUS;
    asteroids[i]->y = SDL_rand(W_HEIGHT) + ASTEROID_BASE_RADIUS;
    asteroids[i]->vx = RAND_SIGN() * SDL_rand(ASTEROID_MAX_SPEED);
    asteroids[i]->vy = RAND_SIGN() * SDL_rand(ASTEROID_MAX_SPEED);
    asteroids[i]->angle = SDL_rand(360);
  }
  return true;
}

void drawAsteroids(SDL_Renderer *r, Object **asteroids, int size) {
  for(int i = 0; i < size; i++) {
    drawObject(r, asteroids[i]);
  }
}

void updateAsteroids(Object **asteroids, int size, Uint64 dt) {
  float deltaTime = dt / 1000.0f;
  for(int i = 0; i < size; i++) {
    turn(&asteroids[i]->angle, ASTEROID_ROTATION_SPEED * deltaTime);
    updateObject(asteroids[i], dt);
  }
}

void freeAsteroids(Object **asteroids, int size) {
  for (int i = 0; i < size; i++) {
    SDL_free(asteroids[i]->shape->x);
    SDL_free(asteroids[i]->shape->y);
    SDL_free(asteroids[i]->shape);
    SDL_free(asteroids[i]);
  }
  SDL_free(asteroids);
}

void initPhotons(Photon photons[], int size) {
  for(int i = 0; i < size; i++) {
    photons[i].active = false;
  }
}

void firePhoton(Photon photons[], int size, float x, float y, int deg, float speed, int *activeCount) {
  for(int i = 0; i < size; i++) {
    if(!photons[i].active) {
      photons[i].x = x;
      photons[i].y = y;
      photons[i].dx = speed * cos_table[deg % 360];
      photons[i].dy = speed * (-sin_table[deg % 360]);
      photons[i].active = true;
      *activeCount = *activeCount + 1;
      return;
    }
  }
}

void drawPhotons(SDL_Renderer *r, Photon photons[], int length) {
  const int size = 20 + 1; // extra point to close the shape
  const int angleStep = 360 / 20;
  SDL_FPoint circle[size];
  const float radius = PHOTON_BASE_RADIUS;
  for(int i = 0; i < length; i++) {
    if(photons[i].active) {
      for(int j = 0; j < size - 1; j++) {
        int tableIndex = (j * angleStep) % 360; 

        circle[j].x = radius * cos_table[tableIndex];
        circle[j].y = radius * (-sin_table[tableIndex]);
        circle[j].x += photons[i].x;
        circle[j].y += photons[i].y;
      }
      circle[size - 1].x = circle[0].x;
      circle[size - 1].y = circle[0].y;
      drawPolygon(r, circle, size);
    }
  }
}

void updatePhotons(Photon photons[], int size, Uint64 dt, int *activeCount) {
  float deltaTime = dt / 1000.0f;
  for(int i = 0; i < size; i++) {
    if(photons[i].active) {
      photons[i].x += photons[i].dx * deltaTime;
      photons[i].y += photons[i].dy * deltaTime;
      if(photons[i].x >= W_WIDTH || photons[i].x < 0 ||
         photons[i].y >= W_HEIGHT || photons[i].y < 0) {
        photons[i].active = false;
        *activeCount = *activeCount - 1;
      }
    }
  }
}

void freePhotons(Photon **photons, int size) {
  for(int i = 0; i < size; i++) {
    if(photons[i]) {
      SDL_free(photons[i]);
    }
  }
  SDL_free(photons);
}

void accelerate(float *vx, float *vy, int deg, float dt, float val) {
  float cos = cos_table[deg % 360];
  float sin = sin_table[deg % 360];

  // acceleration
  float ax = cos * val;
  float ay = (-sin) * val;

  // velocity
  *vx += ax * dt;
  *vy += ay * dt;

  if (*vx >= SHIP_MAX_SPEED)
    *vx = SHIP_MAX_SPEED;
  else if (*vx < -SHIP_MAX_SPEED)
    *vx = -SHIP_MAX_SPEED;
  if (*vy >= SHIP_MAX_SPEED)
    *vy = SHIP_MAX_SPEED;
  else if (*vy < -SHIP_MAX_SPEED)
    *vy = -SHIP_MAX_SPEED;
}

void turn(int *angle, float deg) {
  *angle += deg;
  if (*angle >= 360)
    *angle -= 360;
  else if (*angle < 0)
    *angle += 360;
}

void wrapAround(float *x, float *y) {
  if (*x >= W_WIDTH)
    *x = 0;
  else if (*x < 0)
    *x = W_WIDTH;
  if (*y >= W_HEIGHT)
    *y = 0;
  else if (*y < 0)
    *y = W_HEIGHT;
}

float degToRad(float angle) { return (angle * SDL_PI_F) / 180.0f; }

void initTrigTables(int npoints) {
  float angleStep = SDL_PI_F / 180.0f;
  for(int i = 0; i < npoints; i++) {
    float angle = i * angleStep;
    sin_table[i] = SDL_sinf(angle);
    cos_table[i] = SDL_cosf(angle);
  }
}

SDL_AppResult handleKeyPress(InputState *is, SDL_Scancode keycode, bool isPressed) {
  switch (keycode) {
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

  initTrigTables(NUM_POINTS);

  as->ship = SDL_malloc(sizeof(Object));
  if (!as->ship || !initShip(as->ship)) {
    return SDL_APP_FAILURE;
  }

  as->ast = SDL_calloc(ASTEROID_COUNT, sizeof(Object *));
  if (!as->ast || !initAsteroids(as->ast, ASTEROID_COUNT)) {
    return SDL_APP_FAILURE;
  }

  initPhotons(as->photons, PHOTON_COUNT);
  as->active_photons = 0;

  as->is = SDL_malloc(sizeof(InputState));
  if (!as->is) {
    return SDL_APP_FAILURE;
  }
  *as->is = (InputState){0};

  as->lu_ms = SDL_GetTicks();
  as->lss_ms = 0;

  // for asteroids init
  SDL_srand(0);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  AppState *as = (AppState *)appstate;
  const Uint64 now = SDL_GetTicks();
  const Uint64 dt = now - as->lu_ms;
  const bool secondHasPassed = (now - as->lss_ms) >= 1000;
  static int fps = 0;
  static int frameCount = 0;
  if (dt >= UPDATE_STEP_MS) {
    frameCount++;
    as->lu_ms = now;
    updateShip(as->is, as->ship, dt);
    updateAsteroids(as->ast, ASTEROID_COUNT, dt);
    updatePhotons(as->photons, PHOTON_COUNT, dt, &as->active_photons);
    SDL_SetRenderDrawColor(as->r, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(as->r);
    SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_SetRenderScale(as->r, 2.0f, 2.0f);
    SDL_RenderDebugTextFormat(as->r, 0, 0, "fps: %d", fps);
    SDL_RenderDebugTextFormat(as->r, 0, 10, "angle: %d", as->ship->angle);
    SDL_RenderDebugTextFormat(as->r, 0, 20, "vx: %.2f, vy: %.2f", as->ship->vx, as->ship->vy);
    SDL_RenderDebugTextFormat(as->r, 0, 30, "x: %.2f, y: %.2f", as->ship->x, as->ship->y);
    SDL_RenderDebugTextFormat(as->r, 0, 40, "photons: %d/%d", as->active_photons, PHOTON_COUNT);
    SDL_SetRenderScale(as->r, 1.0f, 1.0f);
    drawShip(as->r, as->ship);
    drawAsteroids(as->r, as->ast, ASTEROID_COUNT);
    drawPhotons(as->r, as->photons, PHOTON_COUNT);
    SDL_RenderPresent(as->r);
  }

  if (secondHasPassed) {
    fps = frameCount;
    frameCount = 0;
    as->lss_ms = now;
    firePhoton(as->photons, PHOTON_COUNT, as->ship->x, as->ship->y, as->ship->angle, PHOTON_SPEED, &as->active_photons);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  AppState *as = (AppState *)appstate;

  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
    bool isPressed = (event->type == SDL_EVENT_KEY_DOWN);
    return handleKeyPress(as->is, event->key.scancode, isPressed);
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  if (appstate != NULL) {
    AppState *as = (AppState *)appstate;
    freeShip(as->ship);
    freeAsteroids(as->ast, ASTEROID_COUNT);
    SDL_free(as->is);
    SDL_DestroyRenderer(as->r);
    SDL_DestroyWindow(as->w);
    SDL_free(as);
  }
}
