/*
 * FNIRSI 2C53T Custom Firmware - Minimal Bootstrap
 *
 * Target: GD32F307 (ARM Cortex-M4F @ 120MHz)
 * Based on reverse-engineered task architecture from original firmware.
 *
 * This minimal main:
 *   1. Initializes system clocks via GD32 HAL (SystemInit in startup)
 *   2. Creates a placeholder "display" task
 *   3. Starts the FreeRTOS scheduler
 */

#include "gd32f30x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Task handles */
static TaskHandle_t xDisplayTaskHandle = NULL;

/* Display command queue (matches original: 20 items x 1 byte) */
static QueueHandle_t xDisplayQueue = NULL;

/* Placeholder variable toggled by display task */
static volatile uint32_t display_tick_count = 0;

/*
 * Display task - placeholder for the UI renderer.
 * In the original firmware this receives commands via a queue
 * and dispatches through a function pointer table.
 */
static void vDisplayTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t cmd;

    for (;;) {
        /* Wait for a display command (or timeout every 500ms) */
        if (xQueueReceive(xDisplayQueue, &cmd, pdMS_TO_TICKS(500)) == pdTRUE) {
            /* Process command - placeholder */
            display_tick_count++;
        } else {
            /* Timeout - toggle counter to show we're alive */
            display_tick_count++;
        }
    }
}

/*
 * Main entry point.
 * SystemInit() is called by the startup code before main().
 * It configures the PLL for 120MHz from HXTAL.
 */
int main(void)
{
    /* SystemInit() already called by startup_gd32f30x_cl.S */
    /* SystemCoreClock is set to 120MHz */

    /* Update the SystemCoreClock variable (in case it wasn't set) */
    SystemCoreClockUpdate();

    /* Enable GPIO clocks (will be needed for peripherals later) */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_AF);

    /* Create display command queue: 20 items, 1 byte each (matches original) */
    xDisplayQueue = xQueueCreate(20, sizeof(uint8_t));

    /* Create display task: 1536 bytes stack, priority 1 (matches original) */
    xTaskCreate(
        vDisplayTask,           /* Task function */
        "display",              /* Name (matches original) */
        384,                    /* Stack depth in words (384 * 4 = 1536 bytes) */
        NULL,                   /* Parameters */
        1,                      /* Priority (lowest user task, matches original) */
        &xDisplayTaskHandle     /* Handle */
    );

    /* Start the scheduler - this never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {
    }
}

/*
 * FreeRTOS stack overflow hook (optional, for debugging)
 */
#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}
#endif

/*
 * FreeRTOS malloc failed hook (optional, for debugging)
 */
#if (configUSE_MALLOC_FAILED_HOOK == 1)
void vApplicationMallocFailedHook(void)
{
    for (;;) {
    }
}
#endif
