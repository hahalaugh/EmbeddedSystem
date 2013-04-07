#include <LPC24xx.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lcd_hw.h"
#include "lcd_grph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "starter.h"
#include "control.h"
#include "macro.h"


/* Maximum task stack size */
#define lcdSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )
#define SECOND 12000;

#define BUTTON_OK 9				//Position of button ok. Located in the buttom left 
#define BUTTON_CANCEL 11		//Position of button cancel. Located in the buttom right

int button_left[12] = {1,80,160,1,80,160,1,80,160,1,80,160};	//Left coordinates of top left point of the rectangle.
int button_right[12] = {75,155,235,75,155,235,75,155,235,75,155,235};	//Right coordinates of buttom right point of the rectangle.
int button_top[12] = {1,1,1,80,80,80,160,160,160,240,240,240};		//Top coordinates of top left point of the rectangle.
int button_buttom[12] = {75,75,75,155,155,155,235,235,235,319,319,319};	//Buttom coordinates of buttom right point of the rectangle.

char* button_string[12] = {"1","2","3","4","5","6","7","8","9","OK","0","CANCEL"};
char* password = "9527";

/* Interrupt handlers */
extern void vLCD_ISREntry( void );
void vLCD_ISRHandler( void );

static xQueueHandle xTouchScreenPressedQ;			//The queue works as binary semaphore 
xQueueHandle xLedControlQ;						//this queue is used for sending operation type to led control task 
xQueueHandle xTimerQ;							//this queue is used for sending the start signal to the timeout control task.


/* The control task include there sub tasks. */
static void vLcdTask( void *pvParameters );
static void vButtonTask( void *pvParameters );	
static void vTimerTask( void *pvParameters );

void ledControl(int led, int operation)
{	
	ledState* ledInfo= (ledState*)malloc(sizeof(ledState));		//ledInfo is the struct store the operation of turn on/off the certain led.
	
	ledInfo->led = led;
	ledInfo->operation = operation;
	
	xQueueSendToBack( xLedControlQ, ledInfo, 0 );			//send the operation type to the led control task.
}

//to change the states of door and door lock. these states could only be changed in control task.
void lock(int* lock)
{
	*lock = LOCK_LOCKED;
}

void release(int* lock)
{
	*lock = LOCK_RELEASED;
}

void open(int* door)
{
	*door = DOOR_OPENED;
}

void close(int* door)
{
	*door = DOOR_CLOSED;
}

