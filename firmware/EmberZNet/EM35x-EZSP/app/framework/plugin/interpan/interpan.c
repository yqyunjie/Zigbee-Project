// *******************************************************************
// * interpan.c
// *
// * Generic code related to the receipt and processing of interpan
// * messages.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/util.h"
#include "interpan.h"
#include "interpan-callback.h"

//------------------------------------------------------------------------------
// Globals

// MAC frame control
// Bits: 
// | 0-2   |   3      |    4    |  5  |  6    |   7-9    | 10-11 |  12-13   | 14-15 |
// | Frame | Security |  Frame  | Ack | Intra | Reserved | Dest. | Reserved | Src   |
// | Type  | Enabled  | Pending | Req | PAN   |          | Addr. |          | Adrr. |
// |       |          |         |     |       |          | Mode  |          | Mode  |

// Frame Type
//   000       = Beacon
//   001       = Data
//   010       = Acknwoledgement
//   011       = MAC Command
//   100 - 111 = Reserved

// Addressing Mode
//   00 - PAN ID and address field are not present
//   01 - Reserved
//   10 - Address field contains a 16-bit short address
//   11 - Address field contains a 64-bit extended address

#define MAC_FRAME_TYPE_DATA           0x0001

#define MAC_SOURCE_ADDRESS_MODE_SHORT 0x8000
#define MAC_SOURCE_ADDRESS_MODE_LONG  0xC000

#define MAC_DEST_ADDRESS_MODE_SHORT   0x0800
#define MAC_DEST_ADDRESS_MODE_LONG    0x0C00

// The two possible incoming MAC frame controls.
// Using short source address is not allowed.
#define SHORT_DEST_FRAME_CONTROL (MAC_FRAME_TYPE_DATA \
                                  | MAC_DEST_ADDRESS_MODE_SHORT \
                                  | MAC_SOURCE_ADDRESS_MODE_LONG)
#define LONG_DEST_FRAME_CONTROL  (MAC_FRAME_TYPE_DATA \
                                  | MAC_DEST_ADDRESS_MODE_LONG \
                                  | MAC_SOURCE_ADDRESS_MODE_LONG)

#define MAC_ACK_REQUIRED              0x0020

#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_APS_ENCRYPTED_MESSAGES)
  #define APS_ENCRYPTION_ALLOWED 1
#else
  #define APS_ENCRYPTION_ALLOWED 0
#endif

#define PRIMARY_ENDPOINT   0
#define ALL_ENDPOINTS      1
#define SPECIFIED_ENDPOINT 2

#if defined(EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO) 
  #if EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO == PRIMARY_ENDPOINT 
    #define ENDPOINT emberAfPrimaryEndpoint()

  #elif EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO == ALL_ENDPOINTS
    #define ENDPOINT EMBER_BROADCAST_ENDPOINT

  #elif EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO == SPECIFIED_ENDPOINT
    #define ENDPOINT EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO_SPECIFIED_ENDPOINT_VALUE

  #else
    #error Unknown value for interpan plugin option: EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO
  #endif
#else
  #error Invalid Interpan plugin Config: EMBER_AF_PLUGIN_INTERPAN_DELIVER_TO not defined.
#endif

//------------------------------------------------------------------------------
// Forward Declarations

static boolean isMessageAllowed(EmberAfInterpanHeader *headerData,
                                int8u messageLength,
                                int8u *messageContents);

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  static void printMessage(EmberAfInterpanHeader *headerData);
#else
  #define printMessage(x)
#endif

#if !defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_APS_ENCRYPTED_MESSAGES)
  #define handleApsSecurity(...) (EMBER_LIBRARY_NOT_PRESENT)
#endif


#if !defined EMBER_AF_PLUGIN_INTERPAN_CUSTOM_FILTER
  #define EMBER_AF_PLUGIN_INTERPAN_CUSTOM_FILTER
#endif

//------------------------------------------------------------------------------

