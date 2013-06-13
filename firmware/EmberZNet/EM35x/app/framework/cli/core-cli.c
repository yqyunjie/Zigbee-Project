// *****************************************************************************
// * core-cli.c
// *
// * Core CLI commands used by all applications regardless of profile.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// common include file
#include "app/framework/util/common.h"
#include "app/framework/util/af-main.h"

#include "app/util/serial/command-interpreter2.h"
#include "stack/include/library.h"
#include "app/framework/security/af-security.h"

#if !defined(EZSP_HOST)
  #include "stack/include/cbke-crypto-engine.h"  // emberGetCertificate()
#endif

#include "app/framework/cli/core-cli.h"
#include "app/framework/cli/custom-cli.h"
#include "app/framework/cli/network-cli.h"
#include "app/framework/cli/plugin-cli.h"
#include "app/framework/cli/security-cli.h"
#include "app/framework/cli/zcl-cli.h"
#include "app/framework/cli/zdo-cli.h"
#include "app/framework/cli/option-cli.h"
#include "app/framework/plugin/test-harness/test-harness-cli.h"
#include "app/framework/util/af-event.h"

#include "app/util/common/library.h"

#if !defined(XAP2B)
  #define PRINT_FULL_COUNTER_NAMES
#endif

//------------------------------------------------------------------------------

void emberCommandActionHandler(const CommandAction action)
{
  emberAfPushNetworkIndex(emAfCliNetworkIndex);
  (*action)();
  emberAfPopNetworkIndex();
}

#if !defined(ZA_CLI_MINIMAL) && !defined(ZA_CLI_FULL)
  // Define this to satisfy external references.
  EmberCommandEntry emberCommandTable[] = { { NULL } };
#endif

#if defined(ZA_TINY_SERIAL_INPUT) || defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)

#if defined(PRINT_FULL_COUNTER_NAMES)
static PGM_NO_CONST PGM_P counterStrings[] = {
  EMBER_COUNTER_STRINGS
};
#endif

static void printMfgString(void)
{
  int8u mfgString[MFG_STRING_MAX_LENGTH + 1];
  emberAfFormatMfgString(mfgString);
  
  // Note:  We use '%s' here because this is a RAM string.  Normally
  // most strings are literals or constants in flash and use '%p'.
  emberAfAppPrintln("MFG String: %s", mfgString);
}

static void printPacketBuffers(void)
{
  emberAfAppPrintln("Buffs: %d / %d",
                    emAfGetPacketBufferFreeCount(),
                    emAfGetPacketBufferTotalCount());
}

static boolean printSmartEnergySecurityInfo(void)
{
#ifdef EMBER_AF_HAS_SECURITY_PROFILE_SE
  boolean securityGood = TRUE;
  emberAfAppPrint("SE Security Info [");
  {
    // for SE security, print the state of ECC, CBKE, and the programmed Cert
    EmberCertificateData cert;
    EmberStatus status = emberGetCertificate(&cert);

    // check the status of the ECC library
    if (emberGetLibraryStatus(EMBER_ECC_LIBRARY_ID)
        & EMBER_LIBRARY_PRESENT_MASK) {
      emberAfAppPrint("RealEcc ");
    } else {
      emberAfAppPrint("NoEcc ");
      securityGood = FALSE;
    }

    // status of EMBER_LIBRARY_NOT_PRESENT means the CBKE is not present
    // in the image.  We don't know anything about the certificate.
    if (status == EMBER_LIBRARY_NOT_PRESENT) {
      emberAfAppPrint("NoCbke UnknownCert");
      securityGood = FALSE;
    } else {
      emberAfAppPrint("RealCbke ");

      // status of EMBER_SUCCESS means the cert is ok
      if (status == EMBER_SUCCESS) {
        emberAfAppPrint("GoodCert");
      }
      // status of EMBER_ERR_FATAL means the cert failed
      else if (status == EMBER_ERR_FATAL) {
        emberAfAppPrint("BadCert");
        securityGood = FALSE;
      }
    }
    emberAfAppPrintln("]");
  }
  emberAfAppFlush();
  return securityGood;
#else
  return FALSE;
#endif
}

void emAfCliCountersCommand(void)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  int8u i;
  int16u counters[EMBER_COUNTER_TYPE_COUNT];
  
  emAfCopyCounterValues(counters);
  for (i=0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
#if defined(PRINT_FULL_COUNTER_NAMES)
    emberAfAppPrintln("%d: %p = %d",
                      i,
                      counterStrings[i],
                      counters[i]);
#else
    emberAfAppPrintln("%d: %d",
                      i,
                      counters[i]);
#endif
    emberAfAppFlush();
  }

  emberAfAppPrintln("");
  printPacketBuffers();
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}

