/*
 * flash_regions.c — region table + bounds-checked allocator for the W25Q128JV.
 *
 * See flash_regions.h for the four design rules. The short version: default
 * deny, fail closed, never erase implicitly, always verify.
 *
 * This file is plain C with no RTOS or vendor dependency so the host tests can
 * link the real code. Serialisation against other SPI2 users is the backend's
 * job — on the device the flash_fs raw primitives already take the fs mutex.
 */

#include "flash_regions.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * The region table
 *
 * Source for the layout: reverse_engineering/analysis_v120/
 * w25q128_flash_map_2026-06-13.md — a live 16 MB dump parsed offline, plus two
 * archived full-chip dumps (2026-04-08 and 2026-05-30) that are byte-identical
 * to each other. Two FAT12 volumes cover the whole chip, no MBR, no raw area:
 *
 *   0x000000-0x1FFFFF   FatFS "3:"  UI JPEGs, System file/, 9999.BIN
 *   0x200000-0xFFFFFF   FatFS "2:"  screenshot store
 *
 * Everything stock owns is therefore READONLY here, including the whole
 * `3:/System file/` path.
 *
 * CORRECTED 2026-08-14: this comment used to say that path holds "a pristine
 * unit's cal_ch1.bin / cal_ch2.bin". Those filenames were INVENTED — they
 * appear nowhere in archive/w25q128_dump_2026_05_30.bin and nowhere in any
 * stock APP binary. The only cal-shaped path stock references is
 * `3:/System file/9999.bin`, and in the dumps we have it is an empty
 * placeholder (FAT entry "9999    BIN", attr 0x20, cluster 0, size 0).
 * Separately, stock can run entirely on calibration-like defaults compiled
 * into its own firmware image (0x080261BE..0x08026506, selected when the
 * sentinel at ms[0x34E] is erased or zero), so the existence and location of
 * any per-device factory calibration is an OPEN QUESTION — see
 * reverse_engineering/analysis_v120/factory_cal_truth_2026-08-14.md.
 *
 * None of that changes the region table below, and the READONLY marking on
 * "sysvol" stays exactly as it is. The rule was never "protect a specific
 * file"; it is "never write into anything stock owns" — the UI JPEGs alone
 * justify it, and the whole-chip sweep in w25q128_flash_map_2026-06-13.md
 * shows both FAT volumes cover all 16 MB with no raw region.
 *
 * And the inverse claim is not proven either: our evidence is two
 * byte-identical archived dumps plus one live read of bench unit #2, all of
 * units we had already reflashed. Absent here is not absent everywhere, a
 * user's unit may carry data ours does not, and we cannot regenerate what we
 * erase.
 *
 * OUR WRITABLE WINDOW is the top 1 MB minus the final sector,
 * 0xF00000-0xFFEFFF. Why there:
 *   - it is 0xFF-erased in both archived full-chip dumps, byte for byte;
 *   - it is the far end of volume "2:", which stock allocates from the bottom,
 *     so it is the last space stock would ever reach;
 *   - the final sector 0xFFF000 is programmed (4096 zero bytes, purpose
 *     unknown) so it is left READONLY rather than assumed free.
 *
 * KNOWN TRADE-OFF, stated plainly: this window is unallocated free space inside
 * volume "2:" as FAT sees it. Raw records there are invisible to FatFS, so a
 * stock firmware writing enough screenshots could allocate those clusters and
 * overwrite US. The risk therefore runs in the safe direction — stock can
 * clobber our data, we can never clobber stock's — and it is the reason this
 * region layer exists rather than a general-purpose writable chip. Ready for
 * bench validation; changing the window is a one-line edit here.
 * ═══════════════════════════════════════════════════════════════════ */

