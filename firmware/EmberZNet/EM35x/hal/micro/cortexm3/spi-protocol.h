/** 
 * @file hal/micro/cortexm3/spi-protocol.h
 * @brief Internal SPI Protocol implementation for use below the EZSP
 * application.
 *
 * <!-- Author(s): Brooks Barrett -->
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.       *80*-->
 */

#ifndef __SPI_PROTOCOL_H__
#define __SPI_PROTOCOL_H__

/**
 * @description  A pointer to the length byte at the start of the Payload.
 * Upper layers will read the command from this location after
 * halHostSerialTick(FALSE) returns TRUE. The upper layer will write the
 * response to this location before calling halHostSerialTick(TRUE). This
 * pointer is the upper layers' primary access into the command/response buffer.
 */
extern int8u *halHostFrame;

/**
 * @description  A flag that is set to TRUE when the Host initiates the wake
 * handshake.
 */
extern boolean spipFlagWakeFallingEdge;


/** @description 
 * 
 * The SPIP Init routine will also set a flag in the SPIP indicating a
 * wakeup handshake should be performed.  The handshake should only be
 * performed on a SerialTick.  Upon the next Tick call, the SPIP can assume
 * we are fully booted and operational and then take control peforming the
 * full handshake.
 */
void halHostSerialInit(void);

/**
 * @description Reinitializes the SPI Protocol when coming out of sleep
 * (powerdown).
 */
void halHostSerialPowerup(void);

/**
 * @description Shuts down the SPI Protocol when entering sleep (powerdown).
 */
void halHostSerialPowerdown(void);

/** @description 
 * 
 * halHostSerialTick is periodically called by the upper application just like
 * emberTick. The return boolean from halHostSerialTick indicates to the upper
 * layer if there is data available to it in the RX buffer.The passed parameter
 * "responseReady" indicates to the SPIP if there is valid data in the TX
 * buffer.  If responseReady is TRUE, the SPIP may immediately begin a transmit
 * DMA.  The SPIP is responsible for properly scheduling DMA transactions.
 * This Tick function maintains no acknowledgement so the two boolean values
 * being passed in both directions are "single cycle" assertions.  This Tick
 * function is the primary method of passing CPU control to the SPIP.
 *
 * @param responseReady: If TRUE, the SPIP may begin a transmit DMA.
 *
 * @return If TRUE, the upper layer knows there is valid data in the command
 * buffer.
 */
boolean halHostSerialTick(boolean responseReady);

/** @description 
 *
 * When the upper application has a callback it needs to deliver to the Host, it
 * calls halHostCallback() at will with haveData set to TRUE. The HAL will
 * indicate to the Host through the nHOST_INT singal that there is a callback
 * pending. The EZSP application must make another call with haveData set to
 * FALSE when there are no more callbacks pending.  The SPIP is responsible
 * for latching this call, timing actual nHOST_INT manipulation, and
 * multiplexing it in with SPIP generated assertions.
 *
 * @param haveData: TRUE indicates there is a callback and the SPIP should
 * schedule nHOST_INT assertion.  FALSE says the SPIP and deassert nHOST_INT.
 */
void halHostCallback(boolean haveData);


/** @description Test function used by haltest. Nothing to see here...
 *  
 * @param : 
 *
 * @param : 
 */
void spipTest(int16u test, int16u params );

#endif // __SPI_PROTOCOL_H__

