/*
 * OpenScope 2C53T - RTT debug transport
 *
 * See rtt.h for why this is a clean-room implementation rather than SEGGER's.
 *
 * Wire format (this is all the host needs to agree on):
 *
 *   control block                 channel descriptor (6 words)
 *   ┌──────────────────────┐      ┌──────────────────────────┐
 *   │ char id[16]          │      │ uint32 name_addr         │
 *   │ uint32 num_up        │      │ uint32 buffer_addr       │
 *   │ uint32 num_down      │      │ uint32 size              │
 *   │ channel up[num_up]   │      │ uint32 write_pos (target)│
 *   │ channel down[n_down] │      │ uint32 read_pos  (host)  │
 *   └──────────────────────┘      │ uint32 flags             │
 *                                 └──────────────────────────┘
 *
 * Both buffers are ring buffers with one slot left unused so that
 * write_pos == read_pos unambiguously means empty.
 *
 * The id string is filled in byte by byte at rtt_init() time rather than
 * initialised statically. That is deliberate: the host locates the control
 * block by scanning SRAM for the id, and a statically initialised copy would
 * be findable before the buffers were valid.
 */

#include "rtt.h"
#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* Kept modest on purpose: __bss_end already sits ~3.5 KB below _estack on the
 * guest link, and the main stack has to live in that gap. Nothing is lost by
 * being small — when a host is attached, a full buffer makes the writer wait
 * for drain rather than drop. */
#define RTT_UP_SIZE        2048u   /* target -> host */
#define RTT_DOWN_SIZE       128u   /* host -> target, one command line      */
#define RTT_TX_TIMEOUT_MS    50u   /* max stall when an attached host lags  */

/* flags[1:0] */
#define RTT_MODE_SKIP         0u
#define RTT_MODE_TRIM         1u
#define RTT_MODE_BLOCK        2u

typedef struct {
    const char     *name_addr;
    char           *buffer_addr;
    uint32_t        size;
    volatile uint32_t write_pos;   /* target writes (up), host writes (down) */
    volatile uint32_t read_pos;    /* host writes (up), target writes (down) */
    uint32_t        flags;
} rtt_channel_t;

typedef struct {
    char            id[16];
    uint32_t        num_up;
    uint32_t        num_down;
    rtt_channel_t   up[1];
    rtt_channel_t   down[1];
} rtt_control_t;

/* scripts/rtt_shell.sh hands OpenOCD a fixed-size search window at &_rtt_cb.
 * If the layout ever grows past it the console goes silent with no diagnostic,
 * so pin it here instead. */
_Static_assert(sizeof(rtt_control_t) <= 256,
               "RTT control block exceeds the window in scripts/rtt_shell.sh");

static char rtt_up_buf[RTT_UP_SIZE];
static char rtt_down_buf[RTT_DOWN_SIZE];

/* Non-static so the address can be pulled out of the ELF with nm and handed
 * to OpenOCD directly, which avoids a slow 224 KB SRAM scan over SWD. */
rtt_control_t _rtt_cb __attribute__((aligned(4)));

static bool rtt_ready;
static bool rtt_seen_host;

/* The buffer name is what shows up as the channel label on the host. */
static const char rtt_up_name[]   = "Terminal";
static const char rtt_down_name[] = "Terminal";

void rtt_init(void)
{
    memset(&_rtt_cb, 0, sizeof(_rtt_cb));

    _rtt_cb.num_up   = 1;
    _rtt_cb.num_down = 1;

    _rtt_cb.up[0].name_addr   = rtt_up_name;
    _rtt_cb.up[0].buffer_addr = rtt_up_buf;
    _rtt_cb.up[0].size        = RTT_UP_SIZE;
    _rtt_cb.up[0].write_pos   = 0;
    _rtt_cb.up[0].read_pos    = 0;
    _rtt_cb.up[0].flags       = RTT_MODE_TRIM;

    _rtt_cb.down[0].name_addr   = rtt_down_name;
    _rtt_cb.down[0].buffer_addr = rtt_down_buf;
    _rtt_cb.down[0].size        = RTT_DOWN_SIZE;
    _rtt_cb.down[0].write_pos   = 0;
    _rtt_cb.down[0].read_pos    = 0;
    _rtt_cb.down[0].flags       = RTT_MODE_SKIP;

    __DMB();

    /* Arm the id last, and not as a single literal — see the file header. */
    _rtt_cb.id[0]  = 'S'; _rtt_cb.id[1]  = 'E'; _rtt_cb.id[2]  = 'G';
    _rtt_cb.id[3]  = 'G'; _rtt_cb.id[4]  = 'E'; _rtt_cb.id[5]  = 'R';
    _rtt_cb.id[6]  = ' '; _rtt_cb.id[7]  = 'R'; _rtt_cb.id[8]  = 'T';
    _rtt_cb.id[9]  = 'T'; _rtt_cb.id[10] = '\0';

    __DMB();
    rtt_ready = true;
}

