#include <SDL3/SDL_rect.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define UPDATE_STEP_MS 16 // run at roughly 60 fps

#define SECOND_MS 1000

#define PHOTON_COUNT 8
#define PHOTON_SPEED 600 // PIXELS per SECOND
#define PHOTON_BASE_RADIUS 2
#define PHOTON_COOLDOWN 0.1f // 0.1 seconds, 100 MS

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

#define DEGREE_RANGE 360
static float sin_table[DEGREE_RANGE];
static float cos_table[DEGREE_RANGE];

#define MAX_CIRCLE_POINTS 32

typedef struct {
  float x, y;
  float dx, dy;
  bool active;
} Photon;

typedef struct {
  int nsides;
  SDL_FPoint *points;
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
  bool SPACE;
  float photonCooldown;
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
void createPoints(SDL_FPoint points[], int size, float cx, float cy, float radius);
void transformPoints(SDL_FPoint *vert, const Shape *poly, float cx, float cy, int deg);
void drawPolygon(SDL_Renderer *r, SDL_FPoint *vert, int nsides);

void drawObject(SDL_Renderer *r, Object *obj);
void updateObject(Object *obj, float dt);

bool initShip(Object *player);
void drawShip(SDL_Renderer *r, Object *player);
void updateShip(InputState *is, Object *player, Photon photons[], float dt);
void freeShip(Object *player);

void createAsteroidRadii(float *radii, int nsides);
bool initAsteroids(Object **asteroids, int size);
void drawAsteroids(SDL_Renderer *r, Object **asteroids, int size);
void updateAsteroids(Object **asteroids, int size, float dt);
void freeAsteroids(Object **asteroids, int size);

void initPhotons(Photon photons[], int size);
bool firePhoton(Photon photons[], int size, float x, float y, int deg, float speed);
void drawPhotons(SDL_Renderer *r, Photon photons[], int size);
int updatePhotons(Photon photons[], int size, float dt);

void accelerate(float *vx, float *vy, int deg, float dt, float val);
void turn(int *angle, float deg);
void wrapAround(float *x, float *y);
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

  Shape *shape = SDL_malloc(sizeof(Shape));
  if (!shape) {
    return NULL;
  }

  shape->nsides = size;
  shape->points = SDL_calloc(size, sizeof(SDL_FPoint));
  if (!shape->points) {
    SDL_free(shape->points);
    SDL_free(shape);
    return NULL;
  }

  const int angleStep = DEGREE_RANGE / nsides;

  for (int i = 0; i < nsides; i++) {
    float radius = radii[i];
    int tableIndex = (i * angleStep) % DEGREE_RANGE;

    shape->points[i].x = radius * cos_table[tableIndex];
    shape->points[i].y = radius * (-sin_table[tableIndex]); // Y axis inverted
  }
  // close the shape
  shape->points[nsides] = shape->points[0];
  return shape;
}

void createPoints(SDL_FPoint points[], int size, float cx, float cy, float radius) {
  const int angleStep = DEGREE_RANGE / (size - 1);
  for (int i = 0; i < size - 1; i++) {
    int angle = (i * angleStep) % DEGREE_RANGE;
    points[i].x = cx + radius * cos_table[angle];
    points[i].y = cy - radius * sin_table[angle];
  }
  // Close the shape
  points[size - 1] = points[0];
}