const flash_region_t flash_region_table[FLASH_REGION_COUNT] = {
    [FLASH_REGION_SYSVOL]   = { "sysvol",   0x000000u, 0x200000u, FLASH_REGION_KIND_READONLY },
    [FLASH_REGION_USERVOL]  = { "uservol",  0x200000u, 0xD00000u, FLASH_REGION_KIND_READONLY },
    [FLASH_REGION_USER_CAL] = { "usercal",  0xF00000u, 0x010000u, FLASH_REGION_KIND_APPEND   },
    [FLASH_REGION_SETTINGS] = { "settings", 0xF10000u, 0x010000u, FLASH_REGION_KIND_APPEND   },
    [FLASH_REGION_MODULES]  = { "modules",  0xF20000u, 0x080000u, FLASH_REGION_KIND_RW       },
    /* scratch loses its top 8 KB to the cal-backup region below (0x05F000 ->
     * 0x05D000). 8 KB out of ~380 KB of general scratch is negligible; the
     * factory-cal mirror needs its own default-deny region so a screenshot
     * stream can never reach it. */
    [FLASH_REGION_SCRATCH]  = { "scratch",  0xFA0000u, 0x05D000u, FLASH_REGION_KIND_RW       },
    /* Factory-cal backup — a 4 KB record (header + the MCU 0x08006000 page) in
     * 2 dedicated sectors abutting the RO tail. See src/util/cal_backup.c. */
    [FLASH_REGION_FACTORY_CAL_BACKUP] = { "calbackup", 0xFFD000u, 0x002000u, FLASH_REGION_KIND_RW },
    [FLASH_REGION_TAIL]     = { "tail",     0xFFF000u, 0x001000u, FLASH_REGION_KIND_READONLY },
};

/* ═══════════════════════════════════════════════════════════════════
 * State
 * ═══════════════════════════════════════════════════════════════════ */

#define FLASH_REGIONS_MAX_TABLE   16u
#define IO_CHUNK                  64u   /* stack budget: usb_dbg task has 2 KB */

static const flash_region_backend_t *g_backend;
static const flash_region_t         *g_table;
static uint32_t                      g_count;
static bool                          g_ready;
static flash_region_stats_t          g_stats;

/* ═══════════════════════════════════════════════════════════════════
 * Small helpers
 * ═══════════════════════════════════════════════════════════════════ */

const char *flash_region_strerror(flash_region_status_t st)
{
    switch (st) {
    case FLASH_REGION_OK:              return "ok";
    case FLASH_REGION_ERR_NOT_INIT:    return "not initialised";
    case FLASH_REGION_ERR_TABLE:       return "region table invalid";
    case FLASH_REGION_ERR_BAD_ARG:     return "bad argument";
    case FLASH_REGION_ERR_UNMAPPED:    return "address is in no region";
    case FLASH_REGION_ERR_READ_ONLY:   return "region is read-only";
    case FLASH_REGION_ERR_BOUNDS:      return "range leaves the region";
    case FLASH_REGION_ERR_ALIGN:       return "not sector-aligned";
    case FLASH_REGION_ERR_NEEDS_ERASE: return "target needs erase first";
    case FLASH_REGION_ERR_FULL:        return "append log full";
    case FLASH_REGION_ERR_NOT_FOUND:   return "no valid record";
    case FLASH_REGION_ERR_IO:          return "flash io error";
    case FLASH_REGION_ERR_VERIFY:      return "readback mismatch";
    }
    return "unknown";
}

static bool region_writable(const flash_region_t *r)
{
    return r->kind == FLASH_REGION_KIND_RW || r->kind == FLASH_REGION_KIND_APPEND;
}

/* Overflow-safe "does [offset, offset+len) fit inside [0, size)". */
static bool range_fits(uint32_t offset, uint32_t len, uint32_t size)
{
    return offset < size && len <= (size - offset);
}

