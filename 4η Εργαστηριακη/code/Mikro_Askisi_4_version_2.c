#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
TO PIN=1 SIMAINEI BIT=1 KAI PIN=0 TOTE BIT=0 (ANAPODI LOGIKI APO AFTI POY ZHTAEI H ASKISI)
AN THELAME THN LOGIKH POU ZHTAEI H ASKISI TOTE THA KANAME: PORTD.OUTSET=15-ram[++rampointer];
*/

int NowReading=0;
int read=0, write=0,RandResult=0;
int ram[32];
int rampointer=0;

int NumberOfElements=0;

int main()
{
	srand(time(NULL));	//Setting time as seed for rand()
	
	//Initializing 4 PORTD Pins (LEDs)
	PORTD.DIR |= PIN0_bm;
	PORTD.DIR |= PIN1_bm;
	PORTD.DIR |= PIN2_bm;
	PORTD.DIR |= PIN3_bm;
	
	
	//Timer Initialization
	TCA0.SINGLE.CTRLA=TCA_SINGLE_CLKSEL_DIV8_gc; //Prescaler=8
	TCA0.SINGLE.PER = 250;  //Positive edge WRITE Clock
	TCA0.SINGLE.CMP0 = 75;  //Positive edge READ clock
	TCA0.SINGLE.CMP1 = 150; //Negative edge WRITE Clock
	TCA0.SINGLE.CMP2 = 200; //Negative edge READ clock
	
	TCA0.SINGLE.CTRLB |= TCA_SINGLE_WGMODE_SINGLESLOPE_gc; //Select Single_Slope_PWM
	
	TCA0.SINGLE.INTCTRL |= TCA_SINGLE_OVF_bm;	//Positive edge WRITE (Enable interrupt)
	TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP1_bm;	//Negative edge WRITE (Enable interrupt)
	TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP2_bm;	//Negative edge READ (Enable interrupt)
	
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; //Start timer
	
	
	PORTC.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; //PORTC PIN5 WRITE INTERRUPT (Switch 5)
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; //PORTF PIN5 READ INTERRUPT (Switch 6)
	
	sei();
	while (ram[rampointer]!=-1){}
}


ISR(TCA0_OVF_vect)					//Positive edge WRITE Clock
{
	//clearing flags
	int intflags = TCA0.SINGLE.INTFLAGS;
	TCA0.SINGLE.INTFLAGS = intflags;
	
	if (NowReading==1){}	//Do nothing if still reading
	else if (write==1)
	{
		insertSortedValue(RandResult); //Write the random value
		write=0;
	}
}

ISR(TCA0_CMP1_vect)					//Negative edge WRITE Clock
{
	//clearing flags
	int intflags = TCA0.SINGLE.INTFLAGS;
	TCA0.SINGLE.INTFLAGS = intflags;
	
	if (NowReading==1){}	//Do nothing if still reading
	else if (write==1)
	{
		insertSortedValue(RandResult); //Write the random value
		write=0;
	}
}

ISR(TCA0_CMP2_vect)					//Negative edge READ Clock
{
	//clearing flags
	int intflags = TCA0.SINGLE.INTFLAGS;
	TCA0.SINGLE.INTFLAGS = intflags;
	
	if (NowReading==1)	//If true it means a period passed and now we turn off LEDs
	{
		PORTD.OUTCLR=15;
		NowReading=0;
		read=0;
	}
	else if (read==1)
	{
		PORTD.OUTSET=ram[rampointer];	//Turn on LEDs for reading
		NowReading=1;
		read=0;
	}
}

ISR(PORTC_PORT_vect)							//WRITE Interrupt (Switch 5 / PORTC Pin 5)
{
	//clearing flags
	int intflags=PORTC.INTFLAGS;
	PORTC.INTFLAGS=intflags;
	
	RandResult=rand()%16;	//Calculate the rand value for storing later [0-15]
	write=1;
}

ISR(PORTF_PORT_vect)							//READ Interrupt (Switch 6 / PORTF Pin 5)
{
	//clearing flags
	int intflags=PORTF.INTFLAGS;
	PORTF.INTFLAGS=intflags;
	
	read=1;
}

void insertSortedValue(int NewElement)	//Insert And sort value into Ram
{
	int i=0;
	for (i=0;i<NumberOfElements;i++)	//Scan through the Ram
	{
		if (NewElement<ram[i])	//If an element bigger than the new value is found push the values above to +1 index and write the new value in current index in ram.
		{
			PushValues(i);
			ram[i]=NewElement;
			NumberOfElements++;
			rampointer=i;
			return; 
		}
	}
	//if no bigger value is found up to this point it means that the new value is the greatest and will be written in the highest index.
	i=NumberOfElements;	//i is the index of the new value to write
	ram[i]=NewElement;
	NumberOfElements++;
	rampointer=i;
}

void PushValues(int i)	//Move all values from i and above to +1 index
{
	for (int j=(NumberOfElements-1);j>=i;j--)	
	{
		ram[j+1]=ram[j];
	}
}