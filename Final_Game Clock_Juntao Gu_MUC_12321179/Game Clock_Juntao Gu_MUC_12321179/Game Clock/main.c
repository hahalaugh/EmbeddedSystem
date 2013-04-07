#include <stdlib.h>
#include <LPC24xx.h>
#include <stdio.h>

#define P210BIT ( ( unsigned long ) 0x4 )
#define MATCH_TIME 120000000				//Game time, it's 10s here
#define PLAYER_NONE -1
#define PLAYER_A 1
#define PLAYER_B 0
#define CURRENT_PLAYER ((button_pushed_times%2)%2)

extern void init_serial(void);

int return_from_timer_reset_flag;			//Indicates if the programme returns from the timer reset handler.
int player_flag;							//indicates which player is playing the game now.
long button_pushed_times;					//used as statues symbol. 

//Store the left time for player a and player b respectively
long clock_player_a;	
long clock_player_b;

int first_play = 1;			//first_play flag indicates the game is begin or not.
int beep = 0;

__irq void timer_reset_handler(void)
{
	beep = 0;
	
	//It will beep before the game clock is reset or the game begins.
	PINSEL1 &= ~(3 << 20); /* Enable P0.26 for AOUT function */
	PINSEL1 |= (2 << 20);
	while(beep++ < 50000)
	{
		DACR =(beep<<6);
	}
	
	 T0IR = 0xFF;
	 T0TCR = 0x2;
	 T1TCR = 0x2;
	 
	 if(first_play)		//if the game is not begin, a 5s press on the button will start the game.The game begins and player A's timer begins to count.
	 {
		printf("game start\r\n");
		//A begin
		player_flag = PLAYER_A;
		T1MR0 = clock_player_a;
		T1TCR = 0x2;
		T1TCR = 0x1;
		first_play = 0;			//game is started so set first_play flag false.
	 }
	 else				//game begins. so a 5s press on the button will reset the game clock
	 {
		 printf("reset\r\n");
													   
		 player_flag = PLAYER_NONE;
		 //button_pushed_times = 0;
		 //reset all the variable to default statues.
		 T1MR0 = MATCH_TIME;
		 clock_player_a = MATCH_TIME;
		 clock_player_b = MATCH_TIME;
		 first_play = 1;
		 T1TCR = 0x02;
		 T0TCR = 0x02;
	 }

	 return_from_timer_reset_flag = 1;		//It's return from the timer reset handler so no more action in the button handler.
	 VICVectAddr = 0;

}

