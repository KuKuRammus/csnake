#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 700
#define TILE_SIZE 20
#define SNAKE_INITIAL_FRICTION 250
#define SNAKE_FRICTION_REDUCTION_PER_PART 4
#define SNAKE_MIN_FRICTION 50

enum Direction {
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_TOP,
    DIRECTION_BOTTOM
};

enum GameState {
    STATE_PAUSED,
    STATE_RUNNING,
    STATE_LOST,
    STATE_QUIT_REQUESTED
};

typedef struct {
    int32_t x;
    int32_t y;
} Pickup;

struct SnakePart {
    int32_t x;
    int32_t y;
    struct SnakePart *next;
    struct SnakePart *prev;
};

typedef struct {
    struct SnakePart *head;
    struct SnakePart *tail;
    enum Direction direction; 
    enum Direction nextDirection;
    int32_t length;
    int32_t friction;
    int32_t pickupsPending;
} Snake;

struct SnakePart* createSnakePart(int32_t x, int32_t y) {
    struct SnakePart* part = malloc(sizeof(struct SnakePart));
    part->x = x;
    part->y = y;

    return part;
}

void updatePickupLocation(Pickup* pickup) {
    uint32_t minX = 1;
    uint32_t minY = 1;
    uint32_t maxX = (WINDOW_WIDTH - TILE_SIZE * 2) / TILE_SIZE;
    uint32_t maxY = (WINDOW_HEIGHT - TILE_SIZE * 2) / TILE_SIZE;
    pickup->x = rand() % (maxX + 1 - minX) + minX;
    pickup->y = rand() % (maxY + 1 - minY) + minY;
}

void addSnakePart(Snake* snake, struct SnakePart* part) {
    snake->length += 1;

    if (snake->head == NULL || snake->tail == NULL) {
        // Snake has not any parts yet
        snake->head = part;
        snake->head->prev = NULL;
        snake->tail = part;
        return;
    }

    // Attach new node at the tail
    part->prev = snake->tail;
    snake->tail->next = part;
    part->next = NULL;
    snake->tail = part;
}

void advanceSnakePosition(Snake* snake) {

    // Store last position in case we need to increase snake size
    int32_t lastX = snake->tail->x;
    int32_t lastY = snake->tail->y;

    // Advance all positions from the tail to head
    struct SnakePart* currentPart = snake->tail;
    while (currentPart != NULL) {
        struct SnakePart* prevPart = currentPart->prev;
        if (prevPart != NULL) {
            currentPart->x = prevPart->x;
            currentPart->y = prevPart->y;
        }
        currentPart = prevPart;
    }

    // Increase snake size if needed
    if (snake->pickupsPending > 0) {
        addSnakePart(snake, createSnakePart(lastX, lastY));
        snake->pickupsPending -= 1;
        snake->length += 1;

        snake->friction -= SNAKE_FRICTION_REDUCTION_PER_PART;
        if (snake->friction < SNAKE_MIN_FRICTION) {
            snake->friction = SNAKE_MIN_FRICTION;
        }
    }

    // Determine next postion of the head
    switch(snake->nextDirection) {
        case DIRECTION_LEFT:
            snake->head->x -= 1;
            break;
        case DIRECTION_RIGHT:
            snake->head->x += 1;
            break;
        case DIRECTION_TOP:
            snake->head->y -= 1;
            break;
        case DIRECTION_BOTTOM:
            snake->head->y += 1;
            break;
    }

    snake->direction = snake->nextDirection;
}

bool hasDeadlyCollisions(Snake* snake) {
    // Check if had bumped into the walls
    if (
        snake->head->x == 0 
        || snake->head->x == (WINDOW_WIDTH / TILE_SIZE) - 1
        || snake->head->y == 0
        || snake->head->y == (WINDOW_HEIGHT / TILE_SIZE) - 1

    ) {
        return true;
    }

    // Check if head colides with some other part
    struct SnakePart* currentPart = snake->head->next;
    do {
        if ((currentPart->x == snake->head->x) && (currentPart->y == snake->head->y)) {
            return true;
        }
    } while (currentPart = currentPart->next);

    return false;
}

bool isSnakeColidesWithPickup(Snake* snake, Pickup* pickup) {
    return (snake->head->x == pickup->x) && (snake->head->y == pickup->y);
}

void freeSnakeParts(Snake* snake) {
    // Free resources if there is some
    if (snake->tail != NULL) {
        struct SnakePart* currentPart = snake->tail;
        while (currentPart != NULL) {
            struct SnakePart* prevPart = currentPart->prev;
            free(currentPart);
            currentPart = prevPart;
        }
    }
}

void resetSnake(Snake* snake) {
    freeSnakeParts(snake);

    // Reinitialize snek
    snake->head = NULL;
    snake->tail = NULL;
    snake->length = 0;
    snake->friction = SNAKE_INITIAL_FRICTION;
    snake->direction = DIRECTION_RIGHT;
    snake->nextDirection = DIRECTION_RIGHT;
    snake->pickupsPending = 0;

    // Reconstruct snek
    addSnakePart(snake, createSnakePart(7, 3));
    addSnakePart(snake, createSnakePart(6, 3));
    addSnakePart(snake, createSnakePart(5, 3));
    addSnakePart(snake, createSnakePart(5, 4));
}