static flash_region_status_t backend_read(uint32_t addr, void *buf, uint32_t len)
{
    if (g_backend->read(g_backend->ctx, addr, buf, len) != 0) {
        g_stats.io_failures++;
        return FLASH_REGION_ERR_IO;
    }
    return FLASH_REGION_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Table validation
 *
 * A malformed table is treated as a hard failure rather than something to work
 * around: if the table cannot be trusted then neither can any bounds check
 * derived from it, so the layer refuses to initialise and every write and erase
 * fails closed. The alignment rule matters more than it looks — an RW region
 * that did not start on a sector boundary would share its first sector with a
 * neighbour, and erasing that sector would destroy 4 KB of the neighbour.
 * ═══════════════════════════════════════════════════════════════════ */

flash_region_status_t flash_regions_check_table(const flash_region_t *table, uint32_t count)
{
    if (table == NULL || count == 0u || count > FLASH_REGIONS_MAX_TABLE) {
        return FLASH_REGION_ERR_TABLE;
    }

    uint32_t prev_end = 0u;
    for (uint32_t i = 0; i < count; i++) {
        const flash_region_t *r = &table[i];

        if (r->name == NULL || r->length == 0u) {
            return FLASH_REGION_ERR_TABLE;
        }
        if ((r->start % FLASH_REGION_SECTOR_SIZE) != 0u ||
            (r->length % FLASH_REGION_SECTOR_SIZE) != 0u) {
            return FLASH_REGION_ERR_TABLE;
        }
        if (!range_fits(r->start, r->length, FLASH_REGION_CHIP_SIZE)) {
            return FLASH_REGION_ERR_TABLE;
        }
        if (r->kind != FLASH_REGION_KIND_READONLY &&
            r->kind != FLASH_REGION_KIND_RW &&
            r->kind != FLASH_REGION_KIND_APPEND) {
            return FLASH_REGION_ERR_TABLE;
        }
        /* Ascending and non-overlapping. Ascending order is what makes a single
         * linear scan a complete overlap check. */
        if (i > 0u && r->start < prev_end) {
            return FLASH_REGION_ERR_TABLE;
        }
        prev_end = r->start + r->length;
    }
    return FLASH_REGION_OK;
}

flash_region_status_t flash_regions_init_table(const flash_region_backend_t *backend,
                                               const flash_region_t *table,
                                               uint32_t count)
{
    g_ready   = false;
    g_backend = NULL;
    g_table   = NULL;
    g_count   = 0u;

    if (backend == NULL || backend->read == NULL ||
        backend->erase_sector == NULL || backend->program == NULL) {
        return FLASH_REGION_ERR_BAD_ARG;
    }

    flash_region_status_t st = flash_regions_check_table(table, count);
    if (st != FLASH_REGION_OK) {
        return st;
    }

    g_backend = backend;
    g_table   = table;
    g_count   = count;
    g_ready   = true;
    return FLASH_REGION_OK;
}

flash_region_status_t flash_regions_init(const flash_region_backend_t *backend)
{
    return flash_regions_init_table(backend, flash_region_table, FLASH_REGION_COUNT);
}

bool flash_regions_ready(void) { return g_ready; }

const flash_region_t *flash_region_get(flash_region_id_t id)
{
    if (!g_ready || (uint32_t)id >= g_count) {
        return NULL;
    }
    return &g_table[id];
}

const flash_region_t *flash_region_find(uint32_t addr)
{
    if (!g_ready) {
        return NULL;
    }
    for (uint32_t i = 0; i < g_count; i++) {
        if (addr >= g_table[i].start && (addr - g_table[i].start) < g_table[i].length) {
            return &g_table[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * The policy check — the one function that decides whether flash may change
 *
 * Every write and erase path in this file goes through here before touching
 * anything. Read-only is tested against EVERY region the range intersects, not
 * just the one containing the first byte, so a write that starts in scratch and
 * runs into a read-only neighbour is refused as READ_ONLY rather than being
 * discovered halfway through.
 * ═══════════════════════════════════════════════════════════════════ */

flash_region_status_t flash_regions_check_abs(uint32_t addr, uint32_t len)
{
    if (!g_ready) {
        return FLASH_REGION_ERR_NOT_INIT;
    }
    if (len == 0u) {
        return FLASH_REGION_ERR_BAD_ARG;
    }
    if (!range_fits(addr, len, FLASH_REGION_CHIP_SIZE)) {
        return FLASH_REGION_ERR_BOUNDS;
    }

    const uint32_t end = addr + len;   /* exclusive; range_fits ruled out wrap */

    /* 1. Does the range touch anything read-only? */
    for (uint32_t i = 0; i < g_count; i++) {
        const flash_region_t *r = &g_table[i];
        uint32_t r_end = r->start + r->length;
        bool intersects = (addr < r_end) && (r->start < end);
        if (intersects && !region_writable(r)) {
            return FLASH_REGION_ERR_READ_ONLY;
        }
    }

    /* 2. Is it entirely inside ONE writable region? A range spanning two
     *    regions is refused even when both are writable: crossing a boundary
     *    is the accident, not the permission. */
    const flash_region_t *host = flash_region_find(addr);
    if (host == NULL) {
        return FLASH_REGION_ERR_UNMAPPED;
    }
    if (!range_fits(addr - host->start, len, host->length)) {
        return FLASH_REGION_ERR_BOUNDS;
    }
    return FLASH_REGION_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Program / verify core
 * ═══════════════════════════════════════════════════════════════════ */

/* Decide, without writing anything, what the range needs.
 * *identical -> content already matches, nothing to do.
 * *bitcompat -> every target bit that must be 1 is already 1, so a plain
 *               program reaches the target with no erase. */
static flash_region_status_t inspect_range(uint32_t addr, const uint8_t *data, uint32_t len,
                                           bool *identical, bool *bitcompat)
{
    uint8_t cur[IO_CHUNK];
    *identical = true;
    *bitcompat = true;

    for (uint32_t done = 0; done < len; ) {
        uint32_t n = len - done;
        if (n > IO_CHUNK) n = IO_CHUNK;

        flash_region_status_t st = backend_read(addr + done, cur, n);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        for (uint32_t i = 0; i < n; i++) {
            uint8_t want = data[done + i];
            if (cur[i] != want)                 *identical = false;
            if ((uint8_t)(cur[i] & want) != want) *bitcompat = false;
        }
        done += n;
    }
    return FLASH_REGION_OK;
}

static flash_region_status_t verify_range(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t cur[IO_CHUNK];

    for (uint32_t done = 0; done < len; ) {
        uint32_t n = len - done;
        if (n > IO_CHUNK) n = IO_CHUNK;

        flash_region_status_t st = backend_read(addr + done, cur, n);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        if (memcmp(cur, data + done, n) != 0) {
            g_stats.verify_failures++;
            return FLASH_REGION_ERR_VERIFY;
        }
        done += n;
    }
    return FLASH_REGION_OK;
}

/* Program a validated range, splitting on 256 B page boundaries, then read it
 * back and compare. An unverified write is not evidence that anything was
 * written — this project has already lost weeks to that class of bug. */
static flash_region_status_t program_verified(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t done = 0;
    while (done < len) {
        uint32_t page_left = FLASH_REGION_PAGE_SIZE - ((addr + done) % FLASH_REGION_PAGE_SIZE);
        uint32_t n = len - done;
        if (n > page_left) n = page_left;

        if (g_backend->program(g_backend->ctx, addr + done, data + done, n) != 0) {
            g_stats.io_failures++;
            return FLASH_REGION_ERR_IO;
        }
        done += n;
    }
    return verify_range(addr, data, len);
}

/* Shared body of the write paths. `addr` has already passed the policy check. */
static flash_region_status_t write_checked(uint32_t addr, const uint8_t *data, uint32_t len)
{
    bool identical = false, bitcompat = false;
    flash_region_status_t st = inspect_range(addr, data, len, &identical, &bitcompat);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    if (identical) {
        g_stats.writes_elided++;         /* no-op write: no erase, no program */
        return FLASH_REGION_OK;
    }
    if (!bitcompat) {
        /* Deliberately NOT a read-modify-write. Erasing on the caller's behalf
         * is how a one-byte update turns into a 4 KB erase nobody asked for. */
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_NEEDS_ERASE;
    }

    st = program_verified(addr, data, len);
    if (st == FLASH_REGION_OK) {
        g_stats.writes_programmed++;
    }
    return st;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public read / write / erase
 * ═══════════════════════════════════════════════════════════════════ */

flash_region_status_t flash_region_read(flash_region_id_t id, uint32_t offset,
                                        void *buf, uint32_t len)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready)             return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL)            return FLASH_REGION_ERR_BAD_ARG;
    if (buf == NULL || len == 0u) return FLASH_REGION_ERR_BAD_ARG;
    if (!range_fits(offset, len, r->length)) return FLASH_REGION_ERR_BOUNDS;

    return backend_read(r->start + offset, buf, len);
}

flash_region_status_t flash_region_write(flash_region_id_t id, uint32_t offset,
                                         const void *data, uint32_t len)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready)  return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL || data == NULL || len == 0u) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BAD_ARG;
    }
    if (!region_writable(r)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_READ_ONLY;
    }
    if (!range_fits(offset, len, r->length)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BOUNDS;
    }
    return write_checked(r->start + offset, (const uint8_t *)data, len);
}

