/**
 * @file bootload-utils.h
 * @brief Utilities used for performing stand-alone bootloading.
 * See @ref util_bootload for documentation.
 *
 * <!--Copyright 2006-2007 by Ember Corporation. All rights reserved.    *80*-->
 *******************************************************************/

/**
 * @addtogroup util_bootload
 *  For a thorough discussion of bootloading, see the Bootloading chapter of the 
 * <i>EmberZNet Application Developer’s Guide</i>.
 * There are three forms of the bootloading API.
 */
 
/**
 * @addtogroup Standalone_bootload
 * All functions and variables defined here can be used by applications.
 * See bootload-utils.h for source code.
 *
 * Applications can use this stand-alone bootload library to:
 * -# Load a new (application) image on itself via serial bootload through
 *    uart port 1 using the xmodem protocol.
 * -# Load a new image on a remote node over-the-air (OTA) from a host (PC),
 *    also known as a passthru bootload.
 * -# Load an existing image of a node onto another node in the network
 *    over-the-air, also known as a clone bootload.
 * -# Recover a node that failed during the bootloading process,
 *    also known as a recovery bootload.
 *
 * Note from the diagrams below that with over-the-air bootloading the source node
 * (node transmitting bootload packets) and the target node (node being loaded
 * with a new image) need to be one hop away because bootload packets are
 * IEEE 802.15.4 packets.
 *
 * In case of recovery, the source (recovery) node does not need
 * to be part of the network since all recovery packets are 802.15.4 packets.
 *
 * <b>A diagram for typical serial bootloading:</b>
 *
 * [host pc] --(RS232 or Ethernet/IP network)-- {uart1 or port 4901}[node]
 *
 *
 * <b>A diagram for typical passthru bootloading:</b>
 *
 * [host pc] --(RS232 or Ethernet)-- [source node]--(OTA)--[target node]
 *
 *
 * <b>A diagram for typical clone bootloading:</b>
 *
 * [source node] --(OTA)--[target node]
 *
 *
 * <b>A diagram for typical recovery bootloading:</b>
 *
 * [source node] --(OTA)--[target node]
 *
 *
 * @note Applications that use the bootload utilities need to 
 * <CODE> \#define EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS </CODE>
 * within their <CODE> CONFIGURATION_HEADER </CODE>.
 *
 * @{
 */
 
// *******************************************************************
// Literals that are needed by the application.

/** @name Authentication Challenge and Response
 * The authentication challenge and response must be the same size.
 * The size is chosen to be evenly divisible by the size of a 128-bit AES block.
 *@{
 */
#define BOOTLOAD_AUTH_COMMON_SIZE    16
#define BOOTLOAD_AUTH_CHALLENGE_SIZE BOOTLOAD_AUTH_COMMON_SIZE
#define BOOTLOAD_AUTH_RESPONSE_SIZE  BOOTLOAD_AUTH_COMMON_SIZE

/**
 *@}  // End set of defines
 */

/** @brief Size of hardware tag which is an array of int8u.
 */
#define BOOTLOAD_HARDWARE_TAG_SIZE 16

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief Bootload modes supported by the bootload utility library.
 */
enum bootloadMode
#else
typedef int8u bootloadMode;
enum
#endif
{
  /** Used when we are not currently doing any bootloading.*/
  BOOTLOAD_MODE_NONE,
  /** Used when doing normal and recovery passthru bootload. */
  BOOTLOAD_MODE_PASSTHRU,
  /** Used when doing normal and recovery clone bootload.*/
  BOOTLOAD_MODE_CLONE,
  /** Used when doing normal and recovery v1 passthru bootload.
   * Note: this is a forced V1 bootload; not normally used by the application.
   */
  BOOTLOAD_MODE_V1_PASSTHRU,
};

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief A bootload state is a value that an application can check to see if
 * bootloading is in progress.
 *
 * This is necessary because we want the application to be aware that
 * bootloading is going on and it needs to limit its activities.  For example,
 * when passthru bootloading is going on, do not print anything to a serial port
 * because it may violate the XModem protocol. Also, try to limit radio activities
 * to a minimum to avoid any interruptions to bootload progress.
 * Used in a bootload state machine.
 */
