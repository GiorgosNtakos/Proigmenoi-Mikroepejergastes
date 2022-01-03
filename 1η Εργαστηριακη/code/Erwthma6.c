volatile unsigned int *  Uart_DR = (unsigned int *)0x101f1000;

void _start() {
	int k,i;
for (k=1;k<=10;k++)
{
	*(Uart_DR+k)=k;
}

for (i=1;i<=10;i++)
{
	*Uart_DR=*Uart_DR+*(Uart_DR+i);
}

while (1) ;
}