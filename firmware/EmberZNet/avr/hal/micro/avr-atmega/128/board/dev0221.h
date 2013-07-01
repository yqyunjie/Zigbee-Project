/** @file dev0221.h 
 *  @brief Functions specific to the BOARD_DEV0221 carrier board.
 *
 * See also @ref board.
 *
 *  Reminder: The term SFD refers to the Start Frame Delimiter.
 * 
 * <!-- Author(s): Lee Taylor, lee@ember.com, Jeff Mathews, jm@ember.com -->
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup board
 *
 * See dev0221.h.
 *
 */

#ifndef __BOARD_H__
#define __BOARD_H__


/** @name Custom Baud Rate Definitions
 *
 * The following define is used with defining a custom baud rate for the UART.
 * This define provides a simple hook into the definition of
 * the baud rates used with the UART.  The baudSettings[] array in uart.c
 * links the BAUD_* defines with the actual register values needed for
 * operating the UART.  The array baudSettings[] can be edited directly for a
 * custom baud rate or another entry (the register settings) can be provided
 * here with this define.
 */
//@{
/**
 * @brief This define is the register setting for generating a baud of
 * 250000.  Refer to the Atmel datasheet's discussion on UART buad rates for
 * the equation used to derive this value.
 */
#define EMBER_SERIAL_BAUD_CUSTOM  1
//@} //END OF CUSTOM BAUD DEFINITIONS


/** @name Radio Connection Items 
 *
 * The following are used to aid in the abstraction with 
 * the radio HAL implementations.  The microcontroller-specific radio 
 * HAL sources use these definitions so they are able to work across a
 * variety of boards which could have different radio connections.  The
 * names and ports/pins used below are intended to match with a schematic
 * of the system to provide the abstraction.
 */
//@{
#define RAD_SPI_PORT     PORTB
#define RAD_SPI_PIN      PINB
#define RAD_SPI_DDR      DDRB
#define RAD_SPI_PSEL     0
#define RAD_SPI_PCLK     1
#define RAD_SPI_PDI      2 // MOSI
#define RAD_SPI_PDO      3 // MISO

#define RAD_VREG_PORT    PORTB
#define RAD_VREG_PIN     PINB
#define RAD_VREG_DDR     DDRB
#define RAD_VREG         5

#define RAD_RESETn_PORT  PORTD
#define RAD_RESETn_PIN   PIND
#define RAD_RESETn_DDR   DDRD
#define RAD_RESETn       7

#define RAD_CCA_PORT     PORTD
#define RAD_CCA_PIN      PIND
#define RAD_CCA_DDR      DDRD
#define RAD_CCA          5

#define RAD_SFD_PORT     PORTD
#define RAD_SFD_PIN      PIND
#define RAD_SFD_DDR      DDRD
#define RAD_SFD          4

#define RAD_FIFOP_PORT   PORTE
#define RAD_FIFOP_PIN    PINE
#define RAD_FIFOP_DDR    DDRE
#define RAD_FIFOP        4

#define RAD_FIFO_PORT    PORTE
#define RAD_FIFO_PIN     PINE
#define RAD_FIFO_DDR     DDRE
#define RAD_FIFO         5
//@} //END OF RADIO CONNECTION ITEMS

/** @name Packet Trace Connection Items
 *
 * The following are used to aid in the abstraction with the Packet Trace
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * @note  This board does not support packet trace, though the definitions are left
 *  here for reference
 */
//@{
//#define PACKET_TRACE  // This board does not support Packet Trace
  
#define PACKET_TRACE_CS_PORT     
#define PACKET_TRACE_CS_PIN      
#define PACKET_TRACE_CS_DDR      
#define PACKET_TRACE_CS          

#define PACKET_TRACE_FRAME_PORT  
#define PACKET_TRACE_FRAME_PIN   
#define PACKET_TRACE_FRAME_DDR   
#define PACKET_TRACE_FRAME       

#define PACKET_TRACE_DIR_PORT    
#define PACKET_TRACE_DIR_PIN     
#define PACKET_TRACE_DIR_DDR     
#define PACKET_TRACE_DIR         
//@} //END OF PACKET TRACE CONNECTION ITEMS

/** @name Debug Sync Connection Items
 *
 * The following are used to aid in the abstraction with the Debug Sync
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 */
//@{
#define DEBUGSYNC_TRIGGER_PORT  PORTE
#define DEBUGSYNC_TRIGGER_PIN   PINE
#define DEBUGSYNC_TRIGGER_DDR   DDRE
#define DEBUGSYNC_TRIGGER       7
//@} //END OF DEBUG SYNC CONNECTION ITEMS

