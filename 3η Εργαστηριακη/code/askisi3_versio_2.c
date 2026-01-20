#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

/*
LEDs (PORTD):
 PD0: right turn indicator
 PD1: forward indicator
 PD2: left turn indicator

 Inputs:
 ADC0 AIN7 : front distance sensor (potentiometer via ADC)
 PF5       : side sensor switch (normal: no wall on right -> right turn,
                                reverse: no wall on left -> left turn)
 PC5       : reverse trigger switch (180 deg turn, then reverse rules)

RES Value < 10			==>		Wall up front  !!!!!   MONO TA 8 DEKSIA BIT PIANEI TO RES !!!!!

Ta breakpoints einai ypoxrewtika!!! 

Kathe fora pou mpainei stin routina interrupt gia toixo mprosta (RES<10) prepei xeirokinita 
na allazoume thn timh tou res pali se ipsili timi (Amesws me to pou mpei sto breakpoint!!!),
enw sta alla interrupts den  xreiazetai, ta flags kanoun clear mona tous.
*/

volatile int rotations=0;			//-1 for left turn and +1 for right turn | When this reaches -4 it means it completed a 360 degree turn
volatile int corners=0;			//Number of corners passed
volatile uint8_t reverse=0;			//0: Normal mode  1: Reverse mode
volatile uint8_t done    = 0; // termination flag (lets ISR stop the system cleanly)

static void begintimer(void);
static void turnleft(void);
static void turnright(void);


int main(void)
{
	/* ---------------- ADC setup (ATmega4808) ---------------- */
	ADC0.CTRLA = ADC_RESSEL_10BIT_gc | ADC_FREERUN_bm | ADC_ENABLE_bm; //10-bit resolution, Free-Running mode enabled, Enable ADC
	ADC0.MUXPOS = ADC_MUXPOS_AIN7_gc; // Analog input -> choose AIN7 explicitly
	ADC0.DBGCTRL = ADC_DBGRUN_bm; // Enable Debug Mode -> keep ADC running during debug
	
	/* Window comparator: interrupt when RESULT < WINLT */
	ADC0.WINLT = 10; //Set low threshold at 10
	ADC0.INTCTRL = ADC_WCMP_bm; //Enable Interrupts for WCM -> mode: RESULT < WINLT
	ADC0.CTRLE = ADC_WINCM0_bm; //Εnable window comparator interrupt -> Interrupt when RESULT < WINLT (RES<10)
	
	/* ---------------- LEDs outputs ---------------- */
	PORTD.DIR |=PIN0_bm | PIN1_bm | PIN2_bm;
	
	/* ---------------- Switch inputs with pull-up + interrupts ----------------
       With pull-up enabled: idle=1, pressed=0 => FALLING edge triggers on press.
    */
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;	// No wall on right or left (interrupt)
	PORTC.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;	// Reverse mode switch (interrupt)
	
	 /* ---------------- Timer TCA0 SETUP (simulator-friendly) ---------------- */
	TCA0.SINGLE.CNT = 0;								// Clear counter
	TCA0.SINGLE.CTRLB = 0;								// Normal Mode
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;	// Clock Frequency/1024
	
	sei(); //Enable interrupts
	
	/* Start ADC conversions (free-run keeps going) */
	ADC0.COMMAND |= ADC_STCONV_bm;
	
	/* Start moving forward */
	PORTD.OUTSET= PIN1_bm; 

	/* ---------------- Main loop: termination conditions ----------------
       Normal mode: stop when corners >= 8 (base requirement)
                    OR rotations == -4 (bonus heuristic)
       Reverse mode: stop when corners <= -1 (after counting 180 as movement)
       Also: if reverse is pressed at start (corners==0), ISR sets done=1 directly.
    */
	
	while(!done)
	{
		if(!reverse)
		{
			if(corners >= 8 || rotations == -4) done = 1;
		}
		else
		{
			if(corners <= -1) done = 1;
		}

		 __asm__ __volatile__("nop");
	}

	/* Stop all LEDs */
    PORTD.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm;

    while (1) { __asm__ __volatile__("nop"); }
}

