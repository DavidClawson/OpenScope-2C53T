/*
 * OpenScope 2C53T: emulator viewer with a key panel (SDL3)
 *
 * Shows the LCD of the firmware running in Renode (/tmp/openscope_fb.bin,
 * written by renode/st7789v_peripheral.py) above a panel of the
 * instrument's keys. Click a key, or use the keyboard:
 *
 *   M=MENU  A=AUTO  S=SAVE (also saves a screenshot)  1=CH1  2=CH2  P=PRM
 *   Space=SELECT  T=TRIGGER  V=MOVE  Arrows  Enter=OK
 *   F12 (or the SHOT key)=screenshot only  Q/Esc=quit
 *
 * How a press reaches the firmware: the emulator build ignores the GPIO key
 * matrix (button_scan.c: Renode models no pull-ups, so the matrix read as
 * every key held) and takes keys from a one-byte mailbox instead,
 * g_emu_key in src/main.c (button id + 1). This viewer writes it through
 * Renode's monitor on localhost:1234 (`sysbus WriteByte <addr> n`). Until
 * 2026-11-27 it wrote /tmp/openscope_buttons.txt for file-backed GPIO
 * models, which that build no longer reads, so its keys did nothing.
 *
 * The mailbox holds one key, and emulated time runs slower than the clock,
 * so presses are queued: the viewer reads the mailbox back and writes the
 * next key only once the firmware has taken the last one. Every 250 ms it
 * also writes an LCD NOP, which makes the LCD model write out the frame.
 *
 * Build:  make            (needs SDL3 3.2 or later: brew install sdl3)
 * Run:    ./run_emulator.sh   (builds the image, starts Renode, opens this)
 *   or by hand: cd firmware && make renode-interactive, then ./lcd_viewer
 *   ./lcd_viewer [scale] [key ...]  LCD scale 1-4 (default 2); key names
 *                    (menu, move, trigger, ok, ...) are pressed in order
 *                    once Renode answers, e.g. ./lcd_viewer menu menu menu
 *   RENODE_MONITOR_PORT=n  if the monitor is not on 1234
 */

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define LCD_W       320
#define LCD_H       240
#define FB_SIZE     (LCD_W * LCD_H * 2)  /* RGB565, little endian */
#define FB_PATH     "/tmp/openscope_fb.bin"
#define TARGET_FPS  30
#define MS          1000000ULL           /* ns per ms */

/* ── Colours (0xRRGGBB) ──────────────────────────────────────── */

#define C_BG        0x15191Eu
#define C_KEY       0x2A3038u
#define C_KEY_EDGE  0x444C56u
#define C_KEY_LIT   0x2F6FD0u
#define C_TEXT      0xE6EDF3u
#define C_DIM       0x8B949Eu
#define C_CH1       0xF2E14Cu            /* the scope's trace colours */
#define C_CH2       0x4CD8F2u
#define C_OK        0x56D364u
#define C_WARN      0xE3B341u
#define C_ERR       0xF85149u

/* ── Keys ────────────────────────────────────────────────────── */

/* Button ids: button_id_t in firmware/src/ui/ui.h (the mailbox takes id + 1). */
enum { ID_CH1 = 1, ID_CH2, ID_MOVE, ID_SELECT, ID_TRIGGER, ID_PRM, ID_AUTO,
       ID_SAVE, ID_MENU, ID_UP, ID_DOWN, ID_LEFT, ID_RIGHT, ID_OK };

enum { BLK_FUNC, BLK_ARROWS };

typedef struct {
    const char *label;      /* on the key */
    const char *hint;       /* keyboard shortcut, small, under the label */
    const char *name;       /* for the terminal log */
    uint8_t     id;
    SDL_Keycode key, key2;
    uint8_t     block, col, row;
    uint32_t    color;
    SDL_FRect   r;          /* placed by layout() */
    Uint64      lit_until;
} pkey_t;

