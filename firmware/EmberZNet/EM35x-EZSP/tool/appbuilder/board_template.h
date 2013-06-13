/** @file hal/micro/xap2b/em250/board/dev0455.h
 * See @ref board for detailed documentation.
 * @note The file dev0455.h is intended to be copied, renamed, and
 * customized for customer-specific hardware.
 * 
 * <!-- Copyright 2006 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup board
 *  @brief Functions and definitions specific to the breakout board.
 * 
 * The file dev0455.h is the default BOARD_HEADER file used with the breakout 
 * board of the development kit.
 *
 *@{
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/**
 * @name Custom Baud Rate Definitions
 * The following define is used when defining a custom baud rate for the UART.
 * This define provides a simple hook into the definition of
 * the baud rates used with the UART.  The baudSettings[] array in uart.c
 * links the BAUD_* defines with the actual register values needed for
 * operating the UART.  The array baudSettings[] can be edited directly for a
 * custom baud rate or another entry (the register settings) can be provided
 * here with this define.
 *@{
 */

/**
 * @description This define is the register setting for generating a baud of
 * 921600.  Refer to the EM250 datasheet's discussion on UART baud rates for
 * the equation used to derive this value.
 */
#define EMBER_SERIAL_BAUD_CUSTOM  13

/** @} END Custom Baud Rate Definitions */

/** @name LED Definitions
 *
 * The following are used to aid in the abstraction with the LED
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The ::HalBoardLedPins enum values should always be used when manipulating the 
 * state of LEDs, as they directly refer to the GPIOs to which the LEDs are
 * connected.
 *
 * @note 
 *  - LEDs 0 and 1 are on the RCM.
 *  - LEDs 2 and 3 are on the breakout board (dev0455).
 *  .
 *@{
 */
 
/**
 * @brief Assign each GPIO with an LED connected to a convenient name.
 * ::BOARD_ACTIVITY_LED and ::BOARD_HEARTBEAT_LED provide a further layer of
 * abstraction ontop of the 4 LEDs for verbose coding.
 */
enum HalBoardLedPins {
  BOARDLED0 = 14,
  BOARDLED1 = 13,
  BOARDLED2 = 12,
  BOARDLED3 = 11,
  BOARD_ACTIVITY_LED  = BOARDLED0,
  BOARD_HEARTBEAT_LED = BOARDLED1
};
/**
 * @brief This mask indicates which GPIO the LEDs are connected to.
 * A bit is set for each GPIO to which an LED is connected.
 */
#define BOARDLED_MASK  0x7800

/** @} END OF LED DEFINITIONS  */

/** @name Button Definitions
 *
 * The following are used to aid in the abstraction with the Button
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The BUTTONn macros should always be used with manipulating the buttons
 * as they directly refer to the GPIOs to which the buttons are connected.
 *
 * @note The GPIO number must match the IRQ letter
 *
 *@{
 */
 
/**
 * @brief The actual GPIO the BUTTON0 is connected to.  This define should
 * be used whenever referencing BUTTON0.
 */
#define BUTTON0           8  //the pin, also value used throughout HAL and app
/**
 * @brief The interrupt configuration register for BUTTON0.
 */
#define BUTTON0_INTCFG    GPIO_INTCFGA
/**
 * @brief The filter bit for BUTTON0.
 */
#define BUTTON0_FLT_BIT   GPIO_INTFILT_BIT
/**
 * @brief The interrupt trigger selection for BUTTON0.
 */
#define BUTTON0_MOD_BITS  GPIO_INTMOD_BIT
/**
 * @brief The interrupt bit for BUTTON0.
 */
#define BUTTON0_INT_BIT   INT_GPIOA

/**
 * @brief The actual GPIO the BUTTON1 is connected to.  This define should
 * be used whenever referencing BUTTON1.
 */
#define BUTTON1           15  //the pin, also value used throughout HAL and app
/**
 * @brief The interrupt configuration register for BUTTON1.
 */
