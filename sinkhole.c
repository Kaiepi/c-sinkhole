#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <term.h>
#include <curses.h>
#include <signal.h>
#include <time.h>

#ifndef TIOCGWINSZ
#error "Don't know how to get the size of a terminal. Please raise an issue!"
#endif

#define PADDING 2

#define COLOR_DEFAULT 9
#define COLOR_BRIGHT_FG 90
#define COLOR_BRIGHT_BG 100

#define TERM "xterm-1003"
#define PUSH_TITLE "22t"
#define SET_TITLE "\b"
#define POP_TITLE "23t"
#define CSI "\e["
#define CUP "H"
#define SGR "m"
#define MOUSE_REPORT "M"

typedef struct field {
    int x, y;
    int w, h;
    int fg;
    int bg;

    struct field *next;
    struct field *prev;
} FIELD;

static FIELD *root;

static int
next_color(int color) {
    switch (color) {
        case COLOR_RED:
            return COLOR_YELLOW;
        case COLOR_YELLOW:
            return COLOR_GREEN;
        case COLOR_GREEN:
            return COLOR_CYAN;
        case COLOR_CYAN:
            return COLOR_BLUE;
        case COLOR_BLUE:
            return COLOR_MAGENTA;
        case COLOR_MAGENTA:
            return COLOR_RED;
        default:
            return COLOR_DEFAULT;
    }
}

static void
init_root(int w, int h)
{
    FIELD *field;
    int x, y;

    root = malloc(sizeof(FIELD));
    root->x = 0;
    root->y = 0;
    root->w = w;
    root->h = h;
    root->fg = COLOR_RED;
    root->bg = next_color(COLOR_RED);
    root->prev = NULL;

    field = root;
    while (w > PADDING * 4 && h > PADDING * 4) {
        FIELD *next = malloc(sizeof(FIELD));
        next->x = field->x + PADDING * 2;
        next->y = field->y + PADDING * 2;
        next->w = w -= PADDING * 4;
        next->h = h -= PADDING * 4;
        next->fg = next_color(field->fg);
        next->bg = next_color(field->bg);
        next->prev = field;
        field->next = next;
        field = field->next;
    }
    field->next = NULL;
}

static void
destroy_field(FIELD *field)
{
    if (field->next)
        destroy_field(field->next);
    free(field);
}

static void
destroy_root(void)
{
    destroy_field(root);
}

static void
move_field(FIELD *field,
    const int x, const int min_x, const int max_x,
    const int y, const int min_y, const int max_y)
{
    if (field == NULL)
        return;
    else {
        if (x < min_x)
            field->x = min_x;
        else if (x < field->x)
            field->x = x;
        else if (x > max_x - field->w)
            field->x = max_x - field->w;
        else if (x > field->x)
            field->x = x;

        if (y < min_y)
            field->y = min_y;
        else if (y < field->y)
            field->y = y;
        else if (y > max_y - field->h)
            field->y = max_y - field->h;
        else if (y > field->y)
            field->y = y;

        move_field(field->next,
            x, field->x + PADDING, field->x + field->w - PADDING,
            y, field->y + PADDING, field->y + field->h - PADDING);
    }
}

static void
move_root(int x, int y)
{
    move_field(root->next, x, PADDING, root->w - PADDING, y, PADDING, root->h - PADDING);
}

static void
recolor_root()
{
    FIELD *f;
    for (f = root; f; f = f->next) {
        f->fg = next_color(f->fg);
        f->bg = next_color(f->bg);
    }
}

static void
change_color(int fg, int bg)
{
    printf(CSI "%d" ";" "%d" SGR, COLOR_BRIGHT_FG + fg, COLOR_BRIGHT_BG + bg);
}

static void
print_row(FIELD *field, int y)
{
    if (y == field->y || y == field->y + field->h) {
        int i;
        change_color(field->fg, field->bg);
        for (i = 0; i < field->w; ++i)
            printf("▓");
    }
    else if (y == field->y + 1 || y == field->y + field->h - 1) {
        int i;
        change_color(field->fg, field->bg);
        printf("▓");
        for (i = 1; i < field->w - 1; ++i)
            printf("▒");
        printf("▓");
    }
    else if (field->next && y >= field->next->y && y <= field->next->y + field->next->h) {
        int i;
        change_color(field->fg, field->bg);
        printf("▓▒");
        for (i = field->x + 2; i < field->next->x; ++i)
            printf("░");
        print_row(field->next, y);
        change_color(field->fg, field->bg);
        for (i += field->next->w; i < field->x + field->w - 2; ++i)
            printf("░");
        printf("▒▓");
    }
    else {
        int i;
        change_color(field->fg, field->bg);
        printf("▓▒");
        for (i = field->x + 2; i < field->x + field->w - 2; ++i)
            printf("░");
        printf("▒▓");
    }
}

static void
print_root(void)
{
    int i;
    for (i = 0; i <= root->h; ++i) {
        printf(CSI "%d" ";" "%d" CUP, i + 1, 0);
        print_row(root, i);
    }
    fflush(stdout);
}

static void
begin(void)
{
    struct termios tp;
    tcgetattr(STDIN_FILENO, &tp);
    tp.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);
    setterm(TERM);
    printf(CSI PUSH_TITLE);
    printf(CSI "2" ";" "sinkhole" SET_TITLE);
    putp(tparm(tigetstr("XM"), 1));
}

static void
resize(int signum)
{
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    destroy_root();
    init_root(window.ws_col, window.ws_row);
    print_root();
}

static void
end()
{
    putp(tparm(tigetstr("XM"), 0));
    destroy_root();
    change_color(COLOR_DEFAULT, COLOR_DEFAULT);
    printf(CSI "1" ";" "1" CUP);
    printf(CSI POP_TITLE);
}

static void
cleanup(int signum)
{
    end();
    exit(EXIT_SUCCESS);
}

int
main(void)
{
    time_t prev, cur;
    begin();
    init_root(tigetnum("cols"), tigetnum("lines"));
    signal(SIGWINCH, resize);
    signal(SIGINT, cleanup);
    print_root();
    prev = time(NULL);
    for (;;) {
        struct pollfd pfd[1] = { { .fd = STDIN_FILENO, .events = POLLIN } };
        int output = 0;
        if (poll(pfd, 1, 0) == 1) {
            /* XXX: Mouse coordinates are bounded by UCHAR_MAX-32.  Movement of
             * fields must be oriented around their top-left corner as a
             * consequence. */
            unsigned char x, y;
            if (ftell(stdin) >= sizeof(CSI) + sizeof(MOUSE_REPORT) + 3) {
                if (fscanf(stdin, CSI MOUSE_REPORT "C" "%c" "%c", &x, &y) == 2) {
                    x -= 32 + 1; /* x and y start at 1 */
                    y -= 32 + 1;
                    move_root(x, y);
                    output = 1;
                }
                else
                    (void)fgetc(stdin);
            }
        }
        if ((cur = time(NULL)) > prev) {
            prev = cur;
            recolor_root();
            output = 1;
        }
        if (output)
            print_root();
    }
    end();
    return EXIT_FAILURE;
}