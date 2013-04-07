#ifndef MACRO_H
#define MACRO_H

#define TRUE 1
#define FALSE 0

#define DOOR_CLOSED 0
#define DOOR_OPENED 1

#define LOCK_LOCKED 0
#define LOCK_RELEASED 1

#define BUTTON_1 0
#define BUTTON_2 1
#define BUTTON_3 2
#define BUTTON_4 3

#define BUTTON_RELEASED 0
#define BUTTON_PRESSED 1

#define I2C_AA      0x00000004
#define I2C_SI      0x00000008
#define I2C_STO     0x00000010
#define I2C_STA     0x00000020
#define I2C_I2EN    0x00000040

typedef struct ledState
{
	int led;
	int operation;
}ledState;


#endif

