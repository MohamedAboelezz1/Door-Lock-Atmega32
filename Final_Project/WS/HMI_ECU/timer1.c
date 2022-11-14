/******************************************************************************
 *
 * Module: TIMER 1
 *
 * File Name: timer1.h
 *
 * Description: Source file for the TIMER 1 AVR driver
 *
 * Author: Mohamed Aboelezz
 *
 *******************************************************************************/
#include<avr/io.h>
#include<avr/interrupt.h>
#include "timer1.h"

/*******************************************************************************
 *                           Global Variables                                  *
 *******************************************************************************/

/* Global variables to hold the address of the call back function in the application */
static volatile void (*g_callBackPtr)(void) = NULL_PTR;

/*******************************************************************************
 *                       Interrupt Service Routines                            *
 *******************************************************************************/
#ifdef TIMER1_CMP
ISR(TIMER1_COMPA_vect)
{
	if(g_callBackPtr != NULL_PTR)
	{
		/* Call the Call Back function in the application after timer reach the compare value */
		(*g_callBackPtr)();
	}
}
#else

ISR(TIMER1_OVF_vect)
{
	if(g_callBackPtr != NULL_PTR)
	{
		/* Call the Call Back function in the application after timer reach the overflow value */
		(*g_callBackPtr)();
	}
}
#endif

/*******************************************************************************
 *                      Functions Definitions                                  *
 *******************************************************************************/
/*
 * Description :
 * Function to initialize the Timer driver.
 */
void Timer1_init(const Timer1_ConfigType * Config_Ptr)
{
	/* Insert Initial Value */
	TCNT1=(Config_Ptr->initial_value);
#ifdef TIMER1_CMP
	/* Insert Compare Value in A Register */
	OCR1A=(Config_Ptr->compare_value);
#endif
	/* Configure timer control register TCCR1A
	 * 1. Disconnect OC1A and OC1B  COM1A1=0 COM1A0=0 COM1B0=0 COM1B1=0
	 * 2. FOC1A=1 FOC1B=0
	 * 3. WGM10, WGM11 to select mode
	 */
	TCCR1A=(1<<FOC1A)|(1<<FOC1B);
	TCCR1A=(TCCR1A&0xFC)|((Config_Ptr->mode)&0x03);
	/* Configure timer control register TCCR1B
	 * 1. CTC Mode WGM12=1 WGM13=0 (Mode Number 4)
	 * 2. Prescaler = F_CPU/1024 CS10=1 CS11=0 CS12=1
	 */
	TCCR1B=((Config_Ptr->prescaler)&0x07);
	TCCR1B=(TCCR1B&0xE7)|(((Config_Ptr->mode)&0x0C)<<3);
	/* Enable Timer 1 Compare A Interrupt and Timer 1 Overflow Interrupt */
	TIMSK|=(1<<OCIE1A);
	TIMSK|=(1<<TOIE1);
}

/*
 * Description :
 * Function to disable the Timer1.
 */
void Timer1_deInit(void)
{
	/* disable timer 1 */
	TCCR1A=0;
	TCCR1B=0;
}

/*
 * Description :
 * Function to set the Call Back function address.
 */
void Timer1_setCallBack(void(*a_ptr)(void))
{
	/* Save the address of the Call back function in a global variable */
	g_callBackPtr = a_ptr;
}
