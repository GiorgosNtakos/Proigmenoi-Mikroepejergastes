volatile unsigned int *Uart_DR = (unsigned int *)0x101f1000;

void _start() {
	
	//Setting value of B=23
	
	*Uart_DR = 23;
	Uart_DR = Uart_DR+1;
	
	//Setting value of C=14
	
	*Uart_DR = 14;
	Uart_DR = Uart_DR+1;
	
	*Uart_DR = *(Uart_DR-1)+*(Uart_DR-2);

while (1) ;
}