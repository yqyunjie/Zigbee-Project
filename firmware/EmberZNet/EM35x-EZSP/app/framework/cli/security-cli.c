// File: ha-security-cli.c
//
// Routines to initiate Key establishment, handle key establishment callbacks,
// and print info about the security keys on the device.
//
// Author(s): Rob Alexander <ralexander@ember.com>
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*

#include "app/framework/util/common.h"

#include "app/framework/util/service-discovery.h"

#include "app/util/serial/command-interpreter2.h"
#include "app/framework/cli/security-cli.h"
#include "app/framework/util/af-main.h"

#include "app/framework/security/af-security.h"


//------------------------------------------------------------------------------
// Globals

EmberKeyData cliPreconfiguredLinkKey = DUMMY_KEY;
EmberKeyData cliNetworkKey           = DUMMY_KEY;


/**
 * @addtogroup cli
 * @{
 */
/**
 * @brief Commands used to change the default link or network key
 *        that will be used when forming or joining a network.
 *
 *        <b>changekey link &lt;key&gt;</b>
 *        - key - 16 byte array. The Link Key provided as a 
 *          16 byte array.
 * 
 *        <b>changekey network &lt;key&gt;</b>
 *        - key - 16 byte array. The Network Key provided
 *          as a 16 byte array.
 *
 */
#define EMBER_AF_DOXYGEN_CLI__SECURITY_CHANGEKEY_COMMANDS
/** @} END addtogroup */

/**
 * @addtogroup cli
 * @{
 */
/**
 * @brief Command used for key establishment.
 *
 *        <b>cbke start &lt;new partner id&gt; &lt;destination endpoint&gt;</b>
 *        - new partner id - int16u. The 2 byte node id of the partner
 *          with whom to start cbke.
 *
 *        <b>cbke interpan &lt;pan id&gt; &lt;eui64&gt;</b>
 *        - pan id - int16u. The 2 byte pan id of the partner with whom to start
 *          cbke.
 *        - eui64 - EmberEUI64. The 8 byte EUI64 of the partner with whom to
 *          start cbke.
 *
 *        <b>cbke partner &lt;node id&gt; &lt;endpoint&gt;</b>
 *        - node id - int16u. The two byte node id of the device
 *          with whom to initiate key establishment.
 *        - endpoint - int8u. The endpoint on which to begin
 *          key establishment.
 *
 */
#define EMBER_AF_DOXYGEN_CLI__SECURITY_CBKE_COMMANDS
/** @} END addtogroup */

EmberCommandEntry changeKeyCommands[] = {
  {"link", changeKeyCommand, "b"},
  {"network", changeKeyCommand, "b"},
  { NULL }
};

/**
 * @addtogroup security
 * @{
 */
/** @brief Commands used for security
 *
 * <b>security mfg-token get</b>
 * <b>security mfg-token set &lt;Magic number&gt; &lt;EmberKeySettings value&gt;</b>
 *   - Magic number - int32u.  The 4-byte magic number 
 *     EMBER_MFG_SECURITY_CONFIG_MAGIC_NUMBER to prevent accidental execution.
 *   - EmberKeySettings value - int16u.  The 2-byte value for EmberKeySettings
 *     indicating key permissions.
 *
 */ 
#define EMBER_AF_DOXYGEN_CLI__SECURITY_MFG_TOKEN_COMMANDS
/** @} END addtogroup */

#if !defined(XAP2B)
static void getSetMfgToken(void);

EmberCommandEntry mfgToken[] = {
  emberCommandEntryAction("get", getSetMfgToken, "",
                          "Print the MFG security config token"),
  emberCommandEntryAction("set", getSetMfgToken, "wv",
                          "Set the MFG Security config token.  CANNOT BE UNSET BY CLI."),
  emberCommandEntryTerminator(),
};
#endif