// Inter-PAN messages are a security risk because they are typically not
// encrypted and yet we must accept them.  Therefore, we are very strict about
// what we allow.  Listed below are ZCL messages that are allowed, including
// the profile, cluster, direction, and command id.  Any inter-PAN messages not
// matching one of these are dropped.
static const EmberAfAllowedInterPanMessage messages[] = {

  // A custom filter can be added using this #define.
  // It should contain one or more 'EmberAfAllowedInterPanMessage' const structs
  EMBER_AF_PLUGIN_INTERPAN_CUSTOM_FILTER

#if defined (EMBER_AF_PLUGIN_INTERPAN_ALLOW_REQUIRED_SMART_ENERGY_MESSAGES)

#ifdef ZCL_USING_PRICE_CLUSTER_SERVER
  {
    SE_PROFILE_ID,
    ZCL_PRICE_CLUSTER_ID,
    ZCL_GET_CURRENT_PRICE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_CLIENT_TO_SERVER,
  },
  {
    SE_PROFILE_ID,
    ZCL_PRICE_CLUSTER_ID,
    ZCL_GET_SCHEDULED_PRICES_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_CLIENT_TO_SERVER,
  },
#endif

#endif // EMBER_AF_PLUGIN_INTERPAN_ALLOW_REQUIRED_SMART_ENERGY_MESSAGES

#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_SMART_ENERGY_RESPONSE_MESSAGES)

#ifdef ZCL_USING_PRICE_CLUSTER_CLIENT
  {
    SE_PROFILE_ID,
    ZCL_PRICE_CLUSTER_ID,
    ZCL_PUBLISH_PRICE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_SERVER_TO_CLIENT,
  },
#endif
#ifdef ZCL_USING_MESSAGING_CLUSTER_CLIENT
  {
    SE_PROFILE_ID,
    ZCL_MESSAGING_CLUSTER_ID,
    ZCL_DISPLAY_MESSAGE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_SERVER_TO_CLIENT,
  },
  {
    SE_PROFILE_ID,
    ZCL_MESSAGING_CLUSTER_ID,
    ZCL_CANCEL_MESSAGE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_SERVER_TO_CLIENT,
  },
#endif

#endif // EMBER_AF_PLUGIN_INTERPAN_ALLOW_SMART_ENERGY_RESPONSE_MESSAGES


#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_KEY_ESTABLISHMENT)
  // Since client and server share the same command ID's,
  // we can get away with only listing the request messages.
  {
    SE_PROFILE_ID,
    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
    ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_BOTH,
  },
  {
    SE_PROFILE_ID,
    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
    ZCL_EPHEMERAL_DATA_RESPONSE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_BOTH,
  },
  {
    SE_PROFILE_ID,
    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
    ZCL_CONFIRM_KEY_DATA_RESPONSE_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_BOTH,
  },
  {
    SE_PROFILE_ID,
    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
    ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID,
    EMBER_AF_INTERPAN_DIRECTION_BOTH,
  },
#endif // defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_KEY_ESTABLISHMENT)

  {0xFFFF}, // terminator
};

//#define ENCRYPT_DEBUG

#if !defined(ENCRYPT_DEBUG)
  #define printData(...) 
#endif

//------------------------------------------------------------------------------

#if defined(ENCRYPT_DEBUG)

static void printData(PGM_P name, int8u* message, int8u length)
{
  int8u i;
  emberAfAppPrint("\n%p: ", name);
  for (i = 0; i < length; i++) {
    emberAfAppPrint("%X ", message[i]);
    emberAfAppFlush();
  }
  emberAfAppPrintln("");
  emberAfAppFlush();
}

#endif

static int8u* pushInt16u(int8u *finger, int16u value)
{
  *finger++ = LOW_BYTE(value);
  *finger++ = HIGH_BYTE(value);
  return finger;
}

static int8u* pushEui64(int8u *finger, int8u *value)
{
  MEMCOPY(finger, value, 8);
  return finger + 8;
}

#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_APS_ENCRYPTED_MESSAGES)

