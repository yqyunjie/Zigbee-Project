// File: common.h
// 
// Description: declarations for common app code.
//
// The common library is deprecated and will be removed in a future release.
//
// Author(s): Richard Kelsey, kelsey@ember.com
//
// Copyright 2004 by Ember Corporation. All rights reserved.                *80*

extern int8u serialPort;
extern SerialBaudRate serialBaudRate;

// For identifying ourselves over the serial port.
extern PGM_NO_CONST PGM_P applicationString;

// Boilerplate
void configureSerial(int8u port, SerialBaudRate rate);
void toggleBlinker(void);

#if !defined EZSP_HOST
  extern EmberEventControl blinkEvent;
#endif

void initialize(void);
void run(EmberEventData* events,
         void (* heartbeat)(void));

// Utilities
void printLittleEndianEui64(int8u port, EmberEUI64 eui64);
void printBigEndianEui64(int8u port, EmberEUI64 eui64);
#define printEui64(port, eui64) printBigEndianEui64((port), (eui64))
void printHexByteArray(int8u port, int8u *byteArray, int8u length);
#define printEncryptionKey(port, keyData) \
  printHexByteArray(port, keyData, EMBER_ENCRYPTION_KEY_SIZE)

int8u asciiHexToByteArray(int8u *bytesOut, int8u* asciiIn, int8u asciiCharLength);

void hexToEui64(int8u *contents, int8u length, EmberEUI64 returnEui64);
void hexToKeyData(int8u *contents, int8u length, EmberKeyData* returnKeyData);
int8u hexDigitValue(int8u digit);

// Host / Non-host specific implementations
boolean setSecurityLevel(int8u level);
boolean generateRandomBytes(int8u* result, int8u length);
boolean getNetworkParameters(EmberNodeType* nodeType, 
                             EmberNetworkParameters* networkParams);
void initializeEmberStack(void);
EmberStatus getOnlineNodeParameters(int8u* childCountReturn, 
                                    int8u* routerCountReturn,
                                    EmberNodeType* nodeTypeReturn,
                                    EmberNodeId* parentNodeIdReturn,
                                    EmberEUI64 parentEuiReturn,
                                    EmberNetworkParameters* networkParamReturn);
EmberStatus getOfflineNodeParameters(EmberNodeId *myNodeIdReturn,
                                     EmberNodeType *myNodeTypeReturn,
                                     int8u* stackProfileReturn);
void runEvents(EmberEventData* events);

#define generateRandomKey(result) \
  (EMBER_SUCCESS == emberGenerateRandomKey(result))

void getOrGenerateKey(int8u argumentIndex, EmberKeyData *key);

void createMulticastBinding(int8u index, int8u *multicastGroup, int8u endpoint);
boolean findEui64Binding(EmberEUI64 key, int8u *index);
void printCommandStatus(EmberStatus status, PGM_P good, PGM_P bad);
void printCommandStatusWithPrefix(EmberStatus status, 
                                  PGM_P prefix,
                                  PGM_P good,
                                  PGM_P bad);
void printOperationStatus(EmberStatus status, PGM_P operation);
void setCommissionParameters(void);
EmberStatus joinNetwork(void);


// Common commands
void helpCommand(void);
void statusCommand(void);
void stateCommand(void);
void rebootCommand(void);
void radioCommand(void);
void formNetworkCommand(void);
void setExtPanIdCommand(void);
void setJoinMethod(void);
void joinNetworkCommand(void);
void rejoinCommand(void);
void networkInitCommand(void);
void leaveNetworkCommand(void);
void addressRequestCommand(void);
void permitJoiningCommand(void);
void setSecurityCommand(void);
void setNetworkCommand(void);

void printCarriageReturn(void);
void printErrorMessage(PGM_P message);
void printErrorMessage2(PGM_P message, PGM_P message2);

