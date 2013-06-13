/**
 * @file ember-debug.h
 * See @ref debug for documentation.
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */

#ifndef __EMBER_DEBUG_H__
#define __EMBER_DEBUG_H__

/**
 * @addtogroup debug
 * @brief EmberZNet debugging utilities.
 *
 * See ember-debug.h for source code.
 * @{
 */

// Define the values for DEBUG_LEVEL
#define NO_DEBUG    0
#define BASIC_DEBUG 1
#define FULL_DEBUG  2

/** @brief This function is obsolete and no longer required to
 *  initialize the debug system. 
 *
 * @param port Ignored because the port used for debug communication
 *  is automatically determined for each platform.
 */
#define emberDebugInit(port) do {} while(FALSE)
 
#if (DEBUG_LEVEL >= BASIC_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)
 /** @brief Prints the filename and line number to the debug serial port.
  *
  * @param filename    The name of the file where the assert occurred.
  * 
  * @param linenumber  The line number in the file where the assert occurred.
  */
 void emberDebugAssert(PGM_P filename, int linenumber);
 

 /** @brief Prints the contents of RAM to the debug serial port.
  *
  * @param start  The start address of the block of RAM to dump.
  * 
  * @param end    The end address of the block of RAM to dump (address of the 
  *  last byte).
  */
 void emberDebugMemoryDump(int8u *start, int8u *end);

/** @brief Prints binary data to the debug channel.
 *
 * This function does not use the normal printf format conventions. To print
 * text debug messages, use ::emberDebugPrintf(). The format string must
 * contain only these conversion specification characters:
 * - B - int8u value.
 * - W - int16u value, printed least significant byte first.
 * - D - int32u value, printed least significant byte first.
 * - F - pointer to null terminated string in Flash (PGM_P).
 * - xxxp - pointer to RAM, length is xxx (max 255).
 * - lp - pointer to RAM, length is int8u argument.
 * - xxxf - pointer to Flash (PGM_P), length is xxx (max 255).
 * - lf - pointer to Flash (PGM_P), length is int8u argument.
 * - b - EmberMessageBuffer.
 *
 * Examples:
 * @code
 * emberDebugBinaryPrintf("BWD", status, panId, channelMask);
 * emberDebugBinaryPrintf("F8p", "string example", eui64);
 * emberDebugBinaryPrintf("lp64fb", length, bytes, dataTable, buffer);
 * @endcode
 *
 * @param formatString  A string of conversion specification characters
 *                      describing the arguments to be printed.
 * @param ...           The arguments to be printed.
 */
void emberDebugBinaryPrintf(PGM_P formatString, ...);

 /** @brief internal debug command used by the HAL to send vuart 
   * data out the the debug channel
   * 
   * @param buff  pointer to the data to send
   *
   * @param len   lenght of the data to send
   */
 void emDebugSendVuartMessage(int8u *buff, int8u len);

#else // (DEBUG_LEVEL >= BASIC_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)
  #define emberDebugAssert(filename, linenumber) do {} while(FALSE)
  #define emberDebugMemoryDump(start, end) do {} while(FALSE)
  #define emberDebugBinaryPrintf(formatstring, ...) do {} while(FALSE)
  #define emDebugSendVuartMessage(buff, len) do {} while(FALSE)
#endif // (DEBUG_LEVEL >= BASIC_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)

#if (DEBUG_LEVEL == FULL_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)
/** @brief Prints an ::EmberStatus return code to the serial port.
  *
  * @param code  The ::EmberStatus code to print.
  */
 void emberDebugError(EmberStatus code);

 /** @brief Turns off all debug output.  
  * 
  * @return The current state (TRUE for on, FALSE for off).
  */
 boolean emberDebugReportOff(void);

 /** @brief Restores the state of the debug output.
  *
  * @param state  The state returned from ::emberDebugReportOff().
  * This is done so that debug output is not blindly turned on.
  */
 void emberDebugReportRestore(boolean state);

// Format: Same as emberSerialPrintf
// emberDebugPrintf("format string"[, parameters ...])
/** @brief Prints text debug messages.
 *
 * @param formatString  Takes the following:
 *
 * <table border=0>
 * <tr><td align="right">%%</td><td>Percent sign</td></tr>
 * <tr><td align="right">%%c</td><td>Single-byte char</td></tr>
 * <tr><td align="right">%%s</td><td>RAM string</td></tr>
 * <tr><td align="right">%%p</td><td>Flash string (does not follow the printf standard)</td></tr>
 * <tr><td align="right">%%u</td><td>Two-byte unsigned decimal</td></tr>
 * <tr><td align="right">%%d</td><td>Two-byte signed decimal</td></tr>
 * <tr><td align="right">%%x, %%2x, %%4x </td><td>1-, 2-, 4-byte hex value (always 0 padded;
 *         does not follow the printf standard)</td></tr>
 * </table>
 */
void emberDebugPrintf(PGM_P formatString, ...);

#else // (DEBUG_LEVEL == FULL_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)
  #define emberDebugError(code) do {} while(FALSE)
  // Note the following doesn't have a do{}while(FALSE) 
  //   because it has a return value
  #define emberDebugReportOff() (FALSE)
  #define emberDebugReportRestore(state) do {} while(FALSE)
  #define emberDebugPrintf(...) do {} while(FALSE)
#endif // (DEBUG_LEVEL == FULL_DEBUG) || defined(DOXYGEN_SHOULD_SKIP_THIS)

/** @} END addtogroup */


#endif // __EMBER_DEBUG_H__

