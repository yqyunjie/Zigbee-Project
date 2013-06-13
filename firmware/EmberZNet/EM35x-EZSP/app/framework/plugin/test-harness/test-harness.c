// *****************************************************************************
// * test-harness.c
// *
// *  Test harness code for validating the behavior of the key establishment
// *  cluster and modifying the normal behavior of App. Framework.  
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// this file contains all the common includes for clusters in the zcl-util
#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"
#include "app/util/serial/command-interpreter2.h"

#include "app/framework/plugin/key-establishment/key-establishment.h"
#include "test-harness.h"
#include "test-harness-cli.h"
#include "app/framework/plugin/price-server/price-server.h"
#include "app/framework/plugin/trust-center-keepalive/trust-center-keepalive.h"

#include "app/framework/plugin/ota-storage-simple-ram/ota-storage-ram.h"

#include "app/util/concentrator/concentrator.h"

#if !defined(EZSP_HOST)
  #include "stack/include/cbke-crypto-engine.h"
#endif

#include "app/framework/plugin/trust-center-nwk-key-update-periodic/trust-center-nwk-key-update-periodic.h"
#include "app/framework/plugin/trust-center-nwk-key-update-broadcast/trust-center-nwk-key-update-broadcast.h"
#include "app/framework/plugin/trust-center-nwk-key-update-unicast/trust-center-nwk-key-update-unicast.h"

#if !defined(EZSP_HOST)
  #if defined(EMBER_TEST) || defined(EMBER_STACK_TEST_HARNESS)
    #define STACK_TEST_HARNESS
  #endif
#endif

//------------------------------------------------------------------------------
// Globals

typedef int8u TestHarnessMode;

#define MODE_NORMAL           0
#define MODE_CERT_MANGLE      1
#define MODE_OUT_OF_SEQUENCE  2
#define MODE_NO_RESOURCES     3
#define MODE_TIMEOUT          4
#define MODE_DELAY_CBKE       5
#define MODE_DEFAULT_RESPONSE 6

static PGM_P modeText[] = {
  "Normal",
  "Cert Mangle",
  "Out of Sequence",
  "No Resources",
  "Cause Timeout",
  "Delay CBKE operation",
  "Default Response",
};

static TestHarnessMode testHarnessMode = MODE_NORMAL;
static int8u respondToCommandWithOutOfSequence = ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID;
static int8s certLengthMod = 0;

#define CERT_MANGLE_NONE    0
#define CERT_MANGLE_LENGTH  1
#define CERT_MANGLE_ISSUER  2
#define CERT_MANGLE_CORRUPT 3
#define CERT_MANGLE_SUBJECT 4

static PGM_P certMangleText[] = {
  "None",
  "Mangle Length",
  "Rewrite Issuer",
  "Corrupt Cert",
};

#if defined(EMBER_AF_HAS_SECURITY_PROFILE_SE_TEST)
  #define DEFAULT_POLICY TRUE
#else
  #define DEFAULT_POLICY FALSE
#endif
boolean emKeyEstablishmentPolicyAllowNewKeyEntries = DEFAULT_POLICY;
boolean emAfTestHarnessSupportForNewPriceFields = TRUE;

typedef int8u CertMangleType;

static CertMangleType certMangleType = CERT_MANGLE_NONE;

// Offset from start of ZCL header
//   - ZCL overhead (3 bytes)
//   - KE suite (2 bytes)
//   - Ephemeral Data Generate Time (1 byte)
//   - Confirm Key Generate Time (1 byte)
#define CERT_OFFSET_IN_INITIATE_KEY_ESTABLISHMENT_MESSAGE (3 + 2 + 1 + 1)

// Public Key Reconstruction Data (22 bytes)
// Subject (8 bytes)
#define SUBJECT_OFFSET_IN_CERT  22
#define ISSUER_OFFSET_IN_CERT (SUBJECT_OFFSET_IN_CERT + 8)

static int8u invalidEui64[] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

// Command List
//
//   test-harness status 
//     Prints the current mode of the test harness.

//   test-harness aps-sec-for-clust on <clusterID>
//   test-harness aps-sec-for-clust off
//     This turns on/off that the specified cluster
//     requires APS security. This does not override the normal
//     rules by the SE profile for security.  It simply supplements
//     the rules with one additional cluster that requires APS security.

//   test-harness registration <on|off>
//     Turns on/off automatic Smart Energy Registration after joining or 
//     reboot.  Default is on.

//   test-harness ke normal-mode
//     Sets normal mode for key establishment on the test harness, which 
//     suppresses any bad behavior and allows normal KE operation.

//   test-harness ke no-resources
//     The device will send a "Terminate Key Estabilshment Message (No resources)"
//     in response to any attempt to initiate KE with it.  As a client, it will send
//     a Terminate message after receiving the Initiate Key establishment response.

//   test-harness ke default-resp
//     The device will send a "Default Response (Failure)" in response to any attempt
//     to initiate KE with it.  As a client, it will send
//     a Default Response after receiving the Initiate Key establishment response.

//   test-harness out-of-sequence <id>
//     This command will send an out-of-sequence message during key establishment.
//     It will look for the local device to send the command with the passed 'ID'
//     and will send a different command ID.  So if the outgoing command is 
//     Initiate KE (ID 0) and the passed ID = 0, then it will send an Ephemeral
//     Data Command (ID 1) instead.

//   test-harness ke timeout
//     This command will induce a timeout by dropping the outgoing message
//     instead of sending it.  This will occur for every command, except
//     when the device is a KE client sending the first message.  In that case
//     it will induce a timeout on the second message (ephemeral data request).

