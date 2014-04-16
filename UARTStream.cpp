#include "SerialStream.h"
#include <avr/interrupt.h>
#include <avr/wdt.h>

UARTSerial serial0(128, 128, &UBRR0H, &UBRR0L, &UCSR0A, &UCSR0B, &UDR0);

void UARTSerial::put(uint8_t c) {
	uint8_t next = (_txBuf.head+1) & _txBuf.mask;
	while (next == _txBuf.tail) { wdt_reset(); }

	_txBuf.data[_txBuf.head] = c;
	// Now there definitely is data in the buffer.
	_txBuf.head = next;
	*_ucsrb |= (1 << UDRIE0);
	*_ucsra |= (1 << TXC0);
}

void UARTSerial::receive(uint8_t c) {
	uint8_t next = (_rxBuf.head+1) & _rxBuf.mask;
	if (next != _rxBuf.tail) {
		_rxBuf.data[_rxBuf.head] = UDR0;
		_rxBuf.head = next;
	}
}

void UARTSerial::udre() {
	if (_txBuf.head == _txBuf.tail) {
		*_ucsrb &= ~(1<<UDRIE0);
	} else {
		uint8_t data = _txBuf.data[_txBuf.tail];
		_txBuf.tail = (_txBuf.tail+1) & _txBuf.mask;
		*_udr = data;
	}
}

size_t Serial::available() {
	// Example : Tail = 100, head = 2, mask = 127:
	// head - tail = -98 = 0b10011110
	// (head - tail) & mask = 0b10011110 & 0b01111111 = 0b00011110 = 30
	return (/*_rxBuf.mask + 1 + */ _rxBuf.head -_rxBuf.tail) & _rxBuf.mask;
}

void Serial::waitFlushed() {
	while (_txBuf.head != _txBuf.tail)
		wdt_reset();
}

void UARTSerial::init(uint32_t baud, int direction) {
	uint16_t ubrr = (baud/8)-1;
	*_ubrrh = ubrr >> 8;
	*_ubrrl = ubrr & 0xff;
	*_ucsra = 1<<1; // That's right, I just assume it runs on a certain type of MCU.
	*_ucsrb = (1<<RXCIE0)|(1<<RXEN0)|(1<<TXEN0);
	// This is default anyway, no need to.
	// UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}


#if defined(USART0_RX_vect)
ISR(USART0_RX_vect) {
	serial1.receive(UDR1);
}
ISR(USART_UDRE0_vect) {
	serial0.udre();
}
#endif
#if defined(USART_RX_vect)
ISR(USART_RX_vect) {
	serial0.receive(UDR0);
}
ISR(USART_UDRE_vect) {
	serial0.udre();
}
#endif
