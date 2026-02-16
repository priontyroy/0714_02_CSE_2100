#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_BALLS 16
#define TABLE_WIDTH 800
#define TABLE_HEIGHT 400
#define BALL_RADIUS 15
#define POCKET_RADIUS 28
#define RAIL_WIDTH 40

// Physics tuning
#define FRICTION 0.985f
#define MIN_VELOCITY 0.06f
#define MAX_POWER_PIXELS 160.0f  
#define MAX_SHOT_SPEED 22.0f    
#define MAX_BALL_SPEED 26.0f    

typedef enum {
    BALL_CUE,
    BALL_SOLID,
    BALL_STRIPE,
    BALL_EIGHT
} BallType;

typedef enum {
    GAME_START,
    GAME_PLAYING,
    GAME_SCRATCH,
    GAME_WON,
    GAME_LOST
} GameState;

typedef enum {
    PLAYER_NONE,
    PLAYER_SOLIDS,
    PLAYER_STRIPES
} PlayerType;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color color;
    BallType type;
    int number;
    bool pocketed;
    bool isStriped;
} Ball;

typedef struct {
    PlayerType type;
    int ballsRemaining;
    char name[20];
} Player;

typedef struct {
    Ball balls[MAX_BALLS];
    Player players[2];
    int currentPlayer;
    GameState state;
    Vector2 cueBallPos;    
    float power;             
    bool aiming;            
    bool ballsMoving;
    bool firstShot;
    bool assignedTypes;
    char statusMessage[100];

   
    Vector2 dragStart;       
    float stickPullPixels;  
    float stickLength;       
    bool stickRecoil;
    float recoilTimer;
} Game;

// prototypes
void InitGame(Game *game);
void ResetBalls(Game *game);
void UpdateGame(Game *game);
void DrawGame(Game *game);
void HandleInput(Game *game);//important
void UpdatePhysics(Game *game);//important
void CheckCollisions(Game *game);//important,বলগুলো একটি আরেকটির সাথে বা টেবিলের দেয়ালে লেগেছে কিনা সেটা পরীক্ষা করে,
void CheckPockets(Game *game);
void CheckWinCondition(Game *game);
void NextTurn(Game *game);
void ApplyScratch(Game *game);//important
void DrawPowerBar(Game *game);
void DrawTable();
bool AreBallsMoving(Game *game);
float Distance(Vector2 a, Vector2 b);
int playerIndexForType(Game *game, BallType btype);
void ResolveElasticCollision(Ball *a, Ball *b);//important\\দুইটি বল আপসের মধ্যে লেগে গেলে তাদের নতুন দিক ও গতি বের করে দেয়

void ClampBallSpeed(Ball *b, float maxSpeed);//কোনো বলের গতি (speed) যদি খুব বেশি হয়ে যায়,তাহলে সেটাকে maxSpeed মানের মধ্যে সীমাবদ্ধ করে (clamp করে)।


void InitGame(Game *game) {
    // Initialize players
    strcpy(game->players[0].name, "Player 1");
    game->players[0].type = PLAYER_NONE;
    game->players[0].ballsRemaining = 7;

    strcpy(game->players[1].name, "Player 2");
    game->players[1].type = PLAYER_NONE;
    game->players[1].ballsRemaining = 7;

    game->currentPlayer = 0;
    game->state = GAME_START;
    game->power = 0.0f;
    game->aiming = false;
    game->ballsMoving = false;
    game->firstShot = true;
    game->assignedTypes = false;
    strcpy(game->statusMessage, "Break shot: click on cue, drag back, release to shoot");

    game->stickPullPixels = 0.0f;
    game->stickLength = 120.0f;
    game->stickRecoil = false;
    game->recoilTimer = 0.0f;

    ResetBalls(game);
}