#define BUTTON1_INTCFG    GPIO_INTCFGC
/**
 * @brief The filter bit for BUTTON1.
 */
#define BUTTON1_FLT_BIT   GPIO_INTFILT_BIT
/**
 * @brief The interrupt trigger selection for BUTTON1.
 */
#define BUTTON1_MOD_BITS  GPIO_INTMOD_BIT
/**
 * @brief The interrupt bit for BUTTON1.
 */
#define BUTTON1_INT_BIT   INT_GPIOC
/** @} END OF BUTTON DEFINITIONS */
 
/** @name UART Protocol Connection Items
 *
 * The following are used to aid in the abstraction with the 
 * EZSP-UART Protocol.  The HAL uses these definitions so it is able to work
 * across a variety of boards which could have different connections.  The
 * names and ports/pins used below are intended to match with a schematic
 * of the system to provide the abstraction.
 *
 *@{
 */
#define SC1_TXD   9
#define SC1_RXD  10
#define SC1_nCTS 11
#define SC1_nRTS 12
/** @} END OF UART PROTOCOL CONNECTION ITEMS */
 
/** @name Temperature sensor ADC channel
 *
 * Define the analog input channel connected to the LM-20 temperature sensor.
 * The scale factor compensates for different platform input ranges.
 * GPIO6/ADC2 must be an analog input.
 * GPIO7 must be an output and set to a high level to power the sensor.
 *
 *@{
 */
#define TEMP_SENSOR_ADC_CHANNEL ADC_SOURCE_ADC2_GND
#define TEMP_SENSOR_SCALE_FACTOR 1
/** @} END OF TEMPERATURE SENSOR ADC CHANNEL DEFINITIONS */

/** @name Packet Trace
 *
 * When ::PACKET_TRACE is defined, ::GPIO_CFG will automatically be setup by 
 * halInit() to enable Packet Trace support on GPIO Pins 4 and 5, 
 * in addition to the configuration specified below.
 * 
 * @note This define will override any settings for GPIO 4 and 5.
 *
 *@{
 */
 
/**
 * @brief This define does not equate to anything.  It is used as a
 * trigger to enable Packet Trace support on the breakout board (dev0455).
 */
#define PACKET_TRACE  // We do have PACKET_TRACE support

/** @} END OF PACKET TRACE DEFINITIONS */
 
/** @name ENABLE_OSC32K
 *
 * When ENABLE_OSC32K is defined, halInit() will configure system
 * timekeeping to utilize the external 32.768 kHz crystal oscillator
 * rather than the internal 1 kHz RC oscillator.
 *
 * On initial powerup the 32.768 kHz crystal oscillator will take a little
 * while to start stable oscillation. This only happens on initial powerup,
 * not on wake-from-sleep, since the crystal usually stays running in deep
 * sleep mode.
 *
 * When ENABLE_OSC32K is defined the crystal oscillator is started as part of
 * halInit(). After the crystal is started we delay for
 * OSC32K_STARTUP_DELAY_MS (time in milliseconds).  This delay allows the
 * crystal oscillator to stabilize before we start using it for system timing.
 *
 * If you set OSC32K_STARTUP_DELAY_MS to less than the crystal's startup time:
 *   - The system timer won't produce a reliable one millisecond tick before
 *     the crystal is stable.
 *   - You may see some number of ticks of unknown period occur before the
 *     crystal is stable.
 *   - halInit() will complete and application code will begin running, but
 *     any events based on the system timer will not be accurate until the
 *     crystal is stable.
 *   - An unstable system timer will only affect the APIs in system-timer.h.
 *
 * Typical 32.768 kHz crystals measured by Ember take about 400 milliseconds
 * to stabilize. Be sure to characterize your particular crystal's stabilization
 * time since crystal behavior can vary.
 *
 *@{
 */
#define OSC32K_STARTUP_DELAY_MS (0)

#if OSC32K_STARTUP_DELAY_MS > MAX_INT16U_VALUE
#error "OSC32K_STARTUP_DELAY_MS must fit in 16 bits."
#endif
 