void transformPolygon(SDL_FPoint *vert, const Shape *poly, float cx, float cy, int deg) {
  const int tableIndex = deg % 360;
  float cos = cos_table[tableIndex];
  float sin = sin_table[tableIndex];

  for (int i = 0; i < poly->nsides; i++) {
    float x = poly->points[i].x;
    float y = poly->points[i].y;

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

  //DEBUGGING
  int size = MAX_CIRCLE_POINTS;
  SDL_FPoint circle[size];
  const float radius = ASTEROID_BASE_RADIUS + (ASTEROID_BASE_RADIUS * 0.2f);
  createPoints(circle, size, x, y, radius);
  //.MAKE IT RED HERE
  drawPolygon(r, circle, size);
  //DEBUGGING
}

void updateObject(Object *obj, float dt) {
  obj->x += obj->vx * dt;
  obj->y += obj->vy * dt;
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

void updateShip(InputState *is, Object *player, Photon photons[], float dt) {
  if (is->LEFT) {
    turn(&player->angle, SHIP_ROTATION_SPEED * dt);
  }
  if (is->RIGHT) {
    turn(&player->angle, -SHIP_ROTATION_SPEED * dt);
  }
  if (is->UP) {
    float acceleration = 100.0f;
    accelerate(&player->vx, &player->vy, player->angle, dt, acceleration);
  }

  if(is->photonCooldown > 0) {
    is->photonCooldown -= dt;
  } else {
    is->photonCooldown = 0;
  }

  if (is->SPACE && is->photonCooldown <= 0) {
    firePhoton(photons, PHOTON_COUNT, player->x, player->y, player->angle, PHOTON_SPEED);
    is->photonCooldown = 0.1f;
  }
  updateObject(player, dt);
}

void freeShip(Object *player) {
  SDL_free(player->shape->points);
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
    asteroids[i]->angle = SDL_rand(DEGREE_RANGE);
  }
  return true;
}

void drawAsteroids(SDL_Renderer *r, Object **asteroids, int size) {
  for(int i = 0; i < size; i++) {
    drawObject(r, asteroids[i]);
  }
}

void updateAsteroids(Object **asteroids, int size, float dt) {
  for(int i = 0; i < size; i++) {
    turn(&asteroids[i]->angle, ASTEROID_ROTATION_SPEED * dt);
    updateObject(asteroids[i], dt);
  }
}

void freeAsteroids(Object **asteroids, int size) {
  for (int i = 0; i < size; i++) {
    SDL_free(asteroids[i]->shape->points);
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

bool firePhoton(Photon photons[], int size, float x, float y, int deg, float speed) {
  for(int i = 0; i < size; i++) {
    if(!photons[i].active) {
      photons[i].x = x;
      photons[i].y = y;
      photons[i].dx = speed * cos_table[deg % DEGREE_RANGE];
      photons[i].dy = speed * (-sin_table[deg % DEGREE_RANGE]);
      photons[i].active = true;
      return true;
    }
  }
  return false;
}

void drawPhotons(SDL_Renderer *r, Photon photons[], int length) {
  const int size = MAX_CIRCLE_POINTS + 1; // extra point to close the shape
  const int angleStep = DEGREE_RANGE / size - 1;
  SDL_FPoint circle[size];
  const float radius = PHOTON_BASE_RADIUS;
  for(int i = 0; i < length; i++) {
    if(photons[i].active) {
      const float x = photons[i].x;
      const float y = photons[i].y;
      createPoints(circle, size, x, y, radius);
      drawPolygon(r, circle, size);
    }
  }
}

int updatePhotons(Photon photons[], int size, float dt) {
  int activeCount = 0;
  for(int i = 0; i < size; i++) {
    if(!photons[i].active) { continue; }

    photons[i].x += photons[i].dx * dt;
    photons[i].y += photons[i].dy * dt;
    if(photons[i].x >= W_WIDTH || photons[i].x < 0 ||
       photons[i].y >= W_HEIGHT || photons[i].y < 0) {
      photons[i].active = false;
    } else {
      activeCount++;
    }
  }
  return activeCount;
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
  float cos = cos_table[deg % DEGREE_RANGE];
  float sin = sin_table[deg % DEGREE_RANGE];

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
    case SDL_SCANCODE_SPACE:
      is->SPACE = isPressed;
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

  initTrigTables(DEGREE_RANGE);

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
  const bool secondHasPassed = (now - as->lss_ms) >= SECOND_MS;
  static int fps = 0;
  static int frameCount = 0;
  if (dt >= UPDATE_STEP_MS) {
    frameCount++;
    as->lu_ms = now;
    const float dt_seconds = dt / 1000.0f;
    updateShip(as->is, as->ship, as->photons, dt_seconds);
    updateAsteroids(as->ast, ASTEROID_COUNT, dt_seconds);
    as->active_photons = updatePhotons(as->photons, PHOTON_COUNT, dt_seconds);
    SDL_SetRenderDrawColor(as->r, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(as->r);
    SDL_SetRenderDrawColor(as->r, 255, 255, 255, SDL_ALPHA_OPAQUE);

    SDL_SetRenderScale(as->r, 2.0f, 2.0f);
    SDL_RenderDebugTextFormat(as->r, 0, 0, "fps: %d", fps);
    SDL_RenderDebugTextFormat(as->r, 0, 10, "score: %d", 0);
    SDL_RenderDebugTextFormat(as->r, 0, 20, "photons: %d/%d", as->active_photons, PHOTON_COUNT);
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