flash_region_status_t flash_regions_write_abs(uint32_t addr, const void *data, uint32_t len)
{
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (data == NULL) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BAD_ARG;
    }
    flash_region_status_t st = flash_regions_check_abs(addr, len);
    if (st != FLASH_REGION_OK) {
        g_stats.writes_refused++;
        return st;
    }
    return write_checked(addr, (const uint8_t *)data, len);
}

/* Erase a validated, sector-aligned range. Sectors already at 0xFF are skipped:
 * a redundant erase is pure risk with no benefit. */
static flash_region_status_t erase_checked(uint32_t addr, uint32_t len)
{
    uint8_t buf[IO_CHUNK];

    for (uint32_t sec = addr; sec < addr + len; sec += FLASH_REGION_SECTOR_SIZE) {
        bool blank = true;
        for (uint32_t off = 0; off < FLASH_REGION_SECTOR_SIZE && blank; off += IO_CHUNK) {
            flash_region_status_t st = backend_read(sec + off, buf, IO_CHUNK);
            if (st != FLASH_REGION_OK) {
                return st;
            }
            for (uint32_t i = 0; i < IO_CHUNK; i++) {
                if (buf[i] != 0xFFu) { blank = false; break; }
            }
        }
        if (blank) {
            g_stats.erases_elided++;
            continue;
        }
        if (g_backend->erase_sector(g_backend->ctx, sec) != 0) {
            g_stats.io_failures++;
            return FLASH_REGION_ERR_IO;
        }
        g_stats.erases_performed++;
    }
    return FLASH_REGION_OK;
}