/**
 * @brief This define does not equate to anything.  It is used as a
 * trigger to enable 32.768 kHz XTAL oscillator on the RCM (0452) plugged
 * into the Breakout board (dev0455).
 * Default is to disable 32.768 kHz XTAL and use 1 kHz RC oscillator instead.
 */
//#define ENABLE_OSC32K  // Enable 32.768 kHz osc instead of 1 kHz RC osc

/** @} END OF ENABLE OSC32K DEFINITIONS */
 
/** @name DISABLE_OSC24M_BIAS_TRIM
 *
 * When ::DISABLE_OSC24M_BIAS_TRIM is defined, ::halInit() will disable adjusting
 * the 24 MHz oscillator's bias trim based on its corresponding Manufacturing
 * token, and instead utilize full bias at power up and deep sleep wakeups.
 * This should be utilized only if the Manufacturing token value proves to be
 * unreliable.
 *
 *@{
 */
 
/**
 * @brief This define does not equate to anything.  It is used as a
 * trigger to disable bias adjustment of the 24 MHz crystal oscillator on
 * the RCM (0452) plugged into the Breakout board (dev0455).
 * Default is to enable bias adjustment.
 */
//#define DISABLE_OSC24M_BIAS_TRIM  // Disable 24 MHz bias trim adjustment

/** @} /ND OF DISABLE OSC24M BIAS TRIM DEFINITIONS */
 
  //Since the two GPIO config registers affect multiple modules, it is not
  //possible for one or more of the modules to own the configuration
  //of the GPIO.  This is the only place the registers should be touched.
  //The groups of comments below are setting for a couple common modes.
/*
  //GPIO[16,15,14,13]
  //+ SC1-2 + SC2-2 + CAP2-0 + CAP1-0 + GPIO[12,11,3,0]
  GPIO_DBG = 0x0000;
  GPIO_CFG = 0x0010;
*/
/*
  //  TMR1OB
  //+ GPIO[15,14,13]
  //+ SC1-2 + SC2-4S + CAP2-0 + CAP1-2h + GPIO[12,11]
  GPIO_DBG = 0x0000;
  GPIO_CFG = 0x0058;
*/
/*
  //GPIO[16,15,14,13]
  //+ SC1-3M + SC2-4S + CAP2-2 + CAP1-2h + GPIO[12]
  GPIO_DBG = 0x0000;
  GPIO_CFG = 0x00F0;
*/
/*
** DEFAULT CONFIGURATION **
  //  TMR1OB
  //+ GPIO[15,14,13]
  //+ SC1-2 + SC2-3M + CAP2-0 + CAP1-2 + GPIO[12,11,3]
  //+ ADC2
  GPIO_DBG = 0x0000;
  GPIO_CFG = 0x0498;
*/

/** @name GPIO Configuration Definitions
 *
 * The following are used to specify the GPIO configuration to establish
 * when Powered (POWERUP_), and when Deep Sleeping (POWERDN_).  The reason
 * for separate Deep Sleep settings is to allow for a slightly different
 * configuration that minimizes power consumption during Deep Sleep.  For
 * example, inputs that might float could be pulled up or down, and output
 * states chosen with care, e.g. to turn off LEDs or other devices that might
 * consume power or be unnecessary when Deep Sleeping.
 *
 *@{
 */
 
/**
 * @brief A convenience macro to convert GPIO# into bit mask for the
 * GPIO registers.
 */
#define GPIO(n) (1u << ((n) & 0xF))

/**
 * @brief Powered setting of GPIO_DBG debug configuration register.
 */
#define POWERUP_GPIO_DBG     0
/**
 * @brief Powered setting of GPIO_CFG configuration register.
 */
#define POWERUP_GPIO_CFG _replace_POWERUP_GPIO_CFG_
/**
 * @brief Powered setting of GPIO_DIRH GPIO16 output-enable register.
 */
