#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "macro.h"
#include "starter.h"
#include "led.h"

#define lcdSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

static void vLedTask( void *pvParameters );

void vStartLed( unsigned portBASE_TYPE uxPriority )
{
	/* Spawn the led task. */
	xTaskCreate( vLedTask, ( signed char * ) "Led", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}

//I2C procedure to operate the led.
void ledOperate(int led, int operation)
{	
	const int LED_ON = 0;
	const int LED_OFF = 1;

	unsigned char ledData;
	unsigned char ledDataToSend = 0;
	
	/* Initialise */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_STO;
	
	/* Request send START */
	I20CONSET =  I2C_STA;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));	
	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */
	
	I20DAT	  =  0xC0;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Send control word to read PCA9532 LS2 register */
	I20DAT = 0x08;			//LED8 - LED11
	I20CONCLR =  I2C_SI;

	/* Wait for DATA with control word to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send repeated START */
	I20CONSET =  I2C_STA;
	I20CONCLR =  I2C_SI;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */ 	
	I20DAT	  =  0xC1;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	I20CONCLR = I2C_SI;

	/* Wait for DATA to be received */
	while (!(I20CONSET & I2C_SI));

	//get current state of LS2 register.
	ledData = I20DAT;

	//form the new LED STATE TO BE WRITTEN.
	if(operation == LED_ON)
	{
		ledDataToSend = (0x01 << (led*2)) | ledData;
	}

	if(operation == LED_OFF)
	{
		ledDataToSend = (~(0x03 << (led*2))) & ledData;
	}

	/* Request send repeated START */
	I20CONSET =  I2C_STA;
	I20CONCLR =  I2C_SI;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));
	
	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */ 	
	I20DAT	  =  0xC0;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Send control word to read PCA9532 LS2 register */
	I20DAT = 0x08;
	I20CONCLR =  I2C_SI;

	/* Wait for DATA with control word to be sent */
	while (!(I20CONSET & I2C_SI));

	I20DAT = ledDataToSend; 	//send the led control data to ls2
	I20CONCLR = I2C_SI;
	
	/* Wait for DATA to be sent */
	while (!(I20CONSET & I2C_SI));
	
	/* Request send NAQ and STOP */
	I20CONSET =  I2C_STO;
	I20CONCLR =  I2C_SI | I2C_AA;

	/* Wait for STOP to be sent */
	while (I20CONSET & I2C_STO);

}

//led task. operate the led based on the received led operation instruction.
static portTASK_FUNCTION( vLedTask, pvParameters )
{
	const int OUTER_DOOR_LED = 0;
	const int INNER_DOOR_LED = 2;
	
	const int LED_ON = 0;
	//const int LED_OFF = 1;

	ledState* ledOperation= (ledState*)malloc(sizeof(ledState));
	
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

	//locks are locked at beginning.
	//light both led when the system is initialized.
	ledOperate(OUTER_DOOR_LED,LED_ON);
	ledOperate(INNER_DOOR_LED,LED_ON);
	
	while(1)
	{
		if(xQueueReceive( xLedControlQ, ledOperation, portMAX_DELAY ))
		{
			//receive the instruction and operate the led.
			ledOperate(ledOperation->led,ledOperation->operation);
		}
		vTaskDelayUntil( &xLastWakeTime, 20);
	}
}
