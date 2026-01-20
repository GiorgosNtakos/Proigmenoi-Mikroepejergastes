#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define del 0.2
#define ped 10

volatile int interr=0;
int PreviousState=0; // State 0: Big GREEN Small RED. State 1: Big RED Small GREEN (Used to skip lines)
volatile int timercomplete=0; // When timer reaches 20seconds this will be turned to 1 through interrupt

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
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc;	// Clock Frequency/1024
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm;			// Interrupt Enable
	
	// Active-low LED: GREEN (ON) => OUTCLR, RED (OFF) => OUTSET
	PORTD.OUTCLR = PIN0_bm; // Big green ON
	PORTD.OUTSET = PIN1_bm; // Small red OFF
	PORTD.OUTSET = PIN2_bm; // Ped red OFF
	_delay_ms(del);
	
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;	// Pedestrian light button (interrupt)
	sei(); // Enable interrupts
	
	start:
	
	while (interr==0)
	{
		int r = rand() % 10;
		if(r == 0 || r == 5 || r == 8)
		{
			if (PreviousState==1) continue;
			PORTD.OUTSET=PIN0_bm; // Turning big light to red.
			_delay_ms(del);
			PORTD.OUTCLR=PIN1_bm; // Turning small light to green.
			_delay_ms(del);
			PreviousState=1;
		}
		else
		{
			if (PreviousState==0) continue;
			PORTD.OUTSET=PIN1_bm; // Turning small light to red.
			_delay_ms(del);
			PORTD.OUTCLR=PIN0_bm; // Turning big light to green.
			_delay_ms(del);
			PreviousState=0;
		}
	}
	
	if (PreviousState==1) goto point;
	PORTD.OUTSET=PIN0_bm;		// Turning big light to red.
	_delay_ms(del);
	PORTD.OUTCLR=PIN1_bm;		// Turning small light to green.
	_delay_ms(del);
	PreviousState=1;
	
	point:
	
	PORTD.OUTCLR=PIN2_bm;		// Turning pedestrian light on (Green).
	_delay_ms(del);
	
	// Start timer
	TCA0.SINGLE.CNT = 0;
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;   // clear pending
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;   // start (αντί για |=1)
	
	while(timercomplete==0){}	// Stay on while until interrupted by timer

	PORTD.OUTSET=PIN2_bm;		// Turning pedestrian light off (Red).
	_delay_ms(del);
	
	timercomplete=0;
	interr=0;
	
	goto start;
	
}

ISR(PORTF_PORT_vect)							// Pedestrians interrupt
{
	uint8_t intflags=PORTF.INTFLAGS;
	PORTF.INTFLAGS=intflags;
	interr=1;
}


ISR(TCA0_CMP0_vect){							// Timer interrupt 20s
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm; // clear CMP0 flag
	TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // stop timer (χωρίς να μηδενίζεις όλο το CTRLA)
	TCA0.SINGLE.CNT=0;							//Zeroing counter
	timercomplete=1;
}
