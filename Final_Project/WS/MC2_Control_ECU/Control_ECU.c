/*
 ================================================================================================
 Name        : Control_ECU.c
 Author      : Mohamed Aboelezz
 Description : Process the data received from the HMI_ECU throw UART and take the right action
 Date        : 26/10/2022
 ================================================================================================
 */

#include "uart.h"
#include "twi.h"
#include "external_eeprom.h"
#include "timer1.h"
#include "dc_motor.h"
#include "buzzer.h"
#include <avr/io.h>


/*******************************************************************************
 *                                Definitions                                  *
 *******************************************************************************/
#define EEPROM_START_ADDRESS 			0x0300
#define DOOR_UNLOCKING_TIME  			15			/* to change door unlock time from here */
#define DOOR_OPEN_TIME 		 			3			/* to change door open time from here */
#define DOOR_LOCKING_TIME    			15			/* to change door lock time from here */
#define NUMBER_OF_ALLOWED_WRONG_PASS 	3			/* the count of wrong password */
#define ERROR_TIME			 			60			/* to change buzzer time from here */

/*******************************************************************************
 *                           Global Variables                                  *
 *******************************************************************************/
static uint8 volatile g_tick = 0;		/* to count Ticks */

/*******************************************************************************
 *                      Functions Prototypes                                   *
 *******************************************************************************/

/* This is the call back function called by the Timer 1 driver.
 * It is used to calculate the required time for each event.
 */
void Timer_callBack(void);

int main(void)
{
	uint8 str_1[6];		/* array to get the 1st pass */
	uint8 str_2[6];		/* array to get the 2nd pass */
	uint8 counter=0;
	uint8 state_flag=0; /* to get error while storing pass */
	uint8 command;		/* to get the command from MC1*/
	uint8 error_flag=0;	/* to know that the user enter wrong pass or not */
	uint8 error_counter=0;	/* to count number of wrong pass */

	/* Enable Global Interrupt I-Bit */
	SREG |= (1<<7);

	/* Create configuration structure for UART driver */
	UART_ConfigType UART_Config = {Eight_Bit,Disabled,One_Bit,9600};
	/* Create configuration structure for I2C driver */
	TWI_ConfigType TWI_Config = {0x01,0x02};
	/* Create configuration structure for Timer 1 driver to give interrupt every 1 sec */
	Timer1_ConfigType Timer1_Config = {0,7812,F_CPU_1024,CTC};

	/* Set the Call back function pointer in the timer 1 driver */
	Timer1_setCallBack(Timer_callBack);

	DcMotor_Init();						/* initialize dc_motor driver */
	Buzzer_init();						/* initialize Buzzer driver */
	UART_init(&UART_Config);			/* initialize UART driver */
	TWI_init(&TWI_Config);				/* initialize I2C driver */
	while(1)
	{
		/* wait until other MC get the data from the user */
		while(UART_recieveByte()!=MC_READY){}
		/* receive the first pass */
		UART_receiveString(str_1);
		/* receive the second pass */
		UART_receiveString(str_2);
		counter=0;
		state_flag=0;
		while(str_1[counter]!='\0')
		{
			/* check if the two pass are the same or not */
			if(str_1[counter]!=str_2[counter])
			{
				state_flag=1;
				break;
			}
			counter++;
		}
		if(1==state_flag)
		{
			/* if two pass aren't the same */
			/* send to the other MC i'm ready to send data */
			UART_sendByte(MC_READY);
			/* send denied */
			UART_sendByte(DENIED);
		}
		else
		{
			/* if two pass are the same */
			/* send to the other MC i'm ready to send data */
			UART_sendByte(MC_READY);
			/* send accepted */
			UART_sendByte(ACCEPTED);
			counter=0;
			do
			{
				/* store the pass at EEPROM */
				EEPROM_writeByte(EEPROM_START_ADDRESS+counter,str_1[counter]);
				counter++;
			}while(str_1[counter-1]!='\0');
			while(1)
			{
				/* wait until other MC get the command and pass from the user to take action */
				while(UART_recieveByte()!=MC_READY){}
				/* receive the pass */
				UART_receiveString(str_1);
				/* receive the command */
				command=UART_recieveByte();
				counter=0;
				error_flag=0;
				while(str_1[counter]!='\0')
				{
					/* get the password stored at EEPROM */
					EEPROM_readByte(EEPROM_START_ADDRESS+counter,str_2+counter);
					/* check if the pass is correct or not */
					if(str_1[counter]!=str_2[counter])
					{
						error_flag=1;
						break;
					}
					counter++;
				}
				if(1==error_flag)
				{
					/* if the pass is not correct */
					/* send to the other MC i'm ready to send data */
					UART_sendByte(MC_READY);
					/* send denied */
					UART_sendByte(DENIED);
					error_counter++;
					if(NUMBER_OF_ALLOWED_WRONG_PASS==error_counter)
					{
						/* send to the other MC i'm ready to send data */
						UART_sendByte(MC_READY);
						/* send denied */
						UART_sendByte(ERROR_DETCTED);
						/* turn on buzzer */
						Buzzer_on();
						/* start the timer */
						Timer1_init(&Timer1_Config);
						while(1)
						{
							if(g_tick==ERROR_TIME)
							{
								g_tick=0;
								break;
							}
						}
						/* turn off buzzer */
						Buzzer_off();
						Timer1_deInit();		/* stop the timer */
						error_counter=0;
					}
					else
					{
						/* send to the other MC i'm ready to send data */
						UART_sendByte(MC_READY);
						/* move to the next try */
					}
				}
				else
				{
					error_counter=0;
					/* if the pass is correct */
					/* send to the other MC i'm ready to send data */
					UART_sendByte(MC_READY);
					/* send accepted */
					UART_sendByte(ACCEPTED);
					/* do the required command */
					if(command=='+')
					{
						/* move the motor CW with max speed */
						DcMotor_Rotate(CW,100);
						/* start the timer */
						Timer1_init(&Timer1_Config);
						while(1)
						{
							if(g_tick==DOOR_UNLOCKING_TIME)
							{
								g_tick=0;
								break;
							}
						}
						/* stop the motor */
						DcMotor_Rotate(stop,0);
						while(1)
						{
							if(g_tick==DOOR_OPEN_TIME)
							{
								g_tick=0;
								break;
							}
						}
						/* move the motor A_CW with max speed */
						DcMotor_Rotate(A_CW,100);
						while(1)
						{
							if(g_tick==DOOR_LOCKING_TIME)
							{
								g_tick=0;
								break;
							}
						}
						/* stop the motor */
						DcMotor_Rotate(stop,0);
						Timer1_deInit();		/* stop the timer */
					}
					else if(command=='-')
					{
						break;
					}
				}
			}
		}
	}
}

/* This is the call back function called by the Timer 1 driver.
 * It is used to calculate the required time for each event.
 */
void Timer_callBack(void)
{
	/* Increment ticks */
	g_tick++;
	/* clear time counted */
	TCNT1=0;
}
