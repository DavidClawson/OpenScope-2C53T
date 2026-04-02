# Resource Planning — RAM, Flash, and Module Strategy

*2026-03-29. Living document — update as hardware bringup reveals actual usage patterns.*

## Current Resource State

### RAM (256 KB SRAM)

| Allocation | Size | Category | Notes |
|-----------|------|----------|-------|
| shared_mem pool | 150 KB | Data buffers | Time-shared: FFT, persistence, screenshot, decode |
| FreeRTOS heap (ucHeap) | 32 KB | RTOS | Task stacks, queues, timers, semaphores |
| waterfall_buf | 20 KB | Display | FFT spectrogram (64 rows x 320 cols) |
| fft_sample_buf | 8 KB | DSP | 4096 x int16 FFT input |
| Everything else | ~10 KB | Misc | Scope state, globals, lookup tables, font refs |
| **Total used** | **~215 KB** | | **84% of 256 KB** |
| **Free** | **~41 KB** | | Available for growth |

### Flash (1 MB internal + 16 MB external SPI)

| Region | Size | Used | Notes |
|--------|------|------|-------|
| MCU flash (code + const) | 1 MB | 182 KB (18%) | Tons of headroom |
| SPI flash (W25Q128) | 16 MB | TBD | UI assets, config, screenshots, modules |

### FreeRTOS Heap Breakdown (32 KB)

| Consumer | Estimated Size | Notes |
|----------|---------------|-------|
| Display task stack | 2 KB (512 words) | Largest task — LCD rendering |
| Input task stack | 1 KB (256 words) | GPIO polling + button dispatch |
| Idle task stack | 512 B | Minimal |
| Timer service task | 1 KB | Runs health check + uptime callbacks |
| 2 queues | ~400 B | Display (20 items) + Input (15 items) |
| 2 timers | ~200 B | 1-second + 500ms health |
| **Allocated** | **~5 KB** | |
| **Free heap** | **~27 KB** | Available for dynamic alloc |

## The Shared Memory Pool Problem

The 150 KB `pool` in shared_mem is the single largest allocation. It exists because four features need large buffers but never simultaneously:

| Feature | Buffer Need | When Active |
|---------|------------|-------------|
| FFT (CMSIS) | 72 KB | Scope FFT/split/waterfall views |
| FFT (software radix-2) | 88 KB | Same, without CMSIS-DSP |
| Persistence | 64 KB | Scope with persistence overlay |
| Screenshot | 150 KB | One-shot capture (not streaming) |
| Protocol decode | 32 KB | Decode overlay active |

Currently the pool is always 150 KB (sized for the largest user: screenshot). This wastes RAM when you're just running the scope in time-domain mode and don't need any of these.

### Optimization: Tiered Pool

Instead of one fixed 150 KB pool, use tiered allocation:

```
Tier 0 (always):     0 KB pool — time-domain scope, meter, siggen
Tier 1 (FFT):       88 KB pool — allocated when entering FFT view
Tier 2 (persist):   64 KB pool — allocated when persistence enabled
Tier 3 (screenshot):150 KB pool — allocated briefly for capture, then freed
```

This can't use the FreeRTOS heap (fragmentation risk). Instead, use a simple arena allocator with a single static 150 KB backing buffer, but track what's currently loaded so we know the budget.

**Immediate win**: Shrink the pool to 88 KB (FFT max) and handle screenshot differently (see Hot Loading section below). This frees 62 KB instantly.

## Hot Loading Strategy

### Concept

On a Cortex-M4 without an MMU, "hot loading" means **time-sharing RAM between features that don't run concurrently.** The code for all features lives permanently in flash (only 18% used). What gets loaded/unloaded is the **working data** — buffers, lookup tables, state.

### Three Levels of Hot Loading

#### Level 1: Buffer Swapping (Already Have This)

The shared_mem pool already does this — FFT and persistence can't use the pool simultaneously. We just need to make it more explicit with init/deinit lifecycle:

```c
// User enters FFT view
shared_pool_claim(POOL_FFT);    // Allocates 88KB from pool
fft_init(&cfg);                  // Uses pool memory for twiddle factors

// User exits FFT view
fft_deinit();                    // Releases pool
shared_pool_release(POOL_FFT);   // 88KB available again
```

**Latency**: ~1ms (just zeroing memory). Invisible to user.

#### Level 2: Feature Module Load/Unload (New — Recommended)

For features like screenshot, Bode plot, compression test, alternator test:

```c
// User selects "Take Screenshot" from menu
screenshot_module_load();       // Claims 150KB from pool
lcd_capture_to_buffer();        // Reads current LCD state
flash_fs_write_bmp(buffer);     // Writes to SPI flash
screenshot_module_unload();     // Releases 150KB

// Total time: ~500ms (dominated by SPI flash write, not RAM)
```

The code for `screenshot_module_load()` is always in MCU flash. Only the 150KB data buffer is temporary. From the user's perspective: they press SAVE, see a brief "Saving..." popup, and it's done.

Same pattern for specialty modules:

```c
// User enters Component Tester from Settings
comp_test_module_load();        // Claims 8KB from pool for measurement buffers
// ... user interacts with component tester ...
comp_test_module_unload();      // Releases 8KB when exiting
```

