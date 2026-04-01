/*
 * FreeRTOS configuration for FNIRSI 2C53T (AT32F403A, Cortex-M4F @ 240MHz)
 *
 * Based on reverse-engineered task architecture:
 *   - 6 user tasks (display, key, osc, fpga, dvom_TX, dvom_RX)
 *   - 2 software timers (10-tick and 1000-tick)
 *   - Priority range 0-4 for user tasks, timer service at priority 10
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M4 specific */
#ifdef __NVIC_PRIO_BITS
  #define configPRIO_BITS __NVIC_PRIO_BITS
#else
  #define configPRIO_BITS 4  /* GD32F307 uses 4 priority bits */
#endif

/* Core configuration */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ((uint32_t)240000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ((size_t)(32768))
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Queue and semaphore features (used heavily by original firmware) */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               8

/* Software timer configuration (original firmware uses 2 timers) */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Hook functions */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* Runtime stats and trace — enable trace for task listing */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Co-routine (not used) */
#define configUSE_CO_ROUTINES                   0

/* Interrupt nesting */
#define configKERNEL_INTERRUPT_PRIORITY         (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << (8 - configPRIO_BITS))
#define configMAX_API_CALL_INTERRUPT_PRIORITY   configMAX_SYSCALL_INTERRUPT_PRIORITY

/* FreeRTOS interrupt handler names for Cortex-M */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/* Assert — catches FreeRTOS API misuse during development.
 * Routes through fault_display() so the user sees what happened
 * before the watchdog resets the MCU (~3 seconds). */
extern void fault_display(const char *title, const char *detail);
#define configASSERT(x) if((x) == 0) { \
    taskDISABLE_INTERRUPTS(); \
    fault_display("ASSERT FAIL", "FreeRTOS API misuse"); \
    for(;;); \
}

/* Include FreeRTOS API functions */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xResumeFromISR              1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_eTaskGetState               0
#define INCLUDE_xTimerPendFunctionCall      0

#endif /* FREERTOS_CONFIG_H */
