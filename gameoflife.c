#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <assert.h>

typedef uint8_t u8;

// Keeps track of the past n states and if the current state matches ANY of them, it's considered game over
#define HISTORY_LENGTH 3

#ifdef DEBUG
#define HIDE_CURSOR ""
#else
#define HIDE_CURSOR "\033[?25l"
#endif
#define SHOW_CURSOR "\033[?25h"
#define CLEAR_SCROLLBACK "\033[3J\033[H"
#define CLEAR_SCREEN_FULL "\033[2J\033[3J\033[H"
#define CLEAR_LINE "\033[K\n"

#define msleep(ms) usleep(ms * 1000)
#define error(msg, ...) (clear_screen_on_exit=0,fprintf(stderr, msg, ##__VA_ARGS__))

bool clear_screen_on_exit = true;

void cleanup(void) {
    printf(SHOW_CURSOR);
    if (clear_screen_on_exit) printf(CLEAR_SCREEN_FULL);
}

void handle_exit_signal(int sig) {
    if (sig == SIGINT) {
        cleanup();
        exit(0);
    } else {
        printf(SHOW_CURSOR);
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

int mod(int a, int divisor) {
    return (a % divisor + divisor) % divisor;
}

int tick(int width, int height, u8 cells[height][width]) {
    int live_cells = 0;
    u8 (*next)[width] = alloca(sizeof(u8) * width * height);

    for (int y = 0; y < height; y++) {
        int yp = mod(y - 1, height);
        int yn = mod(y + 1, height);

        for (int x = 0; x < width; x++) {
            int xp = mod(x - 1, width);
            int xn = mod(x + 1, width);

            int live_neighbors =
                cells[yp][xp] + cells[yp][x] + cells[yp][xn] +
                cells[y ][xp]                + cells[y ][xn] +
                cells[yn][xp] + cells[yn][x] + cells[yn][xn];

            if (cells[y][x]) {
                if (live_neighbors >= 2 && live_neighbors <= 3) {
                    next[y][x] = 1;
                    live_cells++;
                } else {
                    next[y][x] = 0;
                }
            } else {
                if (live_neighbors == 3) {
                    next[y][x] = 1;
                    live_cells++;
                } else {
                    next[y][x] = 0;
                }
            }
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            cells[y][x] = next[y][x];
        }
    }

    return live_cells;
}

void print_cells(int width, int height, u8 cells[height][width], int live_cells) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            printf(cells[y][x] == 1 ? "#" : ".");
        }
        printf("\n");
    }

    printf("Live cells: %d/%d", live_cells, width*height);
}

void seed_cells(int width, int height, u8 cells[height][width]) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            cells[y][x] = rand() & 1; // 50% chance of being alive
        }
    }
}

bool eof() {
    fd_set rfds;
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) == 1) {
        if (getchar() == EOF) {
            return 1;
        }
    }

    return 0;
}

typedef struct {
    u8  *cells;
    int width;
    int height;
} ShapeCells;

typedef struct {
    char       *key;
    ShapeCells cells;
} ShapeEntry;

ShapeEntry shapes[] = {
    {.key = "glider", .cells = { .width = 3, .height = 3, .cells = (u8[]){
        0, 1, 0,
        0, 0, 1,
        1, 1, 1,
    }}},
    {.key = "glider_gun", .cells = { .width = 36, .height = 9, .cells = (u8[]){
//                                    1  1  1  1  1  1  1  1  1  1  2  2  2  2  2  2  2  2  2  2  3  3  3  3  3  3
//      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5
/* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 2 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
/* 3 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
/* 4 */ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 6 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 7 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 8 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    }}},
};

