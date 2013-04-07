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

#define BUTTON_OK 9				//Position of button ok. Located in the buttom left 
#define BUTTON_CANCEL 11		//Position of button cancel. Located in the buttom right

int button_left[12] = {1,80,160,1,80,160,1,80,160,1,80,160};	//Left coordinates of top left point of the rectangle.
int button_right[12] = {75,155,235,75,155,235,75,155,235,75,155,235};	//Right coordinates of buttom right point of the rectangle.
int button_top[12] = {1,1,1,80,80,80,160,160,160,240,240,240};		//Top coordinates of top left point of the rectangle.
int button_buttom[12] = {75,75,75,155,155,155,235,235,235,319,319,319};	//Buttom coordinates of buttom right point of the rectangle.

char* button_string[12] = {"1","2","3","4","5","6","7","8","9","OK","0","CANCEL"};

/* Interrupt handlers */
extern void vLCD_ISREntry( void );
void vLCD_ISRHandler( void );

static xQueueHandle xTouchScreenPressedQ;			//The queue works as binary semaphore 



/* The LCD task. */
static void vLcdTask( void *pvParameters );


void vStartLcd( unsigned portBASE_TYPE uxPriority )
{
	/* Spawn the console task. */
	xTouchScreenPressedQ = xQueueCreate(16,0);			//Initializing the queu
	xTaskCreate( vLcdTask, ( signed char * ) "Lcd", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}


static portTASK_FUNCTION( vLcdTask, pvParameters )
{
	unsigned int pressure;
	unsigned int xPos;
	unsigned int yPos;
	int i = 0;	
	int caseFlag = 0;		//current case according to the location of pressure.
	static int stringFlag = 0;		//String cursor.
	
	char* result = (char*)(malloc(sizeof(char) * 256));		//Initializing the memory. a 256 bit long result could be stored.
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

	//Drawing the buttons and labels.
	for(i = 0; i < 12; i++)
	{
		//printf("x1 = %d, x2 = %d, y1 = %d, y2 = %d\r\n",button_left[i],button_right[i],button_top[i],button_buttom[i]);
		lcd_fillRect(button_left[i],button_top[i],button_right[i],button_buttom[i],BLUE);
		//printf("write %s\r\n",button_string[i]);
		lcd_putString(button_left[i] + (button_right[i] - button_left[i])/2,button_top[i] + (button_buttom[i] - button_top[i])/2,(unsigned char*)button_string[i]);
	}

	printf("Layout initialized\r\n");

	/* Infinite loop blocks waiting for a touch screen interrupt event from
	 * the queue. */

	 while(1)
	 {
		EXTINT = 8;
		VICIntEnable |= (1 << 17);
		//The interrupt occurs
		if(xQueueReceive( xTouchScreenPressedQ, NULL, portMAX_DELAY ))
		{
			//The vector interrupt controler is not disabled since no loop pressure detect is used.
			getTouch(&xPos,&yPos,&pressure);
			vTaskDelay(300);			//In case of resbonding next pressing directly.
			//To avoid the bug that point(1,320) is always detected to be pressed.
			if(xPos != 1)
		 	{
		 		//Decide the case by coordinates.
				for(i = 0; i < 12; i++)
				{
					if((xPos <= button_right[i] && xPos >= button_left[i])&&(yPos >= button_top[i] && yPos <= button_buttom[i]))
					{
						caseFlag = i;
						break;
					}
				}
			
				switch(caseFlag)
				{
					//case number button: add the character to the result string
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
					case 8:
						printf("%c pressed\r\n",'1' + caseFlag);
						result[stringFlag++] = ('1' + caseFlag);
						break;
					case 10:
						printf("%c pressed\r\n",'1' + caseFlag);
						result[stringFlag++] = '0';
						break;
					//print the result and clean the result
					case BUTTON_OK:
						printf("result = %s\r\n",result);
					//Just clean the result
					case BUTTON_CANCEL:
						printf("result cleaned\r\n");
						for(i = 0; i < 256; i ++)
						{
							result[i] = '\0';
						}
						stringFlag = 0;
						break;															 	
					default:
						break;
				}
			}
		}
	}
	 /*
	 The following code could be used but the printf function sometimes crash the program down and I couldn't figure
	 out why would that happened.
	 Here is the possible reasons:
	 1.The Task schedule module of freertos define the priority of IO or other relevant interrupt resource higher than 
	   	EXTINT3.
	 2. The compiler compile the code and doesn't treat xQueueReceive() as a procedure which stuck the program and
	 	should be located before the execution of printf();
	 */
		/*
		if(xQueueReceive( xTouchScreenPressedQ, NULL, portMAX_DELAY ))
		{
			VICIntEnClr = (0 << 17);
			getTouch(&xPos,&yPos,&pressure);
			while(pressure > 0 && xPos != 1)
			{
			 	 printf("x = %d, y = %d, pressure = %d\r\n",xPos,yPos,pressure);
				 getTouch(&xPos,&yPos,&pressure);
			 }
			 for(i = 0; i < 12; i++)
			{
				if((xPos <= button_right[i] && xPos >= button_left[i])&&(yPos >= button_top[i] && yPos <= button_buttom[i]))
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
					//printf("result = %s\r\n",result);
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
		} */
}

void vLCD_ISRHandler( void )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Process the touchscreen interrupt */
	/* We would want to indicate to the task above that an event has occurred */
	xQueueSendFromISR( xTouchScreenPressedQ, 0, &xHigherPriorityTaskWoken  );	//Send the signal to queue when interrupt occurs

    /* Reset the interrupt */
	EXTINT = 8;					/* Reset EINT3 */
	VICVectAddr = 0;			/* Clear VIC interrupt */

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR( xHigherPriorityTaskWoken );
}


