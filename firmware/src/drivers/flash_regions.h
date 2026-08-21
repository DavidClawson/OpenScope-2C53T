/*
 * flash_regions.h — bounds-checked region layer for the external W25Q128JV.
 *
 * WHAT THIS IS FOR
 * ----------------
 * This is not a filesystem. It is a static region table plus an allocator that
 * refuses to write or erase anything outside a region that is explicitly marked
 * writable.
 *
 * The hazard it exists to stop is NOT wear. The W25Q128JV is rated 100,000 P/E
 * cycles per 4 KB sector; at one erase a day a sector lasts ~274 years. The
 * hazard is a STRAY ERASE destroying data we cannot regenerate — the stock UI
 * assets and, on a pristine unit, the factory calibration under
 * `3:/System file/`. A 4 KB sector erase is indivisible and irreversible, and
 * `flash_fs_raw_sector_erase()` will happily erase sector 0 if you pass it 0.
 *
 * DESIGN RULES (all four are load-bearing; read before changing anything)
 * ----------------------------------------------------------------------
 *  1. DEFAULT DENY. Any address not covered by a region in `flash_region_table`
 *     is unmapped and unwritable. Adding write access is an explicit table edit,
 *     never an accident.
 *  2. FAIL CLOSED, NEVER PARTIALLY. Every operation is fully validated before a
 *     single byte of flash is touched. An operation that would cross a region
 *     boundary is an error, not a clamp and not a truncated write.
 *  3. NO IMPLICIT ERASE. `flash_region_write()` never erases. If the target
 *     bytes cannot be reached by clearing bits (NOR can only 1->0), the call
 *     returns FLASH_REGION_ERR_NEEDS_ERASE and writes nothing. The caller must
 *     ask for the erase by name. A hidden read-modify-write is precisely the
 *     stray-erase mechanism this layer exists to prevent.
 *  4. VERIFY AFTER WRITE. Every program is read back and compared. This project
 *     has already been bitten once by writes that silently did nothing (see the
 *     CS-settle comment in flash_fs.c); an unverified write is not evidence.
 *
 * Frequently-updated values (user calibration, saved settings) use the append
 * log (`flash_region_append`), which packs fixed-header records into a region
 * until it fills instead of erase-rewriting one sector every save.
 *
 * PORTABILITY: this file and flash_regions.c depend on nothing but the C
 * standard library, so the host test suite (firmware/tests/test_flash_regions.c)
 * exercises the real firmware code against a NOR-semantics memory model rather
 * than a mock. The device binding lives in flash_regions_w25q.c.
 */
#ifndef FLASH_REGIONS_H
#define FLASH_REGIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* W25Q128JV geometry */
#define FLASH_REGION_CHIP_SIZE    (16u * 1024u * 1024u)
#define FLASH_REGION_SECTOR_SIZE  4096u
#define FLASH_REGION_PAGE_SIZE    256u

/* ── Status ──────────────────────────────────────────────────────────
 * FLASH_REGION_OK is 0; every failure is non-zero. There is deliberately no
 * "partial success" code: an operation either completed in full or changed
 * nothing. */
typedef enum {
    FLASH_REGION_OK = 0,
    FLASH_REGION_ERR_NOT_INIT,      /* flash_regions_init() not called / failed */
    FLASH_REGION_ERR_TABLE,         /* region table failed its self-check      */
    FLASH_REGION_ERR_BAD_ARG,       /* null pointer, zero length, bad id       */
    FLASH_REGION_ERR_UNMAPPED,      /* address covered by no region            */
    FLASH_REGION_ERR_READ_ONLY,     /* range touches a read-only region        */
    FLASH_REGION_ERR_BOUNDS,        /* range leaves the region                 */
    FLASH_REGION_ERR_ALIGN,         /* erase not on 4 KB sector boundaries     */
    FLASH_REGION_ERR_NEEDS_ERASE,   /* target bits cannot be reached by 1->0   */
    FLASH_REGION_ERR_FULL,          /* append log has no room                  */
    FLASH_REGION_ERR_NOT_FOUND,     /* append log holds no valid record        */
    FLASH_REGION_ERR_IO,            /* backend reported failure                */
    FLASH_REGION_ERR_VERIFY,        /* readback did not match what we wrote    */
} flash_region_status_t;

