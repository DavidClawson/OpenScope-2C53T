/*
 * OpenScope 2C53T — SDL3 LCD Viewer
 *
 * Reads the Renode framebuffer (/tmp/openscope_fb.bin) and displays it
 * in a native window at 30fps. Nearest-neighbor scaling for crisp pixels.
 *
 * Build:
 *   cc -O2 -o lcd_viewer lcd_viewer.c $(pkg-config --cflags --libs sdl3)
 *
 * Run:
 *   ./lcd_viewer              # 2x scale (default)
 *   ./lcd_viewer 3            # 3x scale
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
#define TARGET_FPS  30

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

    /* Nearest-neighbor scaling for crisp pixels */
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

    printf("OpenScope LCD Viewer (%dx%d @ %dx scale, SDL3)\n", LCD_W, LCD_H, scale);
    printf("Reading: %s\n", FB_PATH);
    printf("Keys: Q=quit, M=mode, arrows, Enter=OK, Space=select\n");

    while (running) {
        Uint64 t0 = SDL_GetTicksNS();

        /* Handle events */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                switch (ev.key.key) {
                case SDLK_Q:
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                case SDLK_M:
                    printf("[BTN] MENU\n");
                    break;
                case SDLK_A:
                    printf("[BTN] AUTO\n");
                    break;
                case SDLK_S:
                    printf("[BTN] SAVE\n");
                    break;
                case SDLK_UP:
                    printf("[BTN] UP\n");
                    break;
                case SDLK_DOWN:
                    printf("[BTN] DOWN\n");
                    break;
                case SDLK_LEFT:
                    printf("[BTN] LEFT\n");
                    break;
                case SDLK_RIGHT:
                    printf("[BTN] RIGHT\n");
                    break;
                case SDLK_RETURN:
                    printf("[BTN] OK\n");
                    break;
                case SDLK_SPACE:
                    printf("[BTN] SELECT\n");
                    break;
                case SDLK_P:
                    printf("[BTN] PRM\n");
                    break;
                case SDLK_1:
                    printf("[BTN] CH1\n");
                    break;
                case SDLK_2:
                    printf("[BTN] CH2\n");
                    break;
                default:
                    break;
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
                    /* Convert RGB565 → RGB888 */
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

        /* Render */
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        /* Frame rate limit */
        Uint64 elapsed = SDL_GetTicksNS() - t0;
        if (elapsed < frame_ns) {
            SDL_DelayNS(frame_ns - elapsed);
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