void ResetBalls(Game *game) {
    // triangle start near right side
    Vector2 triangleStart = { TABLE_WIDTH * 0.72f, TABLE_HEIGHT * 0.5f };

    // cue ball index 0
    game->balls[0].position = (Vector2){ TABLE_WIDTH * 0.25f, TABLE_HEIGHT * 0.5f };
    game->balls[0].velocity = (Vector2){0, 0};
    game->balls[0].color = WHITE;
    game->balls[0].type = BALL_CUE;
    game->balls[0].number = 0;
    game->balls[0].pocketed = false;
    game->balls[0].isStriped = false;

    // set others; keep index numbers conventional: 1..7 solids, 8 = 8-ball, 9..15 stripes
    Color solidColors[] = { YELLOW, BLUE, RED, PURPLE, ORANGE, GREEN, MAROON };
    Color stripeColors[] = { YELLOW, BLUE, RED, PURPLE, ORANGE, GREEN, MAROON };

    int idx = 1;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col <= row; col++) {
            if (idx >= MAX_BALLS) break;
            float offsetX = row * (BALL_RADIUS * 2 * 0.88f);
            float offsetY = (col * (BALL_RADIUS * 2)) - (row * BALL_RADIUS);
            game->balls[idx].position = (Vector2){ triangleStart.x + offsetX, triangleStart.y + offsetY };
            game->balls[idx].velocity = (Vector2){0,0};
            game->balls[idx].pocketed = false;

            if (idx == 8) {
                game->balls[idx].color = BLACK;
                game->balls[idx].type = BALL_EIGHT;
                game->balls[idx].isStriped = false;
            } else if (idx <= 7) {
                game->balls[idx].color = solidColors[idx-1];
                game->balls[idx].type = BALL_SOLID;
                game->balls[idx].isStriped = false;
            } else { // 9..15 stripes
                int sidx = idx - 9;
                if (sidx < 0) sidx = 0;
                game->balls[idx].color = stripeColors[sidx];
                game->balls[idx].type = BALL_STRIPE;
                game->balls[idx].isStriped = true;
            }
            game->balls[idx].number = idx;
            idx++;
        }
    }

    // Reset cue ball placement memory
    game->cueBallPos = game->balls[0].position;
}

void UpdateGame(Game *game) {
    
    HandleInput(game);

    // recoil animation
    if (game->stickRecoil) {
        game->recoilTimer -= 1.0f/60.0f;
        if (game->recoilTimer <= 0.0f) {
            game->stickRecoil = false;
            game->stickPullPixels = 0.0f;
        } else {
            game->stickPullPixels *= 0.92f;
            game->power = game->stickPullPixels / MAX_POWER_PIXELS;
            if (game->power < 0) game->power = 0;
        }
    }

    if (game->state == GAME_PLAYING || game->state == GAME_SCRATCH) {
        UpdatePhysics(game);

        if (!game->ballsMoving && AreBallsMoving(game)) game->ballsMoving = true;

        if (game->ballsMoving && !AreBallsMoving(game)) {
            game->ballsMoving = false;
            // when balls stop
            if (game->state == GAME_PLAYING) {
                CheckWinCondition(game);
                if (game->state != GAME_WON && game->state != GAME_LOST) {
                    NextTurn(game);
                }
            }
        }
    }
}