const char *flash_region_strerror(flash_region_status_t st);

/* ── Region kinds ────────────────────────────────────────────────────
 * READONLY is enforced by address range. It is not a hint, a naming
 * convention, or a comment — every write and erase path tests it. */
typedef enum {
    FLASH_REGION_KIND_READONLY = 0,
    FLASH_REGION_KIND_RW       = 1,  /* bulk data: explicit erase, then write  */
    FLASH_REGION_KIND_APPEND   = 2,  /* record log: flash_region_append()      */
} flash_region_kind_t;

typedef struct {
    const char         *name;
    uint32_t            start;   /* sector-aligned absolute chip address */
    uint32_t            length;  /* multiple of FLASH_REGION_SECTOR_SIZE */
    flash_region_kind_t kind;
} flash_region_t;

/* Region identities. The ids index flash_region_table[] and the self-check
 * enforces that, so an id can never silently point at the wrong range. */
typedef enum {
    FLASH_REGION_SYSVOL = 0,   /* RO: FatFS "3:" — UI assets + factory cal path */
    FLASH_REGION_USERVOL,      /* RO: FatFS "2:" — stock screenshot volume      */
    FLASH_REGION_FWCACHE,      /* rw: A/B firmware-image cache (fw_loader.c) —
                                * MUST sit here: the table self-check requires
                                * ascending start addresses, and the ids index
                                * the table */
    FLASH_REGION_USER_CAL,     /* append: our user-calibration overlay          */
    FLASH_REGION_SETTINGS,     /* append: our saved settings                    */
    FLASH_REGION_MODULES,      /* rw: module data assets                        */
    FLASH_REGION_SCRATCH,      /* rw: streaming screenshot / general scratch    */
    FLASH_REGION_TAIL,         /* RO: final sector, programmed, purpose unknown */
    FLASH_REGION_COUNT
} flash_region_id_t;

extern const flash_region_t flash_region_table[FLASH_REGION_COUNT];

/* ── Backend ─────────────────────────────────────────────────────────
 * The three primitives the layer needs. All return 0 on success.
 *   read()          any address, any length, within the chip.
 *   erase_sector()  addr is sector-aligned; sets 4096 bytes to 0xFF.
 *   program()       addr..addr+len-1 lies inside one 256 B page; NOR AND
 *                   semantics (bits can only go 1 -> 0).
 * On the device these are bound to the flash_fs raw primitives by
 * flash_regions_bind_w25q(); in host tests they are bound to a memory model. */
typedef struct {
    int (*read)(void *ctx, uint32_t addr, void *buf, uint32_t len);
    int (*erase_sector)(void *ctx, uint32_t addr);
    int (*program)(void *ctx, uint32_t addr, const void *data, uint32_t len);
    void *ctx;
} flash_region_backend_t;

/* ── Init ────────────────────────────────────────────────────────────
 * Runs the table self-check. If the table is malformed the layer stays
 * uninitialised and EVERY write and erase is refused — a bad table fails
 * closed rather than opening a hole. */
flash_region_status_t flash_regions_init(const flash_region_backend_t *backend);

/* Same, with a caller-supplied table. Exists for the host tests, which must be
 * able to prove that a malformed table is rejected. */
flash_region_status_t flash_regions_init_table(const flash_region_backend_t *backend,
                                               const flash_region_t *table,
                                               uint32_t count);

/* Validate a table without installing it: alignment, ordering, overlap, and
 * chip bounds. Returns FLASH_REGION_OK or FLASH_REGION_ERR_TABLE. */
flash_region_status_t flash_regions_check_table(const flash_region_t *table, uint32_t count);

bool flash_regions_ready(void);

/* ── Lookup ──────────────────────────────────────────────────────────*/
const flash_region_t *flash_region_get(flash_region_id_t id);

