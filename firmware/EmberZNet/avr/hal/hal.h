/** @file hal/hal.h
 * @brief Generic set of HAL includes for all platforms.
 *
 * See also @ref hal for more documentation.
 *  
 * Some HAL includes are not used or present in builds intended for the Host
 * processor connected to the EM260 Network Processor.
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2005-2007 by Ember Corporation. All rights reserved.  *80*-->
 */
  
/** @addtogroup hal 
 *  @if Atmega128
 *    <center><h1>Atmel Atmega128 Microprocessors</h1></center>
 *  @endif
 *  @if Atmega32
 *    <center><h1>Atmel Atmega32 Microprocessors</h1></center>
 *  @endif
 *  @if TI
 *    <center><h1>TI Microprocessors</h1></center>
 *  @endif
 *  @if EM250
 *    <center><h1>EM250 Microprocessors</h1></center>
 *  @endif
 *
 * HAL function names have the following prefix conventions:
 *
 *  <b>halCommon:</b>   API that is used by the EmberZNet stack and can also be called
 *                 from an application. This API must be implemented. Custom
 *                 applications can change the implementation of the API but
 *                 its functionality must remain the same.
 *
 *  <b>hal:</b>    API that is used by sample applications. Custom
 *                 applications can remove this API or change its
 *                 implementation as they see fit.
 *
 *
 *  <b>halStack:</b>   API used only by the EmberZNet stack. This API must be implemented
 *                 and should not be directly called from any application.
 *                 Custom applications can change the implementation of the
 *                 API, but its functionality must remain the same.
 *
 *  <b>halInternal:</b>   API that is internal to the HAL. The EmberZNet stack
 *                and applications must never call this API directly.
 *                Custom applications can change this API as they see
 *                fit. However, be careful not to impact the functionalty of
 *                any halStack or halCommon APIs.
 * <br><br>
 *
 * See also hal.h.
 */


#ifndef __HAL_H__
#define __HAL_H__

// Keep micro and board first for specifics used by other headers
#include "micro/micro.h"
#if !defined(STACK) && defined(BOARD_HEADER)
#include BOARD_HEADER
#endif

#include "micro/adc.h"
#include "micro/button.h"
#include "micro/buzzer.h"
#include "micro/crc.h"
#include "micro/led.h"
#include "micro/random.h"
#include "micro/serial.h"
#include "micro/spi.h"
#include "micro/system-timer.h"
//Host processors do not use the following modules, therefore the header
//files should be ignored.
#ifndef EZSP_HOST
  #include "micro/bootloader-app-image.h"
  #include "micro/bootloader-interface-app.h"
  #include "micro/bootloader-interface-standalone.h"
  #include "micro/diagnostic.h"
  #include "micro/symbol-timer.h"
  #include "micro/token.h"
#endif //EZSP_HOST

#endif //__HAL_H__