int main (int argc, const char** argv)
{
    // Init SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error during SDL2 init: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window *window = SDL_CreateWindow(
        "Snake game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (window == NULL) {
        printf("Error during window creation: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, // Use just created window
        -1, // Allow sdl to chose rendering drive automatically
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC // Use hardware with vsync enabled
    );
    if (renderer == NULL) {
        printf("Error during renderer creation: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Define wall color depending on state
    SDL_Color wallColorStateRunning = { .r = 36, .g = 123, .b = 160, .a = 255 };
    SDL_Color wallColorStateLost = { .r = 242, .g = 95, .b = 92, .a = 255 };
    SDL_Color wallColorStatePaused = { .r = 112, .g = 193, .b = 179, .a = 255 };
    SDL_Color* currentWallColor = &wallColorStatePaused;

    // Define game state
    enum GameState state = STATE_PAUSED;

    // Create pickup object
    Pickup pickup;
    updatePickupLocation(&pickup);

    // Create snake object
    Snake snake;
    snake.tail = NULL;
    resetSnake(&snake);

    // Define runtime properties
    SDL_Event event;
    uint32_t lastTime = SDL_GetTicks();
    uint32_t timeDelta = 16;
    int32_t timeTillNextSnakeMove = snake.friction;
    while (state != STATE_QUIT_REQUESTED) {
        timeDelta = SDL_GetTicks() - lastTime;
        lastTime = SDL_GetTicks();
        
        // Handle incoming events
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            state = STATE_QUIT_REQUESTED;
                            break;
                        case SDLK_UP:
                            snake.nextDirection = snake.direction != DIRECTION_BOTTOM ? DIRECTION_TOP : snake.direction;
                            break;
                        case SDLK_DOWN:
                            snake.nextDirection = snake.direction != DIRECTION_TOP ? DIRECTION_BOTTOM : snake.direction;
                            break;
                        case SDLK_LEFT:
                            snake.nextDirection = snake.direction != DIRECTION_RIGHT ? DIRECTION_LEFT : snake.direction;
                            break;
                        case SDLK_RIGHT:
                            snake.nextDirection = snake.direction != DIRECTION_LEFT ? DIRECTION_RIGHT : snake.direction;
                            break;
                        case SDLK_SPACE:
                            switch (state) {
                                case STATE_LOST:
                                    resetSnake(&snake);
                                    updatePickupLocation(&pickup);
                                    state = STATE_RUNNING;
                                    currentWallColor = &wallColorStateRunning;
                                    break;
                                case STATE_PAUSED:
                                    state = STATE_RUNNING;
                                    currentWallColor = &wallColorStateRunning;
                                    break;
                                case STATE_RUNNING:
                                    state = STATE_PAUSED;
                                    currentWallColor = &wallColorStatePaused;
                                    break;
                            }
                    }
            }
        }

        // Update snake timer only if game is running
        if (state != STATE_LOST && state != STATE_PAUSED) {
            timeTillNextSnakeMove -= timeDelta;
        }
        
        if (timeTillNextSnakeMove <= 0) {
            // Advance snake
            advanceSnakePosition(&snake);

            // Check if colides with pickup
            if (isSnakeColidesWithPickup(&snake, &pickup)) {
                snake.pickupsPending += 1;
                updatePickupLocation(&pickup);
            }

            // Check if colides with something else
            if (hasDeadlyCollisions(&snake)) {
                state = STATE_LOST;
                currentWallColor = &wallColorStateLost;
            }

            // Reset snake move time
            timeTillNextSnakeMove = snake.friction;
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 80, 81, 79, 255);
        SDL_RenderClear(renderer);

        // Draw borders
        SDL_SetRenderDrawColor(
            renderer,
            currentWallColor->r,
            currentWallColor->g,
            currentWallColor->b,
            currentWallColor->a
        );
        SDL_Rect left = { .x = 0, .y = 0, .w = TILE_SIZE, .h = WINDOW_HEIGHT - TILE_SIZE };
        SDL_Rect bottom = { .x = 0, .y = WINDOW_HEIGHT - TILE_SIZE, .w = WINDOW_WIDTH - TILE_SIZE, .h = TILE_SIZE };
        SDL_Rect right = { .x = WINDOW_WIDTH - TILE_SIZE, .y = TILE_SIZE, .w = TILE_SIZE, .h = WINDOW_HEIGHT - TILE_SIZE }; 
        SDL_Rect top = { .x = TILE_SIZE, .y = 0, .w = WINDOW_WIDTH - TILE_SIZE, .h = TILE_SIZE };
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &bottom);
        SDL_RenderFillRect(renderer, &right);
        SDL_RenderFillRect(renderer, &top);

        // Draw snake
        SDL_SetRenderDrawColor(renderer, 244, 244, 244, 255); // Set color for head first
        struct SnakePart* currentPart = snake.head;
        do {
            SDL_Rect snakePartRect = {
                .x = currentPart->x * TILE_SIZE,
                .y = currentPart->y * TILE_SIZE,
                .w = TILE_SIZE,
                .h = TILE_SIZE
            };
            SDL_RenderFillRect(renderer, &snakePartRect);

            // For any other part, except head, switch to darker color
            SDL_SetRenderDrawColor(renderer, 219, 219, 219, 255);
        } while (currentPart = currentPart->next);

        // Draw pickup
        SDL_SetRenderDrawColor(renderer, 255, 224, 102, 255);
        SDL_Rect pickupRect = { .x = pickup.x * TILE_SIZE, .y = pickup.y * TILE_SIZE, .w = TILE_SIZE, .h = TILE_SIZE };
        SDL_RenderFillRect(renderer, &pickupRect);


        // Update screen
        SDL_RenderPresent(renderer);
    }

    // Free memory
    freeSnakeParts(&snake);

    // Destroy renderer on exit
    SDL_DestroyRenderer(renderer);

    // Destroy window on exit
    SDL_DestroyWindow(window);

    // Shutdown SDL
    SDL_Quit();

    return 0;
}