**Latency**: 1-5ms for buffer allocation. Maybe 100-500ms for SPI flash I/O.

#### Level 3: External Module Loading from SPI Flash (Future — Advanced)

This is the most interesting possibility. The 16 MB SPI flash is barely used (system graphics + config). We could store **compiled module binaries** on the SPI flash and load them into RAM for execution:

```c
// User installs "K-Line Diagnostics" module via ESP32/WiFi
// Module is stored as a .mod file on SPI flash

module_t *mod = module_load("kline_diag.mod");  // Read from SPI flash into RAM
mod->init();                                     // Call module's init function
// ... user interacts with module UI ...
mod->deinit();                                   // Clean up
module_unload(mod);                              // Free RAM
```

This requires:
- Position-independent code (PIC) compilation: `-fPIC -msingle-pic-base`
- A module header format (magic, version, entry points, RAM requirements)
- A simple loader that reads the binary and patches the GOT (Global Offset Table)
- A stable module API that the module calls for LCD, buttons, ADC, etc.

**This is how the module system in module.yaml.example could actually work.** Users install modules via the ESP32 PWA, they're stored on SPI flash, and loaded into RAM on demand.

RAM budget for Level 3: a module binary of 8-16 KB code + 8-32 KB working data = 16-48 KB. Fits comfortably in the available RAM if we optimize the pool.

## RAM Budget After Optimization

| Scenario | Pool | Waterfall | FFT buf | Free | Available for modules |
|----------|------|-----------|---------|------|----------------------|
| Current | 150 KB | 20 KB | 8 KB | 41 KB | Limited |
| After pool shrink | 88 KB | 20 KB | 8 KB | 103 KB | Comfortable |
| After waterfall on-demand | 88 KB | 0 KB | 8 KB | 123 KB | Generous |
| Aggressive (pool + waterfall on-demand) | 0 KB* | 0 KB | 0 KB | 211 KB | Wide open |

*Pool at 0 means fully on-demand allocation — load FFT buffers only in FFT mode, etc.

## Core vs Nice-to-Have Feature Matrix

### Core (Must Always Work, RAM Always Reserved)

| Feature | RAM Cost | Notes |
|---------|----------|-------|
| Scope time-domain (2 channels) | ~2 KB | State + 1 screen of samples from FPGA |
| Trigger engine state | ~200 B | Mode, level, edge, status |
| Multimeter readings | ~200 B | Current value + min/max/avg |
| Signal generator DDS | ~1 KB | Phase accumulator + config |
| FreeRTOS kernel | ~5 KB | Tasks, queues, timers (from 32 KB heap) |
| Display rendering | ~1 KB | Current screen state, popup text |
| Watchdog / health | ~200 B | Task health array |
| **Total core** | **~10 KB** | Always allocated |

### On-Demand (Load When Needed, Unload When Done)

| Feature | RAM Cost | Load Trigger | Unload Trigger |
|---------|----------|-------------|----------------|
| FFT engine | 72-88 KB | Enter FFT/split/waterfall view | Exit to time-domain |
| Waterfall buffer | 20 KB | Enter waterfall view | Exit waterfall |
| Persistence | 64 KB | Enable in settings | Disable in settings |
| Screenshot | 150 KB | Press SAVE | Write complete |
| Protocol decode | 32 KB | Enable decode overlay | Disable overlay |
| Bode plot | 4 KB | Enter Bode mode | Exit Bode |
| Component tester | 8 KB | Enter component test | Exit component test |
| Compression test | 2 KB | Enter compression test | Exit |
| External modules | 16-48 KB | User selects module | User exits module |

### Design Rules

1. **Core features never fail to allocate.** Their RAM is statically reserved.
2. **On-demand features check before loading.** If not enough RAM, show "Feature requires X KB, Y KB available" and don't crash.
3. **Only one large on-demand feature at a time.** FFT and persistence share the pool — entering FFT mode auto-disables persistence.
4. **Screenshot is always possible** because it briefly claims the pool, captures, writes to flash, and releases — even if FFT is active (just pause FFT for 500ms).
5. **External modules declare their RAM needs** in their header. Loader rejects if insufficient.

## Flash Budget (16 MB SPI)

| Region | Size | Purpose |
|--------|------|---------|
| System graphics | ~2 MB | UI icons, backgrounds (original firmware assets) |
| Configuration | 4 KB | device_config_t serialized |
| Screenshots | ~1 MB | ~6 BMP screenshots at 150 KB each |
| Module storage | ~8 MB | Installable modules (.mod files) |
| Waveform references | ~2 MB | Saved waveforms, mask templates |
| Vehicle definitions | ~2 MB | K-Line/OBD databases for automotive modules |
| **Free** | **~1 MB** | Growth |

## Implementation Priority

1. **Make shared_mem pool lifecycle explicit** — add claim/release with feature ID tracking
2. **Move waterfall to on-demand** — only allocate when entering waterfall view
3. **Implement screenshot as load/capture/unload** — no persistent shadow buffer
4. **Add RAM budget display** to About screen (free heap, pool status)
5. **Design module binary format** for Level 3 hot loading (future)