#define POWERUP_GPIO_DIRH    0                 // GPIO16 = Input
/**
 * @brief Powered setting of GPIO_DIRL GPIO15..0 output-enable register.
 */
#ifdef EMBER_SERIAL1_RTSCTS
 #define POWERUP_GPIO_DIRL   (BOARDLED_MASK|GPIO(7)|GPIO(5)|GPIO(4)|GPIO(3)\
                             |BIT(SC1_nRTS)|BIT(SC1_TXD))//RTS, TXD are output
#else
 #define POWERUP_GPIO_DIRL   (BOARDLED_MASK|GPIO(7)|GPIO(5)|GPIO(4)|GPIO(3))
                                               // LEDs & GPIO7,5,4,3 outputs
#endif
/**
 * @brief Powered setting of ::GPIO_CLRH clear GPIO16 output register.
 */
#define POWERUP_GPIO_CLRH    0                 // No output state change
/**
 * @brief Powered setting of ::GPIO_CLRL clear GPIO15..0 outputs register.
 */
#define POWERUP_GPIO_CLRL    GPIO(4)           // GPIO4 low
/**
 * @brief Powered setting of ::GPIO_SETH set GPIO16 output register.
 */
#define POWERUP_GPIO_SETH    0                 // No output state change
/**
 * @brief Powered setting of ::GPIO_SETL set GPIO15..0 outputs register.
 */
#ifdef EMBER_SERIAL1_RTSCTS
  #define POWERUP_GPIO_SETL   (BOARDLED_MASK|GPIO(7)|GPIO(5) \
                              |BIT(SC1_nRTS)|BIT(SC1_TXD))//RTS, TXD are high
#else
  #define POWERUP_GPIO_SETL   (BOARDLED_MASK|GPIO(7)|GPIO(5))
                                               // LEDs off (high), GPIO7,5 high
#endif
/**
 * @brief Powered setting of ::GPIO_PUH GPIO16 pullup resistor enable
 * register.
 */
#define POWERUP_GPIO_PUH     0                 // No pullup on GPIO16
/**
 * @brief Powered setting of ::GPIO_PUL GPIO15..0 pullup resistors enable
 * register.
 */
#ifdef EMBER_SERIAL1_RTSCTS
  #define POWERUP_GPIO_PUL    (GPIO(BUTTON1)|GPIO(BUTTON0)|BIT(SC1_RXD))
#else
  #define POWERUP_GPIO_PUL    (GPIO(BUTTON1)|GPIO(BUTTON0))
                                               // Pullup Buttons
#endif
/**
 * @brief Powered setting of ::GPIO_PDH GPIO16 pulldown resistor enable
 * register.
 */
#define POWERUP_GPIO_PDH     0                 // No pulldown on GPIO16
/**
 * @brief Powered setting of GPIO_PDL GPIO15..0 pulldown resistors enable
 * register.
 */
#ifdef EMBER_SERIAL1_RTSCTS
  #define POWERUP_GPIO_PDL     (BIT(SC1_nCTS))   // PD only on GPIO11 (nCTS)
#else
  #define POWERUP_GPIO_PDL     0x0000            // No pulldowns on GPIO15..0
#endif

/**
 * @brief Deep Sleep setting of ::GPIO_DBG debug configuration register.
 */
#define POWERDN_GPIO_DBG     POWERUP_GPIO_DBG
/**
 * @brief Deep Sleep setting of ::GPIO_CFG configuration register.
 */
#define POWERDN_GPIO_CFG    (POWERUP_GPIO_CFG & ~0x4F00)
                                               // Disable analog I/O switches
/**
 * @brief Deep Sleep setting of ::GPIO_DIRH GPIO16 output-enable register.
 */
#define POWERDN_GPIO_DIRH    POWERUP_GPIO_DIRH
/**
 * @brief Deep Sleep setting of ::GPIO_DIRL GPIO15..0 output-enable register.
 */
#define POWERDN_GPIO_DIRL    POWERUP_GPIO_DIRL
/**
 * @brief Deep Sleep setting of ::GPIO_CLRH clear GPIO16 output register.
 */
