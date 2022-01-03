/*
* HelloWorld.c - Print out Hello World on Arm system
*/
// Define UART Data Register
volatile unsigned  * Uart_DR = (unsigned int
*)0x101f1000;
// Main C function: print HelloWorld
void _start() {
 char *s = "Hello world!\n";
 // Copy the string to UART Data Register
 while (*s != '\0'){
 *Uart_DR = (unsigned int)(*s++);
 Uart_DR=Uart_DR+1;
 }

 while (1) ;
}