void DrawGame(Game *game) {
    BeginDrawing();
    ClearBackground((Color){8, 80, 23, 255}); // dark green

    DrawTable();

    // pockets - top-left, top-center, top-right, bottom-left, bottom-center, bottom-right
    Vector2 pockets[] = {
        {RAIL_WIDTH, RAIL_WIDTH},
        {TABLE_WIDTH*0.5f, RAIL_WIDTH},
        {TABLE_WIDTH - RAIL_WIDTH, RAIL_WIDTH},
        {RAIL_WIDTH, TABLE_HEIGHT - RAIL_WIDTH},
        {TABLE_WIDTH*0.5f, TABLE_HEIGHT - RAIL_WIDTH},
        {TABLE_WIDTH - RAIL_WIDTH, TABLE_HEIGHT - RAIL_WIDTH}
    };

    for (int i = 0; i < 6; i++) DrawCircleV(pockets[i], POCKET_RADIUS, BLACK);

    // Draw balls
    for (int i = 0; i < MAX_BALLS; i++) {
        if (!game->balls[i].pocketed) {
            DrawCircleV(game->balls[i].position, BALL_RADIUS, game->balls[i].color);
            if (game->balls[i].isStriped) {
                DrawRectangleV((Vector2){game->balls[i].position.x - BALL_RADIUS*0.9f, game->balls[i].position.y - BALL_RADIUS*0.28f},
                               (Vector2){BALL_RADIUS*1.8f, BALL_RADIUS*0.56f}, WHITE);
                DrawCircleV(game->balls[i].position, BALL_RADIUS-1, game->balls[i].color);
            }
            if (game->balls[i].type == BALL_CUE) {
                DrawCircleV((Vector2){game->balls[i].position.x, game->balls[i].position.y}, 4, LIGHTGRAY);
            } else {
                char numStr[4];
                sprintf(numStr, "%d", game->balls[i].number);
                Vector2 tp = { game->balls[i].position.x - MeasureText(numStr, 12)/2.0f, game->balls[i].position.y - 6 };
                DrawText(numStr, tp.x, tp.y, 12, WHITE);
            }
        }
    }

    // Draw cue stick (if cue ball available and not moving)
    if ((game->state == GAME_START || game->state == GAME_PLAYING) && !game->ballsMoving) {
        Vector2 cueBallPos = game->balls[0].pocketed ? game->cueBallPos : game->balls[0].position;
        Vector2 mousePos = GetMousePosition();

        // if aiming: use drag mechanics
        Vector2 dir;
        float angle;
        if (game->aiming || game->stickPullPixels > 0.0f) {
            // direction from cue ball to current mouse
            dir = (Vector2){ mousePos.x - cueBallPos.x, mousePos.y - cueBallPos.y };
            float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; }
            angle = atan2(dir.y, dir.x);
        } else {
            // aim toward mouse normally for preview
            dir = (Vector2){ mousePos.x - cueBallPos.x, mousePos.y - cueBallPos.y };
            float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; }
            angle = atan2(dir.y, dir.x);
        }

        float effectiveLength = game->stickLength + game->stickPullPixels;
        Vector2 stickTip = { cueBallPos.x - dir.x * (BALL_RADIUS + effectiveLength),
                             cueBallPos.y - dir.y * (BALL_RADIUS + effectiveLength) };
        Vector2 stickStart = { cueBallPos.x - dir.x * (BALL_RADIUS + 4), cueBallPos.y - dir.y * (BALL_RADIUS + 4) };

        DrawLineEx(stickTip, stickStart, 8.0f, (Color){100,60,20,255});
        DrawLineEx(stickTip, stickStart, 6.0f, BROWN);
        Vector2 tipPos = { stickTip.x + dir.x*6, stickTip.y + dir.y*6 };
        DrawCircleV(tipPos, 4, LIGHTGRAY);

        // aiming guide
        if (game->aiming) {
            Vector2 lineEnd = { cueBallPos.x + dir.x * 420, cueBallPos.y + dir.y * 420 };
            DrawLineEx(cueBallPos, lineEnd, 1.5f, Fade(WHITE, 0.22f));
        }
    }

    // Draw UI & power bar
    DrawPowerBar(game);

    // bottom UI panel
    DrawRectangle(0, TABLE_HEIGHT, TABLE_WIDTH, 100, (Color){30,18,10,255});
    char scoreText[128];
    sprintf(scoreText, "%s: %d balls remaining", game->players[0].name, game->players[0].ballsRemaining);
    DrawText(scoreText, 18, TABLE_HEIGHT + 12, 18, WHITE);
    sprintf(scoreText, "%s: %d balls remaining", game->players[1].name, game->players[1].ballsRemaining);
    DrawText(scoreText, 18, TABLE_HEIGHT + 40, 18, WHITE);

    char playerText[80];
    if (game->players[game->currentPlayer].type == PLAYER_SOLIDS) sprintf(playerText, "Current: %s (Solids)", game->players[game->currentPlayer].name);
    else if (game->players[game->currentPlayer].type == PLAYER_STRIPES) sprintf(playerText, "Current: %s (Stripes)", game->players[game->currentPlayer].name);
    else sprintf(playerText, "Current: %s (Unassigned)", game->players[game->currentPlayer].name);

    DrawText(playerText, TABLE_WIDTH - 360, TABLE_HEIGHT + 12, 18, WHITE);
    DrawText(game->statusMessage, TABLE_WIDTH - 360, TABLE_HEIGHT + 40, 16, YELLOW);

    // scratch overlay
    if (game->state == GAME_SCRATCH) {
        DrawRectangle(0,0,TABLE_WIDTH,TABLE_HEIGHT+100,(Color){0,0,0,150});
        DrawText("SCRATCH! Click to place cue ball (inside rails)", TABLE_WIDTH/2 - MeasureText("SCRATCH! Click to place cue ball (inside rails)", 20)/2,
                 TABLE_HEIGHT/2 - 10, 20, RED);
    }

    if (game->state == GAME_WON || game->state == GAME_LOST) {
        DrawRectangle(0,0,TABLE_WIDTH,TABLE_HEIGHT+100,(Color){0,0,0,200});
        char winText[64];
        if (game->state == GAME_WON) sprintf(winText, "%s WINS!", game->players[game->currentPlayer].name);
        else { int w = 1 - game->currentPlayer; sprintf(winText, "%s WINS!", game->players[w].name); }
        DrawText(winText, TABLE_WIDTH/2 - MeasureText(winText, 40)/2, TABLE_HEIGHT/2 - 40, 40, GREEN);
        DrawText("Press R to Restart", TABLE_WIDTH/2 - MeasureText("Press R to Restart", 20)/2, TABLE_HEIGHT/2 + 10, 20, WHITE);
    }

    EndDrawing();
}