/* Region containing addr, or NULL if unmapped. */
const flash_region_t *flash_region_find(uint32_t addr);

/* Policy question, no I/O: may [addr, addr+len) be written/erased?
 * Returns OK, or UNMAPPED / READ_ONLY / BOUNDS (range spans two regions). */
flash_region_status_t flash_regions_check_abs(uint32_t addr, uint32_t len);

/* ── Region-relative access (the preferred API) ──────────────────────
 * Offsets are relative to the region start, so a caller cannot even name an
 * address outside its region. */
flash_region_status_t flash_region_read(flash_region_id_t id, uint32_t offset,
                                        void *buf, uint32_t len);

/* Program without erasing. Elides the write entirely if the content already
 * matches (requirement: no-op writes cost nothing). Returns
 * FLASH_REGION_ERR_NEEDS_ERASE, having written nothing, if any target bit must
 * go 0 -> 1. */
flash_region_status_t flash_region_write(flash_region_id_t id, uint32_t offset,
                                         const void *data, uint32_t len);

/* Erase whole sectors. offset and len must both be sector multiples and the
 * range must lie inside the region. Already-erased sectors are skipped. */
flash_region_status_t flash_region_erase(flash_region_id_t id, uint32_t offset, uint32_t len);

/* Erase an entire writable region. */
flash_region_status_t flash_region_reset(flash_region_id_t id);

/* ── Absolute-address access ─────────────────────────────────────────
 * For callers that hold a chip address rather than a region handle. The range
 * must lie entirely inside ONE region; a range that straddles a boundary is
 * refused even when both regions are writable, because an erase or a partial
 * program across a boundary is exactly the accident being prevented. */
flash_region_status_t flash_regions_write_abs(uint32_t addr, const void *data, uint32_t len);
flash_region_status_t flash_regions_erase_abs(uint32_t addr, uint32_t len);

/* ── Append log ──────────────────────────────────────────────────────
 * Records are [8-byte header][payload padded to 4]. The header is programmed
 * first so a torn write leaves a record whose length is known and whose CRC
 * fails: the scanner skips it and the log keeps working. Appending a payload
 * identical to the newest valid record is elided.
 *
 * When the region fills, append returns FLASH_REGION_ERR_FULL and changes
 * nothing; the caller decides when to flash_region_reset() and re-append the
 * live value. There is no automatic compaction, because automatic compaction
 * means an automatic erase. */
#define FLASH_REGION_RECORD_MAX  1024u

flash_region_status_t flash_region_append(flash_region_id_t id,
                                          const void *data, uint32_t len);

/* Newest CRC-valid record. *out_len gets the payload length. Returns
 * FLASH_REGION_ERR_NOT_FOUND on an empty (or entirely corrupt) log. */
flash_region_status_t flash_region_read_latest(flash_region_id_t id,
                                               void *buf, uint32_t buf_size,
                                               uint32_t *out_len);

/* Log occupancy, for diagnostics and for deciding when to reset. Any out
 * pointer may be NULL. */
flash_region_status_t flash_region_log_info(flash_region_id_t id,
                                            uint32_t *bytes_used,
                                            uint32_t *bytes_free,
                                            uint32_t *valid_records);

/* ── Counters ────────────────────────────────────────────────────────
 * Cheap and worth having: "refused" climbing is how a caller with a bad address
 * gets noticed, and "elided" is how the no-op path is shown to be working. */
typedef struct {
    uint32_t writes_programmed;
    uint32_t writes_elided;
    uint32_t writes_refused;
    uint32_t erases_performed;
    uint32_t erases_elided;
    uint32_t erases_refused;
    uint32_t verify_failures;
    uint32_t io_failures;
} flash_region_stats_t;

const flash_region_stats_t *flash_regions_stats(void);
void flash_regions_stats_reset(void);

/* ── Device binding (flash_regions_w25q.c, firmware build only) ──────
 * Binds the backend to the flash_fs raw SPI2 primitives. */
flash_region_status_t flash_regions_bind_w25q(void);

#endif /* FLASH_REGIONS_H */