enum bootloadState
#else
typedef int8u bootloadState;
enum
#endif
{
  BOOTLOAD_STATE_NORMAL,                  /*!< Start state */
  BOOTLOAD_STATE_QUERY,                   /*!< After send query message */
  BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE, /*!< Wait for authentication challenge */
  BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE,  /*!< Wait for authentication response */
  BOOTLOAD_STATE_DELAY_BEFORE_START,      /*!< Delay state before start new action */
  BOOTLOAD_STATE_START_UNICAST_BOOTLOAD,  /*!< After start unicast bootloading */
  BOOTLOAD_STATE_START_BROADCAST_BOOTLOAD,/*!< After start broadcast bootloading */
  BOOTLOAD_STATE_START_SENDING_IMAGE,     /*!< Need to start XMODEM code */
  BOOTLOAD_STATE_SENDING_IMAGE,           /*!< During sending OTA data messages */
  BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK,      /*!< Wait for OTA data ack */
  BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK,   /*!< Wait for OTA end transmission ack */
  BOOTLOAD_STATE_DONE                     /*!< Finish bootloading */
};


// *******************************************************************
// Public functions that are called by the application.

/** @brief Bootload library initialization.
 *
 *  The application needs to define
 *  the ports to be used for printing information and for a (passthru) bootload.
 *
 *  @note Generally it's a good idea to use different ports for the
 *  application and for bootloading because when doing passthru bootloading,
 *  we do not want to print any additional data that can cause an XModem
 *  transaction to fail.
 *
 * @param appPort  Port used for printing information.
 * @param bootloadPort  Port used for passthru bootloading.
 */
void bootloadUtilInit(int8u appPort, int8u bootloadPort);

/** @brief Start the bootload process on a remote node
 *  that is currently running stack/application. 
 *
 *  The source node sends a
 *  bootload request message to initiate the bootload authentication process.
 *  The source node then enters a state waiting for the target node to send an
 *  authentication challenge, which it will encrypt and send back as a response.
 *  MfgId and harwareTag information is sent over the air to the target node
 *  to verify whether to go into bootload mode.  The encryption key is saved on
 *  the source node for later authentication.  The mode indicates the
 *  bootload mode that the source will be using.
 *
 * @param targetEui  Node to be bootloaded.
 * @param mfgId  Manufacturer ID (vendor specific).
 * @param hardwareTag  Hardware ID, such as a board (vendor specific).
 * @param encryptKey  Key used in the authentication process.
 * @param mode  Bootload mode to be used such as passthru (0x01) or clone (0x02).
 * @return EMBER_SUCESS if successful, or EMBER_NO_BUFFERS, or EMBER_ERR_FATAL
 *  if the function was called too soon after a previous call to it.
 *
 */
EmberStatus bootloadUtilSendRequest(EmberEUI64 targetEui,
                                  int16u mfgId,
                                  int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                  int8u encryptKey[BOOTLOAD_AUTH_COMMON_SIZE],
                                  bootloadMode mode);

/** @brief A function to send query message to gather basic
 *  information about the node(s). 
 *
 *  There are two types of query messages: broadcast and unicast.
 *  Broadcast query is generally used to gather information regarding a 
 *  neighboring node, especially the eui64 of the node.
 *  Unicast query is used when we already know the eui64 of the target node
 *  that we needs information from.
 *
 * @param target  The node we want to gather information from. 
 *  If the value is NULL, that means we want to do a broadcast query.
 */
void bootloadUtilSendQuery(EmberEUI64 target);

/** @brief Start the bootload process on a remote node that is already running 
 *  in bootload mode. 
 *
 *  This is generally to recover a node that failed during bootload. 
 *  The failure can be caused by the 
 *  source node resetting, the network being too busy, a software reset, and so on.
 *  However, the failure is not caused by a target node losing power.
 *  After the failure, the node stays in bootload mode on the same
 *  (current) channel.
 *
 * @param target  remote node to be bootloaded.  If the value is NULL,
 *  that means we do not know the eui64 of the target node.  A broadcast
 *  (start bootload) packet is sent and the first node that replies will be
 *  bootloaded.
 * @param mode  bootload mode to be used, such as passthru (0x01) or clone (0x02).
 *
 */