void HandleInput(Game *game) {
    // Allow restart at any time
    if (IsKeyPressed(KEY_R)) {
        InitGame(game);
        return;
    }

    Vector2 mousePos = GetMousePosition();

    // Scratch placement
    if (game->state == GAME_SCRATCH) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (mousePos.x > RAIL_WIDTH + BALL_RADIUS && mousePos.x < TABLE_WIDTH - RAIL_WIDTH - BALL_RADIUS &&
                mousePos.y > RAIL_WIDTH + BALL_RADIUS && mousePos.y < TABLE_HEIGHT - RAIL_WIDTH - BALL_RADIUS) {
                game->cueBallPos = mousePos;
                game->balls[0].position = game->cueBallPos;
                game->balls[0].pocketed = false;
                game->balls[0].velocity = (Vector2){0,0};
                game->state = GAME_PLAYING;
                sprintf(game->statusMessage, "Cue placed. %s's turn", game->players[game->currentPlayer].name);
            } else {
                strcpy(game->statusMessage, "Invalid position! Place inside rails away from pockets");
            }
        }
        return;
    }

    // If balls are moving, ignore aiming input
    if (game->ballsMoving) return;

    // Drag-to-charge (2)
    Vector2 cueBallPos = game->balls[0].pocketed ? game->cueBallPos : game->balls[0].position;

    // Start drag only if mouse pressed near cue ball
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (Distance(mousePos, cueBallPos) <= BALL_RADIUS*1.6f) {
            game->aiming = true;
            game->dragStart = mousePos; // not strictly needed, but keep for reference
            game->stickPullPixels = 0.0f;
            game->power = 0.0f;
        }
    }

    // While holding: update pull distance (distance from cue ball to current mouse)
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && game->aiming) {
        float d = Distance(mousePos, cueBallPos);
        // We want pulling away from cue to increase power, so clamp d
        if (d > MAX_POWER_PIXELS) d = MAX_POWER_PIXELS;
        game->stickPullPixels = d;
        game->power = game->stickPullPixels / MAX_POWER_PIXELS;
    }

    // On release: shoot
    if (game->aiming && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        game->aiming = false;
        Vector2 release = mousePos;
        Vector2 dir = { release.x - cueBallPos.x, release.y - cueBallPos.y };
        float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
        if (len < 0.001f) {
            // tiny release -> no shot
            game->stickPullPixels = 0.0f;
            game->power = 0.0f;
            return;
        }
        dir.x /= len; dir.y /= len;

        float shotSpeed = (game->stickPullPixels / MAX_POWER_PIXELS) * MAX_SHOT_SPEED;
        if (shotSpeed > MAX_SHOT_SPEED) shotSpeed = MAX_SHOT_SPEED;

        // apply to cue ball
        game->balls[0].velocity.x = dir.x * shotSpeed;
        game->balls[0].velocity.y = dir.y * shotSpeed;

        // set state
        game->state = GAME_PLAYING;
        game->firstShot = false;
        game->stickRecoil = true;
        game->recoilTimer = 0.12f;

        // reset UI power (visual goes down via recoil)
        game->power = 0.0f;
    }
}