#define POWERDN_GPIO_CLRH    0                 // No output state change
/**
 * @brief Deep Sleep setting of ::GPIO_CLRL clear GPIO15..0 outputs register.
 */
#define POWERDN_GPIO_CLRL    0x0000            // No output state change
/**
 * @brief Deep Sleep setting of ::GPIO_SETH set GPIO16 output register.
 */
#define POWERDN_GPIO_SETH    0                 // No output state change
/**
 * @brief Deep Sleep setting of ::GPIO_SETL set GPIO15..0 outputs register.
 */
#define POWERDN_GPIO_SETL    BOARDLED_MASK     // LEDs off (high)
/**
 * @brief Deep Sleep setting of ::GPIO_PUH GPIO16 pullup resistor
 * enable register.
 */
#define POWERDN_GPIO_PUH     POWERUP_GPIO_PUH
/**
 * @brief Deep Sleep setting of ::GPIO_PUL GPIO15..0 pullup resistors
 * enable register.
 */
#ifdef  PACKET_TRACE
#define POWERDN_GPIO_PUL    (POWERUP_GPIO_PUL                             \
                            |GPIO(0)            /* SC1  MOSI pull up   */ \
                            |GPIO(1)            /* SC1  MISO pull up   */ \
                            |GPIO(5)            /* PTI  DATA pull up   */ \
                            |GPIO(9)            /* SC2   TXD pull up   */ \
                            |GPIO(10)           /* SC2   RXD pull up   */ \
                            )
#else//!PACKET_TRACE
#define POWERDN_GPIO_PUL    (POWERUP_GPIO_PUL                             \
                            |GPIO(0)            /* SC1  MOSI pull up   */ \
                            |GPIO(1)            /* SC1  MISO pull up   */ \
                            |GPIO(9)            /* SC2   TXD pull up   */ \
                            |GPIO(10)           /* SC2   RXD pull up   */ \
                            )
#endif//PACKET_TRACE
/**
 * @brief Deep Sleep setting of ::GPIO_PDH GPIO16 pulldown resistor
 * enable register.
 */
#define POWERDN_GPIO_PDH     (POWERUP_GPIO_PDH|GPIO(16)) // TMR1 pull down
/**
 * @brief Deep Sleep setting of ::GPIO_PDL GPIO15..0 pulldown resistors
 * enable register.
 */
#ifdef  PACKET_TRACE
#define POWERDN_GPIO_PDL    (POWERUP_GPIO_PDL                             \
                            |GPIO(2)            /* SC1 MSCLK pull down */ \
                            |GPIO(4)            /* PTI    EN pull down */ \
                            |GPIO(6)            /* GPIO6     pull down */ \
                            )
#else//!PACKET_TRACE
#define POWERDN_GPIO_PDL    (POWERUP_GPIO_PDL                             \
                            |GPIO(2)            /* SC1 MSCLK pull down */ \
                            |GPIO(6)            /* GPIO6     pull down */ \
                            )
#endif//PACKET_TRACE

/**
 * @brief A convenient define that chooses if this external signal can
 * be used as source to wake from deep sleep.  Any change in the state of the
 * signal will wake up the CPU.
 */
#define WAKE_ON_GPIO0   FALSE
#define WAKE_ON_GPIO1   FALSE
#define WAKE_ON_GPIO2   FALSE
#define WAKE_ON_GPIO3   FALSE
#define WAKE_ON_GPIO4   FALSE
#define WAKE_ON_GPIO5   FALSE
#define WAKE_ON_GPIO6   FALSE
#define WAKE_ON_GPIO7   FALSE
#define WAKE_ON_GPIO8   TRUE   //BUTTON0
#define WAKE_ON_GPIO9   FALSE
#define WAKE_ON_GPIO10  FALSE
#define WAKE_ON_GPIO11  FALSE
#define WAKE_ON_GPIO12  FALSE
#define WAKE_ON_GPIO13  FALSE
#define WAKE_ON_GPIO14  FALSE
#define WAKE_ON_GPIO15  TRUE   //BUTTON1
#define WAKE_ON_GPIO16  FALSE

