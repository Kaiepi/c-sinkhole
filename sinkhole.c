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

#define TERM "xterm-1003"
#define CSI "\e["
#define PUSH_TITLE "22t"
#define POP_TITLE "23t"
#define SET_TITLE "\b"
#define CUP "H"
#define SGR "m"
#define MOUSE_REPORT "M"

#define COLOR_R 196
#define COLOR_RO 202
#define COLOR_O 208
#define COLOR_OY 220
#define COLOR_Y 226
#define COLOR_YG 154
#define COLOR_G 46
#define COLOR_GB 44
#define COLOR_B 26
#define COLOR_BV 55
#define COLOR_V 90
#define COLOR_VR 125
#define COLOR_DEFAULT 9

typedef struct field {
    int x, y;
    int w, h;
    int fg, bg;
    struct field *next;
} FIELD;

static FIELD *root;

static int
next_color(const int color) {
    switch (color) {
        case COLOR_R:
            return COLOR_RO;
        case COLOR_RO:
            return COLOR_O;
        case COLOR_O:
            return COLOR_OY;
        case COLOR_OY:
            return COLOR_Y;
        case COLOR_Y:
            return COLOR_YG;
        case COLOR_YG:
            return COLOR_G;
        case COLOR_G:
            return COLOR_GB;
        case COLOR_GB:
            return COLOR_B;
        case COLOR_B:
            return COLOR_BV;
        case COLOR_BV:
            return COLOR_V;
        case COLOR_V:
            return COLOR_VR;
        case COLOR_VR:
            return COLOR_R;
        default:
            return COLOR_DEFAULT;
    }
}

static void
init_root(const int w, const int h)
{
    FIELD *field;
    int x, y;

    root = malloc(sizeof(FIELD));
    root->x = 0;
    root->y = 0;
    root->w = w;
    root->h = h;
    root->fg = COLOR_R;
    root->bg = next_color(COLOR_R);

    field = root;
    while (field->w > PADDING * 4 && field->h > PADDING * 4) {
        FIELD *next = malloc(sizeof(FIELD));
        next->x = field->x + PADDING * 2;
        next->y = field->y + PADDING * 2;
        next->w = field->w - PADDING * 4;
        next->h = field->h - PADDING * 4;
        next->fg = next_color(field->fg);
        next->bg = next_color(field->bg);
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
move_root(const int x, const int y)
{
    move_field(root->next, x, PADDING, root->w - PADDING, y, PADDING, root->h - PADDING);
}

static void
recolor_root(void)
{
    FIELD *f;
    for (f = root; f; f = f->next) {
        f->fg = next_color(f->fg);
        f->bg = next_color(f->bg);
    }
}

static void
change_color(const int fg, const int bg)
{
    printf(CSI "38;5;%d" ";" "48;5;%d" SGR, fg, bg);
}

static void
print_row(FIELD *field, const int y)
{
    int x;
    if (y == field->y || y == field->y + field->h) {
        change_color(field->fg, field->bg);
        for (x = 0; x < field->w; ++x)
            printf("▓");
    }
    else if (y == field->y + 1 || y == field->y + field->h - 1) {
        change_color(field->fg, field->bg);
        printf("▓");
        for (x = 1; x < field->w - 1; ++x)
            printf("▒");
        printf("▓");
    }
    else if (field->next && y >= field->next->y && y <= field->next->y + field->next->h) {
        change_color(field->fg, field->bg);
        printf("▓▒");
        for (x = field->x + 2; x < field->next->x; ++x)
            printf("░");
        print_row(field->next, y);
        change_color(field->fg, field->bg);
        for (x += field->next->w; x < field->x + field->w - 2; ++x)
            printf("░");
        printf("▒▓");
    }
    else {
        change_color(field->fg, field->bg);
        printf("▓▒");
        for (x = field->x + 2; x < field->x + field->w - 2; ++x)
            printf("░");
        printf("▒▓");
    }
}

static void
print_root(void)
{
    int y;
    for (y = 0; y <= root->h; ++y) {
        printf(CSI "%d" ";" "%d" CUP, y + 1, 0);
        print_row(root, y);
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
    printf("\e]" "2" ";" "sinkhole" SET_TITLE);
    putp(tparm(tigetstr("XM"), 1));
    init_root(tigetnum("cols"), tigetnum("lines"));
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