static EmberStatus handleApsSecurity(boolean encrypt,
                                     int8u* apsFrame,
                                     int8u apsHeaderLength,
                                     int8u* messageLength,
                                     int8u maxLengthForEncryption,
                                     EmberAfInterpanHeader* header)
{
  int8u keyIndex;
  EmberStatus status;

  if (!(header->options & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS)) {
    // We won't encrypt/decrypt messages that don't have a long address.
    return EMBER_APS_ENCRYPTION_ERROR;
  }
  keyIndex = emberFindKeyTableEntry(header->longAddress, 
                                    TRUE);  // link key? (vs. master key)
  if (keyIndex == 0xFF) {
    return EMBER_APS_ENCRYPTION_ERROR;
  }

  if (encrypt
      && ((maxLengthForEncryption - *messageLength)
          < INTERPAN_APS_ENCRYPTION_OVERHEAD)) {
    return EMBER_APS_ENCRYPTION_ERROR;
  }

  apsFrame[0] |= INTERPAN_APS_FRAME_SECURITY;

  status = emAfInterpanApsCryptMessage(encrypt,
                                       apsFrame,
                                       messageLength,
                                       apsHeaderLength,
                                       header->longAddress);
  return status;
}
#endif


static EmberStatus makeInterPanMessage(EmberAfInterpanHeader *headerData,
                                       int8u *message,
                                       int8u maxLength,
                                       int8u *payload,
                                       int8u *payloadLength,
                                       int8u *returnLength)
{
  int8u *finger = message;
  boolean haveLongAddress = (headerData->options
                             & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS);
  int16u macFrameControl = (haveLongAddress
                            ? LONG_DEST_FRAME_CONTROL
                            : SHORT_DEST_FRAME_CONTROL);
  
  EmberEUI64 eui;
  int8u* apsFrame;
  int8u apsHeaderLength;

  if (maxLength < (MAX_INTER_PAN_HEADER_SIZE + *payloadLength)) {
    emberAfAppPrint("Error: Inter-pan message too big (%d > %d)",
                    MAX_INTER_PAN_HEADER_SIZE + *payloadLength,
                    maxLength);
    return EMBER_MESSAGE_TOO_LONG;
  }

  if (headerData->messageType == EMBER_AF_INTER_PAN_UNICAST) {
    macFrameControl |= MAC_ACK_REQUIRED;
  }

  emberAfGetEui64(eui);

  finger = pushInt16u(finger, macFrameControl);
  finger++;  // Skip Sequence number, stack sets the sequence number.

  finger = pushInt16u(finger, headerData->panId);
  if (headerData->options & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS) {
    finger = pushEui64(finger, headerData->longAddress);
  } else {
    finger = pushInt16u(finger, headerData->shortAddress);
  }

  finger = pushInt16u(finger, emberAfGetPanId());
  finger = pushEui64(finger, eui);

  finger = pushInt16u(finger, STUB_NWK_FRAME_CONTROL);

  apsFrame = finger;
  *finger++ = (headerData->messageType
               | INTERPAN_APS_FRAME_TYPE);

  if (headerData->messageType == EMBER_AF_INTER_PAN_MULTICAST) {
    finger = pushInt16u(finger, headerData->groupId);
  }

  finger = pushInt16u(finger, headerData->clusterId);
  finger = pushInt16u(finger, headerData->profileId);

  apsHeaderLength = finger - apsFrame;

  MEMCOPY(finger, payload, *payloadLength);
  finger += *payloadLength;

  if (headerData->options & EMBER_AF_INTERPAN_OPTION_APS_ENCRYPT) {
    EmberStatus status;
    int8u apsEncryptLength;
    if (!APS_ENCRYPTION_ALLOWED) {
      return EMBER_SECURITY_CONFIGURATION_INVALID;
    }
    apsEncryptLength = finger - apsFrame;
    
    printData("Before Encryption", apsFrame, apsEncryptLength);

    status = handleApsSecurity(TRUE,  // encrypt?
                               apsFrame,
                               apsHeaderLength,
                               &apsEncryptLength,
                               maxLength - (int8u)(apsFrame - message),
                               headerData);
    if (status != EMBER_SUCCESS) {
      return status;
    }

    printData("After Encryption ", apsFrame, apsEncryptLength);

    finger = apsFrame + apsEncryptLength;
  }

  *returnLength = finger - message;

  return EMBER_SUCCESS;
}