void UpdatePhysics(Game *game) {
    for (int i = 0; i < MAX_BALLS; i++) {
        if (game->balls[i].pocketed) continue;
        // integrate
        game->balls[i].position.x += game->balls[i].velocity.x;
        game->balls[i].position.y += game->balls[i].velocity.y;

        // friction
        game->balls[i].velocity.x *= FRICTION;
        game->balls[i].velocity.y *= FRICTION;

        // small velocity -> stop
        if (fabs(game->balls[i].velocity.x) < MIN_VELOCITY) game->balls[i].velocity.x = 0;
        if (fabs(game->balls[i].velocity.y) < MIN_VELOCITY) game->balls[i].velocity.y = 0;

        // rail collisions (bounce)
        if (game->balls[i].position.x - BALL_RADIUS < RAIL_WIDTH) {
            game->balls[i].position.x = RAIL_WIDTH + BALL_RADIUS;
            game->balls[i].velocity.x = -game->balls[i].velocity.x * 0.86f;
        }
        if (game->balls[i].position.x + BALL_RADIUS > TABLE_WIDTH - RAIL_WIDTH) {
            game->balls[i].position.x = TABLE_WIDTH - RAIL_WIDTH - BALL_RADIUS;
            game->balls[i].velocity.x = -game->balls[i].velocity.x * 0.86f;
        }
        if (game->balls[i].position.y - BALL_RADIUS < RAIL_WIDTH) {
            game->balls[i].position.y = RAIL_WIDTH + BALL_RADIUS;
            game->balls[i].velocity.y = -game->balls[i].velocity.y * 0.86f;
        }
        if (game->balls[i].position.y + BALL_RADIUS > TABLE_HEIGHT - RAIL_WIDTH) {
            game->balls[i].position.y = TABLE_HEIGHT - RAIL_WIDTH - BALL_RADIUS;
            game->balls[i].velocity.y = -game->balls[i].velocity.y * 0.86f;
        }

        // clamp speed to avoid runaway
        ClampBallSpeed(&game->balls[i], MAX_BALL_SPEED);
    }

    CheckCollisions(game);
    CheckPockets(game);
}

void CheckCollisions(Game *game) {
    for (int i = 0; i < MAX_BALLS; i++) {
        if (game->balls[i].pocketed) continue;
        for (int j = i+1; j < MAX_BALLS; j++) {
            if (game->balls[j].pocketed) continue;

            float dist = Distance(game->balls[i].position, game->balls[j].position);
            float minDist = BALL_RADIUS*2.0f;

            if (dist < minDist && dist > 0.0001f) {
                // resolve overlap by pushing apart equally
                float overlap = 0.5f * (minDist - dist + 0.001f);
                Vector2 normal = { (game->balls[j].position.x - game->balls[i].position.x) / dist,
                                   (game->balls[j].position.y - game->balls[i].position.y) / dist };
                game->balls[i].position.x -= normal.x * overlap;
                game->balls[i].position.y -= normal.y * overlap;
                game->balls[j].position.x += normal.x * overlap;
                game->balls[j].position.y += normal.y * overlap;

                // perform elastic collision (equal mass)
                ResolveElasticCollision(&game->balls[i], &game->balls[j]);

                // clamp speeds
                ClampBallSpeed(&game->balls[i], MAX_BALL_SPEED);
                ClampBallSpeed(&game->balls[j], MAX_BALL_SPEED);
            }
        }
    }
}