//   test-harness ke delay-cbke <delay-time-seconds>   <advertised-delay-seconds>
//     This command causes the test harness to introduce artificial delays
//     to the ephemeral key and confirm key generation time.  It will delay
//     generation by 'delay-time-seconds' so it takes longer than normal.
//     Thus the ephemeral data request/response message will be delayed.
//     The 'advertised-delay-seconds' is the amount of time that the local
//     device tells the partner it will take them (in the Initiate KE message).  
//     If the delay-time-seconds <= advertised-delay-seconds, then key 
//     establishment should complete successfully but just be delayed.  If 
//     delay-time-seconds > advertised-delay-seconds then the partner device
//     should timeout the operation as the local device will take longer than
//     expected.  However the local device will still send the ephemeral data 
//     message in an attempt to continue key establishment.  It is expected 
//     that the partner will reject this attempt and send back a Terminate 
//     message.

//   test-harness ke cert-mangle length <signed-length>
//     This will modify the initiate key establishment message length by +/-
//     'signed-length' bytes.

//   test-harness ke cert-mangle issuer <new-issuer-value>
//     This will rewrite the issuer field of the certificate with the 
//     'new-issuer-value' when the initiate key establishment message
//     is sent out.

//   test-harness ke cert-mangle subject <new-subject-value>
//     This will rewrite the subject field (EUI64) of the certificate with the 
//     'new-subject-value' when the initiate key establishment message
//     is sent out.

//   test-harness ke cert-mangle corrupt <index>
//     This will rewrite the 'index' byte of the certificate field of the
//     initiate key establishment message to corrupt the certificate.
//     It will bit-flip the 'index' byte to change its value.

//   test-harness ke new-key-policy <boolean>
//     This will change the policy of whether the TC allows new link keys into 
//     its table.  Normally all devices are pre-configured in the TC with an install
//     code and so the TC must have prior knowledge of the device in order to allow
//     CBKE with it.  However when running in the SE_TEST security mode the TC uses
//     a global link key and so there is no prior knowledge.

//   test-harness price send-new-fields <boolean>
//     This will tell the price server whether or not it should send publish price
//     commands with the new SE 1.1 fields: Number of Block Threshholds 
//     and Price Control.  Defaults to TRUE.

//   test-harness tc-keepalive send
//      This command will cause the device to immediatley send a Trust Center Keepalive
//      message.

//   test-harness tc-keepalive stop
//      Turn off the keepalive state machine so it does not periodically send 
//      keepalive messages.

//   test-harness tc-keepalive start
//      Turn on the keepalive state machine so starts periodically sending 
//      keepalive messages.

//   test-harness ota image-mangle
//     This will mangle the image stored in the OTA Simple Storage RAM Driver.
//     Only available when the OTA Simple Storage RAM driver plugin is enabled.
//     This is really only useful in simulated testing.  For testing on
//     real hardware it is better to use a Linux box with a set of
//     images, one of which is corrupted.

//   test-harness key-update unicast
//     This command will set the key-update method to use unicast (as opposed to 
//     broadcast) to individually update devices with the new NWK key.

//   test-harness key-update broadcast
//     This command will set the key-update method to use broadcast (as opposed
//     to unicast) to update all devices at the same time with the new NWK key.

//   test-harness key-update now
//     Start the NWK key update process now.

//   test-harness concentrator start
//     Start periodic MTORR advertisements

//   test-harness concentrator stop
//     Stop periodic MTORR advertisements

//   test-harness stack limit-beacons on
//   test-harness stack limit-beacons off
//     Turns on/off a special mode of the stack where only the first beacon
//     request is responded to.  All subsequent beacon requests are ignored.
//     By default this mode is disabled, which means beacon requests are
//     answered normally.

//   test-harness channel-mask clear
//   test-harness channel-mask add <channel>
//   test-harness channel-mask remove <channel>
//   test-harness channel-mask reset
//     These commands manipulate the current channel mask used by the
//     network-find plugin to find an open network to join.  The 'clear'
//     command will wipe out the channel mask completely so it doesn't
//     have any channels.  'add' or 'remove' will add/remove the 
//     specified channel to the list of channels to search.  'reset'
//     will reset the channel mask back to the default compiled into the
//     application.

//   test-harness radio on
//   test-harness radio off
//     Turns on/off the radio on the Ember chip.  This is useful
//     for forcing the device to miss messages or simulating a 
//     device failure without reboot.

//   test-harness fc-aps reset
//     Reset APS frame counter to 0.

static void setMode(void);
static void setCertMangleType(void);
static void delayEphemeral(void);
static void printStatus(void);
static void setRegistration(void);
static void setApsSecurityForCluster(void);
static void priceSendNewFields(void);
static void tcKeepaliveSendCommand(void);
static void tcKeepaliveStartStopCommand(void);
static void corruptOtaImageCommand(void);
static void radioOnOffCommand(void);

#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
static void concentratorStartStopCommand(void);
#endif

#if defined(EMBER_AF_PLUGIN_NETWORK_FIND)
static void channelMaskResetClearAllCommand(void);
static void channelMaskAddOrRemoveCommand(void);

extern int32u testHarnessChannelMask;
extern const int32u testHarnessOriginalChannelMask;
#endif

static void enableDisableEndpointCommand(void);
static void endpointStatusCommand(void);

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_NWK_KEY_UPDATE_PERIODIC)
  static void keyUpdateCommand(void);
#endif