EmberCommandEntry emAfSecurityCommands[] = {

#if !defined(XAP2B)
  emberCommandEntrySubMenu("mfg-token", mfgToken, 
                           "Get/set the MFG Security config token"),
#endif
  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Forward Declarations

static int8u printKeyTable(boolean preconfiguredKey);
static int32u getOutgoingApsFrameCounter(void);

//------------------------------------------------------------------------------

// ******************************************************
// changekey link    <16 byte key>
// changekey network <16 byte key>
// ******************************************************

// Changes the default link or network key that will be used when
// forming or joining a network.  
void changeKeyCommand(void) 
{
  if (EMBER_NO_NETWORK != emberNetworkState()) {
    emberAfCorePrintln("%pstack must be down.", "ERROR: ");
  } else {
    emberCopyKeyArgument(0, 
                         emberCurrentCommand->name[0] == 'l'
                         ? &cliPreconfiguredLinkKey
                         : &cliNetworkKey);
    emberAfDebugPrintln("set key");
  }
}

#if (defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_CLIENT) \
     && defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_SERVER))

// Key Establishment commands

EmberCommandEntry cbkeCommands[] = {
  {"start", cbkeStartCommand, "vu"},
  {"interpan", cbkeInterPanCommand, "vb"},
  { NULL }
};

// cbke start <node id> <dest endpoint>
void cbkeStartCommand(void)
{
  EmberStatus status;
  EmberNodeId newPartnerId = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u endpoint = (int8u)emberUnsignedCommandArgument(1);
  emberAfCorePrintln("Starting %pment w/ 0x%2x, EP: 0x%x",\
                     "Key Establish",
                     newPartnerId,
                     endpoint);
  emberAfCoreFlush();
  emberSerialBufferTick();

  status = emberAfInitiateKeyEstablishment(newPartnerId, endpoint);
  emberAfCorePrintln("%p", (status == EMBER_SUCCESS ? "Success" : "Error"));
}

// cbke interpan <pan id:2> <eui:8>
void cbkeInterPanCommand(void)
{
  EmberEUI64 eui64;
  EmberPanId panId = (EmberPanId)emberUnsignedCommandArgument(0);
  EmberStatus status;
  emberAfCopyBigEndianEui64Argument(1, eui64);

  emberAfCorePrint("Starting %pment w/ ", "Key Establish");
  emberAfCoreDebugExec(emberAfPrintBigEndianEui64(eui64));
  emberAfCorePrintln("");
  emberAfCoreFlush();
  emberSerialBufferTick();

  status = emberAfInitiateInterPanKeyEstablishment(panId, eui64);
  emberAfCorePrintln("%p", (status == EMBER_SUCCESS ? "Success" : "Error"));
}

#endif //  (defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_CLIENT)  \
       //   && defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_SERVER))


void printKeyInfo(void)
{
  int8u entriesUsed;
  EmberKeyStruct nwkKey;

#if !defined(XAP2B)
  {
    EmberMfgSecurityStruct config;
    if (EMBER_SUCCESS == emberGetMfgSecurityConfig(&config)) {
      if (!(config.keySettings & EMBER_KEY_PERMISSIONS_READING_ALLOWED)) {
        emberAfCorePrintln("\nWARNING:  READING OF KEY VALUES DISABLED BY MFG TOKEN\n");
        emberAfCoreFlush();
      }
    }
  }
#endif

  if ( EMBER_SUCCESS != emberGetKey(EMBER_CURRENT_NETWORK_KEY,
                                    &nwkKey)) {
    MEMSET((int8u*)&nwkKey, 0xFF, sizeof(EmberKeyStruct));
  }
  emberAfCorePrintln("%p out FC: %4x",
                     "NWK Key",
                     nwkKey.outgoingFrameCounter);
  emberAfCorePrintln("%p seq num: 0x%x",  
                     "NWK Key",
                     nwkKey.sequenceNumber);
  emberAfCorePrint("%p: ", 
                   "NWK Key");
  emberAfPrintZigbeeKey(emberKeyContents(&nwkKey.key));
  
  
  emberAfCorePrintln("%p out FC: %4x",
                     "Link Key",
                     getOutgoingApsFrameCounter());
  
  emberAfCorePrintln("TC %p", "Link Key");
  emberAfCoreFlush();
  printKeyTable(TRUE);
  
  emberAfCorePrintln( "%p Table", "Link Key");
  emberAfCoreFlush();
  entriesUsed = printKeyTable(FALSE);
  
  emberAfCorePrintln("%d/%d entries used.", 
                     entriesUsed,
                     emberAfGetKeyTableSize());
  emberAfCoreFlush();
}