static pkey_t keys[] = {
    { "MENU",    "M",     "MENU",    ID_MENU,    SDLK_M,      0,           BLK_FUNC,   0, 0, C_TEXT, {0}, 0 },
    { "AUTO",    "A",     "AUTO",    ID_AUTO,    SDLK_A,      0,           BLK_FUNC,   1, 0, C_TEXT, {0}, 0 },
    { "SAVE",    "S",     "SAVE",    ID_SAVE,    SDLK_S,      0,           BLK_FUNC,   2, 0, C_TEXT, {0}, 0 },
    { "CH1",     "1",     "CH1",     ID_CH1,     SDLK_1,      0,           BLK_FUNC,   0, 1, C_CH1,  {0}, 0 },
    { "CH2",     "2",     "CH2",     ID_CH2,     SDLK_2,      0,           BLK_FUNC,   1, 1, C_CH2,  {0}, 0 },
    { "PRM",     "P",     "PRM",     ID_PRM,     SDLK_P,      0,           BLK_FUNC,   2, 1, C_TEXT, {0}, 0 },
    { "SELECT",  "Space", "SELECT",  ID_SELECT,  SDLK_SPACE,  0,           BLK_FUNC,   0, 2, C_TEXT, {0}, 0 },
    { "TRIGGER", "T",     "TRIGGER", ID_TRIGGER, SDLK_T,      0,           BLK_FUNC,   1, 2, C_TEXT, {0}, 0 },
    { "MOVE",    "V",     "MOVE",    ID_MOVE,    SDLK_V,      0,           BLK_FUNC,   2, 2, C_TEXT, {0}, 0 },
    { "^",       "Up",    "UP",      ID_UP,      SDLK_UP,     0,           BLK_ARROWS, 1, 0, C_TEXT, {0}, 0 },
    { "<",       "Left",  "LEFT",    ID_LEFT,    SDLK_LEFT,   0,           BLK_ARROWS, 0, 1, C_TEXT, {0}, 0 },
    { "OK",      "Enter", "OK",      ID_OK,      SDLK_RETURN, SDLK_KP_ENTER, BLK_ARROWS, 1, 1, C_TEXT, {0}, 0 },
    { ">",       "Right", "RIGHT",   ID_RIGHT,   SDLK_RIGHT,  0,           BLK_ARROWS, 2, 1, C_TEXT, {0}, 0 },
    { "v",       "Down",  "DOWN",    ID_DOWN,    SDLK_DOWN,   0,           BLK_ARROWS, 1, 2, C_TEXT, {0}, 0 },
    /* Not an instrument key: a screenshot of the LCD (id 0 is never sent). */
    { "SHOT",    "F12",   "SHOT",    0,          SDLK_F12,    0,           BLK_ARROWS, 2, 2, C_DIM,  {0}, 0 },
};
#define NKEYS ((int)(sizeof keys / sizeof keys[0]))

/* Layout (window pixels) */
#define KEY_W       118.0f
#define KEY_H       40.0f
#define PAD_W       64.0f   /* arrow keys */
#define GAP         8.0f
#define BLOCK_GAP   28.0f
#define PANEL_TOP   16.0f
#define PANEL_H     206.0f
#define MIN_WIN_W   640

static void layout(int win_w, float lcd_h)
{
    const float func_w  = 3 * KEY_W + 2 * GAP;
    const float arrow_w = 3 * PAD_W + 2 * GAP;
    const float x0 = ((float)win_w - (func_w + BLOCK_GAP + arrow_w)) / 2.0f;
    const float x1 = x0 + func_w + BLOCK_GAP;
    const float y0 = lcd_h + PANEL_TOP;
    for (int i = 0; i < NKEYS; i++) {
        pkey_t *k = &keys[i];
        const float w = (k->block == BLK_FUNC) ? KEY_W : PAD_W;
        k->r.x = ((k->block == BLK_FUNC) ? x0 : x1) + k->col * (w + GAP);
        k->r.y = y0 + k->row * (KEY_H + GAP);
        k->r.w = w;
        k->r.h = KEY_H;
    }
}

/* ── Renode monitor link (the g_emu_key mailbox) ─────────────── */

typedef enum { MON_OFF, MON_ADDR, MON_IDLE, MON_READ } mon_state_t;

