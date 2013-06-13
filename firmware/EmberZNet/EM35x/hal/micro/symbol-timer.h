/** @file symbol-timer.h
 * 
 * @brief Functions that provide access to symbol time. One symbol period is 16
 * microseconds.
 *
 * See @ref symbol_timer for documentation.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved. -->   
 */

/** @addtogroup symbol_timer
 * See symbol-timer.h for source code.
 *@{
 */


#ifndef __SYMBOL_TIMER_H__
#define __SYMBOL_TIMER_H__

/** @name Symbol Timer Functions
 */
//@{

/** @brief Initializes the symbol timer.  This function is only
    implemented when a general purpose microcontroller timer peripheral is used
    to implement the symbol timer (e.g. AVR/EM2420)  When a dedicated symbol
    timer peripheral exists (e.g. EM2xx, EM3xx) this initialization is performed
    directly by the PHY.
  */
void halInternalStartSymbolTimer(void);

/** @brief Should be called by the radio HAL whenever an SFD (Start Frame
 * Delimiter) is detected.
 */
void halInternalSfdCaptureIsr(void);

/** @brief Returns the current symbol time in symbol ticks (16
 * microseconds).
 * @return The least significant 32 bits of the current symbol time in symbol
 * ticks (16 microseconds).
 */
int32u halStackGetInt32uSymbolTick(void);

/** @} END of Symbol Timer Functions */


/** @name MAC Timer Support Functions
 * These functions are used for MAC layer timing.
 *
 * Applications should not directly call these functions. They
 * are used internally by the operation of the stack.
 */
//@{

/**@brief Sets up a timer and calls an interrupt-context callback
 * when it expires.
 *
 * Used by the MAC to request an interrupt callback at a specified amount
 * of time in the future.
 *
 * @param symbols  The delay, in symbol ticks (16 microseconds).
 */
void halStackOrderInt16uSymbolDelayA(int16u symbols);

/**@brief Sets up a timer and calls an interrupt-context callback
 * when it expires. 
 * @note  This is different from halStackOrderInt16uSymbolDelayA()
 * in that it sets up the interrupt a specific number of symbols after the last 
 * timer interrupt occurred, instead of from the time this function is called.
 *
 * Used by the MAC to request periodic timer based interrupts.
 *
 * @param symbols  The delay, in symbol ticks (16 microseconds).
 */
void halStackOrderNextSymbolDelayA(int16u symbols);

/**@brief Cancels the timer set up by halStackOrderInt16uSymbolDelayA().
 */
void halStackCancelSymbolDelayA(void);

/** @brief This is the interrupt level callback into the stack that is
 * called when the timers set by halStackOrder*SymbolDelayA expire.
 */
void halStackSymbolDelayAIsr(void);

/** @} END of MAC Timer Support Functions */

#endif //__SYMBOL_TIMER_H__

/** @} END addtogroup */
