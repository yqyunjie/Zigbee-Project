/**
 * @file bootload-ezsp-utils.h
 * @brief Utilities used for performing stand-alone bootloading over EZSP.
 * See @ref util_bootload for documentation.
 *
 * <!--Copyright 2006-2007 by Ember Corporation. All rights reserved.    *80*-->
 *******************************************************************/

/**
 * @addtogroup Standalone_bootload_EZSP
 * All functions and variables defined here can be used by applications.
 * See bootload-ezsp-utils.h for source code.
 *  @{
 */

// application timers are based on quarter second intervals, each
// quarter second is measured in millisecond ticks. This value defines
// the approximate number of millisecond ticks in a quarter second.
// Account for variations in system timers.
#ifdef AVR_ATMEGA_32
#define TICKS_PER_QUARTER_SECOND 225
#else
#define TICKS_PER_QUARTER_SECOND 250
#endif

// Node build info
extern int16u nodeBlVersion;
extern int8u nodePlat;
extern int8u nodeMicro;
extern int8u nodePhy;


// Both of these need to be correctly handled in the applications's
// ezspErrorHandler().
// ezsp error info
extern EzspStatus bootloadEzspLastError;
// If this is not EZSP_SUCCESS, the next call to ezspErrorHandler()
// will ignore this error.
extern EzspStatus ignoreNextEzspError;


// *******************************************************************
// Callback functions used by the bootload library.

/**@brief A callback function invoked by bootload-ezsp-utils when a
 * bootload launch request message is received. 
 *
 * The application may
 * choose whether or not to enter the bootloader by checking
 * the manufacturerId, hardwareTag, and sourceEui.  If the
 * application chooses to launch the bootloader, the bootloader
 * will launch after successful completion of the bootloader
 * launch authentication protocol.
 *
 * @param lqi  The link quality from the node that generated this bootload
 * launch request.
 *  
 * @param rssi  The energy level (in units of dBm) observed during the
 * reception.
 *  
 * @param manufacturerId  The manufacturer specification (vendor specific) of
 * the sending node.
 *  
 * @param hardwareTag  The hardware specification (vendor specific) of
 * the sending node.
 *  
 * @param sourceEui  The EUI64 of the sending node.
 *  
 * @return  TRUE if the application wishes to launch the bootloader,
 * FALSE if the application does not wish to launch the bootloader.
 */
boolean hostBootloadUtilLaunchRequestHandler(int8u lqi,
                                             int8s rssi,
                                             int16u manufacturerId,
                                             int8u *hardwareTag,
                                             EmberEUI64 sourceEui);

/** @brief A callback function invoked by bootload-ezsp-utils when a
 * bootload query response message is received. 
 * 
 * This is particularly
 * useful when the application needs to decide which node to bootload.
 * Several attributes of the responding node are provided to the application.
 * The application can use these attributes to decide whether to bootload or how
 * to bootload a given node.
 *
 * @param lqi  The link quality from the node that generated this bootload
 * query response.
 *  
 * @param rssi  The energy level (in units of dBm) observed during the
 * reception.
 *  
 * @param bootloaderActive  TRUE if the responding node is running the
 * bootloader, FALSE if not.
 *  
 * @param manufacturerId  The manufacturer specification (vendor specific) of
 * the responding node.
 *  
 * @param hardwareTag  The hardware specification (vendor specific) of
 * the responding node.
 *  
 * @param targetEui  The EUI64 of the responding node.
 *  
 * @param bootloaderCapabilities  If the lsb is 1, the bootloader on the
 * responding node supports encrypted bootloader message payloads.
 *  
 * @param platform  The type of platform of the responding node. 1 is avr-atmega,
 * 2 is xap2b.
 *  
 * @param micro  The type of microcontroller on the responding node.
 * Value depends on platform. 1 is the avr-atmega 64, 2 is the avr-atmega 128,
 * 1 is the xap2b em250.
 *  
 * @param phy  The type of phy of the responding node.  1 is em2420, 2 is em250.
 *  
 * @param blVersion  The version of standalone bootloader of the
 * responding node. This is a 2 byte field. The high byte is the
 * version and the low byte is the build. A value of 0xFFFF means
 * unknown. For example, a version field of 0x1234 is version 1.2, build 34.
 */
void hostBootloadUtilQueryResponseHandler(int8u lqi,
                                          int8s rssi,
                                          boolean bootloaderActive,
                                          int16u manufacturerId,
                                          int8u *hardwareTag,
                                          EmberEUI64 targetEui,
                                          int8u bootloaderCapabilities,
                                          int8u platform,
                                          int8u micro,
                                          int8u phy,
                                          int16u blVersion);

/**@brief A callback function invoked by bootload-ezsp-utils when a NCP
 * has finished being bootloaded.
 *
 * The application can handle this as simply as calling on halReboot() or
 * as complex as needed.
 *
 */
void hostBootloadReinitHandler(void);


// Suppport routines in the bootloader utils library

/**@brief A function to compare EUI64s
 *
 * Compare two EUI64s.
 *
 * @param sourceEui  The EUI64 of the sending node.
 *  
 * @param targetEui  The EUI64 of the responding node.
 *  
 * @return  TRUE if the EUI64s are the same.
 * FALSE if the EUI64s are different.
 */
boolean isTheSameEui64(EmberEUI64 sourceEui, EmberEUI64 targetEui);

/**@brief A function to display an EUI64.
 *
 * Display an EUI64 in little endian format.
 *
 * @param port  The serial port to use.  0 for Mega128 port. 
 * 0 or 1 for Linux ports.
 *  
 * @param eui64  The EUI64 to display.
 *  
 */
void printLittleEndianEui64(int8u port, EmberEUI64 eui64);

/**@brief A function to display an EUI64.
 *
 * Display an EUI64 in big endian format.
 *
 * @param port  The serial port to use.  0 for Mega128 port. 
 * 0 or 1 for Linux ports.
 *  
 * @param eui64  The EUI64 to display.
 *  
 */
void printBigEndianEui64(int8u port, EmberEUI64 eui64);

/**@brief A function to simular to emberSerialPrintf().
 *
 * Output to local ports.
 *
 * @param port  The serial port to use.  0 for Mega128 port. 
 * 0 or 1 for Linux ports.
 *  
 * @param formatString The string to print.
 *  
 * @param ...          Format specifiers. 
 *   
 * @return One of the following (see the Main Page):
 * - EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - EMBER_SUCCESS.
 */
EmberStatus debugPrintf(int8u port, PGM_P formatString, ...);

/** @} END addtogroup */