void bootloadUtilStartBootload(EmberEUI64 target, bootloadMode mode);

/**@brief A function in the application's heartbeat or tick function 
 *  that contains basic bootloading state machine and also
 *  manages the bootload timer.
 *
 */
void bootloadUtilTick(void);


// *******************************************************************
// Callback functions used by the bootload library.

/**@brief A callback function invoked by bootload-utils when a
 * bootload request message is received. 
 *
 * The application may
 * choose whether or not to enter the bootloader by checking
 * the manufacturerId, hardwareTag, and sourceEui.  If the
 * application chooses to launch the bootloader, the bootloader
 * will launch after successful completion of the bootloader
 * launch authentication protocol.
 *
 * @param manufacturerId  The manufacturer specification (vendor specific) of
 * the sending node.
 * @param hardwareTag  The hardware specification (vendor specific) of
 * the sending node.
 * @param sourceEui  The EUI64 of the sending node.
 * @return  TRUE if the application wishes to launch the bootloader,
 * FALSE if the application does not wish to launch the bootloader.
 */
boolean bootloadUtilLaunchRequestHandler(int16u manufacturerId,
                                  int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                  EmberEUI64 sourceEui);

/** @brief A callback function invoked by bootload-utils when a
 * bootload query response message is received. 
 * 
 * This is particularly
 * useful when the application needs to decide which node to bootload.
 * Several attributes of the responding node are provided to the application.
 * The application can use these attributes to decide whether to bootload or how
 * to bootload a given node.
 *
 * @param bootloaderActive  TRUE if the responding node is running the
 * bootloader, FALSE if not.
 * @param manufacturerId  The manufacturer specification (vendor specific) of
 * the responding node.
 * @param hardwareTag  The hardware specification (vendor specific) of
 * the responding node.
 * @param targetEui  The EUI64 of the responding node.
 * @param bootloaderCapabilities  If the lsb is 1, the bootloader on the
 * responding node supports encrypted bootloader message payloads.
 * @param platform  The type of platform of the responding node. 1 is avr-atmega,
 * 2 is xap2b.
 * @param micro  The type of microcontroller on the responding node.
 * Value depends on platform. 1 is the avr-atmega 64, 2 is the avr-atmega 128,
 * 1 is the xap2b em250.
 * @param phy  The type of phy of the responding node.  1 is em2420, 2 is em250.
 * @param blVersion  The version of standalone bootloader of the
 * responding node. This is a 2 byte field. The high byte is the
 * version and the low byte is the build. A value of 0xFFFF means
 * unknown. For example, a version field of 0x1234 is version 1.2, build 34.
 */
void bootloadUtilQueryResponseHandler(boolean bootloaderActive,
                                      int16u manufacturerId,
                                      int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                      EmberEUI64 targetEui,
                                      int8u bootloaderCapabilities,
                                      int8u platform,
                                      int8u micro,
                                      int8u phy,
                                      int16u blVersion);

/** @brief A function called by a parent node to send an authentication
 * response message to the sleepy or mobile end-device target node. 
 *
 * The message is sent as a Just-In-Time (JIT) message, 
 * hence, the end-device target needs to poll for the message.
 *
 * The bootload utility library will call this function automatically
 * if bootloading the router node.
 *
 * @param target  The end-device target node being bootloaded.
 *
 */
void bootloadUtilSendAuthResponse(EmberEUI64 target);


// *******************************************************************
// Bootload state variables

/**@name Bootload State Variables
 * Used to check whether a bootloading process is currently happening.
 *@{
 */
extern bootloadState blState;
#define IS_BOOTLOADING ((blState != BOOTLOAD_STATE_NORMAL) && \
                        (blState != BOOTLOAD_STATE_DONE))
/**
 *@}  // End Bootload State Variables
 */

/** @} END addtogroup */






