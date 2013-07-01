/**
 * @file form-and-join3_2.h
 * @brief Utilities for forming and joining networks. 
 *  Deprecated and will be removed from a future release.
 *  Use form-and-join.h instead.
 *
 * See @ref networks for documentation.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.       *80* -->
 *******************************************************************************
 */

/**
 * @addtogroup deprecated
 * form-and-join3_2.h
 */

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief The current reason for scanning.
 */
enum formAndJoinScanType
#else
extern int8u formAndJoinScanType;
enum
#endif
{
  FORM_AND_JOIN_NOT_SCANNING,
  FORM_AND_JOIN_ENERGY_SCAN,     /*!< Energy scan for finding a quiet channel. */
  FORM_AND_JOIN_PAN_ID_SCAN,     /*!< Active scan to see which PAN IDs are in use. */
  FORM_AND_JOIN_JOINABLE_SCAN,   /*!< Active scan for a network to join. */
  FORM_AND_JOIN_CROSSTALK_SCAN   /*!< Active scan to work around channel crosstalk. */
};


/** @brief Form a network.  
 *  
 * This performs the following actions:
 * -# Do an energy scan on the indicated channels and randomly choose
 *    one from amongst those with the least average energy.
 * -# Randomly pick a short PAN ID that does not appear during an active
 *    scan on the chosen channel.
 * -# use the Extended PAN ID passed in or pick a random one if the Extended
 *    PAN ID passed in is "0" or a null pointer.
 * -# Form a network using the chosen channel, short PAN ID, and extended
 *    PAN ID.
 *
 * If any errors occur, the status code is passed to ::scanError() and no
 * network is formed.  Success is indicated by calling
 * ::emberStackStatusHandler() with the EMBER_NETWORK_UP status value.
 *
 * @param channelMask  
 * @param radioTxPower  
 * @param extendedPanIdDesired  
 */
void formZigbeeNetwork3_2(int32u channelMask, 
                          int8s radioTxPower,
                          int8u* extendedPanIdDesired);

/** @brief Join a network.
 *  
 * This tries to join the first network found on the indicated channels that 
 * -# currently permits joining
 * -# matches the stack profile of the application
 * -# matches the Extended PAN ID passed in, or if "0" is passed in it 
 *    matches any Extended PAN ID.
 *
 * If any errors occur, the status code is passed to scanError() and no
 * network is joined.  Success is indicated by calling emberStackStatusHandler()
 * with the EMBER_NETWORK_UP status value. 
 *
 * With some board layouts, the em250 is susceptible to a dual channel issue
 * in which packets from 12 channels above or below can sometimes be heard 
 * faintly. This affects channels 11, 12, 13, 14, 23, 24, 25, and 26. 
 * Hardware reference designs EM250_REF_DES_LAT, version C0 and 
 * EM250_REF_DES_CER, version B0 solve the problem.  
 *
 * This function also implements a software workaround. After discovering a 
 * network on one of the susceptible channels, joinZigbeeNetwork also scans 
 * the channel 12 up or down.  If the same network is found there, it 
 * chooses the correct one by comparing the link quality of the received 
 * beacons.
 *
 * @param nodeType  
 * @param channelMask  
 * @param radioTxPower  
 * @param extendedPanIdDesired  
 */
void joinZigbeeNetwork3_2(EmberNodeType nodeType,
                          int32u channelMask,
                          int8s radioTxPower,
                          int8u* extendedPanIdDesired);

/** @brief A callback the application needs to provided.
 *  
 * If an error occurs while attempting to form or join a network,
 * this procedure is called and the form or join effort is aborted.
 *
 * @param status  
 */
void scanError(EmberStatus status);