#define COMMON_COMMANDS                                                 \
  emberCommandEntryAction("status",       statusCommand,          "",   \
                          "Print the node's current status"),           \
  emberCommandEntryAction("state",        stateCommand,           "",   \
                          "Print the node's current state"),            \
  emberCommandEntryAction("reboot",       rebootCommand,          "",   \
                          "Reboot the node"),                           \
                                                                        \
  /* Parameters: security bitmask, preconfigured key, network key, */   \
  /*   network key sequence */                                          \
  /* If security bitmask is 0x8000 then security is turned off, */      \
  /*   nothing else is set. */                                          \
  /* If security bitmask is 0xFFFF then security is turned on  */       \
  /*   without setting any other security settings. */                  \
  /* If security bitmask is anything else, security is turned on, */    \
  /*   the bitmask is set and so are the other values. */                \
  /*   (as appropriate for the bitmask) */                               \
  /*   The higher 16-bits of the bitmask are written into the */         \
  /*  extended security bitmask, while the lower 16-bits are written */  \
  /*  into the security bitmask. */                                      \
  emberCommandEntryAction("set_security", setSecurityCommand,     "wbbu",\
                          "Set the security parameters"),               \
                                                                        \
  /* channel, pan id, tx power */                                       \
  emberCommandEntryAction("form",         formNetworkCommand,     "uvs",\
                          "Form a network"),                            \
  /* channel, pan id, tx power, extended pan id */                       \
  emberCommandEntryAction("form_ext",     formNetworkCommand,     "uvsb",\
                          "Form a network with extended settings"),      \
                                                                         \
  /* extended pan id */                                                  \
  emberCommandEntryAction("set_ext_pan",  setExtPanIdCommand,     "b",   \
                          "Set extended PAN ID for forming/joining"),    \
                                                                \
  /* Set the join method: MAC Associate (0), */                 \
  /*   NWK Rejoin w/out security (1), */                        \
  /*   NWK Rejoin with Security (2) */                          \
  /*   NWK commission, i.e. join quietly (3) */                 \
  emberCommandEntryAction("join_method",  setJoinMethod,          "u" , \
                          "Set the method used for joining"),           \
                                                                \
  /* Set the Commissioning parameters: */                       \
  /*   coordinator boolean (1 byte) */                          \
  /*   nwkManagerId (2 bytes) */                                \
  /*   nwkUpdateId  (1 byte) */                                 \
  /*   channels  (lower 16-bits of 32-bit value) */             \
  /*   channels  (upper 16-bits of 32-bit value) */             \
  /*   trust center eui64 */                                    \
  emberCommandEntryAction("commission", setCommissionParameters, "uvuvvb" , \
                          "Set the commission parameters"),             \
                                                                \
  /* channel, pan id, tx power */                               \
  emberCommandEntryAction("join",         joinNetworkCommand,     "uvs", \
                          "Join a network as router"),                  \
  emberCommandEntryAction("join_end",     joinNetworkCommand,     "uvs", \
                          "Join a network as RxOnWhenIdle end  device"), \
  emberCommandEntryAction("join_sleepy",  joinNetworkCommand,     "uvs", \
                          "Join a network as a sleepy end device"),     \
  emberCommandEntryAction("join_mobile",  joinNetworkCommand,     "uvs", \
                          "Join a network as a mobile end device"),     \
                                                                \
  emberCommandEntryAction("nwk_init",     networkInitCommand,     "", \
                          "Execute emberNetworkInit()"),              \
                                                                      \
  emberCommandEntryAction("leave",        leaveNetworkCommand,    "", \
                          "Tell local node to Leave the network"),    \
                                                                \
  /* params: */                                                 \
  /*   - have current network key? (secure vs. insecure) */     \
  /*   - channel mask (0 = search current channel only) */      \
  emberCommandEntryAction("rejoin",      rejoinCommand,           "uw", \
                          "Execute a rejoin operation"),                \
                                                                \
  /* target network id, include kids */                         \
  emberCommandEntryAction("ieee_address", addressRequestCommand,  "vu", \
                          "Send a ZDO IEEE address request"),           \
                                                                \
  /* target EUI64, include kids */                              \
  emberCommandEntryAction("nwk_address",  addressRequestCommand,  "bu", \
                          "Send a ZDO NWK address request"),            \
                                                                \
  /* duration in seconds, 0 to prohibit, 0xff to allow */       \
  emberCommandEntryAction("permit_joins", permitJoiningCommand,   "u", \
                          "Change the MAC permit join flag"),          \
                                                                \
  /* help commands */                                           \
  emberCommandEntryAction("help", helpCommand, "",              \
                          "Print the list of commands"),        \
       															                            \
/* Parameter 1 -  Network index */                              \
  emberCommandEntryAction("set_network", setNetworkCommand, "u",  \
                          "Set the network index"),
