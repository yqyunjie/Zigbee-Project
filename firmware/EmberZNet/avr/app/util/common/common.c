// File: common.c
// 
// Description: common code for apps.
//
// Author(s): Richard Kelsey, kelsey@ember.com
//
// Copyright 2004 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/util/common/common.h"

//----------------------------------------------------------------
// Boilerplate

static EmberNetworkParameters joinParameters;
static int8u joinNodeType;
static EmberEUI64 preconfiguredTrustCenterEui64;

EmberEventControl blinkEvent = {0, 0};

void toggleBlinker(void)
{
  halToggleLed(BOARD_HEARTBEAT_LED);
  emberEventControlSetDelayQS(blinkEvent, 2);       // every half second
}

// Default to 19200 baud on port 1.
int8u serialPort = 1;
SerialBaudRate serialBaudRate = BAUD_19200;

void configureSerial(int8u port, SerialBaudRate rate)
{
  serialPort = port;
  serialBaudRate = rate;
}

void initialize(void)
{
  initializeEmberStack();
  MEMSET(joinParameters.extendedPanId, 0, 8);
  emberCommandReaderInit();
  emberEventControlSetDelayQS(blinkEvent, 2);       // every half second
}

void run(EmberEventData* events,
         void (* heartbeat)(void))
{
  while(TRUE) {
    halResetWatchdog();
    emberTick();
    if (heartbeat != NULL)
      (heartbeat)();
    emberProcessCommandInput(serialPort);
    if (events != NULL) {
        runEvents(events);
    }
    emberSerialBufferTick();
  }
}

//----------------------------------------------------------------
// Common commands

void helpCommand(void)
{
  int8u commandIndex = 0;
  emberSerialPrintfLine(serialPort,
    "COMMAND [PARAMETERS] [- DESCRIPTION]\r\n"
    "  where: b=string, s=int8s, u=int8u, v=int16u, w=int32u, n=nested");
  emberSerialWaitSend(serialPort);

  while (emberCommandTable[commandIndex].action != NULL)
  {
    emberSerialPrintf(serialPort, 
                      "%p %p",
                      emberCommandTable[commandIndex].name,
                      emberCommandTable[commandIndex].argumentTypes);
    if (emberCommandTable[commandIndex].description != NULL)
      emberSerialPrintf(serialPort, " - %p",
                        emberCommandTable[commandIndex].description);
    printCarriageReturn();

    emberSerialWaitSend(serialPort);
    commandIndex++;
  }
}

void statusCommand(void)
{
  emberSerialPrintf(serialPort, "%p ", applicationString);
  printLittleEndianEui64(serialPort, emberGetEui64());
  emberSerialWaitSend(serialPort);
  emberSerialPrintf(serialPort, " (%2x)", emberGetNodeId());
  if( emberStackIsUp() )
  {
    EmberNodeType nodeType;
    EmberNetworkParameters networkParams;

    if (!getNetworkParameters(&nodeType, &networkParams)) {
      emberSerialPrintfLine(serialPort, 
                            "Error: Failed to get Network parameters!");
      return;
    }

#ifdef PHY_EM2420
    emberSerialPrintf(serialPort, " 802.15.4 channel %d", 
                      networkParams.radioChannel);
#else
    emberSerialPrintf(serialPort, " Ember channel %d",
                      networkParams.radioChannel);
#endif
    emberSerialPrintf(serialPort, " power %d PAN ID %2x XPID ",
                      networkParams.radioTxPower,
                      networkParams.panId);
    printLittleEndianEui64(serialPort, networkParams.extendedPanId);
    emberSerialPrintfLine(serialPort, "");
  } else {
    emberSerialPrintfLine(serialPort, " offline");
  }
}