flash_region_status_t flash_region_erase(flash_region_id_t id, uint32_t offset, uint32_t len)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL || len == 0u) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_BAD_ARG;
    }
    if (!region_writable(r)) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_READ_ONLY;
    }
    if ((offset % FLASH_REGION_SECTOR_SIZE) != 0u || (len % FLASH_REGION_SECTOR_SIZE) != 0u) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_ALIGN;
    }
    if (!range_fits(offset, len, r->length)) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_BOUNDS;
    }
    return erase_checked(r->start + offset, len);
}

flash_region_status_t flash_regions_erase_abs(uint32_t addr, uint32_t len)
{
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;

    if ((addr % FLASH_REGION_SECTOR_SIZE) != 0u || (len % FLASH_REGION_SECTOR_SIZE) != 0u ||
        len == 0u) {
        g_stats.erases_refused++;
        return (len == 0u) ? FLASH_REGION_ERR_BAD_ARG : FLASH_REGION_ERR_ALIGN;
    }
    flash_region_status_t st = flash_regions_check_abs(addr, len);
    if (st != FLASH_REGION_OK) {
        g_stats.erases_refused++;
        return st;
    }
    return erase_checked(addr, len);
}

flash_region_status_t flash_region_reset(flash_region_id_t id)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_BAD_ARG;
    }
    return flash_region_erase(id, 0u, r->length);
}

/* ═══════════════════════════════════════════════════════════════════
 * Append log
 *
 * Layout per record:
 *   +0  u16  magic        0xA5C3, little-endian
 *   +2  u16  payload len
 *   +4  u32  crc32 of payload
 *   +8  payload, padded with 0xFF to a 4-byte boundary
 *
 * The header goes down BEFORE the payload. A power cut mid-payload then leaves
 * a record whose length is known and whose CRC fails: the scanner can step over
 * it and the log stays usable. The reverse order would leave a programmed
 * payload under a blank header, and the next append would try to program
 * non-blank flash.
 * ═══════════════════════════════════════════════════════════════════ */

#define REC_MAGIC       0xA5C3u
#define REC_HDR_SIZE    8u

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static uint32_t crc32_of(const uint8_t *data, uint32_t len)
{
    return ~crc32_update(0xFFFFFFFFu, data, len);
}