bool rtt_host_attached(void)
{
    if (!rtt_ready) return false;
    /* A host that has consumed anything has moved read_pos off zero. Latch it:
     * read_pos returns to 0 whenever it wraps, which would otherwise look like
     * the host detaching. */
    if (_rtt_cb.up[0].read_pos != 0) rtt_seen_host = true;
    return rtt_seen_host;
}

/* Bytes that can be written without overtaking read_pos. One slot is reserved
 * so that a full buffer is distinguishable from an empty one. */
static uint32_t up_space(void)
{
    uint32_t wr = _rtt_cb.up[0].write_pos;
    uint32_t rd = _rtt_cb.up[0].read_pos;

    if (rd > wr) return rd - wr - 1u;
    return RTT_UP_SIZE - wr + rd - 1u;
}

/* Copy into the ring and publish. Caller guarantees len <= up_space(). */
static void up_commit(const uint8_t *p, uint32_t len)
{
    uint32_t wr    = _rtt_cb.up[0].write_pos;
    uint32_t first = RTT_UP_SIZE - wr;

    if (first > len) first = len;
    memcpy(rtt_up_buf + wr, p, first);
    if (len > first) memcpy(rtt_up_buf, p + first, len - first);

    wr += len;
    if (wr >= RTT_UP_SIZE) wr -= RTT_UP_SIZE;

    __DMB();                       /* data visible before the host sees it */
    _rtt_cb.up[0].write_pos = wr;
}

size_t rtt_write(const void *data, size_t len)
{
    if (!rtt_ready || data == NULL || len == 0) return 0;

    const uint8_t *p       = (const uint8_t *)data;
    size_t         written = 0;
    uint32_t       waited  = 0;

    while (written < len) {
        uint32_t chunk;

        taskENTER_CRITICAL();
        {
            uint32_t space = up_space();
            uint32_t want  = (uint32_t)(len - written);
            chunk = (want < space) ? want : space;
            if (chunk) up_commit(p + written, chunk);
        }
        taskEXIT_CRITICAL();

        written += chunk;
        if (written >= len) break;

        /* Buffer is full. Only worth waiting if someone is draining it. */
        if (!rtt_host_attached()) break;
        if (waited >= RTT_TX_TIMEOUT_MS) break;

        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            /* Pre-scheduler: spin roughly a millisecond. */
            for (volatile uint32_t i = 0; i < system_core_clock / 4000u; i++) { }
        }
        waited++;
    }

    return written;
}

size_t rtt_puts(const char *s)
{
    if (s == NULL) return 0;
    return rtt_write(s, strlen(s));
}

size_t rtt_read(void *data, size_t len)
{
    if (!rtt_ready || data == NULL || len == 0) return 0;

    uint8_t *out = (uint8_t *)data;
    uint32_t rd  = _rtt_cb.down[0].read_pos;
    uint32_t wr  = _rtt_cb.down[0].write_pos;
    size_t   n   = 0;

    if (wr >= RTT_DOWN_SIZE) return 0;   /* host wrote nonsense; ignore */

    while (rd != wr && n < len) {
        out[n++] = (uint8_t)rtt_down_buf[rd];
        if (++rd >= RTT_DOWN_SIZE) rd = 0;
    }

    if (n) {
        __DMB();
        _rtt_cb.down[0].read_pos = rd;
    }
    return n;
}