void ResolveElasticCollision(Ball *a, Ball *b) {
    // normal vector
    float dx = b->position.x - a->position.x;
    float dy = b->position.y - a->position.y;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist <= 0.0001f) return;
    float nx = dx / dist;
    float ny = dy / dist;

    // tangent vector
    float tx = -ny;
    float ty = nx;

    // project velocities onto normal & tangent
    float va_n = a->velocity.x * nx + a->velocity.y * ny;
    float va_t = a->velocity.x * tx + a->velocity.y * ty;
    float vb_n = b->velocity.x * nx + b->velocity.y * ny;
    float vb_t = b->velocity.x * tx + b->velocity.y * ty;

    // For equal masses, swap normal components, tangential remain
    float va_n_after = vb_n;
    float vb_n_after = va_n;

    // convert scalar normal+tangent back to vectors
    a->velocity.x = va_n_after * nx + va_t * tx;
    a->velocity.y = va_n_after * ny + va_t * ty;
    b->velocity.x = vb_n_after * nx + vb_t * tx;
    b->velocity.y = vb_n_after * ny + vb_t * ty;
}

void ClampBallSpeed(Ball *b, float maxSpeed) {
    float sx = b->velocity.x;
    float sy = b->velocity.y;
    float mag = sqrtf(sx*sx + sy*sy);
    if (mag > maxSpeed) {
        b->velocity.x = (b->velocity.x / mag) * maxSpeed;
        b->velocity.y = (b->velocity.y / mag) * maxSpeed;
    }
}

void CheckPockets(Game *game) {
    Vector2 pockets[] = {
        {RAIL_WIDTH, RAIL_WIDTH},
        {TABLE_WIDTH*0.5f, RAIL_WIDTH},
        {TABLE_WIDTH - RAIL_WIDTH, RAIL_WIDTH},
        {RAIL_WIDTH, TABLE_HEIGHT - RAIL_WIDTH},
        {TABLE_WIDTH*0.5f, TABLE_HEIGHT - RAIL_WIDTH},
        {TABLE_WIDTH - RAIL_WIDTH, TABLE_HEIGHT - RAIL_WIDTH}
    };

    bool cueBallPocketed = false;
    bool anyPocketed = false;

    for (int i = 0; i < MAX_BALLS; i++) {
        if (game->balls[i].pocketed) continue;

        for (int p = 0; p < 6; p++) {
            if (Distance(game->balls[i].position, pockets[p]) < POCKET_RADIUS) {
                game->balls[i].pocketed = true;
                game->balls[i].velocity = (Vector2){0,0};
                anyPocketed = true;

                if (i == 0) {
                    cueBallPocketed = true;
                    game->cueBallPos = (Vector2){ TABLE_WIDTH * 0.25f, TABLE_HEIGHT * 0.5f };
                } else {
                    // assign types on first pocket (break)
                    if (!game->assignedTypes && game->firstShot) {
                        if (game->balls[i].type == BALL_SOLID) {
                            game->players[game->currentPlayer].type = PLAYER_SOLIDS;
                            game->players[1 - game->currentPlayer].type = PLAYER_STRIPES;
                            game->assignedTypes = true;
                            sprintf(game->statusMessage, "%s = Solids, %s = Stripes", game->players[game->currentPlayer].name, game->players[1 - game->currentPlayer].name);
                        } else if (game->balls[i].type == BALL_STRIPE) {
                            game->players[game->currentPlayer].type = PLAYER_STRIPES;
                            game->players[1 - game->currentPlayer].type = PLAYER_SOLIDS;
                            game->assignedTypes = true;
                            sprintf(game->statusMessage, "%s = Stripes, %s = Solids", game->players[game->currentPlayer].name, game->players[1 - game->currentPlayer].name);
                        }
                    }

                    // 8-ball logic
                    if (game->balls[i].type == BALL_EIGHT) {
                        int myIdx = game->currentPlayer;
                        if ((game->players[myIdx].type == PLAYER_SOLIDS && game->players[myIdx].ballsRemaining == 0) ||
                            (game->players[myIdx].type == PLAYER_STRIPES && game->players[myIdx].ballsRemaining == 0)) {
                            game->state = GAME_WON;
                        } else {
                            game->state = GAME_LOST;
                        }
                        return;
                    } else {
                        // decrement the correct player's remaining count if matches assignment
                        int ownerIdx = playerIndexForType(game, game->balls[i].type);
                        if (ownerIdx >= 0 && game->players[ownerIdx].ballsRemaining > 0) {
                            game->players[ownerIdx].ballsRemaining--;
                        }
                    }
                }
                break;
            }
        }
    }

    if (cueBallPocketed) ApplyScratch(game);

    if (anyPocketed) {
        sprintf(game->statusMessage, "%s pocketed a ball!", game->players[game->currentPlayer].name);
    }
}