/* CRC a payload straight off the flash, 64 bytes at a time. Records may be up
 * to 1 KB and the usb_dbg task has 2 KB of stack, so nothing here buffers a
 * whole record. */
static flash_region_status_t crc32_of_flash(uint32_t addr, uint32_t len, uint32_t *out)
{
    uint8_t buf[IO_CHUNK];
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t done = 0; done < len; ) {
        uint32_t n = len - done;
        if (n > IO_CHUNK) n = IO_CHUNK;
        flash_region_status_t st = backend_read(addr + done, buf, n);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        crc = crc32_update(crc, buf, n);
        done += n;
    }
    *out = ~crc;
    return FLASH_REGION_OK;
}

/* Chunked compare of flash against a caller buffer; *equal is only meaningful
 * when the call returns OK. */
static flash_region_status_t flash_equals(uint32_t addr, const uint8_t *data,
                                          uint32_t len, bool *equal)
{
    uint8_t buf[IO_CHUNK];
    *equal = true;

    for (uint32_t done = 0; done < len; ) {
        uint32_t n = len - done;
        if (n > IO_CHUNK) n = IO_CHUNK;
        flash_region_status_t st = backend_read(addr + done, buf, n);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        if (memcmp(buf, data + done, n) != 0) {
            *equal = false;
            return FLASH_REGION_OK;
        }
        done += n;
    }
    return FLASH_REGION_OK;
}

static uint32_t rec_total(uint32_t payload_len)
{
    return REC_HDR_SIZE + ((payload_len + 3u) & ~3u);
}

typedef struct {
    uint32_t next_offset;    /* first free byte in the log            */
    uint32_t latest_offset;  /* offset of newest CRC-valid record, or */
    uint32_t latest_len;     /* UINT32_MAX / 0 when there is none     */
    uint32_t valid_records;
} log_scan_t;

/* Walk the log. Stops at the first blank or unparsable header, which is what
 * makes a torn header self-limiting rather than a way to run off the end. */
static flash_region_status_t log_scan(const flash_region_t *r, log_scan_t *out)
{
    uint8_t hdr[REC_HDR_SIZE];

    out->next_offset   = 0u;
    out->latest_offset = UINT32_MAX;
    out->latest_len    = 0u;
    out->valid_records = 0u;

    uint32_t off = 0u;
    while (off + REC_HDR_SIZE <= r->length) {
        flash_region_status_t st = backend_read(r->start + off, hdr, REC_HDR_SIZE);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8);
        if (magic != REC_MAGIC) {
            break;                       /* blank slot, or a torn header */
        }
        uint32_t len = (uint32_t)hdr[2] | ((uint32_t)hdr[3] << 8);
        if (len == 0u || len > FLASH_REGION_RECORD_MAX) {
            break;                       /* nonsense length: stop, do not guess */
        }
        uint32_t total = rec_total(len);
        if (!range_fits(off, total, r->length)) {
            break;                       /* would run past the region */
        }

        uint32_t crc = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
                       ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);

        uint32_t actual = 0u;
        st = crc32_of_flash(r->start + off + REC_HDR_SIZE, len, &actual);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        if (actual == crc) {
            out->latest_offset = off;
            out->latest_len    = len;
            out->valid_records++;
        }
        /* A CRC failure is a torn or damaged record: skip it and keep going.
         * The header told us how long it is, so the log survives it. */
        off += total;
    }

    out->next_offset = off;
    return FLASH_REGION_OK;
}