/** @name LED Definitions
 *
 * The following are used to aid in the abstraction with the LED
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The BOARDLED<i>n</i> macros should always be used when manipulating the 
 *  state of LEDs, as they directly refer to the pins to which the LEDs are
 *  connected \b 
 *
 * BOARDLED_MASK should be set to a mask with a bit set for each pin to
 *  an LED is connected
 *
 * @note  (1) LEDs 0 and 1 are on the RFuP.
 * @note  (2) LEDs 2, 3, 4, and 5 are on the carrier board (DEV0221).
 */
//@{
enum HalBoardLedPins {
  BOARDLED0 = 4,
  BOARDLED1 = 7,
  BOARDLED2 = 5,
  BOARDLED3 = 6,
  BOARDLED4 = 2,
  BOARDLED5 = 3,
  BOARD_ACTIVITY_LED  = BOARDLED0,
  BOARD_HEARTBEAT_LED = BOARDLED1
};
#define BOARDLED_MASK   0xFC

#define LED_DDR   DDRC
#define LED_PORT  PORTC 
//@} //END OF LED DEFINITIONS

/** @name Button Definitions
 *
 * The following are used to aid in the abstraction with the Button
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The BUTTON<i>n</i> macros should always be used with manipulating the buttons
 *  as they directly refer to the pins to which the buttons are connected
 */
//@{
#define BUTTON0                        0
#define BUTTON0_PORT                   PORTD
#define BUTTON0_DDR                    DDRD
#define BUTTON0_PIN                    PIND
#define BUTTON0_INT                    INT0
#define BUTTON0_INT_VECT               INT0_vect
#define BUTTON0_CONFIG_FALLING_EDGE()  \
  do {                                 \
    EICRA |= BIT(ISC01);               \
    EICRA &= ~BIT(ISC00);              \
  } while(0)
#define BUTTON0_CONFIG_RISING_EDGE()   \
  do {                                 \
    EICRA |= BIT(ISC01);               \
    EICRA |= BIT(ISC00);               \
  } while(0)

#define BUTTON1                        1
#define BUTTON1_PORT                   PORTD
#define BUTTON1_DDR                    DDRD
#define BUTTON1_PIN                    PIND
#define BUTTON1_INT                    INT1
#define BUTTON1_INT_VECT               INT1_vect
#define BUTTON1_CONFIG_FALLING_EDGE()  \
  do {                                 \
    EICRA |= BIT(ISC11);               \
    EICRA &= ~BIT(ISC10);              \
  } while(0)
#define BUTTON1_CONFIG_RISING_EDGE()   \
  do {                                 \
    EICRA |= BIT(ISC11);               \
    EICRA |= BIT(ISC10);               \
  } while(0)

#define buttonPORT PORTD
#define buttonDDR  DDRD
#define buttonPINS PIND
//@} //END OF BUTTON DEFINITIONS

/** @name Temperature sensor ADC channel
 *
 * Define the analog input channel connected to the LM-20 temperature sensor.
 * The scale factor compensates for different platform input ranges.
 * PF1/ADC1 must be an input.
 * PC1 must be an output and set to a high level to power the sensor.
 *
 *@{
 */
#define TEMP_SENSOR_ADC_CHANNEL ADC_SOURCE_ADC1
#define TEMP_SENSOR_SCALE_FACTOR 2
/** @} END OF TEMPERATURE SENSOR ADC CHANNEL DEFINITIONS */

/** @name Board Specific Functions
 *
 * The following macros exist to aid in the initialization, power up from sleep,
 *  and power down to sleep operations.  These macros are responsible for
 *  either initializing directly, or calling initialization functions for any
 *  peripherals that are specific to this board implementation.  These
 *  macros are called from halInit(), halPowerDown(), and halPowerUp() respectively.
 */
 //@{
#define halInternalInitBoard()      \
        do {                        \
          halInternalInitAdc();     \
          halInternalInitButton();  \
          halInternalInitLed();     \
          /* temp sensor */         \
          DDRC |= BIT(1);           \
          PORTC |= BIT(1);          \
          /* Buzzer */              \
          DDRB |= 0x80;             \
          PORTB |= 0x80;            \
        } while(0)

#define halInternalPowerDownBoard()                                 \
        do {                                                        \
          halInternalSleepAdc();                                    \
          halInternalInitLed(); /* Turns leds off */                \
          /* Disable Buzzer */                                      \
          TCCR2 = BIT(WGM21); /* no clock, no timer override */     \
          TIMSK &= ~(BIT(OCIE2));          /* disable interrupt */  \
          /* Disable Temp sensor  */                                \
          PORTC &= ~BIT(1);                                         \
        } while(0) 

#define halInternalPowerUpBoard()   \
        do {                        \
          halInternalInitAdc();     \
          /* Enable Temp sensor */  \
          DDRC |= BIT(1);           \
          PORTC |= BIT(1);          \
        } while(0)
//@} //END OF BOARD SPECIFIC FUNCTIONS

#endif //__BOARD_H__