void stateCommand(void)
{
  int8u childCount;
  int8u routerCount;
  EmberNodeType myNodeType;
  EmberNodeId parentId;
  EmberEUI64 parentEui;
  EmberNetworkParameters params;
  int8u stackProfile;

  if ( emberStackIsUp()
       && (EMBER_SUCCESS
           == getOnlineNodeParameters(&childCount,
                                      &routerCount,
                                      &myNodeType,
                                      &parentId,
                                      // no '&' operand necessary here
                                      parentEui, 
                                      &params)) ) {
    emberSerialPrintf(serialPort,
                      "%p id=0x%2X parent=0x%2X ",
                      (myNodeType >= EMBER_END_DEVICE
                       ? "End device"
                       : "Router"),
                      emberGetNodeId(),
                      parentId);
    printLittleEndianEui64(serialPort, parentEui);
    emberSerialPrintfLine(serialPort,
                          " pan=0x%2X end devices=%d routers=%d",
                          params.panId,
                          childCount,
                          routerCount);
    emberSerialPrintfLine(serialPort,
                          "nwkManager=0x%2x nwkUpdateId=%d channelMask=0x%4x",
                          params.nwkManagerId,
                          params.nwkUpdateId,
                          params.channels);
  } else {
    EmberNodeId myNodeId;
    emberSerialPrintf(serialPort, 
                      "Not joined.  ");
    
    if ( EMBER_SUCCESS == getOfflineNodeParameters(&myNodeId,
                                                   &myNodeType,
                                                   &stackProfile) ) {
      emberSerialPrintfLine(serialPort, 
                            "Tokens: id:0x%2x type:%d stackProfile:%d",
                            myNodeId,
                            myNodeType,
                            stackProfile);
    } else {
      emberSerialPrintfLine(serialPort,
                            "Could not obtain token data.");
    }
  }
}

void rebootCommand(void)
{
  halReboot();
}

void getOrGenerateKey(int8u argumentIndex, EmberKeyData *key)
{
  if (! emberCopyKeyArgument(1, key)) {
    emberGenerateRandomKey(key);
  }
}

//----------------------------------------------------------------
// Utilities

// The initial < or > is meant to indicate the endian-ness of the EUI64
// '>' is big endian (most significant first)
// '<' is little endian (least significant first)

void printLittleEndianEui64(int8u port, EmberEUI64 eui64)
{
  emberSerialPrintf(port, "(<)%X%X%X%X%X%X%X%X",
                    eui64[0], eui64[1], eui64[2], eui64[3],
                    eui64[4], eui64[5], eui64[6], eui64[7]);
}

void printBigEndianEui64(int8u port, EmberEUI64 eui64)
{
  emberSerialPrintf(port, "(>)%X%X%X%X%X%X%X%X",
                    eui64[7], eui64[6], eui64[5], eui64[4],
                    eui64[3], eui64[2], eui64[1], eui64[0]);
}

void printHexByteArray(int8u port, int8u *byteArray, int8u length) {
  int8u index = 0;

  while ((index + 4) <= length) {
    emberSerialPrintf(port, " %X %X %X %X", 
                                byteArray[index], byteArray[index + 1],
                                byteArray[index + 2], byteArray[index + 3]);
    index += 4;
  }

  while (index < length) {
    emberSerialPrintf(port, " %X", byteArray[index]);
    index += 1;
  }
  
}

int8u asciiHexToByteArray(int8u *bytesOut, int8u* asciiIn, int8u asciiCharLength)
{
  int8u destIndex = 0;
  int8u srcIndex = 0;

  // We need two characters of input for each byte of output.
  while (srcIndex < (asciiCharLength - 1)) {
    bytesOut[destIndex]  = ((((hexDigitValue(asciiIn[srcIndex])) & 0x0F) << 4) +
                            ((hexDigitValue(asciiIn[srcIndex + 1])) & 0x0F));
    destIndex += 1;
    srcIndex  += 2;
  }

  return destIndex;
}

int8u hexDigitValue(int8u digit)
{
  if ('0' <= digit && digit <= '9')
    return digit - '0';
  else if ('A' <= digit && digit <= 'F')
    return digit - 'A' + 10;
  else if ('a' <= digit && digit <= 'f')
    return digit - 'a' + 10;
  else
    return 0;
}

void createMulticastBinding(int8u index, int8u *multicastGroup, int8u endpoint)
{
  EmberBindingTableEntry entry;

  entry.type = EMBER_MULTICAST_BINDING;
  MEMCOPY(entry.identifier, multicastGroup, 8);
  entry.local = endpoint;

  assert(emberSetBinding(index, &entry) == EMBER_SUCCESS);
}

