// Standard includes
#include <stdio.h>
#include <string.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"


/*********************************************************************************************************
*												MACROS
*********************************************************************************************************/
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY	1

#define mainDELAY_LOOP_COUNT 100
#define ROUND_ROBIN_TIME_SLICE 1000 	// Time slice for round-robin scheduling.

/* Priority levels for tasks.
	Note that: low priority numbers denote low priority tasks. */
#define TASK_1_PRIORITY 6
#define TASK_2_PRIORITY 1
#define TASK_3_PRIORITY 6
#define TASK_4_PRIORITY (configMAX_PRIORITIES - 1)

/* Dimensions of the buffer that the task being created will use as its stack.
	NOTE:  This is the number of words the stack will hold, not the number of bytes.
	For example, if each stack item is 32-bits, and this is set to 100, then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 200


/*********************************************************************************************************
*										UART: printf handling
*********************************************************************************************************/
//printf() output uses the UART.  These constants define the addresses of the required UART registers.
#define UART0_ADDRESS 	( 0x40004000UL )												// Base address of the UART peripheral
#define UART0_DATA		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 0UL ) ) ) )	// Register to write data to for transmission
#define UART0_STATE		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 4UL ) ) ) )	// Register that holds the status of the UART
#define UART0_CTRL		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 8UL ) ) ) )	// Control register for configuring the UART
#define UART0_BAUDDIV	( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 16UL ) ) ) )	// Register for configuring the baud rate
#define TX_BUFFER_MASK	( 1UL )															// Constant used for masking the transmission buffer

// Initialization function to set up the UART peripheral => Printf() output is sent to the serial port
static void prvUARTInit(void);

/* Declare a mutex. The use of this mutex ensures that access to the UART is synchronized among the tasks to avoid data
	corruption or conflicts during simultaneous access. */
SemaphoreHandle_t xMutex;


/*********************************************************************************************************
*										  APPLICATION GLOBALS
*********************************************************************************************************/
/* For tasks, a task handle is essentially a reference to a specific task.
	For example after the task creation, you can use the task's handle to delete the task (vTaskDelete(xHandle)). */
TaskHandle_t xHandle_1 = NULL;
TaskHandle_t xHandle_2 = NULL;
TaskHandle_t xHandle_3 = NULL;
TaskHandle_t xHandle_4 = NULL;
TimerHandle_t xTimer;

/* Define the strings that will be passed as the task parameters.
	These are defined as const and not on the stack to ensure they remain valid when the tasks are executing. */
static const char *pcTextForTask1 = "Task 1 is running\r\n";
static const char *pcTextForTask2 = "Task 2 is running\r\n";
static const char *pcTextForTask3 = "Task 3 is running\r\n";
static const char *pcTextForTask4 = "Time is up! Task 4 is running\r\n";


/*********************************************************************************************************
*									   LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************/
// Function prototypes for task functions and timer callback
static void vTask1(void *);
static void vTask2(void *);
static void vTask3(void *);
static void vTask4(void *);

static void vTimerCallback(TimerHandle_t xTimer);


/**********************************************************************************************************
*												main()
*********************************************************************************************************/
void main(void)
{
    prvUARTInit();

    BaseType_t xReturned_1, xReturned_2, xReturned_3, xReturned_4;

    // Create a mutex and a timer
    xMutex = xSemaphoreCreateMutex();
    xTimer = xTimerCreate("Timer", pdMS_TO_TICKS(10000), pdTRUE, 0, vTimerCallback);

        if (xTimer != NULL)
        {
        	// Task creation
            xReturned_1 = xTaskCreate(	vTask1, 					// Pointer to the task entry function (name of function)
            							"vTask1", 					// Name for the task
            							STACK_SIZE, 				// The number of words (not bytes!) to allocate for use as the task's stack
            							(void *)pcTextForTask1,   	// A value that is passed as the paramater to the created task
            							TASK_1_PRIORITY, 			// Priority at which the task is created
            							&xHandle_1);				// Used to pass out the created task's handle

            xReturned_2 = xTaskCreate(vTask2, "vTask2", STACK_SIZE, (void *)pcTextForTask2, TASK_2_PRIORITY, &xHandle_2);
            xReturned_3 = xTaskCreate(vTask3, "vTask3", STACK_SIZE, (void *)pcTextForTask3, TASK_3_PRIORITY, &xHandle_3);
            xReturned_4 = xTaskCreate(vTask4, "vTask4", STACK_SIZE, (void *)pcTextForTask4, TASK_4_PRIORITY, &xHandle_4);

            if (xReturned_1 == pdPASS && xReturned_2 == pdPASS && xReturned_3 == pdPASS && xReturned_4 == pdPASS)
            {
            	// All the tasks have been created correctly
            	xTimerStart(xTimer, 0);
                vTaskStartScheduler();
            }
            else
            {
                printf("Error creating tasks. Code 1: %ld, Code 2: %ld, Code 3: %ld, Code 4: %ld\r\n",
                       xReturned_1, xReturned_2, xReturned_3, xReturned_4);
            }
        }
        else
        {
            printf("Error creating timer\r\n");
        }

    for (;;);
}