static int8u parseInterpanMessage(int8u *message,
                                  int8u *messageLength,
                                  EmberAfInterpanHeader *headerData)
{
  int8u *finger = message;
  int16u macFrameControl;
  int8u remainingLength;
  int8u apsFrameControl;
  int8u apsHeaderIndex;
  int8u apsHeaderLength;

  MEMSET(headerData, 0, sizeof(EmberAfInterpanHeader));

  // We rely on the stack to insure that the MAC frame is formatted
  // correctly and that the length is at least long enough
  // to contain that frame.

  macFrameControl = HIGH_LOW_TO_INT(finger[1], finger[0]);

  if (macFrameControl == LONG_DEST_FRAME_CONTROL) {
    // control, sequence, dest PAN ID, long dest
    finger += 2 + 1 + 2 + 8;      
  }  else if (macFrameControl == SHORT_DEST_FRAME_CONTROL) {
    // control, sequence, dest PAN ID, short dest
    finger += 2 + 1 + 2 + 2;      
  } else {
    return 0;
  }

  // Source PAN ID
  headerData->panId = HIGH_LOW_TO_INT(finger[1], finger[0]);
  finger += 2;

  // It is expected that the long Source Address is always present and
  // that the stack MAC filters insured that to be the case.
  headerData->options |= EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS;
  MEMCOPY(headerData->longAddress, finger, 8);
  finger += 8;

  // Now that we know the correct MAC length, verify the interpan
  // frame is the correct length.

  remainingLength = *messageLength - (int8u)(finger - message);

  if (remainingLength < (STUB_NWK_SIZE + MIN_STUB_APS_SIZE)) {
    return 0;
  }

  if (HIGH_LOW_TO_INT(finger[1], finger[0]) != STUB_NWK_FRAME_CONTROL) {
    return 0;
  }
  finger += 2;
  apsHeaderIndex = (finger - message);
  remainingLength -= 2;

  apsFrameControl = (*finger++);

  if ((apsFrameControl & ~(INTERPAN_APS_FRAME_DELIVERY_MODE_MASK)
       & ~INTERPAN_APS_FRAME_SECURITY)
      != INTERPAN_APS_FRAME_CONTROL_NO_DELIVERY_MODE) {
    emberAfAppPrintln("%pBad APS frame control 0x%X",
                      "ERR: Inter-PAN ",
                      apsFrameControl);
    return 0;
  }
  headerData->messageType = (apsFrameControl
                             & INTERPAN_APS_FRAME_DELIVERY_MODE_MASK);

  switch (headerData->messageType) {
  case EMBER_AF_INTER_PAN_UNICAST:
  case EMBER_AF_INTER_PAN_BROADCAST:
    // Broadcast and unicast have the same size messages
    if (remainingLength < INTERPAN_APS_UNICAST_SIZE) {
      return 0;
    }
    break;
  case EMBER_AF_INTER_PAN_MULTICAST:
    if (remainingLength < INTERPAN_APS_MULTICAST_SIZE) {
      return 0;
    }
    headerData->groupId = HIGH_LOW_TO_INT(finger[1], finger[0]);
    finger += 2;
    break;
  default:
    emberAfAppPrintln("%pBad Delivery Mode 0x%X",
                      "ERR: Inter-PAN ",
                      headerData->messageType);
    return 0;
  }
  
  headerData->clusterId = HIGH_LOW_TO_INT(finger[1], finger[0]);
  finger += 2;
  headerData->profileId = HIGH_LOW_TO_INT(finger[1], finger[0]);
  finger += 2;

  if (apsFrameControl & INTERPAN_APS_FRAME_SECURITY) {
    EmberStatus status;
    int8u apsEncryptLength = *messageLength - apsHeaderIndex;
    int8u apsHeaderLength = (int8u)(finger - message) - apsHeaderIndex;
    headerData->options |= EMBER_AF_INTERPAN_OPTION_APS_ENCRYPT;

    printData("Before Decryption", 
              message + apsHeaderIndex, 
              apsEncryptLength);

    status = handleApsSecurity(FALSE,   // encrypt?
                               message + apsHeaderIndex,
                               apsHeaderLength,
                               &apsEncryptLength,
                               0,       // maxLengthForEncryption (ignored)
                               headerData);
    
    if (status != EMBER_SUCCESS) {
      emberAfAppPrintln("%pAPS decryption failed (0x%X).",
                        "ERR: Inter-PAN ",
                        status);
      return 0;
    }

    printData("After Decryption ", 
              message + apsHeaderIndex, 
              apsEncryptLength);

    *messageLength = apsHeaderIndex + apsEncryptLength;
  }

  return (finger - message);
}