static struct {
    int         fd;
    int         port;
    mon_state_t st;
    uint32_t    addr;           /* g_emu_key */
    bool        no_symbol;      /* the ELF has no g_emu_key: not an emu build */
    Uint64      t_sent, t_next, t_try, t_flush;
    char        rx[8192];
    size_t      rxn;
    uint8_t     q[32];          /* queued key indexes */
    unsigned    qh, qt;
} mon = { .fd = -1, .port = 1234 };

static unsigned mon_queued(void) { return (mon.qt - mon.qh) & 31u; }

static void mon_drop(void)
{
    if (mon.fd >= 0) close(mon.fd);
    mon.fd = -1;
    mon.st = MON_OFF;
    mon.addr = 0;
    mon.rxn = 0;
}

static void mon_send(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (mon.fd < 0 || n <= 0) return;
    if (send(mon.fd, buf, (size_t)n, 0) < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        mon_drop();
}

static void mon_connect(Uint64 now)
{
    if (mon.fd >= 0 || now < mon.t_try) return;
    mon.t_try = now + 1000 * MS;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_in sa = { 0 };
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)mon.port);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) {
        close(fd);
        return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    mon.fd = fd;
    mon.st = MON_ADDR;
    mon.no_symbol = false;
    mon.t_sent = now;
    mon_send("sysbus GetSymbolAddress \"g_emu_key\"\n");
    printf("[RENODE]  connected to the monitor on localhost:%d\n", mon.port);
}

static void light(uint8_t idx, Uint64 now, unsigned ms)
{
    const Uint64 until = now + ms * MS;
    if (keys[idx].lit_until < until) keys[idx].lit_until = until;
}

/* A monitor reply line. Replies are bare hex numbers: GetSymbolAddress
 * answers with 16 digits, ReadByte with 2. The monitor echoes our commands,
 * whose address is written with 8 digits, so the echo never matches. */
static void mon_line(char *line, Uint64 now)
{
    if (mon.st == MON_ADDR && (strstr(line, "No symbol") || strstr(line, "rror")))
        mon.no_symbol = true;

    for (char *p = line; (p = strstr(p, "0x")) != NULL; p += 2) {
        if (p > line && isalnum((unsigned char)p[-1])) continue;
        size_t d = 0;
        while (isxdigit((unsigned char)p[2 + d])) d++;
        if (isalnum((unsigned char)p[2 + d])) continue;
        const unsigned long long v = strtoull(p + 2, NULL, 16);

        if (mon.st == MON_ADDR && d == 16) {
            mon.addr = (uint32_t)v;
            mon.st = MON_IDLE;
            printf("[RENODE]  g_emu_key at 0x%08X\n", mon.addr);
        } else if (mon.st == MON_READ && d == 2) {
            mon.st = MON_IDLE;
            if (v == 0 && mon_queued() > 0) {
                const uint8_t idx = mon.q[mon.qh];
                mon.qh = (mon.qh + 1u) & 31u;
                mon_send("sysbus WriteByte 0x%08X %u\n", mon.addr, keys[idx].id + 1u);
                light(idx, now, 160);
                printf("[PRESS]   %s\n", keys[idx].name);
                mon.t_next = now + 40 * MS;
            } else {
                mon.t_next = now + 50 * MS;   /* the last key is not taken yet */
            }
        }
    }
}

static void mon_service(Uint64 now)
{
    mon_connect(now);
    if (mon.fd < 0) return;

    for (;;) {
        if (mon.rxn == sizeof mon.rx) mon.rxn = 0;      /* a line too long: drop it */
        ssize_t n = recv(mon.fd, mon.rx + mon.rxn, sizeof mon.rx - mon.rxn, 0);
        if (n > 0) {
            mon.rxn += (size_t)n;
            char *start = mon.rx, *nl;
            while ((nl = memchr(start, '\n', mon.rxn - (size_t)(start - mon.rx))) != NULL) {
                *nl = '\0';
                mon_line(start, now);
                start = nl + 1;
            }
            mon.rxn -= (size_t)(start - mon.rx);
            memmove(mon.rx, start, mon.rxn);
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            printf("[RENODE]  monitor closed\n");
            mon_drop();
            return;
        }
        break;
    }

    if ((mon.st == MON_ADDR || mon.st == MON_READ) && now - mon.t_sent > 2000 * MS) {
        if (mon.st == MON_ADDR) {
            mon.t_sent = now;
            mon_send("sysbus GetSymbolAddress \"g_emu_key\"\n");
        } else {
            mon.st = MON_IDLE;
        }
    }
    /* The LCD model writes FB_PATH only every 5000 pixels, so the end of a
     * redraw on a still screen (a menu highlight) stays unwritten; an LCD
     * NOP makes it write the frame (renode/st7789v_peripheral.py). The
     * model keeps its pixel state across it, so a draw in progress goes on. */
    if (mon.addr != 0 && now >= mon.t_flush) {
        mon.t_flush = now + 250 * MS;
        mon_send("sysbus WriteWord 0x6001FFFE 0x0000\n");
    }
    if (mon.st == MON_IDLE && mon_queued() > 0 && now >= mon.t_next) {
        mon.st = MON_READ;
        mon.t_sent = now;
        mon_send("sysbus ReadByte 0x%08X\n", mon.addr);
    }
}

static void press(int idx, Uint64 now)
{
    if (keys[idx].id == 0) return;                      /* SHOT: not a key */
    if (mon_queued() == 31u) return;                    /* full: drop it */
    mon.q[mon.qt] = (uint8_t)idx;
    mon.qt = (mon.qt + 1u) & 31u;
    light((uint8_t)idx, now, 120);
}

/* ── Screenshots ─────────────────────────────────────────────── */

/* 24-bit BMP of the last frame, at the next free
 * /tmp/openscope_screenshot_NNN.bmp (earlier ones are kept). */
static void save_screenshot(const uint8_t *fb)
{
    char path[64];
    struct stat st;
    int i = 0;
    do {
        snprintf(path, sizeof path, "/tmp/openscope_screenshot_%03d.bmp", i++);
    } while (stat(path, &st) == 0 && i < 1000);

    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return; }

    const uint32_t row_padded = (LCD_W * 3 + 3) & ~3u;
    const uint32_t file_size = 54 + row_padded * LCD_H;
    uint8_t hdr[54] = { 'B', 'M' };
    hdr[2] = (uint8_t)file_size;         hdr[3] = (uint8_t)(file_size >> 8);
    hdr[4] = (uint8_t)(file_size >> 16); hdr[5] = (uint8_t)(file_size >> 24);
    hdr[10] = 54;                        /* pixel data offset */
    hdr[14] = 40;                        /* DIB header size */
    hdr[18] = (uint8_t)LCD_W;            hdr[19] = (uint8_t)(LCD_W >> 8);
    hdr[22] = (uint8_t)LCD_H;            hdr[23] = (uint8_t)(LCD_H >> 8);
    hdr[26] = 1;                         /* planes */
    hdr[28] = 24;                        /* bits per pixel */
    fwrite(hdr, 1, sizeof hdr, f);

    uint8_t row[LCD_W * 3 + 4] = { 0 };
    for (int y = LCD_H - 1; y >= 0; y--) {          /* bottom-up, BGR */
        for (int x = 0; x < LCD_W; x++) {
            const uint16_t c = (uint16_t)(fb[(y * LCD_W + x) * 2] | (fb[(y * LCD_W + x) * 2 + 1] << 8));
            row[x * 3 + 0] = (uint8_t)((c & 0x1F) * 255 / 31);
            row[x * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
            row[x * 3 + 2] = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
        }
        fwrite(row, 1, row_padded, f);
    }
    fclose(f);
    printf("[SCREENSHOT] Saved: %s\n", path);
}

/* A panel key, clicked or typed: SHOT saves the LCD; SAVE also saves it (the
 * instrument has no screenshot store, so this is the emulator's stand-in);
 * every other key is queued for the firmware. fb is NULL before a frame. */