static EmberCommandEntry certMangleCommands[] = {
  emberCommandEntryAction( "length",  setCertMangleType,   "s", "Mangles the length of the certificate"),
  emberCommandEntryAction( "issuer",  setCertMangleType,   "b", "Changes the issuer in the certificate"),
  emberCommandEntryAction( "corrupt", setCertMangleType,   "u", "Corrupts a syngle byte in the cert"),
  emberCommandEntryAction( "subject", setCertMangleType,   "b", "Changes the subject (IEEE) of the cert"),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry registrationCommands[] = {
  emberCommandEntryAction( "on",  setRegistration, "", "Turns automatic SE registration on."),
  emberCommandEntryAction( "off", setRegistration, "", "Turns automatic SE registration off."),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry apsSecurityForClusterCommands[] = {
  emberCommandEntryAction( "on",  setApsSecurityForCluster, "v", "Turns on automatic APS security based on cluster." ),
  emberCommandEntryAction( "off", setApsSecurityForCluster, "",  "Turns off automatic APS security based on cluster." ),
  emberCommandEntryTerminator(), 
};

static EmberCommandEntry keyEstablishmentCommands[] = {
  emberCommandEntryAction( "normal-mode",     setMode, "",   "Sets key establishment to normal, compliant mode."),
  emberCommandEntryAction( "no-resources",    setMode, "",   "All received KE requests will be responded with 'no resources'"),
  emberCommandEntryAction( "out-of-sequence", setMode, "u",  "Sends an out-of-sequence KE message based on the passed command ID."),
  emberCommandEntryAction( "timeout",         setMode, "" ,  "Artificially creates a timeout by delaying an outgoing message."),
  emberCommandEntryAction( "delay-cbke",      setMode, "vv", "Changes the advertised delays by the local device for CBKE."),
  emberCommandEntrySubMenu( "cert-mangle",    certMangleCommands, "Commands to mangle the certificate sent by the local device."),
  emberCommandEntryAction( "default-resp",    setMode, "",   "Sends a default response error message in response to initate KE."),
  emberCommandEntryAction( "new-key-policy",  setMode, "u",  "Sets the policy of whether the TC allows new KE requests."),
  emberCommandEntryAction( "reset-aps-fc",    setMode, "",   "Forces the local device to reset its outgoing APS FC."),
  emberCommandEntryAction( "adv-aps-fc",    setMode, "",   "Advances the local device's outgoing APS FC by 4096."),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry priceCommands[] = {
  emberCommandEntryAction( "send-new-fields", priceSendNewFields, "u", "Controls whether the new SE 1.1 price fields are included."),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry tcKeepaliveCommands[] = {
  emberCommandEntryAction( "send",  tcKeepaliveSendCommand,      "", "Sends a new TC keepalive." ),
  emberCommandEntryAction( "start", tcKeepaliveStartStopCommand, "", "Starts the TC keepalive state machine."),
  emberCommandEntryAction( "stop",  tcKeepaliveStartStopCommand, "", "Stops the TC keepalive state machine."),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry otaCommands[] = {
  emberCommandEntryAction( "image-mangle", corruptOtaImageCommand, "v", "Mangles the Simple Storage RAM OTA image."),
  emberCommandEntryTerminator(),
};

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_NWK_KEY_UPDATE_PERIODIC)
static EmberCommandEntry keyUpdateCommands[] = {
  emberCommandEntryAction( "unicast",   keyUpdateCommand, "", "Changes TC NWK key update mechanism to unicast with APS security."),
  emberCommandEntryAction( "broadcast", keyUpdateCommand, "", "Changes TC NWK key update mechanism to broadcast."),
  emberCommandEntryAction( "now",       keyUpdateCommand, "", "Starts a TC NWK key update now"),
  emberCommandEntryTerminator(),
};
#endif

#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
static EmberCommandEntry concentratorCommands[] = {
  emberCommandEntryAction( "start",     concentratorStartStopCommand, "", "Starts the concentrator's periodic broadcasts." ),
  emberCommandEntryAction( "stop",      concentratorStartStopCommand, "", "Stops the concentrator's periodic broadcasts." ),
  emberCommandEntryTerminator(),
};
#endif

#if defined(STACK_TEST_HARNESS)
static void limitBeaconsOnOffCommand(void);

static EmberCommandEntry limitBeaconsCommands[] = {
  emberCommandEntryAction( "on",     limitBeaconsOnOffCommand, "", "Enables a limit to the max number of outgoing beacons."),
  emberCommandEntryAction( "off",    limitBeaconsOnOffCommand, "", "Disables a limit to the max number of outgoing beacons."),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry stackTestHarnessCommands[] = {
  emberCommandEntrySubMenu( "limit-beacons", limitBeaconsCommands, "Commands to limit the outgoing beacons." ),
  emberCommandEntryTerminator(),
};
#endif


#if defined(EMBER_AF_PLUGIN_NETWORK_FIND)
static EmberCommandEntry channelMaskCommands[] = {
  emberCommandEntryAction( "clear",   channelMaskResetClearAllCommand, "", "Clears the channel mask used by network find."),
  emberCommandEntryAction( "reset",   channelMaskResetClearAllCommand, "", "Resets the channel mask back to the app default"),
  emberCommandEntryAction( "all",     channelMaskResetClearAllCommand, "", "Sets the channel mask to all channels"),
  emberCommandEntryAction( "add",     channelMaskAddOrRemoveCommand, "u",  "Add a channel to the mask"),
  emberCommandEntryAction( "remove",  channelMaskAddOrRemoveCommand, "u",  "Removes a channel from the mask"),
  emberCommandEntryTerminator(),
};
#endif

static EmberCommandEntry endpointCommands[] = {
  emberCommandEntryAction( "enable",    enableDisableEndpointCommand, "u", "Enables the endpont to receive messages and be discovered" ),
  emberCommandEntryAction( "disable",   enableDisableEndpointCommand, "u", "Disables the endpont to receive messages and be discovered" ),
  emberCommandEntryAction( "status",    endpointStatusCommand,        "",  "Prints the status of all endpoints"),
  emberCommandEntryTerminator(),
};

static EmberCommandEntry radioCommands[] = {
  emberCommandEntryAction("on", radioOnOffCommand, "", 
                          "Turns on the radio if it was previously turned off"),
  emberCommandEntryAction("off", radioOnOffCommand, "",
                          "Turns off the radio so that no messages are sent."),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emAfTestHarnessCommands[] = {
  emberCommandEntryAction( "status",            printStatus, "",  "Prints the status of the test harness plugin"),

  emberCommandEntrySubMenu("aps-sec-for-clust", apsSecurityForClusterCommands, "APS Security on clusters"),
  emberCommandEntrySubMenu("registration",      registrationCommands, "Enables/disables auto SE registration"),
  emberCommandEntrySubMenu("ke",                keyEstablishmentCommands, "Key establishment commands"),

#if defined(EMBER_AF_PLUGIN_PRICE_SERVER)
  emberCommandEntrySubMenu("price",             priceCommands, "Commands for manipulating price server"),
#endif

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE)
  emberCommandEntrySubMenu("tc-keepalive",      tcKeepaliveCommands, "Commands for Keepalive messages to TC"),
#endif

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_NWK_KEY_UPDATE_PERIODIC)
  emberCommandEntrySubMenu("key-update",        keyUpdateCommands,  "Trust Center Key update commands"),
#endif

  emberCommandEntrySubMenu("ota",               otaCommands,      "OTA cluster commands"),
  
#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
  emberCommandEntrySubMenu("concentrator",      concentratorCommands, "Concentrator commands"),
#endif

#if defined(STACK_TEST_HARNESS)
  emberCommandEntrySubMenu("stack",             stackTestHarnessCommands, "Stack test harness commands"),
#endif

#if defined(EMBER_AF_PLUGIN_NETWORK_FIND)
  emberCommandEntrySubMenu("channel-mask",      channelMaskCommands, "Manipulates the Network Find channel mask"),
#endif

  emberCommandEntrySubMenu("endpoint",          endpointCommands, "Commands for manipulating endpoint messaging/discovery."),

  emberCommandEntrySubMenu("radio",             radioCommands, "Commands to turn on/off the radio"),

  emberCommandEntryTerminator(),
};

static int8u certIndexToCorrupt = 0;
#define TEST_HARNESS_BACK_OFF_TIME_SECONDS 30

#define CBKE_KEY_ESTABLISHMENT_SUITE         0x0001   // per the spec

#define TEST_HARNESS_PRINT_PREFIX "TEST HARNESS: "

// Holds either the ephemeral public key or the 2 SMAC values.
// The SMAC values are the biggest piece of data.
static int8u* delayedData[EMBER_SMAC_SIZE * 2];
static boolean stopRecursion = FALSE;
static int16u cbkeDelaySeconds = 0;
static int8u delayedCbkeOperation = CBKE_OPERATION_GENERATE_KEYS;

EmberEventControl emAfKeyEstablishmentTestHarnessEventControl;

// This is what is reported to the partner of key establishment.
int16u emAfKeyEstablishmentTestHarnessGenerateKeyTime = DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS;
int16u emAfKeyEstablishmentTestHarnessConfirmKeyTime = DEFAULT_GENERATE_SHARED_SECRET_TIME_SECONDS;
int16u emAfKeyEstablishmentTestHarnessAdvertisedGenerateKeyTime = DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS;

#define NULL_CLUSTER_ID 0xFFFF
static EmberAfClusterId clusterIdRequiringApsSecurity = NULL_CLUSTER_ID;

// Packet must be at least 3 bytes for ZCL overhead.
//   Frame Control (1-byte)
//   Sequence Number (1-byte)
//   Command Id (1-byte)
#define ZCL_OVERHEAD 3

#if defined(EMBER_AF_PLUGIN_TEST_HARNESS_AUTO_REGISTRATION_START)
  #define AUTO_REG_START TRUE
#else
  #define AUTO_REG_START FALSE
#endif

boolean emAfTestHarnessAllowRegistration = AUTO_REG_START;

static boolean unicastKeyUpdate = FALSE;

//------------------------------------------------------------------------------
// Misc. forward declarations

void emberAfPluginTrustCenterNwkKeyUpdatePeriodicMyEventHandler(void);

#if defined(STACK_TEST_HARNESS)
void emTestHarnessBeaconSuppressSet(boolean enable);
int8u emTestHarnessBeaconSuppressGet(void);
#endif

//------------------------------------------------------------------------------

static void testHarnessPrintVarArgs(PGM_P format,
                                   va_list vargs)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_KEY_ESTABLISHMENT_CLUSTER)
  emberAfKeyEstablishmentClusterFlush();
  emberAfKeyEstablishmentClusterPrint(TEST_HARNESS_PRINT_PREFIX);
  emberSerialPrintfVarArg(EMBER_AF_PRINT_OUTPUT, format, vargs);
  emberAfKeyEstablishmentClusterFlush();
#endif
}

static void testHarnessPrint(PGM_P format,
                               ...)
{
  va_list vargs;
  va_start(vargs, format);
  testHarnessPrintVarArgs(format, vargs);
  va_end(vargs);
}


static void testHarnessPrintln(PGM_P format,
                               ...)
{
  va_list vargs;
  va_start(vargs, format);
  testHarnessPrintVarArgs(format, vargs);
  va_end(vargs);
  emberSerialPrintCarriageReturn(EMBER_AF_PRINT_OUTPUT);
}

static void resetTimeouts(void)
{
  emAfKeyEstablishmentTestHarnessGenerateKeyTime = DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS;
  emAfKeyEstablishmentTestHarnessConfirmKeyTime = DEFAULT_GENERATE_SHARED_SECRET_TIME_SECONDS;
  emAfKeyEstablishmentTestHarnessAdvertisedGenerateKeyTime = DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS;
}

static void setCertMangleType(void)
{
  int8u commandChar = emberCurrentCommand->name[0];

  testHarnessMode = MODE_CERT_MANGLE;

  if (commandChar == 'l') {
    certMangleType = CERT_MANGLE_LENGTH;
    certLengthMod = (int8s)emberSignedCommandArgument(0);
  } else if (commandChar == 'i') {
    certMangleType = CERT_MANGLE_ISSUER;
    emberCopyEui64Argument(0, invalidEui64);
  } else if (commandChar == 'c' ) {
    certMangleType = CERT_MANGLE_CORRUPT;
    certIndexToCorrupt = (int8u)emberUnsignedCommandArgument(0);
  } else if (commandChar == 's' ) {
    certMangleType = CERT_MANGLE_SUBJECT;
    emberCopyEui64Argument(0, invalidEui64);
  } else {
    testHarnessPrintln("Error: Unknown command.");
    return;
  }

  // Reset this value back to its normal value in case 
  // it was set.
  resetTimeouts();
}

static void setMode(void)
{
  int8u commandChar0 = emberCurrentCommand->name[0];
  int8u commandChar2 = emberCurrentCommand->name[2];

  if (commandChar0 == 'o') {
    testHarnessMode = MODE_OUT_OF_SEQUENCE;
    respondToCommandWithOutOfSequence = (int8u)emberUnsignedCommandArgument(0);

  } else if (commandChar0 == 'n') {
    if (commandChar2 == '-') {
      testHarnessMode = MODE_NO_RESOURCES;
    } else if (commandChar2 == 'r') {
      testHarnessMode = MODE_NORMAL;
    } else if (commandChar2 == 'w') {
      testHarnessMode = MODE_NORMAL;
      emKeyEstablishmentPolicyAllowNewKeyEntries = 
        (int8u)emberUnsignedCommandArgument(0);
    }

  } else if (commandChar0 == 't') {
    testHarnessMode = MODE_TIMEOUT;

  } else if (commandChar2 == 'l' ) {  // delay-cbke
    int16u delay = (int16u)emberUnsignedCommandArgument(0);
    int16u advertisedDelay = (int16u)emberUnsignedCommandArgument(1);
    testHarnessMode = MODE_DELAY_CBKE;
    cbkeDelaySeconds = delay;
    emAfKeyEstablishmentTestHarnessGenerateKeyTime = delay;
    emAfKeyEstablishmentTestHarnessConfirmKeyTime  = advertisedDelay;
    emAfKeyEstablishmentTestHarnessAdvertisedGenerateKeyTime = advertisedDelay;
    return;

  } else if (commandChar2 == 'f') { // default-resp
    testHarnessMode = MODE_DEFAULT_RESPONSE;
    return;

  } else if (commandChar0 == 'r') {
    testHarnessMode = MODE_NORMAL;
    emAfTestHarnessResetApsFrameCounter();  
  } else if (commandChar0 == 'a') {
    testHarnessMode = MODE_NORMAL;
    emAfTestHarnessAdvanceApsFrameCounter();
  } else {
    testHarnessPrintln("Error: Unknown command.");
    return;
  }

  resetTimeouts();
}

static void setRegistration(void)
{
  int8u commandChar1 = emberCurrentCommand->name[1];
  if (commandChar1 == 'n') {
    emAfTestHarnessAllowRegistration = TRUE;
  } else if (commandChar1 == 'f') {
    emAfTestHarnessAllowRegistration = FALSE;
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }
}

static void setApsSecurityForCluster(void)
{
  int8u commandChar1 = emberCurrentCommand->name[1];
  if (commandChar1 == 'n') {
    clusterIdRequiringApsSecurity = (int16u)emberUnsignedCommandArgument(0);
  } else if (commandChar1 == 'f') {
    clusterIdRequiringApsSecurity = NULL_CLUSTER_ID;
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }

}


boolean emAfKeyEstablishmentTestHarnessMessageSendCallback(int8u message)
{
  int8u i;
  int8u* ptr = (appResponseData
                + CERT_OFFSET_IN_INITIATE_KEY_ESTABLISHMENT_MESSAGE);
  int8u direction = (*appResponseData & ZCL_FRAME_CONTROL_DIRECTION_MASK);

  if (MODE_NORMAL == testHarnessMode) {
    return TRUE;
  }

  if (testHarnessMode == MODE_CERT_MANGLE) {
    if (message == ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID) {
      if (certMangleType == CERT_MANGLE_LENGTH) {
        if (certLengthMod > 0) {
          ptr = appResponseData + appResponseLength;
          for (i = 0; i < certLengthMod; i++) {
            *ptr = i;
            ptr++;
          }
        }
        appResponseLength += certLengthMod;

        testHarnessPrintln("Mangling certificate length by %p%d bytes",
                           (certLengthMod > 0
                            ? "+"
                            : ""),
                           certLengthMod);

      } else if (certMangleType == CERT_MANGLE_ISSUER
                 || certMangleType == CERT_MANGLE_SUBJECT) {
        ptr += (certMangleType == CERT_MANGLE_ISSUER
                ? ISSUER_OFFSET_IN_CERT
                : SUBJECT_OFFSET_IN_CERT);

        MEMCOPY(ptr, invalidEui64, EUI64_SIZE);
        testHarnessPrintln("Mangling certificate %p to be (>)%X%X%X%X%X%X%X%X",
                           (certMangleType == CERT_MANGLE_ISSUER
                            ? "issuer"
                            : "subject"),
                           invalidEui64[0],
                           invalidEui64[1],
                           invalidEui64[2],
                           invalidEui64[3],
                           invalidEui64[4],
                           invalidEui64[5],
                           invalidEui64[6],
                           invalidEui64[7]);
      
      } else if (certMangleType == CERT_MANGLE_CORRUPT) {
        // We bit flip the byte to make sure it is different than 
        // its real value.
        ptr += certIndexToCorrupt;
        *ptr = ~ (*ptr);
        testHarnessPrintln("Mangling byte at index %d in certificate.", 
                           certIndexToCorrupt);
      }
    } else if (message == ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID) {
      if (certMangleType == CERT_MANGLE_CORRUPT) {
        // To simulate that the test harness has NOT cancelled
        // key establishment due to a problem with calculating
        // or comparing the SMAC (acting as KE server), we
        // send a confirm key message instead.  The local server
        // has really cancelled KE but we want to test that the other
        // side will cancel it and send its own terminate message.
        ptr = appResponseData + 2; // jump over frame control and sequence num
        *ptr++ = ZCL_CONFIRM_KEY_DATA_RESPONSE_COMMAND_ID;
        for (i = 0; i < EMBER_SMAC_SIZE; i++) {
          *ptr++ = i;
        }
        appResponseLength = (ZCL_OVERHEAD
                             + EMBER_SMAC_SIZE);
        testHarnessPrintln("Rewriting Terminate Message as Confirm Key Message");
      }
    }

  } else if (testHarnessMode == MODE_OUT_OF_SEQUENCE) {
    if (message == respondToCommandWithOutOfSequence) {
      int8u *ptr = appResponseData + 2;  // ZCL Frame control and sequence number

      testHarnessPrintln("Sending out-of-sequence message");

      ptr[0] = (message == ZCL_CONFIRM_KEY_DATA_REQUEST_COMMAND_ID
                ? ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID
                : (message + 1));

      // Setting the outgoing message to the right length without really
      // filling the message with valid data means there would
      // probably be garbage or bad data.  However the receiving device should
      // check for an invalid command ID first before parsing the specific
      // fields in the message.
      appResponseLength = ZCL_OVERHEAD + emAfKeyEstablishMessageToDataSize[message];
    }

  } else if (testHarnessMode == MODE_NO_RESOURCES) {

    // If we are the client then we have to wait until after the first
    // message to send the 'no resources' message.
    if (!(direction == ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
          && message == ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID)) {
      int8u *ptr = appResponseData + 2;  // ZCL Frame control and sequence number
      *ptr++ = ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID;
      *ptr++ = EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_NO_RESOURCES;
      *ptr++ = TEST_HARNESS_BACK_OFF_TIME_SECONDS;
      *ptr++ = LOW_BYTE(CBKE_KEY_ESTABLISHMENT_SUITE);
      *ptr++ = HIGH_BYTE(CBKE_KEY_ESTABLISHMENT_SUITE);

      appResponseLength = ptr - appResponseData;
      
      testHarnessPrintln("Sending Terminate: NO_RESOURCES");
    }
    
  } else if (testHarnessMode == MODE_DEFAULT_RESPONSE) {

    // If we are the client then we have to wait until after the first
    // message to send the Default Response message.
    if (!(direction == ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
          && message == ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID)) {
      int8u *ptr = appResponseData;  
      int8u oldCommandId;
      *ptr++ = direction;
      ptr++; // skip sequence number, it was already written correctly.

      oldCommandId = *ptr;
      *ptr++ = ZCL_DEFAULT_RESPONSE_COMMAND_ID;

      // If we are the client, we send a response to the previous command ID.
      // If we are the server, we send a response to the current command ID.
      // This works because the client -> server and server -> client commands
      // are identical.  The client code will be sending the NEXT command ID,
      // which we are rewriting into a default response.  The server is sending
      // a command ID for the previous client command (a response ID to the
      // request), which has the same command ID as we are rewriting.
      *ptr++ = (direction == ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                ? oldCommandId - 1
                : oldCommandId);
      *ptr++ = EMBER_ZCL_STATUS_FAILURE;
      
      appResponseLength = ptr - appResponseData;
      
      testHarnessPrintln("Sending Default Response: FAILURE");
    }
    
  } else if (testHarnessMode == MODE_TIMEOUT) {

    // If we are the client then we have to wait until after the first
    // message to induce a timeout.
    if (!(direction == ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
          && message == ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID)) {
      testHarnessPrintln("Suppressing message to induce timeout.");
      return FALSE;
    }
  }

  // Send the message
  return TRUE;
}

void emAfKeyEstablishementTestHarnessEventHandler(void)
{
  emberEventControlSetInactive(emAfKeyEstablishmentTestHarnessEventControl);
  testHarnessPrintln("Test Harness Event Handler fired, Tick: 0x%4X",
                     halCommonGetInt32uMillisecondTick());

  stopRecursion = TRUE;
  testHarnessPrintln("Generating %p callback.",
                     ((delayedCbkeOperation
                       == CBKE_OPERATION_GENERATE_KEYS)
                      ? "emberGenerateCbkeKeysHandler()"
                      : "emberCalculateSmacsHandler()"));
  
  if (delayedCbkeOperation == CBKE_OPERATION_GENERATE_KEYS) {
    emAfPluginKeyEstablishmentGenerateCbkeKeysHandler(EMBER_SUCCESS,
                                                      (EmberPublicKeyData*)delayedData);
  } else {
    emAfPluginKeyEstablishmentCalculateSmacsHandler(EMBER_SUCCESS,
                                                    (EmberSmacData*)delayedData,
                                                    (EmberSmacData*)(delayedData + EMBER_SMAC_SIZE));
  }
  
  stopRecursion = FALSE;
}

boolean emAfKeyEstablishmentTestHarnessCbkeCallback(int8u cbkeOperation,
                                                    int8u* data1,
                                                    int8u* data2)
{
  if (testHarnessMode != MODE_DELAY_CBKE) {
    return FALSE;
  }

  if (stopRecursion) {
    return FALSE;
  }

  testHarnessPrintln("Delaying %p key callback for %d seconds", 
                     (cbkeOperation == CBKE_OPERATION_GENERATE_KEYS
                      ? "ephemeral"
                      : "confirm"),
                     cbkeDelaySeconds);

  MEMCOPY(delayedData,
          data1, 
          (cbkeOperation == CBKE_OPERATION_GENERATE_KEYS
           ? sizeof(EmberPublicKeyData)
           : sizeof(EmberSmacData)));

  if (data2 != NULL) {
    MEMCOPY(delayedData + sizeof(EmberSmacData),
            data2,
            sizeof(EmberSmacData));
  }

  delayedCbkeOperation = cbkeOperation;
  
  testHarnessPrintln("Test Harness Tick: 0x%4X",
                     halCommonGetInt32uMillisecondTick());
  emberAfEventControlSetDelay(&emAfKeyEstablishmentTestHarnessEventControl, 
                              (((int32u)(cbkeDelaySeconds)) 
                               * MILLISECOND_TICKS_PER_SECOND));  // convert to MS
  return TRUE;
}

static void printStatus(void)
{
  emberAfKeyEstablishmentClusterPrint("Test Harness Mode: %p", modeText[testHarnessMode]);  
  if (testHarnessMode == MODE_CERT_MANGLE) {
    emberAfKeyEstablishmentClusterPrintln("");
    emberAfKeyEstablishmentClusterPrint("Cert Mangling Type: %p", certMangleText[certMangleType]);
    if (certMangleType == CERT_MANGLE_LENGTH) {
      emberAfKeyEstablishmentClusterPrint(" (%p%d bytes)",
                                          ((certLengthMod > 0)
                                           ? "+"
                                           : ""),
                                          certLengthMod);
    } else if (certMangleType == CERT_MANGLE_CORRUPT) {
      emberAfKeyEstablishmentClusterPrint(" (index: %d)",
                                          certIndexToCorrupt);
    }
  } else if (testHarnessMode == MODE_DELAY_CBKE) {
    emberAfKeyEstablishmentClusterPrint(" (by %d seconds",
                                        cbkeDelaySeconds);
  }
  emberAfKeyEstablishmentClusterPrintln("");

  emberAfKeyEstablishmentClusterPrintln("Auto SE Registration: %p",
                                        (emAfTestHarnessAllowRegistration
                                         ? "On"
                                         : "Off"));
  emberAfKeyEstablishmentClusterPrint("Additional Cluster Security: ");
  if (clusterIdRequiringApsSecurity == NULL_CLUSTER_ID) {
    emberAfKeyEstablishmentClusterPrintln("off");
  } else {
    emberAfKeyEstablishmentClusterPrintln("0x%2X", 
                                          clusterIdRequiringApsSecurity);
  }

  emberAfKeyEstablishmentClusterPrintln("Publish Price includes SE 1.1 fields: %p",
                                        (emAfTestHarnessSupportForNewPriceFields
                                         ? "yes"
                                         : "no"));
  emberAfKeyEstablishmentClusterFlush();

#if defined(STACK_TEST_HARNESS)  
  {
    int8u beaconsLeft = emTestHarnessBeaconSuppressGet();
    emberAfKeyEstablishmentClusterPrint("Beacon Suppress: %p",
                                        (beaconsLeft == 255
                                         ? "Disabled "
                                         : "Enabled "));
    if (beaconsLeft != 255) {
      emberAfKeyEstablishmentClusterPrint(" (%d left to be sent)",
                                          beaconsLeft);
    }
    emberAfKeyEstablishmentClusterFlush();
    emberAfKeyEstablishmentClusterPrintln("");
  }
#endif

#if defined(EMBER_AF_PLUGIN_NETWORK_FIND)
  {
    emberAfKeyEstablishmentClusterPrint("Channel Mask: ");
    emberAfPrintChannelListFromMask(testHarnessChannelMask);
    emberAfKeyEstablishmentClusterPrintln("");
  }
#endif
}

boolean emberAfClusterSecurityCustomCallback(EmberAfProfileId profileId,
                                             EmberAfClusterId clusterId,
                                             boolean incoming, 
                                             int8u commandId)
{
  return (clusterIdRequiringApsSecurity != NULL_CLUSTER_ID
          && clusterId == clusterIdRequiringApsSecurity);
}


static void priceSendNewFields(void)
{
#if defined(EMBER_AF_PLUGIN_PRICE_SERVER)
  emAfTestHarnessSupportForNewPriceFields = (boolean)emberUnsignedCommandArgument(0);
#else
  testHarnessPrintln("No Price server plugin included.");
#endif
}

static void tcKeepaliveSendCommand(void)
{
#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE)
  emAfSendKeepaliveSignal();
#else
  testHarnessPrintln("No TC keepalive plugin included.");
#endif
}

static void tcKeepaliveStartStopCommand(void)
{
#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE)

  int8u commandChar2 = emberCurrentCommand->name[2];
  if (commandChar2 == 'o') {  // stop
    emberAfTrustCenterKeepaliveAbortCallback();

  } else if (commandChar2 == 'a') { // start
    emberAfTrustCenterKeepaliveUpdateCallback(TRUE); // registration complete?  
                                                     // assume this is only called when device is done with that

  } else {
    testHarnessPrintln("Unknown keepalive command.");
  }

#else
  testHarnessPrintln("No TC keepalive plugin included.");
#endif
  
}

static void corruptOtaImageCommand(void)
{
#if defined (EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_RAM)
  int16u index = (int16u)emberUnsignedCommandArgument(0);
  if (index >= emAfOtaStorageDriveGetImageSize()) {
    testHarnessPrintln("Error: Index %d > image size of %d",
                       index,
                       emAfOtaStorageDriveGetImageSize());
  } else {
    emAfOtaStorageDriverCorruptImage(index);
  }
#else
  testHarnessPrintln("No OTA Storage Simple RAM plugin included");
#endif
}

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_NWK_KEY_UPDATE_PERIODIC)
static void keyUpdateCommand(void)
{
  int8u commandChar0 = emberCurrentCommand->name[0];

  if (commandChar0 == 'u') {
    unicastKeyUpdate = TRUE;
  } else if (commandChar0 == 'b') {
    unicastKeyUpdate = FALSE;
  } else if (commandChar0 == 'n') {
    emberAfPluginTrustCenterNwkKeyUpdatePeriodicMyEventHandler();
  }
}

EmberStatus emberAfTrustCenterStartNetworkKeyUpdate(void)
{
  testHarnessPrintln("Using %p key update method",
                     (unicastKeyUpdate
                      ? "unicast"
                      : "broadcast"));
  return (unicastKeyUpdate
          ? emberAfTrustCenterStartUnicastNetworkKeyUpdate()
          : emberAfTrustCenterStartBroadcastNetworkKeyUpdate());
}
#endif // EMBER_AF_PLUGIN_TRUST_CENTER_NWK_KEY_UPDATE_PERIODIC

#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
static void concentratorStartStopCommand(void)
{
  int8u commandChar2 = emberCurrentCommand->name[2];
  if (commandChar2 == 'o') {
    emberConcentratorStopDiscovery();
  } else if (commandChar2 == 'a') {
    emberConcentratorStartDiscovery();
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }
}
#endif // EMBER_AF_PLUGIN_CONCENTRATOR


#if defined(STACK_TEST_HARNESS)
static void limitBeaconsOnOffCommand(void)
{
  int8u commandChar1 = emberCurrentCommand->name[1];
  if (commandChar1 == 'f') {
    emTestHarnessBeaconSuppressSet(255);
  } else if (commandChar1 == 'n') {
    emTestHarnessBeaconSuppressSet(1);
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }
}
#endif

#if defined(EMBER_AF_PLUGIN_NETWORK_FIND)

static void channelMaskAddOrRemoveCommand(void)
{
  int8u commandChar0 = emberCurrentCommand->name[0];
  int8u channel = (int8u)emberUnsignedCommandArgument(0);

  if (channel < 11 || channel > 26) {
    testHarnessPrintln("Error: Invalid channel '%d'.", channel);
    return;
  }
  if (commandChar0 == 'a') {
    testHarnessChannelMask |= (1 << channel);
  } else if (commandChar0 == 'r') {
    testHarnessChannelMask &= ~(1 << channel);
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }
}

static void channelMaskResetClearAllCommand(void)
{
  int8u commandChar0 = emberCurrentCommand->name[0];

  if (commandChar0 == 'c') {
    testHarnessChannelMask = 0;
  } else if (commandChar0 == 'r') {
    testHarnessChannelMask = testHarnessOriginalChannelMask;
  } else if (commandChar0 == 'a') {
    testHarnessChannelMask = EMBER_ALL_802_15_4_CHANNELS_MASK;
  } else {
    testHarnessPrintln("Error: Unknown command.");
  }
}

#endif

static void enableDisableEndpointCommand(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  int8u commandChar0 = emberCurrentCommand->name[0];

#if defined(EZSP_HOST)
  boolean disable = commandChar0 == 'd';

  ezspSetEndpointFlags(endpoint,
                       (disable
                        ? EZSP_ENDPOINT_DISABLED
                        : EZSP_ENDPOINT_ENABLED));
#else

  testHarnessPrintln("Unsupported on SOC.");

#endif
}

static void endpointStatusCommand(void)
{
#if defined(EZSP_HOST)
  int8u i;

  for (i = 0; i < emberAfEndpointCount(); i++) {
    int8u endpoint = emberAfEndpointFromIndex(i);
    EzspEndpointFlags flags;
    EzspStatus status = ezspGetEndpointFlags(endpoint,
                                             &flags);
    testHarnessPrintln("EP %d, Flags 0x%2X [%p]",
                       endpoint,
                       flags,
                       ((flags & EZSP_ENDPOINT_ENABLED)
                        ? "Enabled"
                        : "Disabled"));
  }

#else

  testHarnessPrintln("Unsupported on SOC");
  
#endif
}

#define RADIO_OFF_SCAN (EMBER_ACTIVE_SCAN + 1 + 4)  // EM_START_RADIO_OFF_SCAN (from ember-stack.h)

static void radioOnOffCommand(void)
{
  boolean radioOff = (emberCurrentCommand->name[1] == 'f');
  EmberStatus status;
  if (radioOff) {
    status = emberStartScan(RADIO_OFF_SCAN,
                            0,   // channels (doesn't matter)
                            0);  // duration (doesn't matter)
  } else {
    status = emberStopScan();
  }
  emberAfCorePrintln("Radio %p status: 0x%X", 
                     (radioOff ? "OFF": "ON"),
                     status);
}


//------------------------------------------------------------------------------