int num_shapes = sizeof(shapes) / sizeof(ShapeEntry);

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    signal(SIGINT, handle_exit_signal);
    signal(SIGSEGV, handle_exit_signal);
    signal(SIGBUS, handle_exit_signal);
    signal(SIGABRT, handle_exit_signal);
    atexit(cleanup);

    int seed = -1;
    char shape[24] = { 0 };
    int shape_x_offset = 0;
    int shape_y_offset = 0;
    bool infinite = false;

    int width = -1;
    int height = -1;
    int tps = 5;

    int opt = 0;
    while ((opt = getopt(argc, argv, "s:S:x:y:w:h:t:i")) != -1) {
        switch (opt) {
            case 's':
                seed = atoi(optarg);
                break;
            case 'S':
                strlcpy(shape, optarg, sizeof(shape));
                break;
            case 'x':
                shape_x_offset = atoi(optarg);
                break;
            case 'y':
                shape_y_offset = atoi(optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 't':
                tps = atoi(optarg);
                break;
            case 'i':
                infinite = true;
                break;
            case '?':
            default:
                error("Usage: %s [-s seed OR -S shape] [-x shape offset] [-y shape offset] [-w width] [-h height] [-t tps]\n", argv[0]);
                return 1;
        }
    }

    if (seed == -1 && shape[0] == 0) {
        seed = time(NULL);
    }

    if (width == -1 || height == -1) {
        struct winsize ws;
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);

        if (height < 1) width = ws.ws_col;
        if (height < 1) height = ws.ws_row - 3;

        assert(width > 1);
        assert(height > 1);
    }

    u8 (*cells)[width] = malloc(sizeof(u8) * width * height);
    u8 (*history[HISTORY_LENGTH])[width];

    for (int i = 0; i < HISTORY_LENGTH; i++) {
        history[i] = malloc(sizeof(u8) * width * height);
    }

    if (shape[0] != 0) {
        bool ok = false;
        for (int i = 0; i < num_shapes; i++) {
            if (strcmp(shape, shapes[i].key) == 0) {
                ok = true;
                for (int y = 0; y < shapes[i].cells.height; y++) {
                    for (int x = 0; x < shapes[i].cells.width; x++) {
                        cells[y + shape_y_offset][x + shape_x_offset] = shapes[i].cells.cells[shapes[i].cells.width * y + x];
                    }
                }
                break;
            }
        }

        if (! ok) {
            error("Invalid shape: %s\n", shape);
            return 1;
        }
    }

    if (seed != -1) {
        srand(seed);
        seed_cells(width, height, cells);
    }

    printf(HIDE_CURSOR);
    printf(CLEAR_SCREEN_FULL);
    fflush(stdout);
    size_t ticks = 0;

    while (true) {
        // improvement: first draw initial state before doing tick()
        int live_cells = tick(width, height, cells);
        ticks++;

        if (live_cells == 0) {
            if (infinite) {
                srand(++seed);
                seed_cells(width, height, cells);
                ticks = 0;
                continue;
            } else {
                clear_screen_on_exit = false;
                printf("Game over - all dead.\n");
                exit(0);
            }
        }

        if (eof()) {
            clear_screen_on_exit = false;
            exit(0);
        }

        for (int i = 0; i < HISTORY_LENGTH; i++) {
            if (memcmp(cells, history[i], sizeof(u8) * width * height) == 0) {
                if (infinite) {
                    srand(++seed);
                    seed_cells(width, height, cells);
                    ticks = 0;
                    goto cont;
                } else {
                }
                clear_screen_on_exit = false;
                printf("Game over - stuck in a loop (same as %d ticks ago)\n.", HISTORY_LENGTH - i);
                exit(0);
            }

            if (i < (HISTORY_LENGTH - 1)) {
                memcpy(history[i], history[i+1], sizeof(u8) * width * height);
            } else {
                memcpy(history[i], cells, sizeof(u8) * width * height);
            }
        }

        printf(CLEAR_SCROLLBACK);
        print_cells(width, height, cells, live_cells);
        if (shape[0]) {
            printf(" Shape: %s", shape);
        } else {
            printf(" Seed: %d", seed);
        }
        printf(" Tick: %zu", ticks);
        printf(CLEAR_LINE); // dead charaters when the line with grows shorter

        fflush(stdout);
        if (tps == 0) {
            clear_screen_on_exit = false;
            break;
        }
        msleep(1000/tps);

    cont:
        continue;
    }

    cleanup();
}