int playerIndexForType(Game *game, BallType btype) {
    if (btype == BALL_SOLID) {
        if (game->players[0].type == PLAYER_SOLIDS) return 0;
        if (game->players[1].type == PLAYER_SOLIDS) return 1;
    } else if (btype == BALL_STRIPE) {
        if (game->players[0].type == PLAYER_STRIPES) return 0;
        if (game->players[1].type == PLAYER_STRIPES) return 1;
    }
    return -1;
}

void ApplyScratch(Game *game) {
    game->state = GAME_SCRATCH;
    strcpy(game->statusMessage, "Scratch! Place cue ball");
    // Scratch: opponent gets ball in hand (we set current player to opponent so when they place it's their turn)
    game->currentPlayer = 1 - game->currentPlayer;
}

void CheckWinCondition(Game *game) {
    // If 8-ball pocketed handled in CheckPockets
    int idx = game->currentPlayer;
    if (game->players[idx].type == PLAYER_SOLIDS && game->players[idx].ballsRemaining == 0) {
        strcpy(game->statusMessage, "Shoot the 8-ball!");
    } else if (game->players[idx].type == PLAYER_STRIPES && game->players[idx].ballsRemaining == 0) {
        strcpy(game->statusMessage, "Shoot the 8-ball!");
    }
}

void NextTurn(Game *game) {
    game->currentPlayer = 1 - game->currentPlayer;
    sprintf(game->statusMessage, "%s's turn", game->players[game->currentPlayer].name);
}

void DrawPowerBar(Game *game) {
    int x = 18;
    int y = TABLE_HEIGHT + 70;
    int width = 240;
    int height = 16;

    DrawText("Power:", x, TABLE_HEIGHT + 36, 16, WHITE);
    DrawRectangle(x + 80, y, width, height, GRAY);
    int filled = (int)(width * (game->stickPullPixels / MAX_POWER_PIXELS));
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    DrawRectangle(x + 80, y, filled, height, RED);
    DrawRectangleLines(x + 80, y, width, height, BLACK);

    char pstr[32];
    sprintf(pstr, "%d%%", (int)((game->stickPullPixels / MAX_POWER_PIXELS) * 100.0f));
    DrawText(pstr, x + 80 + width + 8, y - 2, 16, WHITE);
}

void DrawTable() {
    // bed
    DrawRectangle(RAIL_WIDTH, RAIL_WIDTH, TABLE_WIDTH - 2*RAIL_WIDTH, TABLE_HEIGHT - 2*RAIL_WIDTH, GREEN);
    // rails
    DrawRectangle(0, 0, TABLE_WIDTH, RAIL_WIDTH, BROWN);
    DrawRectangle(0, TABLE_HEIGHT - RAIL_WIDTH, TABLE_WIDTH, RAIL_WIDTH, BROWN);
    DrawRectangle(0, 0, RAIL_WIDTH, TABLE_HEIGHT, BROWN);
    DrawRectangle(TABLE_WIDTH - RAIL_WIDTH, 0, RAIL_WIDTH, TABLE_HEIGHT, BROWN);
}

bool AreBallsMoving(Game *game) {
    for (int i = 0; i < MAX_BALLS; i++) {
        if (game->balls[i].pocketed) continue;
        if (fabs(game->balls[i].velocity.x) > MIN_VELOCITY || fabs(game->balls[i].velocity.y) > MIN_VELOCITY) return true;
    }
    return false;
}

float Distance(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx*dx + dy*dy);
}

int main(void) {
    InitWindow(TABLE_WIDTH, TABLE_HEIGHT + 100, "8 Ball Pool - Drag to Charge (Fixed)");
    SetTargetFPS(60);

    Game game;
    InitGame(&game);

    while (!WindowShouldClose()) {
        UpdateGame(&game);
        DrawGame(&game);
    }

    CloseWindow();
    return 0;
}
