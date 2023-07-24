/*
    FreeRTOS V8.2.0 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/**
 * FreeRTOS V8.2.0 lacks an implementation of uxTaskGetStackSize.
 * To address this, certain type declarations were copied from freertos/tasks.c
 */

#include "freertos_task_stack_size.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum
{
    eNotWaitingNotification = 0,
    eWaitingNotification,
    eNotified
} eNotifyValue;

typedef struct tskTaskControlBlock
{
    volatile StackType_t* pxTopOfStack; /*< Points to the location of the last item placed on the tasks stack.  THIS
                                           MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */

#if (portUSING_MPU_WRAPPERS == 1)
    xMPU_SETTINGS xMPUSettings; /*< The MPU settings are defined as part of the port layer.  THIS MUST BE THE SECOND
                                   MEMBER OF THE TCB STRUCT. */
#endif

    ListItem_t xGenericListItem; /*< The list that the state list item of a task is reference from denotes the state of
                                    that task (Ready, Blocked, Suspended ). */
    ListItem_t   xEventListItem; /*< Used to reference a task from an event list. */
    UBaseType_t  uxPriority;     /*< The priority of the task.  0 is the lowest priority. */
    StackType_t* pxStack;        /*< Points to the start of the stack. */
    char         pcTaskName[configMAX_TASK_NAME_LEN];
    /*< Descriptive name given to the task when created.  Facilitates debugging only. */ /*lint !e971 Unqualified
                                                                                            char types are allowed
                                                                                            for strings and single
                                                                                            characters only. */
    BaseType_t xCoreID; /*< Core this task is pinned to */
    /* If this moves around (other than pcTaskName size changes), please change the define in xtensa_vectors.S as well.
     */
#if (portSTACK_GROWTH > 0 || configENABLE_TASK_SNAPSHOT == 1)
    StackType_t*
        pxEndOfStack; /*< Points to the end of the stack on architectures where the stack grows up from low memory. */
#endif

#if (portCRITICAL_NESTING_IN_TCB == 1)
    UBaseType_t uxCriticalNesting; /*< Holds the critical section nesting depth for ports that do not maintain their own
                                      count in the port layer. */
    uint32_t uxOldInterruptState;  /*< Interrupt state before the outer taskEnterCritical was called */
#endif

#if (configUSE_TRACE_FACILITY == 1)
    UBaseType_t uxTCBNumber;  /*< Stores a number that increments each time a TCB is created.  It allows debuggers to
                                 determine when a task has been deleted and then recreated. */
    UBaseType_t uxTaskNumber; /*< Stores a number specifically for use by third party trace code. */
#endif

#if (configUSE_MUTEXES == 1)
    UBaseType_t
        uxBasePriority; /*< The priority last assigned to the task - used by the priority inheritance mechanism. */
    UBaseType_t uxMutexesHeld;
#endif

#if (configUSE_APPLICATION_TASK_TAG == 1)
    TaskHookFunction_t pxTaskTag;
#endif

#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
    void* pvThreadLocalStoragePointers[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
#if (configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS)
    TlsDeleteCallbackFunction_t pvThreadLocalStoragePointersDelCallback[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
#endif
#endif

#if (configGENERATE_RUN_TIME_STATS == 1)
    uint32_t ulRunTimeCounter; /*< Stores the amount of time the task has spent in the Running state. */
#endif

#if (configUSE_NEWLIB_REENTRANT == 1)
    /* Allocate a Newlib reent structure that is specific to this task.
    Note Newlib support has been included by popular demand, but is not
    used by the FreeRTOS maintainers themselves.  FreeRTOS is not
    responsible for resulting newlib operation.  User must be familiar with
    newlib and must provide system-wide implementations of the necessary
    stubs. Be warned that (at the time of writing) the current newlib design
    implements a system-wide malloc() that must be provided with locks. */
    struct _reent xNewLib_reent;
#endif

#if (configUSE_TASK_NOTIFICATIONS == 1)
    volatile uint32_t     ulNotifiedValue;
    volatile eNotifyValue eNotifyState;
#endif

    /* See the comments above the definition of
    tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
#if defined(tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE) && (tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0)
    uint8_t ucStaticallyAllocated; /*< Set to pdTRUE if the task is a statically allocated to ensure no attempt is made
                                      to free the memory. */
#endif

} tskTCB;

typedef tskTCB TCB_t;

static inline TCB_t*
prvGetTCBFromHandle(TaskHandle_t pxHandle)
{
    return ((pxHandle) == NULL) ? (TCB_t*)xTaskGetCurrentTaskHandle() : (TCB_t*)(pxHandle);
}

UBaseType_t
uxTaskGetStackSize(TaskHandle_t xTask)
{
    const TCB_t*      pxTCB    = prvGetTCBFromHandle(xTask);
    const UBaseType_t uxReturn = (pxTCB->pxEndOfStack - pxTCB->pxStack);
    return uxReturn;
}
