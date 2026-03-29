/*
 * OpenScope 2C53T — SDL3 LCD Viewer + Interactive Input
 *
 * Reads the Renode framebuffer (/tmp/openscope_fb.bin) and displays it
 * in a native window at 30fps. Keyboard input is written to
 * /tmp/openscope_buttons.txt which Renode GPIO peripherals read.
 *
 * Build:
 *   make              (in emulator/ directory)
 *   # or: cc -O2 -o lcd_viewer lcd_viewer.c $(pkg-config --cflags --libs sdl3)
 *
 * Run:
 *   ./lcd_viewer              # 2x scale (default)
 *   ./lcd_viewer 3            # 3x scale
 *
 * Button mapping:
 *   M=MENU  A=AUTO  S=SAVE  1=CH1  2=CH2  P=PRM  Space=SELECT
 *   Arrows=UP/DOWN/LEFT/RIGHT  Enter=OK  Q/Esc=quit
 */

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LCD_W       320
#define LCD_H       240
#define FB_SIZE     (LCD_W * LCD_H * 2)  /* RGB565 */
#define FB_PATH     "/tmp/openscope_fb.bin"
#define BTN_PATH    "/tmp/openscope_buttons.txt"
#define TARGET_FPS  30

/* ── Button-to-GPIO mapping ──────────────────────────────────── */

typedef struct {
    SDL_Keycode key;
    char        port;   /* 'B' or 'C' */
    int         pin;
    const char *name;
} button_map_t;

static const button_map_t buttons[] = {
    { SDLK_M,      'C', 0, "MENU"   },
    { SDLK_A,      'C', 1, "AUTO"   },
    { SDLK_S,      'C', 2, "SAVE"   },
    { SDLK_1,      'C', 3, "CH1"    },
    { SDLK_2,      'C', 4, "CH2"    },
    { SDLK_P,      'C', 5, "PRM"    },
    { SDLK_SPACE,  'C', 6, "SELECT" },
    { SDLK_UP,     'B', 0, "UP"     },
    { SDLK_DOWN,   'B', 1, "DOWN"   },
    { SDLK_LEFT,   'B', 2, "LEFT"   },
    { SDLK_RIGHT,  'B', 3, "RIGHT"  },
    { SDLK_RETURN, 'B', 4, "OK"     },
};
#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

/* Write button state to shared file */
static void write_button_state(char port, int pin)
{
    FILE *f = fopen(BTN_PATH, "w");
    if (!f) return;
    if (port == 0) {
        fprintf(f, "IDLE\n");
    } else {
        fprintf(f, "%c %d\n", port, pin);
    }
    fclose(f);
}

/* Save current framebuffer as 24-bit BMP */
static int screenshot_count = 0;

static void save_screenshot(const uint8_t *fb, size_t fb_size)
{
    if (fb_size < (size_t)FB_SIZE) return;

    char path[64];
    snprintf(path, sizeof(path), "/tmp/openscope_screenshot_%03d.bmp", screenshot_count);

    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return; }

    /* 24-bit BMP (universally supported, no bitfield hassles) */
    uint32_t row_stride = LCD_W * 3;
    /* BMP rows must be 4-byte aligned */
    uint32_t row_padded = (row_stride + 3) & ~3u;
    uint32_t pixel_size = row_padded * LCD_H;
    uint32_t file_size = 54 + pixel_size;

    /* BMP file header (14 bytes) */
    uint8_t hdr[54];
    memset(hdr, 0, 54);
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2]  = (uint8_t)(file_size);
    hdr[3]  = (uint8_t)(file_size >> 8);
    hdr[4]  = (uint8_t)(file_size >> 16);
    hdr[5]  = (uint8_t)(file_size >> 24);
    hdr[10] = 54; /* pixel data offset */

    /* DIB header (40 bytes) */
    hdr[14] = 40; /* header size */
    hdr[18] = (uint8_t)(LCD_W);       hdr[19] = (uint8_t)(LCD_W >> 8);
    hdr[22] = (uint8_t)(LCD_H);       hdr[23] = (uint8_t)(LCD_H >> 8);
    hdr[26] = 1;   /* planes */
    hdr[28] = 24;  /* bits per pixel */
    /* compression = 0 (BI_RGB), image size can be 0 for BI_RGB */

    fwrite(hdr, 1, 54, f);

    /* BMP stores rows bottom-to-top, BGR order */
    uint8_t row_buf[LCD_W * 3 + 4]; /* +4 for padding */
    memset(row_buf, 0, sizeof(row_buf));

    for (int y = LCD_H - 1; y >= 0; y--) {
        for (int x = 0; x < LCD_W; x++) {
            uint16_t c = (uint16_t)(fb[((y * LCD_W) + x) * 2] |
                                    (fb[((y * LCD_W) + x) * 2 + 1] << 8));
            uint8_t r, g, b;
            r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
            g = (uint8_t)(((c >> 5)  & 0x3F) * 255 / 63);
            b = (uint8_t)(( c        & 0x1F) * 255 / 31);
            row_buf[x * 3 + 0] = b;  /* BMP is BGR */
            row_buf[x * 3 + 1] = g;
            row_buf[x * 3 + 2] = r;
        }
        fwrite(row_buf, 1, row_padded, f);
    }

    fclose(f);
    screenshot_count++;
    printf("[SCREENSHOT] Saved: %s\n", path);
}

