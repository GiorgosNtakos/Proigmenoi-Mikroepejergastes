/*
* HelloWorld.c - Print out Hello World on Arm system
*/
// Define UART Data Register

volatile unsigned  *Uart_DR = (unsigned int 
*)0x101f1000;
// Main C function: print HelloWorld
void _start() {

unsigned int message;
unsigned int temp;

 char *s = "Hello world!\n";

 while (*s != '\0'){

temp=(unsigned int)(*s++);
message=temp;
temp=(unsigned int)(*s++);
message=message+(temp*256);
temp=(unsigned int)(*s++);
message=message+(temp*65536);
temp=(unsigned int)(*s++);
message=message+(temp*16777216);
*Uart_DR =message;


 //*Uart_DR = (unsigned int)(byte);
 Uart_DR = Uart_DR+1;
 }

 while (1) ;
}