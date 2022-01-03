#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define del 0.2
#define ped 10

int interr=0;
int PreviousState=0; // State 0: Big GREEN Small RED. State 1: Big RED Small GREEN (Used to skip lines)
int timercomplete=0; // When timer reaches 20seconds this will be turned to 1 through interrupt

int main(void)
{
	
	time_t t;
	srand((unsigned) time(&t));	// Setting seed for rand() function.
	
	PORTD.DIR |=PIN0_bm;	// PIN 0 (Right bit)	-->		BIG Traffic Light
	PORTD.DIR |=PIN1_bm;	// PIN 1 (Middle bit)	-->		SMALL Traffic Light
	PORTD.DIR |=PIN2_bm;	// PIN 2 (Left bit)		-->		PEDESTRIANS Light
	
	TCA0.SINGLE.CNT = 0;								// Clear counter
	TCA0.SINGLE.CTRLB = 0;								// Normal Mode
	TCA0.SINGLE.CMP0 = ped;								// When reaches this value -> interrupt
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc;	// Clock Frequency/256
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm;			// Interrupt Enable
	
	PORTD.OUTSET=PIN0_bm; // Turning big light to green.
	_delay_ms(del);
	
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;	// Pedestrian light button (interrupt)
	sei(); // Enable interrupts
	
	start:
	
	while (interr==0)
	{
		if(rand()%10==0,rand()%10==5,rand()%10==8)
		{
			if (PreviousState==1) continue;
			PORTD.OUTCLR=PIN0_bm; // Turning big light to red.
			_delay_ms(del);
			PORTD.OUTSET=PIN1_bm; // Turning small light to green.
			_delay_ms(del);
			PreviousState=1;
		}
		else
		{
			if (PreviousState==0) continue;
			PORTD.OUTCLR=PIN1_bm; // Turning small light to red.
			_delay_ms(del);
			PORTD.OUTSET=PIN0_bm; // Turning big light to green.
			_delay_ms(del);
			PreviousState=0;
		}
	}
	
	if (PreviousState==1) goto point; 
	PORTD.OUTCLR=PIN0_bm;		// Turning big light to red.
	_delay_ms(del);
	PORTD.OUTSET=PIN1_bm;		// Turning small light to green.
	_delay_ms(del);
	PreviousState=1;
	
	point:
	
	PORTD.OUTSET=PIN2_bm;		// Turning pedestrian light on (Green).
	_delay_ms(del);
	
	TCA0.SINGLE.CTRLA |=1;		// Start timer of 20s for pedestrians
	
	while(timercomplete==0){}	// Stay on while until interrupted by timer

	PORTD.OUTCLR=PIN2_bm;		// Turning pedestrian light off (Red).
	_delay_ms(del);
	
	timercomplete=0;
	interr=0;
	
	goto start;
	
	cli();	// Just in case
}

ISR(PORTF_PORT_vect)							// Pedestrians interrupt
{
	int intflags=PORTF.INTFLAGS;
	PORTF.INTFLAGS=intflags;
	interr=1;
}


ISR(TCA0_CMP0_vect){							// Timer interrupt 20s
	TCA0.SINGLE.CTRLA = 0;						// Disable Timer
	int intflags = TCA0.SINGLE.INTFLAGS;		// Clear Interrupt Flags
	TCA0.SINGLE.INTFLAGS=intflags;
	TCA0.SINGLE.CNT=0;							//Zeroing counter
	timercomplete=1;
}