// info
void emAfCliInfoCommand(void)
{
  EmberNodeType nodeTypeResult = 0xFF;
  int8u commandLength;
  EmberEUI64 myEui64;
  EmberNetworkParameters networkParams;
  emberStringCommandArgument(-1, &commandLength);
  printMfgString();
  emberAfGetEui64(myEui64);
  emberAfGetNetworkParameters(&nodeTypeResult, &networkParams);
  emberAfAppPrint("node [");
  emberAfAppDebugExec(emberAfPrintBigEndianEui64(myEui64));
  emberAfAppFlush();
  emberAfAppPrintln("] chan [%d] pwr [%d]",
                    networkParams.radioChannel,
                    networkParams.radioTxPower);
  emberAfAppPrint("panID [0x%2x] nodeID [0x%2x] ",
                 networkParams.panId,
                 emberAfGetNodeId());
  emberAfAppFlush();
  emberAfAppPrint("xpan [0x");
  emberAfAppDebugExec(emberAfPrintBigEndianEui64(networkParams.extendedPanId));
  emberAfAppPrintln("]");
  emberAfAppFlush();

  emAfCliVersionCommand();
  emberAfAppFlush();

  emberAfAppPrint("nodeType [");
  if (nodeTypeResult != 0xFF) {
    emberAfAppPrint("0x%x", nodeTypeResult);
  } else {
    emberAfAppPrint("unknown");
  }
  emberAfAppPrintln("]");
  emberAfAppFlush();

  emberAfAppPrint("%p level [%x]", "Security", emberAfGetSecurityLevel());

  printSmartEnergySecurityInfo();

  emberAfAppPrint("network state [%x] ", emberNetworkState());
  printPacketBuffers();
  emberAfAppFlush();

  // print the endpoint information
  {
    int8u i, j;
    emberAfAppPrintln("Ep cnt: %d", emberAfEndpointCount());
    // loop for each endpoint
    for (i = 0; i < emberAfEndpointCount(); i++) {
      EmberAfEndpointType *et = emAfEndpoints[i].endpointType;
      emberAfAppPrint("ep %d [endpoint %p, device %p] ",
                      emberAfEndpointFromIndex(i),
                      (emberAfEndpointIndexIsEnabled(i)
                       ? "enabled"
                       : "disabled"),
                      emberAfIsDeviceEnabled(emberAfEndpointFromIndex(i))
                      ? "enabled" 
                      : "disabled");
      emberAfAppPrintln("nwk [%d] profile [0x%2x] devId [0x%2x] ver [0x%x]",
                        emberAfNetworkIndexFromEndpointIndex(i),
                        emberAfProfileIdFromIndex(i),
                        emberAfDeviceIdFromIndex(i),
                        emberAfDeviceVersionFromIndex(i));    
      // loop for the clusters within the endpoint
      for (j = 0; j < et->clusterCount; j++) {
        EmberAfCluster *zc = &(et->cluster[j]);
        emberAfAppPrint("    %p cluster: 0x%2x ", 
                       (emberAfClusterIsClient(zc)
                        ? "out(client)"
                        : "in (server)" ),
                       zc->clusterId);
        emberAfAppDebugExec(emberAfDecodeAndPrintCluster(zc->clusterId));
        emberAfAppPrintln("");
        emberAfAppFlush();
      }
      emberAfAppFlush();
    }
  }

  {
    PGM_P names[] = {
      EMBER_AF_GENERATED_NETWORK_STRINGS
    };
    int8u i;
    emberAfAppPrintln("Nwk cnt: %d", EMBER_SUPPORTED_NETWORKS);
    for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
      emberAfAppPrintln("nwk %d [%p]", i, names[i]);
      emberAfAppPrintln("  nodeType [0x%x]", emAfNetworks[i].nodeType);
      emberAfAppPrintln("  securityProfile [0x%x]", emAfNetworks[i].securityProfile);
    }
  }
}
#endif

#if defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)

//------------------------------------------------------------------------------
// "debugprint" commands.

#ifdef ZA_CLI_FULL

static void printOnCommand(void)
{
  int16u area = (int16u)emberUnsignedCommandArgument(0);
  emberAfPrintOn(area);
}

static void printOffCommand(void)
{
  int16u area = (int16u)emberUnsignedCommandArgument(0);
  emberAfPrintOff(area);
}

static EmberCommandEntry debugPrintCommands[] = {
  {"status", emberAfPrintStatus, ""},
  {"all_on", emberAfPrintAllOn, ""},
  {"all_off", emberAfPrintAllOff, ""},
  {"on", printOnCommand, "v"},
  {"off", printOffCommand, "v"},
  { NULL }
};

#endif

//------------------------------------------------------------------------------
// Miscellaneous commands.

