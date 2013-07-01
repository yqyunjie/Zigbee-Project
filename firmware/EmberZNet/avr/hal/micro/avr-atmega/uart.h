/** @file uart.h
 * @brief Prototypes and defines for the hardware-specific UARTs.
 *
 * See @ref uart for documentation.
 *
 * <!-- Author(s): Brooks Barrett, brooks.barrett@ember.com -->
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->                
 */

/** @addtogroup uart
 * This contains the prototypes and defines for direct hardware manipulation
 * of UART0. Normally, one would go through @ref serial and the HAL. Use these
 * if you want to break the HAL (that is, save code size) and go straight
 * to the hardware.
 *
 * Some functions in this file return an ::EmberStatus value. See 
 * stack/include/error-def.h for definitions of all ::EmberStatus return values.
 *
 * See uart.h for source code.
 *
 *@{
 */

#ifndef __UART_H__
#define __UART_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define ENABLE_BIT_DEFINITIONS

#include <ioavr.h>

#if defined(AVR_ATMEGA_8) || defined(AVR_ATMEGA_32)
#define UCSR0C UCSRC
#define UPM00 UPM0
#define UPM01 UPM1
#define USBS0 USBS
#define UBRR0L UBRRL
#define UBRR0H UBRRH
#define UCSR0B UCSRB
#define TXCIE0 TXCIE
#define RXCIE0 RXCIE
#define TXEN0 TXEN
#define RXEN0 RXEN
#define UCSR0A UCSRA
#define TXC0 TXC
#define UDRIE0 UDRIE
#define USART0_RXC_vect USART_RXC_vect
#define FE0 FE
#define DOR0 DOR
#define UPE0 PE
#define UDR0 UDR
#define USART0_UDRE_vect USART_UDRE_vect
#define UDRE0 UDRE
#define RXC0 RXC

#else

#define TXCIE TXCIE0
#define URSEL 7
#define UBRRH UBRR0H
#define GIFR EIFR
#define GICR EIMSK
#ifndef AVR_ATMEGA_169
  #define AS2 AS0
  #define TCN2UB TCN0UB
#endif

#endif

#endif //DOXYGEN_SHOULD_SKIP_THIS

/////////////////////////////////////////////////////////////////////////////////
/** @name UART0 Tinies
 *
 *  UART I/O tiny implementations for UART0.
 */
////////////////////////////////////////////////////////////////////////////////
//@{

#ifndef AVR_ATMEGA_8
/** @brief Simple hardware initialization of UART0. 
 *
 * BIT(USBS0) is set 
 * for 2 stop bits. Remove it for only 1 stop bit.  The 25 is for a 9600 baud
 * rate on a system running at 4MHz. The other hard-coded BIT(#)s are for
 * generating 8-bit packets.
 */
#define halUart0InitTiny()  \
  UBRR0L = UDR0;  \
  UBRR0L = UBRRH;  \
  UCSR0C = BIT(URSEL) | BIT(UCSZ01) | BIT(UCSZ00) | BIT(USBS0);  \
  UBRR0L  = 25;  \
  UCSR0B = BIT(TXEN0) | BIT(RXEN0);  \
  UCSR0A = BIT(TXC0);
#else  //MEGA8

/** @brief Simple hardware initialization of UART0.
 * 
 * BIT(USBS0) is set 
 * for 2 stop bits, remove it for only 1 stop bit.  The 51 is for a 9600 baud
 * rate on a system running at 8MHz. The other hard-coded BIT(#)s are for
 * generating 8-bit packets.
 */
#define halUart0InitTiny()  \
  UBRR0L = UDR0;  \
  UBRR0L = UBRRH;  \
  UCSR0C = BIT(URSEL) | BIT(UCSZ1) | BIT(UCSZ0) | BIT(USBS0);  \
  UBRR0L  = 51;  \
  UCSR0B = BIT(TXEN0) | BIT(RXEN0);  \
  UCSR0A = BIT(TXC0);
#endif

/** @brief Goes straight to hardware, blocks until the UART is ready,
 * and sets the UART transmit register to the passed parameter for transmission.
 *
 * @param x  The byte to send out on the UART.
 */
#define halUart0WriteByteTiny(x) \
  while ( !( UCSR0A & BIT(UDRE0)) ) {} \
  UDR0 = x;

/** @brief Goes straight to hardware and reads a byte from UART0.
 *
 * Waits for the receive flag to become true, copies the received byte to
 * the passed pointer's location, and returns with the UART's error code.
 *  
 * @param dataByte  Pointer to where the read byte will end up.
 * 
 * @return Returns ::EMBER_SERIAL_RX_EMPTY if the read times out before any
 * activity occurs on the serial line, within about 80 milliseconds.
 * Otherwise the function returns an error code that corresponds to the 
 * error bits for UART0 of an Atmel microcontroller.
 */
EmberStatus halUart0ReadByteTimeoutTiny(int8u *dataByte);

////////////////////////////////////////////////////////////////////////////////
//@}  // end of UART I/O tiny implementations for UART0.
////////////////////////////////////////////////////////////////////////////////


#endif //__HAL_SERIAL_H__

/** @} END addtogroup */