/**********************************************************************************************************
*											Task Functions
*********************************************************************************************************/
void vTask1(void *pvParameters)
{
    // Cast the generic parameter to a character pointer representing the task name
    char *pcTaskName = (char *)pvParameters;

    // Infinite loop for the task
    for (;;)
    {
        // Attempt to take the mutex, blocking indefinitely if necessary
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // Mutex acquired successfully => print the task name
            printf("%s", pcTaskName);

            // Release the mutex
            xSemaphoreGive(xMutex);

            // Introduce a time-slice delay
            vTaskDelay(pdMS_TO_TICKS(ROUND_ROBIN_TIME_SLICE));
        }
        else
        {
            // Unable to acquire the mutex, print an error message
            printf("Error taking the mutex\r\n");
        }
    }
}
/*-----------------------------------------------------------*/

void vTask2(void *pvParameters)
{
    char *pcTaskName = (char *)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("%s", pcTaskName);
            xSemaphoreGive(xMutex);

            vTaskDelay(pdMS_TO_TICKS(ROUND_ROBIN_TIME_SLICE));
        }
        else
        {
            printf("Error taking the mutex\r\n");
        }
    }
}
/*-----------------------------------------------------------*/

void vTask3(void *pvParameters)
{
    char *pcTaskName = (char *)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("%s", pcTaskName);
            xSemaphoreGive(xMutex);

            // Introduce time-slice delay
            vTaskDelay(pdMS_TO_TICKS(ROUND_ROBIN_TIME_SLICE));
        }
        else
        {
            printf("Error taking the mutex\r\n");
        }
    }
}
/*-----------------------------------------------------------*/

static void vTask4(void *pvParameters)
{
    char *pcTaskName = (char *)pvParameters;

    for (;;)
    {
    	// Wait for the timer callback to signal that 10 seconds have passed
       	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

       	if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
       		printf("%s", pcTaskName);
       		xSemaphoreGive(xMutex);

       		vTaskDelay(pdMS_TO_TICKS(ROUND_ROBIN_TIME_SLICE));
        }

       	else
       		printf("Error taking the mutex\r\n");
    }
}
/*-----------------------------------------------------------*/

static void vTimerCallback(TimerHandle_t xTimer)
{
    // Notify the fourth task that 10 seconds have passed
    vTaskNotifyGiveFromISR(xHandle_4, NULL);
}


/**********************************************************************************************************
*											Already defined Functions
*********************************************************************************************************/
void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created using the dynamic allocation (as opposed to
	static allocation) option.  It is also called by various parts of the
	demo application.  If heap_1.c, heap_2.c or heap_4.c is being used, then the
	size of the	heap available to pvPortMalloc() is defined by
	configTOTAL_HEAP_SIZE in FreeRTOSConfig.h, and the xPortGetFreeHeapSize()
	API function can be used to query the size of free heap space that remains
	(although it does not provide information on how the remaining heap might be
	fragmented).  See http://www.freertos.org/a00111.html for more
	information. */
	printf( "\r\n\r\nMalloc failed\r\n" );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If application tasks make use of the
	vTaskDelete() API function to delete themselves then it is also important
	that vApplicationIdleHook() is permitted to return to its calling function,
	because it is the responsibility of the idle task to clean up memory
	allocated by the kernel to any task that has since deleted itself. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	printf( "\r\n\r\nStack overflow in %s\r\n", pcTaskName );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */

	#if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY != 1 )
	{
		extern void vFullDemoTickHookFunction( void );

		vFullDemoTickHookFunction();
	}
	#endif /* mainCREATE_SIMPLE_BLINKY_DEMO_ONLY */
}
/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook( void )
{
	/* This function will be called once only, when the daemon task starts to
	execute (sometimes called the timer task).  This is useful if the
	application includes initialisation code that would benefit from executing
	after the scheduler has been started. */
}
/*-----------------------------------------------------------*/

void vAssertCalled( const char *pcFileName, uint32_t ulLine )
{
volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

	/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */

	printf( "ASSERT! Line %d, file %s\r\n", ( int ) ulLine, pcFileName );

 	taskENTER_CRITICAL();
	{
		/* You can step out of this function to debug the assertion by using
		the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
		value. */
		while( ulSetToNonZeroInDebuggerToContinue == 0 )
		{
			__asm volatile( "NOP" );
			__asm volatile( "NOP" );
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

static void prvUARTInit( void )
{
	UART0_BAUDDIV = 16;
	UART0_CTRL = 1;
}
/*-----------------------------------------------------------*/

int __write( int iFile, char *pcString, int iStringLength )
{
	int iNextChar;

	/* Avoid compiler warnings about unused parameters. */
	( void ) iFile;

	/* Output the formatted string to the UART. */
	for( iNextChar = 0; iNextChar < iStringLength; iNextChar++ )
	{
		while( ( UART0_STATE & TX_BUFFER_MASK ) != 0 );
		UART0_DATA = *pcString;
		pcString++;
	}

	return iStringLength;
}
/*-----------------------------------------------------------*/

void *malloc( size_t size )
{
	( void ) size;

	/* This project uses heap_4 so doesn't set up a heap for use by the C
	library - but something is calling the C library malloc().  See
	https://freertos.org/a00111.html for more information. */
	printf( "\r\n\r\nUnexpected call to malloc() - should be usine pvPortMalloc()\r\n" );
	portDISABLE_INTERRUPTS();
	for( ;; );

}
/*-----------------------------------------------------------*/