boolean findEui64Binding(EmberEUI64 key, int8u *index)
{
  int8u i;
  int8u unused = 0xFF;

  for (i = 0; i < emberBindingTableSize; i ++) {
    EmberBindingTableEntry binding;
    if (emberGetBinding(i, &binding) == EMBER_SUCCESS) {
      if (binding.type == EMBER_UNICAST_BINDING
          && MEMCOMPARE(key, binding.identifier, 8) == 0) {
        *index = i;
        return TRUE;
      } else if (binding.type == EMBER_UNUSED_BINDING
                 && unused == 0xFF)
        unused = i;
    }
  }

  *index = unused;
  return FALSE;
}

void printCommandStatus(EmberStatus status, 
                        PGM_P good,
                        PGM_P bad)
{
  if (status == EMBER_SUCCESS) {
    if ( good != NULL )
      emberSerialPrintfLine(serialPort, "%p", good);
  } else {
    emberSerialPrintfLine(serialPort, "%p, status:0x%x", bad, status);
  }
}

void printOperationStatus(EmberStatus status,
                          PGM_P operation)
{
  emberSerialPrintf(serialPort, "%p", operation);
  printCommandStatus(status, "", " failed");
}

//----------------------------------------------------------------
// Common zigbee commands

int16u lastJoinTime;

void setSecurityCommand(void)
{
  EmberInitialSecurityState securityState;
  EmberStatus status = EMBER_SUCCESS;
  int8u securityLevel;

  securityState.bitmask = emberUnsignedCommandArgument(0);

  securityLevel = (((securityState.bitmask & EM_SECURITY_INITIALIZED)
                    && (securityState.bitmask != 0xFFFF))
                   ? 0
                   : 5);

  status = (setSecurityLevel(securityLevel)
            ? EMBER_SUCCESS
            : EMBER_ERR_FATAL);

  if ( status == EMBER_SUCCESS 
       && securityLevel > 0 
       && securityState.bitmask != 0xFFFF) {
    // If the bit is set saying that a key is being passed, and the key buffer
    // is NOT empty, use the passed key.

    // If the bit is set saying that a key is being passed, and the key buffer
    // is empty (""), generate a random key.

    // If the bit is NOT set saying that a key is being passed, but the key
    // buffer is NOT empty, set the appropriate bit for that key and use that
    // key.

    if (emberCopyKeyArgument(1, &securityState.preconfiguredKey)) {
      securityState.bitmask |= EMBER_HAVE_PRECONFIGURED_KEY;
    } else if ((securityState.bitmask & EMBER_HAVE_PRECONFIGURED_KEY)
               == EMBER_HAVE_PRECONFIGURED_KEY) {
      emberGenerateRandomKey(&securityState.preconfiguredKey);
    }
   
    if (emberCopyKeyArgument(2, &securityState.networkKey)) {
      securityState.bitmask |= EMBER_HAVE_NETWORK_KEY;
    } else if ((securityState.bitmask & EMBER_HAVE_NETWORK_KEY)
               == EMBER_HAVE_NETWORK_KEY) {
      emberGenerateRandomKey(&securityState.networkKey);
    }

    securityState.networkKeySequenceNumber = emberUnsignedCommandArgument(3);
    if (securityState.bitmask & EMBER_HAVE_TRUST_CENTER_EUI64) {
      MEMCOPY(securityState.preconfiguredTrustCenterEui64,
              preconfiguredTrustCenterEui64,
              EUI64_SIZE);
    }

    status = emberSetInitialSecurityState(&securityState);
  }
  printOperationStatus(status,
                       "Security set");
}

