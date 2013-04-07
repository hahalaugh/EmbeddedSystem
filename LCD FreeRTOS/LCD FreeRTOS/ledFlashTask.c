/* 
	Simple task that causes the P2.10 LED to flash.
*/

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lpc24xx.h"


/* Maximum task stack size */
#define ledFlashTaskSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )


/* Hardware definitions */
#define P210BIT			( ( unsigned long ) 0x0004 )


/* The LCD task. */
static void vLedFlashTask( void *pvParameters );


void vStartLedFlashTask( unsigned portBASE_TYPE uxPriority )
{
	/* Spawn the console task . */
	xTaskCreate( vLedFlashTask, ( signed char * ) "Flash", ledFlashTaskSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}


static portTASK_FUNCTION( vLedFlashTask, pvParameters )
{
	unsigned long ulCurrentState;
	portTickType xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

	while(1)
	{
		vTaskDelayUntil( &xLastWakeTime, 1000);

		/* If this bit is already set, clear it, and visa versa. */
		ulCurrentState = FIO2PIN1;
		if( ulCurrentState & P210BIT )
		{
			FIO2CLR1 = P210BIT;
		}
		else
		{
			FIO2SET1 = P210BIT;			
		}
	}
}
