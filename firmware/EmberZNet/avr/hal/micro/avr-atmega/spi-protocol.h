/** 
 * @file hal/micro/avr-atmega/spi-protocol.h
 * @brief Example AVR SPI Protocol implementation for interfacing with EM260.
 *
 * See @ref spi for documentation.
 *
 * <!-- Author(s): Brooks Barrett -->
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.       *80*-->
 */

/** @addtogroup spi
 *
 * See also spi-protocol.h for source code.
 *
 * For complete documentation of the SPI Protocol, refer to the EM260 Datasheet.
 *
 *@{
 */

#ifndef __SPI_PROTOCOL_H__
#define __SPI_PROTOCOL_H__

#include "app/util/ezsp/ezsp-enum.h"

/**
 * @name SPI Protocol Interface
 */
//@{


/**
 * A pointer to the length byte at the start of the Payload. Upper
 * layers will write the command to this location before starting a transaction.
 * The upper layer will read the response from this location after a transaction
 * completes. This pointer is the upper layers' primary access into the
 * command/response buffer.
 */
extern int8u *hal260Frame;

/**
 * This error byte is the third byte found in a special SPI
 * Protocol error case.  It provides more detail concerning the error.
 * Refer to the EM260 Datasheet for a more detailed description of this byte.
 * The application does not need to work with this byte, but it can be
 * useful information when developing.
 */
extern int8u hal260SpipErrorByte;

/**
 * @brief Initializes the SPI Protocol.
 */
void hal260SerialInit(void);

/**
 * @brief Reinitializes the SPI Protocol when coming out of sleep
 * (powerdown).
 */
void hal260SerialPowerup(void);

/**
 * @brief Shuts down the SPI Protocol when entering sleep (powerdown).
 */
void hal260SerialPowerdown(void);

/**
 * @brief Forcefully resets the EM260 by pulling on the nRESET line;
 * waits for the EM260 to boot; verifies that is has booted; verifies the
 * EM260 is active; verifies the SPI Protocol version.  When this function
 * returns, the EM260 is ready to accept all commands.
 *
 * This function is the same as hal260HardResetReqBootload(), except that the
 * EM260 cannot be told to enter bootload mode through the nWAKE signal.
 *
 * @return A EzspStatus value indicating the success or failure of the
 * command.
 */
EzspStatus hal260HardReset(void);

/**
 * @brief Forcefully resets the EM260 by pulling on the nRESET line;
 * sets the nWAKE signal based upon the state of the requestBootload boolean;
 * waits for the EM260 to boot; verifies that is has booted; verifies the
 * EM260 is active; verifies the SPI Protocol version.  When this function
 * returns, the EM260 is ready to accept all commands.
 *
 * This function is the same as hal260HardReset(), except that the ability to
 * request the EM260 enter bootload mode through the nWAKE signal is made
 * available.
 *
 * @return A EzspStatus value indicating the success or failure of the
 * command.
 */
EzspStatus hal260HardResetReqBootload(boolean requestBootload);

/**
 * @brief If the Host thinks that the EM260 is sleeping and wants to
 * wake it up, the EZSP calls hal260WakeUp(). 
 * 
 * Waking up can take some time
 * (milliseconds) so hal260WakeUp() returns immediately and the SPI Protocol
 * calls hal260IsAwakeIsr() once the wakeup handshaking is complete and the
 * EM260 is ready to accept commands.
 */
void hal260WakeUp(void);

/**
 * @brief The EZSP writes a command into the command buffer and then
 * calls hal260SendCommand(). 
 *
 * This function assumes the command being sent
 * is an EZSP frame and therefore sets the SPI Byte for an EZSP Frame.  If
 * sending a command other than EZSP, use hal260SendRawCommand().  This
 * function returns immediately after transmission of the Command has
 * completed and the transaction has entered the Wait section. The EZSP must
 * now call hal260PollForResponse() until the Response is received.
 */
void hal260SendCommand(void);

/**
 * @brief The upper layer writes a command into the command buffer and
 * then calls hal260SendRawCommand(). 
 *
 * This function makes no assumption about
 * the data in the SpipBuffer, it will just faithly try to perform the
 * transaction. This function returns immediately after transmission of the
 * Command has completed and the transaction has entered the Wait section.
 * The upper layer must now call hal260PollForResponse() until the the
 * Response is received.
 */
void hal260SendRawCommand(void);

/**
 * @brief After sending a Command with hal260SendCommand(), the upper
 * layer repeatedly calls this function until the SPI Protocol has finished
 * reception of a Response.
 *
 * @return A EzspStatus value indicating the success or failure of the
 * command.
 */
EzspStatus hal260PollForResponse(void);

/**
 * @brief The SPI Protocol calls hal260IsAwakeIsr() once the wakeup
 * handshaking is complete and the EM260 is ready to accept a command.
 *
 * @param isAwake  TRUE if the wake handshake completed and the EM260 is awake.
 * FALSE is the wake handshake failed and the EM260 is unresponsive.
 */
void hal260IsAwakeIsr(boolean isAwake);

/** @brief If the Host wants to find out whether the EM260 has a
 * pending callback, the EZSP calls hal260HasData(). If this function returns
 * TRUE then the EZSP will send a callback command.
 */
boolean hal260HasData(void);


/**
 * @brief Transmits the SPI Protocol Version Command and checks the
 * response against a literal value to verify the SPI Protocol version.
 *  
 * @return TRUE if the SPI Protocol Version used in this function matches the
 * version returned by the EM260.  FALSE is the versions do not match.
 */
boolean hal260VerifySpiProtocolVersion(void);

/**
 * @brief Transmits the SPI Status Command and checks the
 * response against a literal value to verify the SPI Protocol is active.
 *  
 * @return TRUE if the SPI Protocol is active. FALSE if the SPI Protocol is
 * not active.
 */
boolean hal260VerifySpiProtocolActive(void);

//@}

/**
 * @name SPI Protocol Test Function
 */
//@{


/**
 * @brief A collection of code for use in testing pieces of the SPI
 * Protocol.  
 * 
 * The tests available here undocumented and unused by a formal
 * application.  They can be called from any test application or used as
 * inspiration or a template for further testing.  To invoke a test, simply
 * call spipTest() with the desired parameters. (For example, this function
 * can be accessed through a user driven interface to provide a convenient
 * method for invoking a test or set of tests.)
 *  
 * @param test  The number provided here invokes a specific piece of the test
 * code.  For example, '1' loads the Command buffer with the SPI Protocol
 * Version Commmand and '0' initiates a transaction with whatever data is in
 * buffer.
 *
 * @param params  The parameters passed into a given test.  For example, test
 * 'A' simply write a byte on the SPI.  The byte written is provided by params.
 */
void spipTest(int16u test, int16u params);

//@}

#endif // __SPI_PROTOCOL_H__

/** @} END addtogroup */