void formNetworkCommand(void)
{
  EmberStatus status;
  EmberNetworkParameters parameters;
  int8u commandLength;

  parameters.radioChannel = emberUnsignedCommandArgument(0);
  parameters.panId = emberUnsignedCommandArgument(1);
  parameters.radioTxPower = emberSignedCommandArgument(2);
  emberStringCommandArgument(-1, &commandLength);
  if (commandLength == 4) {
    MEMSET(parameters.extendedPanId, 0, 8);
  } else {
    emberCopyEui64Argument(3, parameters.extendedPanId);
  }
  lastJoinTime = halCommonGetInt16uMillisecondTick();
  status = emberFormNetwork(&parameters);
  printCommandStatus(status, "Formed", "Form failed");
}

EmberStatus joinNetwork(void)
{
  lastJoinTime = halCommonGetInt16uMillisecondTick();
  return emberJoinNetwork(joinNodeType, &joinParameters);
}

void setExtPanIdCommand(void)
{
  emberCopyEui64Argument(0, joinParameters.extendedPanId);
}

void setJoinMethod(void)
{
  joinParameters.joinMethod = emberUnsignedCommandArgument(0);
}

void setCommissionParameters(void)
{
  joinNodeType = (emberUnsignedCommandArgument(0) > 0
                  ? EMBER_COORDINATOR
                  : EMBER_ROUTER);
  joinParameters.nwkManagerId = emberUnsignedCommandArgument(1);
  joinParameters.nwkUpdateId  = emberUnsignedCommandArgument(2);
  joinParameters.channels     = emberUnsignedCommandArgument(3);
  joinParameters.channels    |= (emberUnsignedCommandArgument(4) << 16);
  emberCopyEui64Argument(5, preconfiguredTrustCenterEui64);
}

void joinNetworkCommand(void)
{
  EmberStatus status;
  int8u commandLength;
  int8u *command = emberStringCommandArgument(-1, &commandLength);
  joinParameters.radioChannel = emberUnsignedCommandArgument(0);
  joinParameters.panId = emberUnsignedCommandArgument(1);
  joinParameters.radioTxPower = emberSignedCommandArgument(2);
  if (joinParameters.joinMethod != EMBER_USE_NWK_COMMISSIONING) {
    joinNodeType = (commandLength == 4 ? EMBER_ROUTER
                    : command[5] == 'e' ? EMBER_END_DEVICE
                    : command[5] == 's' ? EMBER_SLEEPY_END_DEVICE
                    : EMBER_MOBILE_END_DEVICE);
  }
  status = joinNetwork();
  if ( ! (status == EMBER_SUCCESS
          && joinParameters.joinMethod == EMBER_USE_NWK_COMMISSIONING)) {
    printOperationStatus(status, 
                         "Joining");
  }
}

void networkInitCommand(void)
{
  EmberStatus status;
  
  lastJoinTime = halCommonGetInt16uMillisecondTick();
  status = emberNetworkInit();
  printOperationStatus(status, "Re-initializing network");
}

void rejoinCommand(void)
{
  boolean secure = (boolean)emberUnsignedCommandArgument(0);
  printOperationStatus(emberRejoinNetwork(secure),
                       "Rejoining");
}

void leaveNetworkCommand(void)
{
  EmberStatus status = emberLeaveNetwork();
  printCommandStatus(status, "Left", "Leave failed");
}

void addressRequestCommand(void)
{
  boolean reportKids = emberUnsignedCommandArgument(1);
  int8u *command = emberStringCommandArgument(-1, NULL);
  if (command[0] == 'n') {
    EmberEUI64 targetEui64;
    emberCopyEui64Argument(0, targetEui64);
    emberNetworkAddressRequest(targetEui64, reportKids, 0);
  } else {
    EmberNodeId target = emberUnsignedCommandArgument(0);
    emberIeeeAddressRequest(target, reportKids, 0, 0);
  }
}

void permitJoiningCommand(void)
{
  int8u duration = emberUnsignedCommandArgument(0);
  emberSerialPrintfLine(serialPort,
                        "permitJoining(%d) -> 0x%X",
                        duration,
                        emberPermitJoining(duration));
}

// Routine to save some const/flash space
void printCarriageReturn(void)
{
  emberSerialPrintCarriageReturn(serialPort);
}

void printErrorMessage(PGM_P message)
{
  emberSerialPrintfLine(serialPort, "Error: %p", message);
}
