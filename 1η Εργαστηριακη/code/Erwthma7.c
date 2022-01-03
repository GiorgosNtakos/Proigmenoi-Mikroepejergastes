volatile unsigned int *  Uart_DR = (unsigned int *)0x101f1000;

void _start() {
	
	int i;
	
	//Setting values of Ak
	
	*(Uart_DR+1)=4;
	*(Uart_DR+2)=3;
	*(Uart_DR+3)=2;
	*(Uart_DR+4)=5;
	*(Uart_DR+5)=3;
	*(Uart_DR+6)=2;
	*(Uart_DR+7)=2;
	*(Uart_DR+8)=4;
	*(Uart_DR+9)=1;
	*(Uart_DR+10)=2;
	
	//Setting values of Bk
	
	*(Uart_DR+11)=2;
	*(Uart_DR+12)=3;
	*(Uart_DR+13)=3;
	*(Uart_DR+14)=2;
	*(Uart_DR+15)=5;
	*(Uart_DR+16)=2;
	*(Uart_DR+17)=6;
	*(Uart_DR+18)=2;
	*(Uart_DR+19)=3;
	*(Uart_DR+20)=2;
	
for (i=1;i<=10;i++)
{
	*Uart_DR=*Uart_DR+(*(Uart_DR+i)**(Uart_DR+i+10));
}

while (1) ;
}