/* ---------------- ISR: Front wall (ADC RESULT < WINLT) ---------------- */
ISR(ADC0_WCOMP_vect)
{
	/* Clear only the WCOMP flag*/
	ADC0.INTFLAGS = ADC_WCMP_bm;

	if(!reverse)
	{
		/* normal: front wall -> turn-left*/
		turnleft();
		corners++;
	}
	else
	{
		/* reverse: front-wall -> turn-right*/
		turnright();
		corners--;
	}
}

/* ---------------- ISR: Side sensor PF5 ---------------- */
ISR(PORTF_PORT_vect)					
{
	/* Clear port flags (write back the bits that are set) */
	PORTF.INTFLAGS = PORTF.INTFLAGS;

	if(!reverse)
	{
		/* normal: no wall on right -> turn-right*/
		turnright();
		corners++;
	}
	else
	{
		/* reverse: no wall on left -> turn-left*/
		turnleft();
		corners--;
	}
}

/* ---------------- ISR: Reverse trigger PC5 ---------------- */
ISR(PORTC_PORT_vect)
{
	PORTC.INTFLAGS = PORTC.INTFLAGS;

	/* Ignore if already in reverse (avoid re-trigger) */
    if (reverse) return;
	
	reverse=1;
	
	/* 180 degrees: ALL 3 LEDs ON (spec requirement) */
	PORTD.OUTSET= PIN0_bm | PIN1_bm | PIN2_bm;
	_delay_ms(1);
	begintimer();
	
	/* After 180°, stop turn LEDs; keep forward ON */
	PORTD.OUTCLR = PIN0_bm | PIN2_bm;
	PORTD.OUTSET = PIN1_bm;
	_delay_ms(1);

	/* If we are at the initial point (corners==0), no need to turn left/right.
       We already performed the required 180° and we are still at start -> finish now.
    */
    if (corners == 0)
    {
        done = 1;
        return;
    }

    /* Otherwise, per slide note: count the 180° rotation as one recorded movement */
    corners--;
}


/* ---------------- Timer helper ----------------
   Keeps your "simulator-friendly" timing approach.
*/
static void begintimer(void)	//Timer for LEDs (Turning right,left and 180 degrees)
{
	//Oi times tou timer einai oti xeirotero ston simulator
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;	 //Start timer
	while (TCA0.SINGLE.CNT<1){}					 //Stop program flow until the counter reaches the desired value
	TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // Stop Timer
	TCA0.SINGLE.CNT=0;							 // Zeroing counter
	_delay_ms(2);								 //To stop the program from progressing too fast (Not needed normally)
}


/* ---------------- Turn left ---------------- */
static void turnleft(void)
{
	/* Stop forward */
	PORTD.OUTCLR= PIN1_bm; //Stop walking straight
	_delay_ms(1);

	/* Left indicator ON */
	PORTD.OUTSET= PIN2_bm; //Turn left
	begintimer();

	 /* Left indicator OFF */
	PORTD.OUTCLR |= PIN2_bm; //Stop turning left
	_delay_ms(1);

	/* Forward ON */
	PORTD.OUTSET= PIN1_bm; //Start walking straight again
	_delay_ms(1);

	rotations--;
}


/* ---------------- Turn right ---------------- */
static void turnright(void)	
{
	/* Stop forward */
	PORTD.OUTCLR= PIN1_bm; //Stop walking straight
	_delay_ms(1);

	/* Right indicator ON */
	PORTD.OUTSET= PIN0_bm; //Turn right
	begintimer();

	/* Right indicator OFF */
	PORTD.OUTCLR |= PIN0_bm; //Stop turning right
	_delay_ms(1);

	/* Forward ON */
	PORTD.OUTSET= PIN1_bm; //Start walking straight again
	_delay_ms(1);

	rotations++;
}