static void helpCommand(void)
{

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  EmberCommandEntry *commandFinger = emberCommandTable;
  for (; commandFinger->name != NULL; commandFinger++) {
    emberAfAppPrintln("%p", commandFinger->name);
    emberAfAppFlush();
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}

static void resetCommand(void)
{
  halReboot();
}

static void echoCommand(void)
{
  int8u echoOn = (int8u)emberUnsignedCommandArgument(0);
  if ( echoOn ) {
    emberCommandInterpreterEchoOn();
  } else {
    emberCommandInterpreterEchoOff();
  }
}

static void printEvents(void)
{
  int8u i = 0;
  int32u nowMS32 = halCommonGetInt32uMillisecondTick();
  while (emAfEvents[i].control != NULL) {
    emberAfCorePrint("%p  : ", emAfEventStrings[i]);
    if (emAfEvents[i].control->status == EMBER_EVENT_INACTIVE) {
      emberAfCorePrintln("inactive");
    } else {
      emberAfCorePrintln("%l ms", emAfEvents[i].control->timeToExecute - nowMS32);
    }
    i++;
  }
}

//------------------------------------------------------------------------------
// "endpoint" commands

static void endpointPrint(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  int8u i;
  for (i = 0; i < emberAfEndpointCount(); i++) {
    emberAfCorePrint("EP %d: %p ", 
                     emAfEndpoints[i].endpoint,
                     (emberAfEndpointIndexIsEnabled(i)
                      ? "Enabled"
                      : "Disabled"));
    emAfPrintEzspEndpointFlags(emAfEndpoints[i].endpoint);
    emberAfCorePrintln("");
  }
}

static void enableDisableEndpoint(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  boolean enable = (emberCurrentCommand->name[0] == 'e'
                    ? TRUE
                    : FALSE);
  if (!emberAfEndpointEnableDisable(endpoint, 
                                    enable)) {
    emberAfCorePrintln("Error:  Unknown endpoint.");
  }
}

static EmberCommandEntry endpointCommands[] = {
  emberCommandEntryAction("print",  endpointPrint, "",
                          "Print the status of all the endpoints."),
  emberCommandEntryAction("enable", enableDisableEndpoint, "u",
                          "Enables the endpoint for processing ZCL messages."),
  emberCommandEntryAction("disable", enableDisableEndpoint, "u",
                          "Disable the endpoint from processing ZCL messages."),
  
  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Commands

EmberCommandEntry emberCommandTable[] = {

#ifdef ZA_CLI_FULL

    #if (defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_CLIENT) \
         && defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_SERVER))
      {"cbke",             NULL,                             (PGM_P)cbkeCommands},
    #endif

    #ifdef EMBER_AF_PRINT_ENABLE
      {"print",            NULL,                             (PGM_P)printCommands},
      {"debugprint",       NULL,                             (PGM_P)debugPrintCommands},
    #endif

    #if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
      {"version",          emAfCliVersionCommand,            ""},

      LIBRARY_COMMANDS       // Defined in app/util/common/library.h
    #endif

    {"cha",              NULL,                             (PGM_P)changeKeyCommands},
    {"interpan",         NULL,                             (PGM_P)interpanCommands},
    {"option",           NULL,                             (PGM_P)emAfOptionCommands},
    {"read",             emAfCliReadCommand,               "uvvu"},
    {"time",             emAfCliTimesyncCommand,           "vuu"},
    {"write",            emAfCliWriteCommand,              "uvvuub"},
    {"zcl",              NULL,                             (PGM_P)zclCommands}, // from zcl-cli.c

#endif // ZA_CLI_FULL

  {"bsend",            emAfCliBsendCommand,              "u"},
  {"keys",             NULL,                             (PGM_P)keysCommands},
  {"network",          NULL,                             (PGM_P)networkCommands}, // from network-cli.c
  {"raw",              emAfCliRawCommand,                "vb"},
  {"send",             emAfCliSendCommand,               "vuu"},
  {"send_multicast",   emAfCliSendCommand,               "vu"},

  emberCommandEntrySubMenu("security", emAfSecurityCommands, 
                           "Commands for setting/getting security parameters."),
  emberCommandEntryAction("counters", emAfCliCountersCommand, "",
                          "Print the stack counters"),
  emberCommandEntryAction("help",     helpCommand, "",
                          "Print the list of commands."),
  emberCommandEntryAction("reset", resetCommand, "",
                          "Perform a software reset of the device."),
  emberCommandEntryAction("echo",  echoCommand, "u",
                          "Turn on/off command interpreter echoing."),
  emberCommandEntryAction("events",  printEvents, "",
                          "Print the list of timer events."),
  emberCommandEntrySubMenu("endpoint", endpointCommands,
                           "Commands to manipulate the endpoints."),
  
#ifndef EMBER_AF_CLI_DISABLE_INFO
  emberCommandEntryAction("info", emAfCliInfoCommand, "", \
                          "Print infomation about the network state, clusters, and endpoints"),
#endif

  EMBER_AF_PLUGIN_COMMANDS
  ZDO_COMMANDS
  CUSTOM_COMMANDS
  TEST_HARNESS_CLI_COMMANDS

  emberCommandEntryTerminator(),
};

#endif // defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)