__irq void button_handler(void)
{

	button_pushed_times++;					//increment the variable every time the button is pressed and released.The variable is used to indicates the current 
											//player or the statues of the programme.

	EXTINT = 1;
	EXTPOLAR = !EXTPOLAR;					//respond to the opposite direction of electrical level hop.

	T0TCR = 0x2;							//reset the counter to avoid potential fault.
	if(EXTPOLAR != 0)
	{
		//printf("start timing \r\n");		//the timer begin to count only if when the button is pressed down.
		T0TCR = 0x1;
	}

	if(first_play)							//if the game is not begin
	{
		if(button_pushed_times%2 == 0)		//if the press time is not longer than 5s. nothing happened but only tips.
		{
			printf("please press more than 5s to start game\r\n");
			button_pushed_times = 0;
			T0TCR = 0x2;
		}
	}
	else									//game started.
	{
	if(return_from_timer_reset_flag == 1)	//the game is reset or started. do nothing in this loop and continues.
	{
		//return_from_timer_reset_flag = 0;
		T0TCR = 0x2;				//clear the timer0 twice in case of the user push the button for too long.
		return_from_timer_reset_flag = 0;		
	}
	else
	{
		if(EXTPOLAR == 0 && (button_pushed_times != 2))			// Falling Edge Sensitive to respond the release button action.
																// and no response if the button is pushed before game is started.
		{
			T0TCR = 0x2;
				if(player_flag == PLAYER_A)
				{
					//switch clock A to clock B
					//save the statues relevant to clock B.
					clock_player_a =(clock_player_a - T1TC);// save the remainning time.
					T1MR0 = clock_player_b;
					T1TCR = 0x2;
					
					T1TCR = 0x1;			//clock B begins to count.
					player_flag = PLAYER_B;
				}
				else
				{
					//switch clock B to clock A
					//save the statues relevant to clock B.
					clock_player_b = (clock_player_b - T1TC);
					T1MR0 = clock_player_a;
					T1TCR = 0x2;
					
					T1TCR = 0x1;			//clock A begins to count.
					player_flag = PLAYER_A;
				}
			//indicates the current player and timer for both player.
 			printf("current player %d, time_a = %ld, timer_b = %ld \r\n", player_flag, clock_player_a, clock_player_b);
		}
	}
	}	
	VICVectAddr = 0;
}
__irq void timeout_handler(void)
{
	T0IR = 0xFF;
	T1IR = 0xFF;

	//a long beep when the game over.
	PINSEL1 &= ~(3 << 20); /* Enable P0.26 for AOUT function */
	PINSEL1 |= (2 << 20);
	beep = 0;
	while(beep++<999999)
	{
		DACR =(beep<<6);
	}
	
	//print the result who is timeout.
	if(player_flag== PLAYER_A)
	{
		printf("Player A timeout!\r\n");
	}
	else
	{
		printf("Player B timeout!\r\n");
	}

	//reset all relevent registers and variables.
	
	T1MR0 = MATCH_TIME;
	T0TCR = 0x2;
	T1TCR = 0x2;

	T1MR0 = MATCH_TIME;
	//reset the two timer.
	clock_player_a = MATCH_TIME;
	clock_player_b = MATCH_TIME;
	return_from_timer_reset_flag = 0;
	//no current player.
	player_flag = PLAYER_NONE;
	button_pushed_times = 0;
	//game is not started.
	first_play = 1;
	
	VICVectAddr = 0;
}

int main(void)
{
	//Initialize the serial port.
	init_serial();

	return_from_timer_reset_flag = 0;
	player_flag = PLAYER_NONE;
	button_pushed_times = 0;
	clock_player_a = MATCH_TIME;
	clock_player_b = MATCH_TIME;
			   
	PINSEL4 &= ~(3<<20);
	PINSEL4 |= ~(2<<20);		//Set the P2.10 to external interrupt mode 01
	FIO2DIR1 = P210BIT;

	//Initialise the EINT0
	EXTMODE = (1<<0);				//Edge sensitive
	EXTPOLAR = (0<<0);				//Falling edge sensitive
				
	VICIntSelect &= ~(1 << 14);
    VICVectPriority14 = 8;
    VICVectAddr14 = (unsigned long)button_handler;		//invoke the button_handler when the button pressed.
    VICIntEnable = (1 << 14);

	//Timer0 store the time of press the button down.5s timeout and invoke the timer_reset handler.
	T0IR = 0xFF;		/* Clear any previous interrupts */
	T0TCR = 0x2;		/* Stop and reset TIMER0 */
	T0CTCR = 0x0;		/* Timer mode */
	T0MR0 = 60000000;	/* Match every 5 seconds */
	T0MCR = 0x3;		/* Interrupt, reset and re-start on match */
	T0PR = 0x0;			/* Prescale = 1 */

    VICIntSelect &= ~(1 << 4);
    VICVectPriority4 = 8;
    VICVectAddr4 = (unsigned long)timer_reset_handler;		//reset the game clock or begin the game.
    VICIntEnable = (1 << 4);
	
	//Timer1 stores the each pleayer's remaining time.
	T1IR = 0xFF;		/* Clear any previous interrupts */
	T1TCR = 0x2;		/* Stop and reset TIMER1 */
	T1CTCR = 0x0;		/* Timer mode */
	T1MR0 = MATCH_TIME;	/* Match every match time*/
	T1MCR = 0x3;		/* Interrupt, reset and re-start on match */
	T1PR = 0x0;			/* Prescale = 1 */

	VICIntSelect &= ~(1 << 5);
    VICVectPriority5 = 8;
    VICVectAddr5 = (unsigned long)timeout_handler;			//invoke the timeout_handler.Reset the game. 
    VICIntEnable = (1 << 5);


	while (1)
	{
	}
}