boolean emAfPluginInterpanProcessMessage(int8u messageLength,
                                         int8u *messageContents)
{
  EmberApsFrame apsFrame;
  EmberIncomingMessageType type;
  boolean loopback;
  EmberAfInterpanHeader headerData;

  int8u payloadOffset = parseInterpanMessage(messageContents,
                                             &messageLength,
                                             &headerData);
  if (payloadOffset == 0) {
    return FALSE;
  }
  printMessage(&headerData);

  messageContents += payloadOffset;
  messageLength -= payloadOffset;

  if (emberAfPluginInterpanPreMessageReceivedCallback(&headerData,
                                                      messageLength,
                                                      messageContents)) {
    return TRUE;
  }

  if (!isMessageAllowed(&headerData, messageLength, messageContents)) {
    return FALSE;
  }

  if (headerData.options & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS) {
    headerData.shortAddress = EMBER_NULL_NODE_ID;
    loopback = emberIsLocalEui64(headerData.longAddress);
  } else {
    loopback = (emberAfGetNodeId() == headerData.shortAddress);
  }

  apsFrame.profileId = headerData.profileId;
  apsFrame.clusterId = headerData.clusterId;
  apsFrame.sourceEndpoint = 1;   // arbitrary since not sent over-the-air
  apsFrame.destinationEndpoint = ENDPOINT;
  apsFrame.options = (EMBER_APS_OPTION_NONE
                      | ((headerData.options
                         & EMBER_AF_INTERPAN_OPTION_APS_ENCRYPT)
                         ? EMBER_APS_OPTION_ENCRYPTION
                         : 0));
  apsFrame.groupId = headerData.groupId;
  apsFrame.sequence = 0x00; // unknown sequence number

  switch (headerData.messageType) {
  case EMBER_AF_INTER_PAN_UNICAST:
    type = EMBER_INCOMING_UNICAST;
    break;
  case EMBER_AF_INTER_PAN_BROADCAST:
    type = (loopback
            ? EMBER_INCOMING_BROADCAST_LOOPBACK
            : EMBER_INCOMING_BROADCAST);
    break;
  case EMBER_AF_INTER_PAN_MULTICAST:
    type = (loopback
            ? EMBER_INCOMING_MULTICAST_LOOPBACK
            : EMBER_INCOMING_MULTICAST);
    break;
  default:
    return FALSE;
  }

  return emberAfProcessMessage(&apsFrame,
                               type,
                               messageContents,
                               messageLength,
                               headerData.shortAddress,
                               &headerData);
}

