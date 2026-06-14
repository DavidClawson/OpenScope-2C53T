# wf4 — decode + adversarial-verify of 80 generic-note functions (2026-06-13)

96 agents, 3.7M tokens, ~13min. Pipeline: per-function full register decode (D→3) →
adversarial verifier refutes every REIMPL_FAITHFUL / NA_OURS claim against our actual
`firmware/src/`. Render fns batched (4×7). Script: `scripts/wf_resolve_fs_drivers.js`.

## Outcome

- Headline **9.3% → 18.6%** (13/70 meaningful-resolved). D3 **107 → 180** (58%).
- **+2 genuine numerator**, both mislabeled EXMC-LCD primitives matching `lcd.c`:
  - `0x08019e6c` (was `scope_get_ch_offset`) = single 16-bit store to LCD data port `0x60020000` → R2/V1
  - `0x08019e78` (was `scope_set_ch_coupling`) = cached word → LCD command reg `0x6001FFFE` → R2/V1
- **Verifier overturned 19/52 decode claims** (the honesty guard): 17× `NA_OURS→ABSENT`,
  1× `REIMPL→DECODE_ONLY` (`scope_set_ch_offset` window-setter, not faithful), 1× other.

## The ABSENT subsystem (real device behavior our firmware LACKS)

`flash_fs.c` is **stubbed** (every `f_*` is a TODO) and vendor `ff.c` is **not in the
Makefile**, so the decoders' "we replace FatFs with a library" was false. These are
genuinely missing features — file browser, screenshot save, settings persistence:

- `0x0802a664` **fs_fat_traverse** (5748B)
- `0x0802c250` **fs_directory_operations** (3138B)
- `0x0802f6d8` **spi_flash_fat_update** (1196B)
- `0x0802bde4` **fs_write_data** (1130B)
- `0x0802e12c` **spi_flash_create_entry** (1026B)
- `0x0803586c` **spi_flash_cluster_chain_read** (1016B)
- `0x0802ff18` **spi_flash_cluster_io** (994B)
- `0x080337a0` **spi_flash_read_sector_cached** (880B)
- `0x0802e530` **spi_flash_rename_entry** (652B)
- `0x0802d534` **spi_flash_fs_sync** (572B)
- `0x0802df20` **spi_flash_delete_entry** (524B)
- `0x0802e7bc` **spi_flash_fs_operations** (330B)
- `0x0802f16c` **spi_flash_write_block** (320B)
- `0x08034878` **fs_init_sequence** (318B)
- `0x08036934` **fs_flush_and_sync** (282B)
- `0x08029b80` **fs_read_sector** (276B)
- `0x080027e8` **lcd_read_image_data** (248B)
- `0x0802ea08` **spi_flash_init_fs** (242B)
- `0x0802d80c` **spi_flash_format_check** (172B)
- `0x0802f36c` **spi2_page_program** (118B)
- `0x0802f11c` **spi2_wait_busy** (78B)
- `0x0802ee9c` **spi2_sector_erase** (76B)
- `0x080304f0` **fs_close_file** (50B)
- `0x0802f344` **spi2_write_enable** (38B)

## NA_OURS (18)

- `0x080278e4` usb_endpoint_handler → RNA/VNA
- `0x0802d8b8` spi_flash_directory_scan → RNA/VNA
- `0x0802dcbc` spi_flash_read_dir_entry → RNA/VNA
- `0x0802a078` fs_create_file → RNA/VNA
- `0x08033cfc` display_alloc_buffer → RNA/VNA
- `0x08035ed4` spi_flash_fs_mount → RNA/VNA
- `0x0802a2d4` fs_extend_cluster → RNA/VNA
- `0x0802e908` spi_flash_error_handler → RNA/VNA
- `0x0802912c` fs_format_filename → RNA/VNA
- `0x08029cc4` fs_write_entry → RNA/VNA
- `0x0803e538` scope_set_sampling_params → RNA/VNA
- `0x0802d774` spi_flash_cache_invalidate → RNA/VNA
- `0x0803e600` scope_set_measure_config → RNA/VNA
- `0x0802dc40` spi_flash_alloc_cluster → RNA/VNA
- `0x08029da0` fs_init_header → RNA/VNA
- `0x08001830` FUN_08001830 → RNA/VNA
- `0x0802985c` fs_check_path → RNA/VNA
- `0x0800bcd4` mode_dispatch_indirect → RNA/VNA

## DIVERGENT (4)

- `0x0802920c` fs_read_file_data → RNA/VNA
- `0x08029e0c` fs_open_file → RNA/VNA
- `0x0803bee0` dma1_configure → R0/V0
- `0x0802f2ac` spi2_page_write_loop → R1/V0

## REIMPL_FAITHFUL (3)

- `0x08036084` spi_flash_fs_format → R2/V0
- `0x08019e78` scope_set_ch_coupling → R2/V1
- `0x08019e6c` scope_get_ch_offset → R2/V1

## FPGA_BLOCKED (2)

- `0x08022d40` FUN_08022d40 → R0/V0
- `0x08034070` fs_close_helper → R0/V0

## DECODE_ONLY (1)

- `0x08019e18` scope_set_ch_offset → R1/V0

## Render batch (28) — all NA_OURS

All 28 UI/draw functions classified NA_OURS (we render our own UI/fonts). Batched, **not**
individually adversarially verified — but our-own-UI is a sound wholesale-replacement
justification (we will never reimplement stock's draw routines). One was flagged a
memory-mgmt (font heap free) rather than pure UI; still NA.

## Honest reading

The denominator shrink (118→70) is legitimate (USB-device stack = Artery lib; UI = ours).
The decode win (D3 +73) is large and real. But the headline number's *rise* is mostly
denominator correction — **the substantive discovery is the ABSENT subsystem**, which the
old "~99% understood" metric completely hid. Still **zero scope/FPGA acquisition resolved**.