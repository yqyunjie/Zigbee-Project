/** @file hal/micro/cortexm3/bootloader/bootloader-serial.h
 * See @ref cbhc_serial for detailed documentation.
 *
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.       *80* -->
 */
 
//[[ Author(s): David Iacobone, diacobone@ember.com
//              Lee Taylor, lee@ember.com
//]]

/** @addtogroup cbhc_serial
 * @brief EM35x common bootloader serial definitions.
 *
 * See bootloader-serial.h for source code.
 *@{
 */

#ifndef __BOOTLOADER_SERIAL_H__
#define __BOOTLOADER_SERIAL_H__

/** @brief Initialize serial port.
 */
void serInit(void);

/** @brief Flush the transmiter.
 */
void serPutFlush(void);
/** @brief Transmit a character.
 * @param ch A character.
 */
void serPutChar(int8u ch);
/** @brief  Transmit a string.
 * @param str A string.
 */
void serPutStr(const char *str);
/** @brief Transmit a buffer.
 * @param buf A buffer.
 * @param size Length of buffer.
 */
void serPutBuf(const int8u buf[], int8u size);
/** @brief Transmit a 16bit value in decimal.
 * @param val The data to print.
 */
void serPutDecimal(int16u val);
/** @brief Transmit a byte as hex.
 * @param byte A byte.
 */
void serPutHex(int8u byte);
/** @brief  Transmit a 16bit integer as hex.
 * @param word A 16bit integer.
 */
void serPutHexInt(int16u word);

/** @brief Determine if a character is available.
 * @return ::TRUE if a character is available, ::FALSE otherwise.
 */
boolean serCharAvailable(void);
/** @brief Get a character if available, otherwise return an error.
 * @param ch Pointer to a location where the received byte will be placed.
 * @return ::BL_SUCCESS if a character was obtained, ::BL_ERR otherwise.
 */
int8u   serGetChar(int8u* ch);

/** @brief Flush the receiver.
 */
void    serGetFlush(void);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef BTL_HAS_EZSP_SPI
extern int8u preSpiComBufIndex;
#define serResetCharInput() preSpiComBufIndex = 0;
#endif
#endif

#endif //__BOOTLOADER_SERIAL_H__

/**@} end of group*/

