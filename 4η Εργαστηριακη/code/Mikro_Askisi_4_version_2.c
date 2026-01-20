#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
TO PIN=1 SIMAINEI BIT=1 KAI PIN=0 TOTE BIT=0 (ANAPODI LOGIKI APO AFTI POY ZHTAEI H ASKISI)
AN THELAME THN LOGIKH POU ZHTAEI H ASKISI TOTE THA KANAME: PORTD.OUTSET=15-ram[++rampointer];
*/

volatile uint8_t NowReading=0;
volatile uint8_t read=0, write=0, RandResult=0;
volatile int ram[32];
volatile uint8_t rampointer=0;

volatile uint8_t NumberOfElements=0;

static void PushValues(uint8_t i);
static void insertSortedValue(uint8_t NewElement);

int main(void)
{
	srand(time(NULL));	//Setting time as seed for rand()

	// Init RAM με sentinel (-1) (χρήσιμο για debug/έλεγχο)
	for (uint8_t i = 0; i < 32; i++) ram[i] = -1;
	
	// LEDs PD0..PD3 outputs
	PORTD.DIR |= PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
	PORTD.OUTCLR = 0x0F;
	
	// Timer TCA0 (PWM clocks) Initialization
	TCA0.SINGLE.CTRLA=TCA_SINGLE_CLKSEL_DIV8_gc; //Prescaler=8
	TCA0.SINGLE.PER = 250;  //Positive edge WRITE Clock
	TCA0.SINGLE.CMP0 = 75;  //Positive edge READ clock
	TCA0.SINGLE.CMP1 = 150; //Negative edge WRITE Clock
	TCA0.SINGLE.CMP2 = 200; //Negative edge READ clock
	
	TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc; //Select Single_Slope_PWM
	
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm | TCA_SINGLE_CMP1_bm | TCA_SINGLE_CMP2_bm;	//Positive edge WRITE (Enable interrupt), Negative edge WRITE (Enable interrupt), Negative edge READ (Enable interrupt)
	
	// Switch interrupts
	PORTC.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; //PORTC PIN5 WRITE INTERRUPT (Switch 5)
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; //PORTF PIN5 READ INTERRUPT (Switch 6)

	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; //Start timer
	
	sei();
	// Embedded main loop – οι πράξεις γίνονται από ISR
	while (1) { __asm__ __volatile__("nop"); }
}

/* WRITE clock edge: OVF */
ISR(TCA0_OVF_vect) //Positive edge WRITE Clock
{
	//clearing ovf flag
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
	
	if (NowReading) return;	//Do nothing if still reading
	else if (write)
	{
		insertSortedValue((uint8_t)RandResult); //Write the random value
		write=0;
	}
}

/* WRITE clock edge: CMP1 */
ISR(TCA0_CMP1_vect)					//Negative edge WRITE Clock
{
	//clearing CMP1 flag
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP1_bm;
	
	if (NowReading) return;	//Do nothing if still reading
	else if (write)
	{
		insertSortedValue((uint8_t)RandResult); //Write the random value
		write=0;
	}
}

/* READ clock edge: CMP2 */
ISR(TCA0_CMP2_vect)					//Negative edge READ Clock
{
	//clearing CMP2 flag
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP2_bm;
	
	if (NowReading)	//If true it means a period passed and now we turn off LEDs
	{
		PORTD.OUTCLR=0x0F;
		NowReading=0;
		read=0;
		return;
	}
	else if (read)
	{
		// Καθάρισε πρώτα τα 4 LED, μετά άναψε το νέο 4-bit
		PORTD.OUTCLR = 0x0F;
		PORTD.OUTSET = ((uint8_t)ram[rampointer]) & 0x0F;	//Turn on LEDs for reading
		NowReading=1;
		read=0;
	}
}

ISR(PORTC_PORT_vect)							//WRITE Interrupt (Switch 5 / PORTC Pin 5)
{
	//clearing flags
	PORTC.INTFLAGS = PORTC.INTFLAGS;
	
	RandResult = (uint8_t)(rand() % 16);	//Calculate the rand value for storing later [0-15]
	write=1;
}

ISR(PORTF_PORT_vect)							//READ Interrupt (Switch 6 / PORTF Pin 5)
{
	//clearing flags
	PORTF.INTFLAGS = PORTF.INTFLAGS;
	
	read=1;
}

static void insertSortedValue(uint8_t NewElement)	//Insert And sort value into Ram
{

	// Προστασία: αν γέμισε η RAM, αγνόησε το write
	if (NumberOfElements >= 32) return;

	// Αν είναι το πρώτο στοιχείο
	if (NumberOfElements == 0)
	{
		ram[0] = NewElement;
		NumberOfElements = 1;
		rampointer = 0;
		return;
	}

	for (uint8_t i=0; i < NumberOfElements; i++)	//Scan through the Ram
	{
		if ( NewElement < (uint8_t)ram[i])	//If an element bigger than the new value is found push the values above to +1 index and write the new value in current index in ram.
		{
			PushValues(i);
			ram[i] = NewElement;
			NumberOfElements++;
			rampointer=i;
			return;
		}
	}
	//if no bigger value is found up to this point it means that the new value is the greatest and will be written in the highest index.
	ram[NumberOfElements] = NewElement;
	rampointer = NumberOfElements;
	NumberOfElements++;
}

static void PushValues(uint8_t i)	//Move all values from i and above to +1 index
{
	for (int j= (int)NumberOfElements-1; j>=(int)i; j--)
	{
		ram[j + 1] = ram[j];
	}
}