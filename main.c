/*
 * main.c
 *
 *  Created on: 22 lip 2019
 *      Author: lukasz
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "libs/uart.h"


int main(void)
{
	usart_init( UART_BAUD_SELECT(9600, F_CPU) );

	sei();
	while(1)
	{
		uart_puts("lukasOS\r\n");
		_delay_ms(1000);
	}
	return 0;
}
