/* 
	Sample task that initialises the EA QVGA LCD display
	with touch screen controller and processes touch screen
	interrupt events.

	Jonathan Dukes (jdukes@scss.tcd.ie)
*/

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lcd.h"
#include "lcd_hw.h"
#include "lcd_grph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Maximum task stack size */
#define lcdSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )
#define BUTTON_OK 9
#define BUTTON_CANCEL 11
#define CYCLE_RATE_MS 10

int button_left[12] = {1,80,160,1,80,160,1,80,160,1,80,160};
int button_right[12] = {80,160,239,80,160,239,80,160,239,80,160,239};
int button_top[12] = {1,80,160,240,1,80,160,240,1,80,160,240};
int button_buttom[12] = {80,160,240,319,80,160,240,319,80,160,240,319};

char* button_string[12] = {"1","2","3","4","5","6","7","8","9","OK","0","CANCEL"};

/* Interrupt handlers */
extern void vLCD_ISREntry( void );
void vLCD_ISRHandler( void );
static xQueueHandle xTouchScreenPressedQ;


/* The LCD task. */
static void vLcdTask( void *pvParameters );


void vStartLcd( unsigned portBASE_TYPE uxPriority )
{
	/* Spawn the console task. */
	xTouchScreenPressedQ = xQueueCreate(16,0);
	xTaskCreate( vLcdTask, ( signed char * ) "Lcd", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}


static portTASK_FUNCTION( vLcdTask, pvParameters )
{
	unsigned int pressure;
	unsigned int xPos;
	unsigned int yPos;
	portTickType xLastWakeTime;
	int i = 0;
	int caseFlag = 0;
	static int stringFlag = 0;
	
	char* result = (char*)(malloc(sizeof(char) * 256));
	for(i = 0; i < 256; i ++)
	{
		result[i] = '\0';
	}


	/* Just to stop compiler warnings. */
	( void ) pvParameters;

	printf("Touchscreen task running\r\n");

	/* Initialise LCD display */
	/* NOTE: We needed to delay calling lcd_init() until here because it uses
	 * xTaskDelay to implement a delay and, as a result, can only be called from
	 * a task */
	lcd_init();

	lcd_fillScreen(MAROON);

	for(i = 0; i < 12; i++)
	{
		lcd_fillRect(button_left[i],button_right[i],button_top[i],button_buttom[i],BLUE);
		lcd_putString((button_right[i] - button_left[i])/2,(button_right[i] - button_left[i])/2,(unsigned char*)button_string[i]);
	}

	/* Infinite loop blocks waiting for a touch screen interrupt event from
	 * the queue. */
	while(1)
	{
		EXTINT = 0x8;			/* Clear TS interrupts (EINT3) */
		VICIntEnable = (1 << 17);	/* Enable TS interrupt vector (VIC) (vector 17) */
		 xQueueReceive( xTouchScreenPressedQ, NULL, portMAX_DELAY );		/* Block waiting for an event indicated by the TS interrupt handler */		
		VICIntEnable = (0 << 17);	/* Disable TS interrupt vector (VIC) (vector 17) */
		/* +++ This point in the code can be interpreted as a screen button push event +++ */
		/* Start repeatedly polling the touchscreen pressure and position ( getTouch(...) ) */
		getTouch(&xPos,&yPos,&pressure);
		xLastWakeTime = xTaskGetTickCount();
		while(pressure > 0)
		{
			vTaskDelayUntil( &xLastWakeTime, CYCLE_RATE_MS );
			for(i = 0; i < 12; i++)
			{
				if((xPos <= button_right[i] && xPos >= button_left[i])&&(yPos <= button_top[i] && yPos >= button_buttom[i]))
				{
					caseFlag = i;
					break;
				}
			}
			
			switch(caseFlag)
			{
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
					result[stringFlag++] = ('1' + caseFlag);
					break;
				case 10:
					result[stringFlag++] = '0';
					break;
				case BUTTON_OK:
					printf("%s",result);
					for(i = 0; i < 256; i ++)
					{
						result[i] = '\0';
					}
					stringFlag = 0;
					break;
				case BUTTON_CANCEL:
					for(i = 0; i < 256; i ++)
					{
						result[i] = '\0';
					}
					stringFlag = 0;
					break;
				default:
					break;
			}			
			getTouch(&xPos,&yPos,&pressure);
		}/* Repeat polling until pressure == 0 */
		/* +++ This point in the code can be interpreted as a screen button release event +++ */
	}
}


void vLCD_ISRHandler( void )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Process the touchscreen interrupt */
	/* We would want to indicate to the task above that an event has occurred */
	xQueueSendFromISR( xTouchScreenPressedQ, 0, &xHigherPriorityTaskWoken  );

    /* Reset the interrupt */
	EXTINT = 8;					/* Reset EINT3 */
	VICVectAddr = 0;			/* Clear VIC interrupt */

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR( xHigherPriorityTaskWoken );
}