void vStartControl( unsigned portBASE_TYPE uxPriority )
{
	/* Spawn the console task. */
	//initializing the queues
	xLedControlQ = xQueueCreate(16,( unsigned portBASE_TYPE )sizeof(ledState));// the size of queue unit should be same with size of struct ledState.
	xTouchScreenPressedQ = xQueueCreate(16,0);			
	xTimerQ = xQueueCreate(16,0);

	
	/* Initialise LCD hardware */
	lcd_hw_init();

	/* Setup LCD interrupts */
	PINSEL4 |= 1 << 26;				/* Enable P2.13 for EINT3 function */
	EXTMODE |= 8;					/* EINT3 edge-sensitive mode */
	EXTPOLAR &= ~0x8;				/* Falling edge mode for EINT3 */

	/* Setup VIC for LCD interrupts */
	VICIntSelect &= ~(1 << 17);		/* Configure vector 17 (EINT3) for IRQ */
	VICVectPriority17 = 15;			/* Set priority 15 (lowest) for vector 17 */
	VICVectAddr17 = (unsigned long)vLCD_ISREntry;
									/* Set handler vector */				
	/* Enable and configure I2C0 */
	PCONP    |=  (1 << 7);                /* Enable power for I2C0              */

	/* Initialize pins for SDA (P0.27) and SCL (P0.28) functions                */
	PINSEL1  &= ~0x03C00000;
	PINSEL1  |=  0x01400000;

	/* Clear I2C state machine                                                  */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_I2EN;
	
	/* Setup I2C clock speed                                                    */
	I20SCLL   =  0x80;
	I20SCLH   =  0x80;
	
	I20CONSET =  I2C_I2EN;

	/* Spawn the control sub tasks. */
	xTaskCreate( vButtonTask, ( signed char * ) "Button", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
	xTaskCreate( vLcdTask, ( signed char * ) "Lcd", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
	xTaskCreate( vTimerTask, ( signed char * ) "Timer", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}


unsigned char getButtons()
{
	unsigned char ledData;

	/* Initialise */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_STO;
	
	/* Request send START */
	I20CONSET =  I2C_STA;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */		
	I20DAT    =  0xC0;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Send control word to read PCA9532 INPUT0 register */
	I20DAT = 0x00;
	I20CONCLR =  I2C_SI;

	/* Wait for DATA with control word to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send repeated START */
	I20CONSET =  I2C_STA;
	I20CONCLR =  I2C_SI;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */		
	I20DAT    =  0xC1;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	I20CONCLR = I2C_SI;

	/* Wait for DATA to be received */
	while (!(I20CONSET & I2C_SI));

	ledData = I20DAT;

	/* Request send NAQ and STOP */
	I20CONSET =  I2C_STO;
	I20CONCLR =  I2C_SI | I2C_AA;

	/* Wait for STOP to be sent */
	while (I20CONSET & I2C_STO);

	return ledData ^ 0xf;
}


static portTASK_FUNCTION( vButtonTask, pvParameters )
{
		portTickType xLastWakeTime;
		unsigned char buttonState;
		unsigned char lastButtonState;
		unsigned char changedState;
		unsigned int i;
		unsigned char mask;

		//operation options
		const int OUTER_DOOR_LED = 0;
		const int INNER_DOOR_LED = 2;

		const int LED_ON = 0;
		const int LED_OFF = 1;

		//current button and button operation
		int buttonFlag = 0;		
		int operationFlag = 0;
	
		/* Just to stop compiler warnings. */
		( void ) pvParameters;
	
		printf("button task begin\r\n");
		/* initialise lastState with all buttons off */
		lastButtonState = 0;
		
		xLastWakeTime = xTaskGetTickCount();
						 
		/* Infinite loop blocks waiting for a touch screen interrupt event from
		 * the queue. */
		while( 1 )
		{
			/* Read buttons */
			//printf("before press,innerlock = %d innerdoor = %d outerlock = %d outerdoor = %d\r\n",innerLock,innerDoor,outerLock,outerDoor);
			//get current button flag and operation.
			buttonState = getButtons();
	
			changedState = buttonState ^ lastButtonState;
			
			if (buttonState != lastButtonState)
			{
				/* iterate over each of the 4 LS bits looking for changes in state */
				for (i = 0; i <= 3; i = i++)
				{
					mask = 1 << i;
					
					if (changedState & mask)
					{
						buttonFlag = i;
						operationFlag = (buttonState & mask) ? BUTTON_PRESSED : BUTTON_RELEASED;
						//printf("button %d is %d\r\n",buttonFlag,operationFlag);
						switch(buttonFlag)
						{
							//control the led according to the states of locks and doors.
							case BUTTON_1:
								if(operationFlag == BUTTON_PRESSED )
								{
									if(innerLock == LOCK_LOCKED && outerLock== LOCK_LOCKED)
									{
										//only one door could be released at same time.
										release(&outerLock);
										//timeout task start counting
										xQueueSendToBack( xTimerQ, 0, 0 );
										//turn the led off.
										ledControl(OUTER_DOOR_LED, LED_OFF);
									}
								}
								break;
							case BUTTON_2:
								if(operationFlag == BUTTON_PRESSED )
								{
									//pressing the button simulates pulling the door open.
									if(innerLock == LOCK_LOCKED && outerLock== LOCK_RELEASED && outerDoor == DOOR_CLOSED)
									{	
										//only the door whose lock is released could be opened.
										//only one door could be opened at same time.
										open(&outerDoor);
									}
								}

								if(operationFlag == BUTTON_RELEASED )
								{
									//pressing the button simultes pushing the door close.
									close(&outerDoor);
									//the lock would be locked as soon as the door is closed.
									lock(&outerLock);
									//turn on the led to indicate the lock is locked.
									ledControl(OUTER_DOOR_LED, LED_ON);
								}
								break;
							case BUTTON_3:
								//similar procedure and control logic for inner door.
								if(operationFlag == BUTTON_PRESSED )
								{
									if(outerLock == LOCK_LOCKED && innerLock == LOCK_LOCKED)
									{
										release(&innerLock);
										xQueueSendToBack( xTimerQ, 0, 0 );
										ledControl(INNER_DOOR_LED, LED_OFF);
									}
								}
								break;
								
							case BUTTON_4:
								if(operationFlag == BUTTON_PRESSED )
								{
									if(innerLock == LOCK_RELEASED && outerLock== LOCK_LOCKED&& innerDoor == DOOR_CLOSED)
									{
										open(&innerDoor);
									}
								}

								if(operationFlag == BUTTON_RELEASED )
								{
									close(&innerDoor);
									lock(&innerLock);
									ledControl(INNER_DOOR_LED, LED_ON);
								}
								break;
							default:
								break;
						}
						printf("current states: innerlock = %d innerdoor = %d outerlock = %d outerdoor = %d\r\n",innerLock,innerDoor,outerLock,outerDoor);
					}
				}
				
				/* remember new state */
				lastButtonState = buttonState;
	
			}
			
			/* delay before next poll */
			vTaskDelayUntil( &xLastWakeTime, 20);
		}
	}

//comparing and judging if two strings are same with each other.
int sameCode(char* code1, char* code2)
{
	int i = 0;

	//when the string is not over.
	while(code1[i] != '\0' && code2[i] != '\0')
	{
		
		if(code1[i] != code2[i])
		{
			return FALSE;
		}
		i++;
	}

	//string with different length cannot be the same.
	if(code1[i] == '\0' && code2[i] == '\0')
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

// lcd control task. read the lcd input and do operation.
//use the code of previous key pad assignment.
// add some operations when the password is correct.
static portTASK_FUNCTION( vLcdTask, pvParameters )
{
	
	const int OUTER_DOOR_LED = 0;
	//const int INNER_DOOR_LED = 2;

	//const int LED_ON = 0;
	const int LED_OFF = 1;
		
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
	 	
	printf("lcd task begins\r\n");
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
						if(sameCode(result,password))
						{
							//if the code is correct, release the outer lock.
							if(innerLock == LOCK_LOCKED && outerDoor == LOCK_LOCKED)
							{
								//release the outer lock and turn off the led light.
								release(&outerLock);
								xQueueSendToBack( xTimerQ, 0, 0 );
								ledControl(OUTER_DOOR_LED,LED_OFF);
								printf("Password Right! Door opened!\r\n");
							}
						}
						else
						{
							printf("Password Incorrect or operation failed\r\n");
						}
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
}

static portTASK_FUNCTION( vTimerTask, pvParameters )
{
	const int OUTER_DOOR_LED = 0;
	const int INNER_DOOR_LED = 2;

	const int LED_ON = 0;
	//const int LED_OFF = 1;
	
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

	/* Just to stop compiler warnings. */
	( void ) pvParameters;
	
	while(1)
	{
		//begin a 5s time delay and lock the corresponding door lock after 5s.
		if(xQueueReceive( xTimerQ, NULL, portMAX_DELAY ))
		{
			vTaskDelay(5000);
			if(outerLock == LOCK_RELEASED)
			{
				lock(&outerLock);
				ledControl(OUTER_DOOR_LED, LED_ON);
				//printf("timeout!outer door locked automatically\r\n");
			}
			
			if(innerLock == LOCK_RELEASED)
			{
				lock(&innerLock);
				ledControl(INNER_DOOR_LED, LED_ON);
				//printf("timeout!inner door locked automatically\r\n");
			}
		}
		vTaskDelayUntil( &xLastWakeTime, 20);
	}
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


