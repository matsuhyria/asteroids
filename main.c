#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define PI 3.14159265358979323846264338327950288
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAX_ASTEROIDS 10
#define THRUST 100
#define ROTATE_DEGREES 5

typedef struct {
    bool isRunning;
    bool leftPressed;
    bool rightPressed;
    bool forwardPressed;
} InputState;

typedef struct {
    SDL_Texture *fpsTexture;
    SDL_Rect *fpsRect;
    SDL_Texture *angleTexture;
    SDL_Rect *angleRect;
    TTF_Font *font;
} UIElements;

typedef struct {
    float x, y;
    float angle;
    float vx, vy;
    float size;
} Asteroid;

typedef struct {
    float x, y;
    float angle;
    float vx, vy;
    float ax, ay;
    bool isThrusting;
} Ship;

bool initSDL(SDL_Window **window, SDL_Renderer **renderer, TTF_Font **font);
void cleanup(SDL_Window *window, SDL_Renderer *renderer, TTF_Font *font, SDL_Texture *fpsTexture, SDL_Texture *angleTexture);
void handleInput(SDL_Event *event, InputState *inputState);
void updateGame(Ship *ship, Asteroid *asteroids, InputState *input, float deltaTime);
void renderGame(SDL_Renderer *renderer, Ship *ship, Asteroid *asteroids, UIElements *ui, float fps);
SDL_Texture* renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Rect *textRect, SDL_Color color, int x, int y);

void initAsteroids(Asteroid *asteroids);
void updateAsteroids(Asteroid *asteroids, float deltaTime);
void drawAsteroids(SDL_Renderer *renderer, Asteroid *asteroids);

void drawShip(SDL_Renderer *renderer, Ship *ship);
void rotateShip(Ship *ship, int degrees);
void updatePosition(Ship *ship, float deltaTime);
void applyThrust(Ship *ship, float thrustPower);
void updateVelocity(Ship *ship, float deltaTime);
void stopAcceleration(Ship *ship);

void wrapAround(float *x, float *y);


int main(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font *font = NULL;

    if(!initSDL(&window, &renderer, &font)) {
        return 1;
    }

    SDL_Rect fpsRect, angleRect;
    SDL_Texture *fpsTexture = NULL, *angleTexture = NULL;

    UIElements ui = { fpsTexture, &fpsRect, angleTexture, &angleRect, font };

    Ship ship = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 90, 0.0, 0.0, 0.0, 0.0, false};
    Asteroid asteroids[MAX_ASTEROIDS];
    initAsteroids(asteroids);

    InputState input = { true, false, false, false };
    SDL_Event event;

    float deltaTime = 0.02;
    Uint32 lastTime = SDL_GetTicks();
    float targetFPS = 60.0;
    Uint32 frameDelay = 1000 / targetFPS;

    while(input.isRunning) {
        Uint32 frameStart = SDL_GetTicks();
        
        Uint32 currentTime = SDL_GetTicks();
        deltaTime = (currentTime - lastTime) / 1000.0;
        lastTime = currentTime;

        float fps = (deltaTime > 0) ? (1.0 / deltaTime) : 0.0;

        while (SDL_PollEvent(&event)) {
            handleInput(&event, &input);
        }

        updateGame(&ship, asteroids, &input, deltaTime);

        renderGame(renderer, &ship, asteroids, &ui, fps);

        // frame rate capping
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if(frameTime < frameDelay) {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    cleanup(window, renderer, font, fpsTexture, angleTexture);
    return 0;
}

void drawShip(SDL_Renderer *renderer, Ship *ship) {
    float rad = ship->angle * (PI / 180.0);
    int SHIP_LENGTH = 20;
    int BASE_LENGTH = 10;

    int x = (int) ship->x; 
    int y = (int) ship->y;

    // head
    int hX = x + (SHIP_LENGTH * cos(rad));
    int hY = y + (SHIP_LENGTH * -sin(rad)); // -sin to account for y growing downwards

    // bottom-left vertex
    int blX = x + (BASE_LENGTH * cos(rad - PI/2));
    int blY = y + (BASE_LENGTH * -sin(rad - PI/2));

    // bottom-right vertex
    int brX = x + (BASE_LENGTH * cos(rad + PI/2));
    int brY = y + (BASE_LENGTH * -sin(rad + PI/2));

    SDL_Point points[4] = {
        { hX, hY },
        { blX, blY },
        { brX, brY },
        { hX, hY },
    };

    SDL_RenderDrawLines(renderer, points, 4);

    if(ship->isThrusting) {
        int midX = (blX + brX) / 2;
        int midY = (blY + brY) / 2;

        int fX = midX + (SHIP_LENGTH * cos(rad + PI));
        int fY = midY + (SHIP_LENGTH * -sin(rad + PI));

        SDL_Point flamePoints[2] = {
            { midX, midY },
            { fX, fY },
        };
        SDL_RenderDrawLines(renderer, flamePoints, 2);
    }
}

void rotateShip(Ship *ship, int degrees) {
    ship->angle += degrees;

    if (ship->angle >= 360) {
        ship->angle -= 360;
    } else if(ship->angle < 0) {
        ship->angle += 360;
    }
}

void updatePosition(Ship *ship, float deltaTime) {
    ship->x += ship->vx * deltaTime;
    ship->y += ship->vy * deltaTime;

    wrapAround(&ship->x, &ship->y);
}

void applyThrust(Ship *ship, float thrustPower) {
    ship->isThrusting = true;
    float rad = ship->angle * (PI / 180.0);

    ship->ax = cos(rad) * thrustPower;
    ship->ay = -sin(rad) * thrustPower;
}

void updateVelocity(Ship *ship, float deltaTime) {
    ship->vx += ship->ax * deltaTime; 
    ship->vy += ship->ay * deltaTime; 
}

void stopAcceleration(Ship *ship) {
    ship->ax = 0;
    ship->ay = 0;
    ship->isThrusting = false;
}

void initAsteroids(Asteroid *asteroids) {
    const int MIN_SIZE = 20;
    const int MAX_SPEED = 50;
    for(int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].x = rand() % SCREEN_WIDTH;
        asteroids[i].y = rand() % SCREEN_HEIGHT;
        asteroids[i].angle = (rand() % 360) * (PI / 180.0);
        asteroids[i].vx = cos(asteroids[i].angle) * (rand() % MAX_SPEED);
        asteroids[i].vy = -sin(asteroids[i].angle) * (rand() % MAX_SPEED);
        asteroids[i].size = rand() % 20 + MIN_SIZE;
    }
}