/* Convert RGB565 (little-endian in file) to RGB888 */
static inline void rgb565_to_rgb888(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    *g = (uint8_t)(((c >> 5)  & 0x3F) * 255 / 63);
    *b = (uint8_t)(( c        & 0x1F) * 255 / 31);
}

int main(int argc, char *argv[])
{
    int scale = 2;
    if (argc > 1) {
        scale = atoi(argv[1]);
        if (scale < 1 || scale > 6) scale = 2;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("OpenScope 2C53T",
                                       LCD_W * scale, LCD_H * scale, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(ren, 1);

    SDL_Texture *tex = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
        LCD_W, LCD_H);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    uint8_t fb_raw[FB_SIZE];
    uint8_t rgb_buf[LCD_W * LCD_H * 3];
    time_t last_mtime = 0;
    int running = 1;
    Uint64 frame_ns = 1000000000ULL / TARGET_FPS;

    /* Initialize button state */
    write_button_state(0, 0);

    printf("OpenScope LCD Viewer (%dx%d @ %dx scale, SDL3)\n", LCD_W, LCD_H, scale);
    printf("Reading: %s\n", FB_PATH);
    printf("Buttons: %s\n", BTN_PATH);
    printf("\n");
    printf("  M=MENU   A=AUTO   S=SAVE   P=PRM\n");
    printf("  1=CH1    2=CH2    Space=SELECT\n");
    printf("  Arrows=navigate   Enter=OK   Q=quit\n\n");

    while (running) {
        Uint64 t0 = SDL_GetTicksNS();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;

            } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
                if (ev.key.key == SDLK_Q || ev.key.key == SDLK_ESCAPE) {
                    running = 0;
                    break;
                }
                /* Screenshot on SAVE key (also sends button to Renode) */
                if (ev.key.key == SDLK_S) {
                    save_screenshot(fb_raw, sizeof(fb_raw));
                }
                /* Find matching button and press it */
                for (int i = 0; i < (int)NUM_BUTTONS; i++) {
                    if (ev.key.key == buttons[i].key) {
                        write_button_state(buttons[i].port, buttons[i].pin);
                        printf("[PRESS]   %s\n", buttons[i].name);
                        break;
                    }
                }

            } else if (ev.type == SDL_EVENT_KEY_UP) {
                /* Release: any key-up clears the button state */
                for (int i = 0; i < (int)NUM_BUTTONS; i++) {
                    if (ev.key.key == buttons[i].key) {
                        write_button_state(0, 0);
                        printf("[RELEASE] %s\n", buttons[i].name);
                        break;
                    }
                }
            }
        }

        /* Check if framebuffer file was updated */
        struct stat st;
        if (stat(FB_PATH, &st) == 0 && st.st_mtime != last_mtime) {
            last_mtime = st.st_mtime;

            SDL_IOStream *io = SDL_IOFromFile(FB_PATH, "rb");
            if (io) {
                size_t n = SDL_ReadIO(io, fb_raw, FB_SIZE);
                SDL_CloseIO(io);

                if (n == FB_SIZE) {
                    for (int i = 0; i < LCD_W * LCD_H; i++) {
                        uint16_t c = (uint16_t)(fb_raw[i * 2] | (fb_raw[i * 2 + 1] << 8));
                        rgb565_to_rgb888(c,
                            &rgb_buf[i * 3],
                            &rgb_buf[i * 3 + 1],
                            &rgb_buf[i * 3 + 2]);
                    }
                    SDL_UpdateTexture(tex, NULL, rgb_buf, LCD_W * 3);
                }
            }
        }

        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        Uint64 elapsed = SDL_GetTicksNS() - t0;
        if (elapsed < frame_ns) {
            SDL_DelayNS(frame_ns - elapsed);
        }
    }

    /* Clean up button state on exit */
    write_button_state(0, 0);

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
