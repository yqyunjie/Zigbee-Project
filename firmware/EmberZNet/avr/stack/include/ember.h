/**
 * @file ember.h
 * @brief The master include file for the EmberZNet API.
 *
 *  See @ref ember for documentation.
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com,        -->
 * <!--            Richard Kelsey, richard@ember.com -->
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */
 
/** 
 * @addtogroup ember EmberZNet Stack API Reference
 * This documentation describes the application programming interface (API)
 * for the EmberZNet stack. 
 * The file ember.h is the master include file for the EmberZNet API modules.
 */ 


#ifndef __EMBER_H__
#define __EMBER_H__

#include "ember-types.h"
#include "stack-info.h"
#include "network-formation.h"
#include "packet-buffer.h"
#include "message.h"
#include "child.h"
#include "security.h"
#include "binding-table.h"
#include "bootload.h"
#include "error.h"
#include "zigbee-device-stack.h"
#include "event.h"
#include "ember-debug.h"
#include "library.h"

/** @name PHY Information 
 * Bit masks for TOKEN_MFG_RADIO_BANDS_SUPPORTED.
 */
//@{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define RADIO_BANDS_SUPPORTED_868   BIT(0)
#define RADIO_BANDS_SUPPORTED_915   BIT(1)
#define RADIO_BANDS_SUPPORTED_433   BIT(2)
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief 2.4GHz band */
#define RADIO_BANDS_SUPPORTED_2400  BIT(3)

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define RADIO_BANDS_SUPPORTED_408   BIT(4)
#endif // DOXYGEN_SHOULD_SKIP_THIS

//@} //END PHY INFO

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/** @name Stack Build Configuration Settings */
//@{
#define BUILD_DEF(name, value) \
    name = value,
  enum {
    #include "config/config.h"
  };
#undef BUILD_DEF
//@} //END STACK BUILD CONFIG SETTINGS

#ifdef DEBUG_ASSERT
extern boolean enableFailure;
extern int8u rateOfFailure; 
extern int8u failuresInARow; 
static int8u bufferFailure;
boolean generateFailure(void);
void dumpFailure(void);
#endif

#endif //DOXYGEN_SHOULD_SKIP_THIS

#endif // __EMBER_H__

/**
 * <!-- HIDDEN
 * @page 2p5_to_3p0
 * <hr>
 * Many functions have been moved from ember.h to the following files:
 * - binding-table.h
 * - bootload.h
 * - child.h
 * - network-formation.h
 * - stack-info.h
 * - trust-center.h
 * 
 * Changes include:
 * <ul>
 * <li> <b>New items</b>
 * <li> <b>Changed items</b>
 *   - emberJoinNetwork()
 *   . 
 * <li> <b>Removed items</b>
 *   - emberCloseConnection()
 *   - emberConnectionStatus()
 *   - emberConnectionStatusHandler()
 *   - emberCreateAggregationRoutes()
 *   - emberGetBindingDestinationNodeId()
 *   - emberGetCachedDescription()
 *   - emberIncomingRawMessageHandler()
 *   - emberIncomingSpdoMessageHandler()
 *   - emberMaximumTransportPayloadLength()
 *   - emberMobileNodeHasMoved()
 *   - emberOpenConnection()
 *   - emberSendDatagram() - now use ::emberSendUnicast().  
 *     Porting utility provided in app/util/legacy.c.
 *   - emberSendDiscoveryInformationToParent()
 *   - emberSendLimitedMulticast()
 *   - emberSendSequenced()
 *   - emberSendSpdoDatagramToParent()
 *   - emberSetBindingDestinationNodeId()
 *   - emberSetEncryptionKey()
 *   - emberSpdoUnicastSent()
 *   - emberUnicastSent()
 *   . 
 * </ul> 
 * HIDDEN -->
 */