static boolean isMessageAllowed(EmberAfInterpanHeader *headerData,
                                int8u messageLength,
                                int8u *messageContents)
{
  int8u incomingMessageOptions = 0;
  int8u commandId;
  int8u i;

  if (messageLength < EMBER_AF_ZCL_OVERHEAD) {
    emberAfAppPrintln("%pmessage too short (%d < %d)!", 
                      "ERR: Inter-PAN ",
                      messageLength,
                      EMBER_AF_ZCL_OVERHEAD);
    return FALSE;
  }
  
  if (headerData->options & EMBER_AF_INTERPAN_OPTION_APS_ENCRYPT) {
    return APS_ENCRYPTION_ALLOWED;
  }

  // Only the first bit is used for ZCL Frame type
  if (messageContents[0] & BIT(1)) {
    emberAfAppPrintln("%pUnsupported ZCL frame type.",
                      "ERR: Inter-PAN ");
    return FALSE;
  }

  if (messageContents[0] & ZCL_MANUFACTURER_SPECIFIC_MASK) {
    incomingMessageOptions |= EMBER_AF_INTERPAN_MANUFACTURER_SPECIFIC;
  }
  if ((messageContents[0] & ZCL_CLUSTER_SPECIFIC_COMMAND)
      == ZCL_PROFILE_WIDE_COMMAND) {
    incomingMessageOptions |= EMBER_AF_INTERPAN_GLOBAL_COMMAND;
  }

  incomingMessageOptions |= ((messageContents[0] & ZCL_FRAME_CONTROL_DIRECTION_MASK)
                             ? EMBER_AF_INTERPAN_DIRECTION_SERVER_TO_CLIENT
                             : EMBER_AF_INTERPAN_DIRECTION_CLIENT_TO_SERVER);
  if (messageContents[0] & ZCL_MANUFACTURER_SPECIFIC_MASK) {
    if (messageLength < EMBER_AF_ZCL_MANUFACTURER_SPECIFIC_OVERHEAD) {
      emberAfAppPrintln("%pmessage too short!", "ERR: Inter-PAN ");
      return FALSE;
    }
    commandId = messageContents[4];
  } else {
    commandId = messageContents[2];
  }

  i = 0;
  while (messages[i].profileId != 0xFFFF) {
    int8u bothDirectionsAllowedMask = 
      ((messages[i].options
        & EMBER_AF_INTERPAN_DIRECTION_BOTH)
       == EMBER_AF_INTERPAN_DIRECTION_BOTH
       ? EMBER_AF_INTERPAN_DIRECTION_BOTH
       : 0);
                                   
    if (messages[i].profileId == headerData->profileId
        && messages[i].clusterId == headerData->clusterId
        && messages[i].commandId == commandId
        && (messages[i].options 
            == (incomingMessageOptions
                | bothDirectionsAllowedMask))) {
      return TRUE;
    }
    i++;
  }

  emberAfAppPrintln("%pprofile 0x%2x, cluster 0x%2x, command 0x%x not permitted",
                    "ERR: Inter-PAN ",
                    headerData->profileId,
                    headerData->clusterId,
                    commandId);
  return FALSE;
}

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)

static void printMessage(EmberAfInterpanHeader *headerData)
{
  emberAfAppPrint("RX inter PAN message(");
  if (headerData->messageType == EMBER_AF_INTER_PAN_UNICAST) {
    emberAfAppPrint("uni");
  } else if (headerData->messageType == EMBER_AF_INTER_PAN_BROADCAST) {
    emberAfAppPrint("broad");
  } else if (headerData->messageType == EMBER_AF_INTER_PAN_MULTICAST) {
    emberAfAppPrint("multi");
  }
  emberAfAppPrintln("cast):");
  emberAfAppFlush();
  emberAfAppPrintln("  src pan id: 0x%2x", headerData->panId);
  if (headerData->options & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS) {
    emberAfAppPrint("  src long id: ");
    emberAfPrintBigEndianEui64(headerData->longAddress);
    emberAfAppPrintln("");
  } else {
    emberAfAppPrintln("  src short id: 0x%2x", headerData->shortAddress);
  }
  emberAfAppFlush();
  emberAfAppPrintln("  profile id: 0x%2x", headerData->profileId);
  emberAfAppPrint("  cluster id: 0x%2x ", headerData->clusterId);
  emberAfAppDebugExec(emberAfDecodeAndPrintCluster(headerData->clusterId));
  emberAfAppPrintln("");
  if (headerData->messageType == EMBER_AF_INTER_PAN_MULTICAST) {
    emberAfAppPrintln("  group id: 0x%2x", headerData->groupId);
  }
  emberAfAppFlush();
}

#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)


EmberStatus emberAfInterpanSendMessageCallback(EmberAfInterpanHeader* header,
                                               int16u messageLength,
                                               int8u* messageBytes)
{
  EmberStatus status;
  int8u message[EMBER_AF_MAXIMUM_INTERPAN_LENGTH];
  int8u length;
  int8u payloadLength = (int8u)messageLength;

  status = makeInterPanMessage(header,
                               message,
                               EMBER_AF_MAXIMUM_INTERPAN_LENGTH,
                               messageBytes,
                               &payloadLength,
                               &length);
  if (status != EMBER_SUCCESS) {
    return status;
  }
  status = emAfPluginInterpanSendRawMessage(length, message);

  emberAfAppPrint("T%4x:Inter-PAN TX (%d) [", 
                  emberAfGetCurrentTime(), 
                  messageLength);
  emberAfAppPrintBuffer(messageBytes, messageLength, TRUE);
  emberAfAppPrintln("], 0x%x", status);
  emberAfAppFlush();

  return status;
}