static void activate(int idx, Uint64 now, const uint8_t *fb)
{
    if ((keys[idx].id == 0 || keys[idx].id == ID_SAVE) && fb != NULL)
        save_screenshot(fb);
    if (keys[idx].id == 0)
        light((uint8_t)idx, now, 160);
    else
        press(idx, now);
}

/* ── Drawing ─────────────────────────────────────────────────── */

static void color(SDL_Renderer *r, uint32_t rgb)
{
    SDL_SetRenderDrawColor(r, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
}

static float text_w(const char *s, float scale)
{
    return (float)strlen(s) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

static void text(SDL_Renderer *r, float x, float y, float scale, uint32_t rgb, const char *s)
{
    color(r, rgb);
    SDL_SetRenderScale(r, scale, scale);
    SDL_RenderDebugText(r, x / scale, y / scale, s);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static void draw_key(SDL_Renderer *r, const pkey_t *k, bool lit)
{
    color(r, lit ? C_KEY_LIT : C_KEY);
    SDL_RenderFillRect(r, &k->r);
    color(r, C_KEY_EDGE);
    SDL_RenderRect(r, &k->r);
    text(r, k->r.x + (k->r.w - text_w(k->label, 2)) / 2, k->r.y + 6, 2, k->color, k->label);
    text(r, k->r.x + (k->r.w - text_w(k->hint, 1)) / 2, k->r.y + 28, 1, C_DIM, k->hint);
}

static bool inside(const SDL_FRect *rc, float x, float y)
{
    return x >= rc->x && x < rc->x + rc->w && y >= rc->y && y < rc->y + rc->h;
}

static void draw_status(SDL_Renderer *r, float y)
{
    char s[128];
    uint32_t c;
    if (mon.fd < 0) {
        snprintf(s, sizeof s, "Renode: not connected (localhost:%d). Start it with ./run_emulator.sh", mon.port);
        c = C_WARN;
    } else if (mon.st == MON_ADDR) {
        if (mon.no_symbol) {
            snprintf(s, sizeof s, "Renode: no g_emu_key in the image. Build it with 'make emu'");
            c = C_ERR;
        } else {
            snprintf(s, sizeof s, "Renode: connected, looking up g_emu_key...");
            c = C_WARN;
        }
    } else {
        unsigned q = mon_queued();
        if (q > 0)
            snprintf(s, sizeof s, "Renode: connected, keys go to g_emu_key (0x%08X), %u queued", mon.addr, q);
        else
            snprintf(s, sizeof s, "Renode: connected, keys go to g_emu_key (0x%08X)", mon.addr);
        c = C_OK;
    }
    text(r, keys[0].r.x, y, 1, c, s);
    text(r, keys[0].r.x, y + 14, 1, C_DIM,
         "Click a key or type the letter under it. S also saves a screenshot. Q: quit");
}

/* ── Main ────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* [scale] [key ...]: a number is the LCD scale; names (menu, move, ok,
     * ...) are pressed in order once Renode answers, for scripts. */
    int scale = 2;
    int first_key = 1;
    if (argc > 1 && isdigit((unsigned char)argv[1][0])) {
        scale = atoi(argv[1]);
        if (scale < 1 || scale > 4) scale = 2;
        first_key = 2;
    }
    const char *port = getenv("RENODE_MONITOR_PORT");
    if (port && atoi(port) > 0) mon.port = atoi(port);
    signal(SIGPIPE, SIG_IGN);   /* a closed monitor socket must not kill us */
    setvbuf(stdout, NULL, _IOLBF, 0);   /* the log reads live when redirected */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const float lcd_w = (float)(LCD_W * scale), lcd_h = (float)(LCD_H * scale);
    const int win_w = (LCD_W * scale > MIN_WIN_W) ? LCD_W * scale : MIN_WIN_W;
    const int win_h = LCD_H * scale + (int)PANEL_H;

    SDL_Window *win = SDL_CreateWindow("OpenScope 2C53T (emulator)", win_w, win_h, 0);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24,
                                               SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H) : NULL;
    if (!tex) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    layout(win_w, lcd_h);

    for (int a = first_key; a < argc; a++) {
        int found = -1;
        for (int i = 0; i < NKEYS; i++)
            if (keys[i].id != 0 && SDL_strcasecmp(argv[a], keys[i].name) == 0) found = i;
        if (found < 0) {
            fprintf(stderr, "unknown key '%s' (menu auto save ch1 ch2 prm select trigger "
                            "move up down left right ok)\n", argv[a]);
            SDL_Quit();
            return 2;
        }
        press(found, 0);
    }

    static uint8_t fb_raw[FB_SIZE], fb_new[FB_SIZE], rgb[LCD_W * LCD_H * 3];
    bool have_frame = false;
    int mouse_key = -1;
    bool running = true;
    const Uint64 frame_ns = 1000000000ULL / TARGET_FPS;
    const SDL_FRect lcd_rect = { ((float)win_w - lcd_w) / 2, 0, lcd_w, lcd_h };

    printf("OpenScope emulator viewer (LCD %dx scale, SDL3)\n", scale);
    printf("  frames from %s, keys to Renode's monitor on localhost:%d\n", FB_PATH, mon.port);
    printf("  M A S 1 2 P Space T V, arrows, Enter; F12 screenshot; Q quit\n\n");

    while (running) {
        const Uint64 t0 = SDL_GetTicksNS();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
                const SDL_Keycode k = ev.key.key;
                if (k == SDLK_Q || k == SDLK_ESCAPE) {
                    running = false;
                } else {
                    for (int i = 0; i < NKEYS; i++)
                        if (k == keys[i].key || (keys[i].key2 && k == keys[i].key2)) {
                            activate(i, t0, have_frame ? fb_raw : NULL);
                            break;
                        }
                }
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
                for (int i = 0; i < NKEYS; i++)
                    if (inside(&keys[i].r, ev.button.x, ev.button.y)) {
                        mouse_key = i;
                        activate(i, t0, have_frame ? fb_raw : NULL);
                        break;
                    }
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
                mouse_key = -1;
            }
        }

        mon_service(t0);

        /* Re-read the frame every time (150 KB, cheap): the file's mtime has
         * one-second resolution, so a change within the same second as the
         * last one used to be missed until the next change. */
        SDL_IOStream *io = SDL_IOFromFile(FB_PATH, "rb");
        if (io) {
            const size_t n = SDL_ReadIO(io, fb_new, FB_SIZE);
            SDL_CloseIO(io);
            if (n == FB_SIZE && (!have_frame || memcmp(fb_new, fb_raw, FB_SIZE) != 0)) {
                memcpy(fb_raw, fb_new, FB_SIZE);
                for (int i = 0; i < LCD_W * LCD_H; i++) {
                    const uint16_t c = (uint16_t)(fb_raw[i * 2] | (fb_raw[i * 2 + 1] << 8));
                    rgb[i * 3 + 0] = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
                    rgb[i * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
                    rgb[i * 3 + 2] = (uint8_t)((c & 0x1F) * 255 / 31);
                }
                SDL_UpdateTexture(tex, NULL, rgb, LCD_W * 3);
                have_frame = true;
            }
        }

        color(ren, C_BG);
        SDL_RenderClear(ren);
        if (have_frame) {
            SDL_RenderTexture(ren, tex, NULL, &lcd_rect);
        } else {
            color(ren, 0x000000);
            SDL_RenderFillRect(ren, &lcd_rect);
            const char *w = "waiting for the LCD (" FB_PATH ")";
            text(ren, lcd_rect.x + (lcd_w - text_w(w, 1)) / 2, lcd_h / 2, 1, C_DIM, w);
        }
        for (int i = 0; i < NKEYS; i++)
            draw_key(ren, &keys[i], i == mouse_key || keys[i].lit_until > t0);
        draw_status(ren, keys[NKEYS - 1].r.y + KEY_H + 14.0f);
        SDL_RenderPresent(ren);

        const Uint64 elapsed = SDL_GetTicksNS() - t0;
        if (elapsed < frame_ns) SDL_DelayNS(frame_ns - elapsed);
    }

    mon_drop();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