flash_region_status_t flash_region_append(flash_region_id_t id, const void *data, uint32_t len)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL || data == NULL || len == 0u || len > FLASH_REGION_RECORD_MAX) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BAD_ARG;
    }
    if (r->kind != FLASH_REGION_KIND_APPEND) {
        g_stats.writes_refused++;
        return (r->kind == FLASH_REGION_KIND_READONLY) ? FLASH_REGION_ERR_READ_ONLY
                                                       : FLASH_REGION_ERR_BAD_ARG;
    }

    log_scan_t scan;
    flash_region_status_t st = log_scan(r, &scan);
    if (st != FLASH_REGION_OK) {
        return st;
    }

    /* No-op elision at the record level. This is the case that actually matters
     * for a hot value: saving unchanged settings costs nothing at all. */
    if (scan.latest_offset != UINT32_MAX && scan.latest_len == len) {
        bool same = false;
        st = flash_equals(r->start + scan.latest_offset + REC_HDR_SIZE,
                          (const uint8_t *)data, len, &same);
        if (st != FLASH_REGION_OK) {
            return st;
        }
        if (same) {
            g_stats.writes_elided++;
            return FLASH_REGION_OK;
        }
    }

    uint32_t total = rec_total(len);
    if (!range_fits(scan.next_offset, total, r->length)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_FULL;
    }

    uint32_t addr = r->start + scan.next_offset;

    /* The slot must be blank. If it is not, something already lives here and
     * this layer does not overwrite by guessing — it refuses. */
    {
        uint8_t buf[IO_CHUNK];
        for (uint32_t done = 0; done < total; ) {
            uint32_t n = total - done;
            if (n > IO_CHUNK) n = IO_CHUNK;
            st = backend_read(addr + done, buf, n);
            if (st != FLASH_REGION_OK) {
                return st;
            }
            for (uint32_t i = 0; i < n; i++) {
                if (buf[i] != 0xFFu) {
                    g_stats.writes_refused++;
                    return FLASH_REGION_ERR_NEEDS_ERASE;
                }
            }
            done += n;
        }
    }

    uint32_t crc = crc32_of((const uint8_t *)data, len);
    uint8_t hdr[REC_HDR_SIZE];
    hdr[0] = (uint8_t)(REC_MAGIC & 0xFFu);
    hdr[1] = (uint8_t)(REC_MAGIC >> 8);
    hdr[2] = (uint8_t)(len & 0xFFu);
    hdr[3] = (uint8_t)(len >> 8);
    hdr[4] = (uint8_t)(crc & 0xFFu);
    hdr[5] = (uint8_t)((crc >> 8) & 0xFFu);
    hdr[6] = (uint8_t)((crc >> 16) & 0xFFu);
    hdr[7] = (uint8_t)((crc >> 24) & 0xFFu);

    st = program_verified(addr, hdr, REC_HDR_SIZE);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    st = program_verified(addr + REC_HDR_SIZE, (const uint8_t *)data, len);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    g_stats.writes_programmed++;
    return FLASH_REGION_OK;
}

flash_region_status_t flash_region_read_latest(flash_region_id_t id, void *buf,
                                               uint32_t buf_size, uint32_t *out_len)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL || buf == NULL || buf_size == 0u) return FLASH_REGION_ERR_BAD_ARG;
    if (r->kind != FLASH_REGION_KIND_APPEND)        return FLASH_REGION_ERR_BAD_ARG;

    log_scan_t scan;
    flash_region_status_t st = log_scan(r, &scan);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    if (scan.latest_offset == UINT32_MAX) {
        return FLASH_REGION_ERR_NOT_FOUND;
    }
    if (scan.latest_len > buf_size) {
        return FLASH_REGION_ERR_BOUNDS;
    }
    st = backend_read(r->start + scan.latest_offset + REC_HDR_SIZE, buf, scan.latest_len);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    if (out_len != NULL) {
        *out_len = scan.latest_len;
    }
    return FLASH_REGION_OK;
}

flash_region_status_t flash_region_log_info(flash_region_id_t id, uint32_t *bytes_used,
                                            uint32_t *bytes_free, uint32_t *valid_records)
{
    const flash_region_t *r = flash_region_get(id);
    if (!g_ready) return FLASH_REGION_ERR_NOT_INIT;
    if (r == NULL || r->kind != FLASH_REGION_KIND_APPEND) return FLASH_REGION_ERR_BAD_ARG;

    log_scan_t scan;
    flash_region_status_t st = log_scan(r, &scan);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    if (bytes_used)    *bytes_used    = scan.next_offset;
    if (bytes_free)    *bytes_free    = r->length - scan.next_offset;
    if (valid_records) *valid_records = scan.valid_records;
    return FLASH_REGION_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Stats
 * ═══════════════════════════════════════════════════════════════════ */

const flash_region_stats_t *flash_regions_stats(void) { return &g_stats; }

void flash_regions_stats_reset(void) { memset(&g_stats, 0, sizeof(g_stats)); }
