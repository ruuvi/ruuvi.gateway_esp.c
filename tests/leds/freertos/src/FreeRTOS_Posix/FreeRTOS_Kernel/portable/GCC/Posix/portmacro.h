/*
 * FreeRTOS Kernel V10.2.1
 */


#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "mux" data structure (spinlock) */
typedef struct {
    /* owner field values:
     * 0                - Uninitialized (invalid)
     * portMUX_FREE_VAL - Mux is free, can be locked by either CPU
     * CORE_ID_PRO / CORE_ID_APP - Mux is locked to the particular core
     *
     * Any value other than portMUX_FREE_VAL, CORE_ID_PRO, CORE_ID_APP indicates corruption
     */
    uint32_t owner;
    /* count field:
     * If mux is unlocked, count should be zero.
     * If mux is locked, count is non-zero & represents the number of recursive locks on the mux.
     */
    uint32_t count;
#ifdef CONFIG_FREERTOS_PORTMUX_DEBUG
    const char *lastLockedFn;
	int lastLockedLine;
#endif
} portMUX_TYPE;

#define portMUX_FREE_VAL		0xB33FFFFF

/* Special constants for vPortCPUAcquireMutexTimeout() */
#define portMUX_NO_TIMEOUT      (-1)  /* When passed for 'timeout_cycles', spin forever if necessary */
#define portMUX_TRY_LOCK        0     /* Try to acquire the spinlock a single time only */

// Keep this in sync with the portMUX_TYPE struct definition please.
#ifndef CONFIG_FREERTOS_PORTMUX_DEBUG
#define portMUX_INITIALIZER_UNLOCKED {					\
		.owner = portMUX_FREE_VAL,						\
		.count = 0,										\
	}
#else
#define portMUX_INITIALIZER_UNLOCKED {					\
		.owner = portMUX_FREE_VAL,						\
		.count = 0,										\
		.lastLockedFn = "(never locked)",				\
		.lastLockedLine = -1							\
	}
#endif



/* Type definitions. */
#define portSTACK_TYPE unsigned long
#define portBASE_TYPE long
#define portLONG long
#define portSHORT short
#define portUBASE_TYPE unsigned long
#define portMAX_DELAY  (TickType_t) 0xffffffffffffffffUL
#define portPOINTER_SIZE_TYPE uint64_t

typedef portSTACK_TYPE StackType_t;
typedef portBASE_TYPE BaseType_t;
typedef portUBASE_TYPE UBaseType_t;
typedef portUBASE_TYPE TickType_t;

/* Architecture specifics. */
#define portSTACK_GROWTH ( -1 )
#define portTICK_PERIOD_MS ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portTICK_RATE_MICROSECONDS ( ( TickType_t ) 1000000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT 8

/* Scheduler utilities. */
extern void vPortYieldFromISR( void );
extern void vPortYield( void );
extern void vTaskSwitchContext( void );
#define portYIELD() vPortYield()
#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired ) vPortYieldFromISR()
#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )


/* Critical section management. */
extern void vPortDisableInterrupts( void );
extern void vPortEnableInterrupts( void );
#define portSET_INTERRUPT_MASK()( vPortDisableInterrupts() )
#define portCLEAR_INTERRUPT_MASK()( vPortEnableInterrupts() )

extern portBASE_TYPE xPortSetInterruptMask( void );
extern void vPortClearInterruptMask( portBASE_TYPE xMask );

#define portSET_INTERRUPT_MASK_FROM_ISR() xPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x) vPortClearInterruptMask(x)

#define portCRITICAL_NESTING_IN_TCB 1
extern void vTaskEnterCritical( void );
extern void vTaskExitCritical( void );

extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portDISABLE_INTERRUPTS() portSET_INTERRUPT_MASK()
#define portENABLE_INTERRUPTS() portCLEAR_INTERRUPT_MASK()
#define portENTER_CRITICAL() vTaskEnterCritical()
#define portEXIT_CRITICAL() vTaskExitCritical()


/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

#define portNOP()

#define portOUTPUT_BYTE( a, b )

extern void vPortForciblyEndThread( void *pxTaskToDelete );
#define traceTASK_DELETE( pxTaskToDelete )		vPortForciblyEndThread( pxTaskToDelete )

extern void vPortAddTaskHandle( void *pxTaskHandle );
#define traceTASK_CREATE( pxNewTCB )			vPortAddTaskHandle( pxNewTCB )

/* Posix Signal definitions that can be changed or read as appropriate. */
#define SIG_SUSPEND					SIGUSR1
#define SIG_RESUME					SIGUSR2

/* Enable the following hash defines to make use of the real-time tick where time progresses at real-time. */
#define SIG_TICK					SIGALRM
#define TIMER_TYPE					ITIMER_REAL
/* Enable the following hash defines to make use of the process tick where time progresses only when the process is executing.
#define SIG_TICK					SIGVTALRM
#define TIMER_TYPE					ITIMER_VIRTUAL		*/
/* Enable the following hash defines to make use of the profile tick where time progresses when the process or system calls are executing.
#define SIG_TICK					SIGPROF
#define TIMER_TYPE					ITIMER_PROF */

/* Make use of times(man 2) to gather run-time statistics on the tasks. */
extern void vPortFindTicksPerSecond( void );
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()	vPortFindTicksPerSecond()		/* Nothing to do because the timer is already present. */
extern unsigned long ulPortGetTimerValue( void );
#define portGET_RUN_TIME_COUNTER_VALUE()			ulPortGetTimerValue()			/* Query the System time stats for this process. */

#define xTaskCreatePinnedToCore(code, name, stack, parameters, priority, handle, core) xTaskCreate(code, name, stack, parameters, priority, handle)

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */

