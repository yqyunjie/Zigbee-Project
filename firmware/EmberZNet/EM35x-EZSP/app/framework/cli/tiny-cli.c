// ****************************************************************************
// * tiny-cli.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/cli/security-cli.h"
#include "app/framework/security/af-security.h"
#include "app/framework/util/af-main.h"
#include "app/framework/util/util.h"

#ifdef ZA_TINY_SERIAL_INPUT

#warning Tiny CLI has been deprecated. It will be removed in a future version.

//------------------------------------------------------------------------------

// i = info
// f = form
// j = join
// p = permit join
// l = leave
// s = print counters
// t = read time from ZC
// b = read basic from ZC
// k = print keys
// c = clear keys

/** @deprecated */
#define TINY_SERIAL_CHANNEL 11
/** @deprecated */
#define TINY_SERIAL_PANID   0x00AB

/** @deprecated */
void emAfTinySerialInputTick(void) 
{
  int8u cmd;
  EmberNetworkParameters networkParams;
  EmberStatus status;
  MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));

  if(emberSerialReadByte(APP_SERIAL, &cmd) == 0) {
    if (cmd != '\n') emberAfAppPrintln("");
                                
    switch(cmd) {
     // info
    case 'i':
      emAfCliInfoCommand();
      break;

      // form
    case 'f':
#ifdef EMBER_AF_HAS_COORDINATOR_NETWORK
      emberAfGetFormAndJoinExtendedPanIdCallback(networkParams.extendedPanId);
      networkParams.radioChannel = TINY_SERIAL_CHANNEL;
      networkParams.radioTxPower = 2;
      networkParams.panId = TINY_SERIAL_PANID;
      status = emberAfFormNetwork(&networkParams);
      //emberAfGuaranteedPrintln("form 0x%x\r\n", status);
#else
      emberAfGuaranteedPrintln("only coordinators can form networks");
#endif
      break;

      // join
    case 'j':
      emberAfGetFormAndJoinExtendedPanIdCallback(networkParams.extendedPanId);
      networkParams.radioChannel = TINY_SERIAL_CHANNEL;
      networkParams.radioTxPower = 2;
      networkParams.panId = TINY_SERIAL_PANID;
      status = emberAfJoinNetwork(&networkParams);
      emberAfGuaranteedPrintln("join 0x%x", status);
      break;
      // pjoin
    case 'p':
      status = emberPermitJoining(20);
      emberAfGuaranteedPrintln("pJoin for %x sec: 0x%x", 20, status);
      break;

      // pjoin
    case 'l':
      status = emberLeaveNetwork();
      emberAfGuaranteedPrintln("leave 0x%x", status);
      break;

    case 's':
      emAfCliCountersCommand();
      break;
   
      // send read time to coord
    case 't':
      {
        int8u attributeIds[] = {
          LOW_BYTE(ZCL_TIME_ATTRIBUTE_ID), HIGH_BYTE(ZCL_TIME_ATTRIBUTE_ID),
        };
        emberAfFillCommandGlobalClientToServerReadAttributes(ZCL_TIME_CLUSTER_ID,
                                                             attributeIds,
                                                             sizeof(attributeIds));
        emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                  EMBER_ZIGBEE_COORDINATOR_ADDRESS);
        break;
      }

      // send read basic to coord
    case 'b':
      {
        int8u attributeIds[] = {
          LOW_BYTE(ZCL_VERSION_ATTRIBUTE_ID), HIGH_BYTE(ZCL_VERSION_ATTRIBUTE_ID),
        };
        emberAfFillCommandGlobalClientToServerReadAttributes(ZCL_BASIC_CLUSTER_ID,
                                                             attributeIds,
                                                             sizeof(attributeIds));
        emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                  EMBER_ZIGBEE_COORDINATOR_ADDRESS);
        break;
      }

      // print keys
    case 'k':
      printKeyInfo();
      break;

      // clear keys
    case 'c':
      {
        int8u i;
        for (i = 0; i < EMBER_KEY_TABLE_SIZE; i++) {
          emberEraseKeyTableEntry(i);
        }
      }
      break;

    case '\n':
    case '\r':
      break;

    default:
      emberAfGuaranteedPrintln("ukwn cmd");
      break;
     }

    if (cmd != '\n') {
      emberAfGuaranteedPrint("\r\n%p>", ZA_PROMPT);
    }
  }

}
#endif //ZA_TINY_SERIAL_INPUT