void updateAsteroids(Asteroid *asteroids, float deltaTime) {
    for(int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].x += asteroids[i].vx * deltaTime;
        asteroids[i].y += asteroids[i].vy * deltaTime;
        asteroids[i].angle += ROTATE_DEGREES;

        wrapAround(&asteroids[i].x, &asteroids[i].y);
    }
}

void drawAsteroids(SDL_Renderer *renderer, Asteroid *asteroids) {
    const int angleNum = 5; //pentagon
    for(int i = 0; i < MAX_ASTEROIDS; i++) {
        int x = (int) asteroids[i].x;
        int y = (int) asteroids[i].y;
        int size = (int) asteroids[i].size;
        float angle = asteroids[i].angle;

        const float angleStep = 2 * PI / angleNum;

        SDL_Point points[6];

        for(int j = 0; j < angleNum; j++) {
            float originalX = size * cos(j * angleStep);
            float originalY = size * -sin(j * angleStep);

            points[j].x = (int) (x + originalX * cos(angle) - originalY * sin(angle));
            points[j].y = (int) (y + originalX * sin(angle) + originalY * cos(angle));
        }
        points[5] = points[0];
        SDL_RenderDrawLines(renderer, points, 6);
    }
}

void wrapAround(float *x, float *y) {
    if (*x < 0) *x = SCREEN_WIDTH;
    if (*x > SCREEN_WIDTH) *x = 0;
    if (*y < 0.0) *y = SCREEN_HEIGHT;
    if (*y > SCREEN_HEIGHT) *y = 0;
}

bool initSDL(SDL_Window **window, SDL_Renderer **renderer, TTF_Font **font) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() == -1) {
        printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    *window = SDL_CreateWindow("Asteroids", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!*window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    *font = TTF_OpenFont("assets/UbuntuMono-R.ttf", 24);
    if (!*font) {
        printf("Font could not be loaded! TTF_Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

void cleanup(SDL_Window *window, SDL_Renderer *renderer, TTF_Font *font, SDL_Texture *fpsTexture, SDL_Texture *angleTexture) {
    if (fpsTexture) SDL_DestroyTexture(fpsTexture);
    if (angleTexture) SDL_DestroyTexture(angleTexture);
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

void handleInput(SDL_Event *event, InputState *input) {
    if (event->type == SDL_QUIT) {
        input->isRunning = false;
    }  

    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP ) {
        bool isPressed = (event->type == SDL_KEYDOWN);
        switch (event->key.keysym.sym) {
            case SDLK_LEFT: input->leftPressed = isPressed; break;
            case SDLK_RIGHT: input->rightPressed = isPressed; break;
            case SDLK_UP: input->forwardPressed = isPressed; break;
        }
    }
}

void updateGame(Ship *ship, Asteroid *asteroids, InputState *input, float deltaTime) {
    if(input->leftPressed) rotateShip(ship, ROTATE_DEGREES);
    if(input->rightPressed) rotateShip(ship, -ROTATE_DEGREES);
    if(input->forwardPressed) {
        applyThrust(ship, THRUST);
    } else {
        stopAcceleration(ship);
    }

    updateVelocity(ship, deltaTime);
    updatePosition(ship, deltaTime);
    updateAsteroids(asteroids, deltaTime);
}

void renderGame(SDL_Renderer *renderer, Ship *ship, Asteroid *asteroids, UIElements *ui, float fps) {
    char text[50];

    snprintf(text, sizeof(text), "FPS: %.2f", fps);
    if(ui->fpsTexture != NULL) {
        SDL_DestroyTexture(ui->fpsTexture);
    }
    ui->fpsTexture = renderText(renderer, ui->font, text, ui->fpsRect, (SDL_Color){255, 255, 255, 255}, 10, 5);

    snprintf(text, sizeof(text), "Angle: %.2f", ship->angle);
    if(ui->angleTexture != NULL) {
        SDL_DestroyTexture(ui->angleTexture);
    }
    ui->angleTexture = renderText(renderer, ui->font, text, ui->angleRect, (SDL_Color){255, 255, 255, 255}, 10, 30);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, ui->fpsTexture, NULL, ui->fpsRect);
    SDL_RenderCopy(renderer, ui->angleTexture, NULL, ui->angleRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawAsteroids(renderer, asteroids);
    drawShip(renderer, ship);
    SDL_RenderPresent(renderer);    
}

SDL_Texture* renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Rect *textRect, SDL_Color color, int x, int y) {
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
    if(!textSurface) {
        printf("Error rendering text: surface error: %s\n", TTF_GetError());
        return NULL;
    }

    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);

    if(!textTexture) {
        printf("Error rendering text: texture error: %s\n", TTF_GetError());
        return NULL;
    }

    textRect->x = x;
    textRect->y = y;
    SDL_QueryTexture(textTexture, NULL, NULL, &textRect->w, &textRect->h);

    return textTexture;
}