static int8u printKeyTable(boolean preconfiguredKey)
{
  int8u i;
  int8u entriesUsed = 0;
  int8u loopCount = (preconfiguredKey ? 1 : emberAfGetKeyTableSize());

  emberAfDebugPrintln( "Index IEEE Address         In FC     Type  Auth  Key");

  for ( i = 0; i < loopCount; i++ ) {
    EmberKeyStruct entry;

    if (preconfiguredKey) {
      i = 0xFE; // last

      // Try to get whatever key type is stored in the preconfigured key slot.
      if ( (EMBER_SUCCESS !=
            emberGetKey(EMBER_TRUST_CENTER_MASTER_KEY, &entry))
           && (EMBER_SUCCESS
               != emberGetKey(EMBER_TRUST_CENTER_LINK_KEY, &entry)) ) {
        continue;
      }
    } else if ( EMBER_SUCCESS != emberGetKeyTableEntry(i, &entry) )
      continue;

    if (!preconfiguredKey) {
      emberAfCorePrint("%d     ", i);
    } else {
      emberAfCorePrint("-     ");
    }
    emberAfCoreDebugExec(emberAfPrintBigEndianEui64(entry.partnerEUI64));
    emberAfCorePrint("  %4x  ", entry.incomingFrameCounter);
    emberAfCorePrint("%c     %c     ",
                  (( entry.type == EMBER_APPLICATION_MASTER_KEY
                     || entry.type == EMBER_TRUST_CENTER_MASTER_KEY )
                   ? 'M'
                   : 'L' ),
                  ( entry.bitmask & EMBER_KEY_IS_AUTHORIZED 
                    ? 'y'
                    : 'n' ));
    
    emberAfPrintZigbeeKey(emberKeyContents(&(entry.key)));

    emberAfCoreFlush();
    entriesUsed++;
  }

  return entriesUsed;
}

static int32u getOutgoingApsFrameCounter(void)
{
  EmberKeyStruct entry;
  if (emberGetKey(EMBER_TRUST_CENTER_MASTER_KEY, &entry) != EMBER_SUCCESS
      && emberGetKey(EMBER_TRUST_CENTER_LINK_KEY, &entry) != EMBER_SUCCESS) {
    return 0xFFFFFFFFUL;
  }
  return entry.outgoingFrameCounter;
}

#if !defined(XAP2B)
static void getSetMfgToken(void)
{
  EmberMfgSecurityStruct config;
  EmberStatus status;
  if (emberCurrentCommand->name[0] == 'g') {
    status = emberGetMfgSecurityConfig(&config);
    if (status == EMBER_SUCCESS) {
      emberAfSecurityPrintln("EmberKeySettings: 0x%2X", config.keySettings);
      emberAfSecurityPrintln("  Permissions: %p", 
                             ((config.keySettings & EMBER_KEY_PERMISSIONS_READING_ALLOWED)
                              ? "Reading Allowed"
                              : ((config.keySettings & EMBER_KEY_PERMISSIONS_HASHING_ALLOWED)
                                 ? "Hashing only"
                                 : "NONE")));
    }
  } else {
    int32u magicNumber = emberUnsignedCommandArgument(0);
    config.keySettings = (EmberKeySettings)emberUnsignedCommandArgument(1);
    status = emberSetMfgSecurityConfig(magicNumber, &config);
  }

  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("Failed: 0x%X", status);
  }
}
#endif