/** @} END OF GPIO Configuration Definitions */


/** @name Board Specific Functions
 *
 * The following macros exist to aid in the initialization, power up from sleep,
 * and power down to sleep operations.  These macros are responsible for
 * either initializing directly, or calling initialization functions for any
 * peripherals that are specific to this board implementation.  These macros 
 * are called from ::halInit(), ::halPowerDown(), and ::halPowerUp() respectively.
 *
 *@{
 */
 
/**
 * @brief Initialize the board.  This function is called from ::halInit().
 */
#ifndef EZSP_UART
  #define halInternalInitBoard()                                  \
          do {                                                    \
            halInternalPowerUpBoard();                            \
            halInternalRestartUart();                             \
            halInternalInitButton();                              \
         } while(0)
#else
  #define halInternalInitBoard()                                  \
          do {                                                    \
            halInternalPowerUpBoard();                            \
         } while(0)
#endif

/**
 * @brief Power down the board.  This macro is called from ::halPowerDown().
 */
#define halInternalPowerDownBoard()                             \
        do {                                                    \
          /* GPIO Output configuration */                       \
          GPIO_CLRH  = POWERDN_GPIO_CLRH; /* output states  */  \
          GPIO_CLRL  = POWERDN_GPIO_CLRL; /* output states  */  \
          GPIO_SETH  = POWERDN_GPIO_SETH; /* output states  */  \
          GPIO_SETL  = POWERDN_GPIO_SETL; /* output states  */  \
          GPIO_DIRH  = POWERDN_GPIO_DIRH; /* output enables */  \
          GPIO_DIRL  = POWERDN_GPIO_DIRL; /* output enables */  \
          /* GPIO Mode Config (see above) */                    \
          GPIO_DBG   = POWERDN_GPIO_DBG;                        \
          GPIO_CFG   = POWERDN_GPIO_CFG;                        \
          /* GPIO Pullup/Pulldown Config */                     \
          GPIO_PDH   = POWERDN_GPIO_PDH;                        \
          GPIO_PDL   = POWERDN_GPIO_PDL;                        \
          GPIO_PUH   = POWERDN_GPIO_PUH;                        \
          GPIO_PUL   = POWERDN_GPIO_PUL;                        \
        } while(0)

/**
 * @brief Power up the board.  This macro is called from  ::halPowerUp().
 */
#define halInternalPowerUpBoard()                               \
        do {                                                    \
          /* GPIO Pullup/Pulldown Config */                     \
          GPIO_PUH   = POWERUP_GPIO_PUH;                        \
          GPIO_PUL   = POWERUP_GPIO_PUL;                        \
          GPIO_PDH   = POWERUP_GPIO_PDH;                        \
          GPIO_PDL   = POWERUP_GPIO_PDL;                        \
          /* GPIO Mode Config (see above) */                    \
          GPIO_DBG   = POWERUP_GPIO_DBG;                        \
          GPIO_CFG   = POWERUP_GPIO_CFG;                        \
          /* GPIO Output configuration */                       \
          GPIO_CLRH  = POWERUP_GPIO_CLRH; /* output states  */  \
          GPIO_CLRL  = POWERUP_GPIO_CLRL; /* output states  */  \
          GPIO_SETH  = POWERUP_GPIO_SETH; /* output states  */  \
          GPIO_SETL  = POWERUP_GPIO_SETL; /* output states  */  \
          GPIO_DIRH  = POWERUP_GPIO_DIRH; /* output enables */  \
          GPIO_DIRL  = POWERUP_GPIO_DIRL; /* output enables */  \
        } while(0)
//@} //END OF BOARD SPECIFIC FUNCTIONS

#endif //__BOARD_H__

/** @} END Board Specific Functions */
 
/** @} END addtogroup */




