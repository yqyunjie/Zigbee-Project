/** @file hal/micro/crc.h
 * See @ref crc for detailed documentation.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

#ifndef __CRC_H__
#define __CRC_H__

/** @addtogroup crc
 * @brief Functions that provide access to cyclic redundancy code (CRC) 
 * calculation. See crc.h for source code.
 *@{
 */

/** @brief Calculates cyclic redundancy code (CITT CRC 16).
 *
 * @note On some radios or micros, the CRC
 * for error detection on packet data is calculated in hardware.
 *
 * Applies the standard CITT CRC 16 polynomial to a
 * single byte. It should support being called first with an initial
 * remainder, then repeatedly until a whole packet is processed.
 *
 * @param newByte       The new byte to be run through CRC.
 *
 * @param prevRemainder The previous CRC remainder.
 *
 * @return The new CRC remainder.
 */
int16u halCommonCrc16(int8u newByte, int16u prevRemainder);

/**@}  // end of CRC Functions
 */
 
#endif //__CRC_H__

