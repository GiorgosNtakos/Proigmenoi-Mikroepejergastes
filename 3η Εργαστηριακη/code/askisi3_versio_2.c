#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

int rotations=0;			//-1 for left turn and +1 for right turn | When this reaches -4 it means it completed a 360 degree turn
int corners=0;			//Number of corners passed
int reverse=0;			//0: Normal mode  1: Reverse mode

/*
RES Value < 10			==>		Wall up front  !!!!!   MONO TA 8 DEKSIA BIT PIANEI TO RES !!!!!
PORTF intflags pin 5	==>		No wall on right (normal mode) or no wall on left (reverse mode)
PORTC intflags pin 5	==>		Begin reverse mode

Ta breakpoints einai ypoxrewtika!!! 

Kathe fora pou mpainei stin routina interrupt gia toixo mprosta (RES<10) prepei xeirokinita 
na allazoume thn timh tou res pali se ipsili timi (Amesws me to pou mpei sto breakpoint!!!),
enw sta alla interrupts den  xreiazetai, ta flags kanoun clear mona tous.
*/



int main(void)
{
	//initialize the ADC for Free-Running mode
	ADC0.CTRLA |= ADC_RESSEL_10BIT_gc; //10-bit resolution
	ADC0.CTRLA |= ADC_FREERUN_bm; //Free-Running mode enabled
	ADC0.CTRLA |= ADC_ENABLE_bm; //Enable ADC
	ADC0.MUXPOS |= ADC_MUXPOS_AIN7_gc; //Analog input
	
	//Enable Debug Mode
	ADC0.DBGCTRL |= ADC_DBGRUN_bm;
	
	//Window Comparator Mode
	ADC0.WINLT |= 10; //Set low threshold at 10
	ADC0.INTCTRL |= ADC_WCMP_bm; //Enable Interrupts for WCM
	ADC0.CTRLE |= ADC_WINCM0_bm; //Interrupt when RESULT < WINLT (RES<10)
	
	//LED Setup
	PORTD.DIR |=PIN0_bm; // Turning right led
	PORTD.DIR |=PIN1_bm; // Moving straight led
	PORTD.DIR |=PIN2_bm; // Turning left led
	
	//Switches Setup
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;	// No wall on right or left (interrupt)
	PORTC.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;	// Reverse mode switch (interrupt)
	
	//Timer Setup
	TCA0.SINGLE.CNT = 0;								// Clear counter
	TCA0.SINGLE.CTRLB = 0;								// Normal Mode
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;	// Clock Frequency/1024
	
	sei(); //Enable interrupts
	
	ADC0.COMMAND |= ADC_STCONV_bm; //Start ADC Conversion
	
	
	PORTD.OUTSET= PIN1_bm; //Start walking straight
	
	while(corners>-1) //When in reverse mode, if corners=-1 we reached the starting point.
	{
		if (rotations==-4) break;	//When in normal mode, rotations=-4 means (in most cases) we reached the starting point.
	}
}

ISR(ADC0_WCOMP_vect)				//Interrupt for wall up front (RES Value<10)
{
	if (reverse==0)					//Turning left routine (Normal mode)
	{
		int intflags = ADC0.INTFLAGS;
		ADC0.INTFLAGS = intflags;
		turnleft();
		corners++;
	}
	
	else if (reverse==1 && corners==0)	//No need to turn right or left when you are on the starting point
	{
		int intflags=ADC0.INTFLAGS;
		ADC0.INTFLAGS=intflags;
		corners--;
	}
	
	else if (reverse==1)			//Turning right routine (Reverse mode)
	{
		int intflags = ADC0.INTFLAGS;
		ADC0.INTFLAGS = intflags;
		turnright();
		corners--;
	}
}


ISR(PORTF_PORT_vect)	//Interrupt for no wall on right or left depending on mode (PORTF pin 5 intflags)						
{
	if (reverse==0)		//No wall on right so turn right (Normal mode)
	{
		int intflags=PORTF.INTFLAGS;
		PORTF.INTFLAGS=intflags;
		turnright();
		corners++;
	}
	
	else if (reverse==1 && corners==0)		//No need to turn right or left when you are on the starting point
	{
		int intflags=PORTF.INTFLAGS;
		PORTF.INTFLAGS=intflags;
		corners--;
	}
	
	else if (reverse==1)		//No wall on left so turn left (Reverse mode)
	{
		int intflags=PORTF.INTFLAGS;
		PORTF.INTFLAGS=intflags;
		turnleft();
		corners--;
	}
}

ISR(PORTC_PORT_vect)	//Interrupt to begin Reverse mode (PORTE pin 5 intflags)
{
	int intflags=PORTC.INTFLAGS;
	PORTC.INTFLAGS=intflags;
	
	reverse=1;
	
	//Start turning 180 degrees
	PORTD.OUTSET= PIN0_bm;
	PORTD.OUTSET= PIN2_bm;
	_delay_ms(1);
	
	begintimer();
	
	//Stop turning 180 degrees and start walking straight again
	PORTD.OUTCLR= PIN0_bm;
	PORTD.OUTCLR= PIN2_bm;
	_delay_ms(1);
}



begintimer()	//Timer for LEDs (Turning right,left and 180 degrees)
{
	//Oi times tou timer einai oti xeirotero ston simulator
	TCA0.SINGLE.CTRLA |=1;						//Start timer
	while (TCA0.SINGLE.CNT<1){}					//Stop program flow until the counter reaches the desired value
	TCA0.SINGLE.CTRLA = 0;						// Stop Timer
	TCA0.SINGLE.CNT=0;							// Zeroing counter
	_delay_ms(2);								//To stop the program from progressing too fast (Not needed normally)
}



turnleft()		//Function to turn left
{
	PORTD.OUTCLR= PIN1_bm; //Stop walking straight
	_delay_ms(1);
	PORTD.OUTSET= PIN2_bm; //Turn left

	begintimer();

	PORTD.OUTCLR |= PIN2_bm; //Stop turning left
	_delay_ms(1);
	PORTD.OUTSET= PIN1_bm; //Start walking straight again
	_delay_ms(1);
	rotations--;
}



turnright()		//Function to turn right
{
	PORTD.OUTCLR= PIN1_bm; //Stop walking straight
	_delay_ms(1);
	PORTD.OUTSET= PIN0_bm; //Turn right

	begintimer();

	PORTD.OUTCLR |= PIN0_bm; //Stop turning right
	_delay_ms(1);
	PORTD.OUTSET= PIN1_bm; //Start walking straight again
	_delay_ms(1);
	rotations++;
}