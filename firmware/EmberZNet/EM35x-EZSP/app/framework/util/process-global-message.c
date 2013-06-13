// *******************************************************************
// * process-global-message.c
// *
// * This file contains function that processes global ZCL message.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../include/af.h"
#include "common.h"
#include "../plugin/smart-energy-registration/smart-energy-registration.h"
#include "../plugin/trust-center-keepalive/trust-center-keepalive.h"

// flag to keep track of the fact that we just sent a read attr for time and
// we should set our time to the result of the read attr response.
boolean emAfSyncingTime = FALSE;

boolean emAfProcessGlobalCommand(EmberAfClusterCommand *cmd)
{
  int16u attrId;
  int8u frameControl;
  // This is a little clumsy but easier to read and port
  // from earlier implementation.
  EmberAfClusterId clusterId = cmd->apsFrame->clusterId;
  int8u zclCmd = cmd->commandId;
  int8u *message = cmd->buffer;
  int16u msgLen = cmd->bufLen;
  int16u msgIndex = cmd->payloadStartIndex;
  int8u clientServerMask = (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER
                            ? CLUSTER_MASK_SERVER
                            : CLUSTER_MASK_CLIENT);

  // If we are disabled then we can only respond to read or write commands
  // or identify cluster (see device enabled attr of basic cluster)
  if (!emberAfIsDeviceEnabled(cmd->apsFrame->destinationEndpoint)
      && zclCmd != ZCL_READ_ATTRIBUTES_COMMAND_ID
      && zclCmd != ZCL_WRITE_ATTRIBUTES_COMMAND_ID
      && zclCmd != ZCL_WRITE_ATTRIBUTES_UNDIVIDED_COMMAND_ID
      && zclCmd != ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID
      && clusterId != ZCL_IDENTIFY_CLUSTER_ID) {
    emberAfCorePrintln("disabled");
    emberAfDebugPrintln("%pd, dropping global cmd:%x", "disable", zclCmd);
    emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_FAILURE);
    return TRUE;
  }

  // If a manufacturer-specific command arrives using our special internal "not
  // manufacturer specific" code, we need to reject it outright without letting
  // it pass through to the rest of the code.  The internal read and write APIs
  // would interpret it as a standard attribute or cluster and return incorrect
  // results.
  if (cmd->mfgSpecific && cmd->mfgCode == EMBER_AF_NULL_MANUFACTURER_CODE) {
    goto kickout;
  }

  // Clear out the response buffer by setting its length to zero
  appResponseLength = 0;

  // Make the ZCL header for the response
  // note: cmd byte is set below
  frameControl = (ZCL_PROFILE_WIDE_COMMAND
                  | (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER
                     ? ZCL_FRAME_CONTROL_SERVER_TO_CLIENT 
                       | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES
                     : ZCL_FRAME_CONTROL_CLIENT_TO_SERVER 
                       | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES));
  if (cmd->mfgSpecific) {
    frameControl |= ZCL_MANUFACTURER_SPECIFIC_MASK;
  }
  emberAfPutInt8uInResp(frameControl);
  if (cmd->mfgSpecific) {
    emberAfPutInt16uInResp(cmd->mfgCode);
  }
  emberAfPutInt8uInResp(cmd->seqNum);


  switch (zclCmd) {

  // The format of the read attributes cmd is:
  // ([attr ID:2]) * N
  // The format of the read attributes response is:
  // ([attr ID:2] [status:1] [data type:0/1] [data:0/N]) * N
  case ZCL_READ_ATTRIBUTES_COMMAND_ID:
    {
      emberAfAttributesPrintln("%p: clus %2x", "READ_ATTR", clusterId);
      // Set the cmd byte - this is byte 3 index 2, but since we have
      // already incremented past the 3 byte ZCL header (our index is at 3),
      // this gets written to "-1" since 3 - 1 = 2.
      emberAfPutInt8uInResp(ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID);

      // This message contains N 2-byte attr IDs after the 3 byte ZCL header,
      // for each one we need to look it up and make a response
      while (msgIndex + 2 <= msgLen) {
        // Get the attribute ID and store it in the response buffer
        // least significant byte is first OTA
        attrId = emberAfGetInt16u(message, msgIndex, msgLen);

        // This function reads the attribute and creates the correct response
        // in the response buffer
        emberAfRetrieveAttributeAndCraftResponse(cmd->apsFrame->destinationEndpoint,
                                                 clusterId,
                                                 attrId,
                                                 clientServerMask,
                                                 cmd->mfgCode,
                                                 (EMBER_AF_RESPONSE_BUFFER_LEN
                                                  - appResponseLength));

        // Go to next attrID
        msgIndex += 2;
      }
    }
    emberAfSendResponse();
    return TRUE;

  // Write undivided means all attributes must be written in order to write
  // any of them. So first do a check. If the check fails, send back a fail
  // response. If it works, fall through to the normal write attr code.
  // write attr responses are the same for undivided and normal writes.
  case ZCL_WRITE_ATTRIBUTES_UNDIVIDED_COMMAND_ID:
    {
      int8u numFailures = 0;
      int8u dataType;
      int8u dataSize;
      EmberAfStatus status;

      emberAfPutInt8uInResp(ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID);

      // Go through the message until there are no more attrID/type/data
      while (msgLen > msgIndex + 3) {
        attrId = emberAfGetInt16u(message, msgIndex, msgLen);
        dataType = emberAfGetInt8u(message, msgIndex + 2, msgLen);

        // For strings, the data size is the length of the string (specified by
        // the first byte of data) plus one for the length byte itself.  For
        // everything else, the size is just the size of the data type.
        dataSize = (emberAfIsThisDataTypeAStringType(dataType)
                    ? emberAfStringLength(message + msgIndex + 3) + 1
                    : emberAfGetDataSize(dataType));

        status = emberAfVerifyAttributeWrite(cmd->apsFrame->destinationEndpoint,
                                             clusterId,
                                             attrId,
                                             clientServerMask,
                                             cmd->mfgCode,
                                             &(message[msgIndex + 3]),
                                             dataType);
        if (status != EMBER_ZCL_STATUS_SUCCESS) {
          numFailures++;
          // Write to the response buffer - status and then attrID
          emberAfPutInt8uInResp(status);
          emberAfPutInt16uInResp(attrId);

          emberAfAttributesPrintln("WRITE: clus %2x attr %2x ", clusterId, attrId);
          emberAfAttributesPrintln("FAIL %x", status);
          emberAfCoreFlush();
        }

        // Increment past the attribute id (two bytes), the type (one byte), and
        // the data (N bytes, including the length byte for strings).
        msgIndex += 3 + dataSize;
      }
      // If there are any failures, send the response and exit
      if (numFailures > 0) {
        emberAfSendResponse();
        return TRUE;
      }
    }
    // Reset message back to start
    msgIndex = cmd->payloadStartIndex;
    appResponseLength = (cmd->mfgSpecific ? 4 : 2);
    // DO NOT BREAK from this case

  // the format of the write attributes cmd is:
  // ([attr ID:2] [data type:1] [data:N]) * N
  // the format of the write attributes response is:
  // ([status 1] [attr ID 2]) * n
  // ONLY errors are reported unless all are successful then a single success
  // is sent. write attr no response is handled by just executing the same
  // code but not setting the flag that sends the response at the end.
  case ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID:
  case ZCL_WRITE_ATTRIBUTES_COMMAND_ID:
    {
      int8u numFailures = 0;
      int8u numSuccess = 0;
      int8u dataType;
      int8u dataSize;
#if (BIGENDIAN_CPU)
      int8u writeData[ATTRIBUTE_LARGEST];
#endif //(BIGENDIAN_CPU)
      EmberAfStatus status;

      // set the cmd byte - this is byte 3 index 2, but since we have
      // already incremented past the 3 byte ZCL header (our index is at 3),
      // this gets written to "-1" since 3 - 1 = 2.
      emberAfPutInt8uInResp(ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID);

      // go through the message until there are no more attrID/type/data
      while (msgLen > msgIndex + 3) {
        attrId = emberAfGetInt16u(message, msgIndex, msgLen);
        dataType = emberAfGetInt8u(message, msgIndex + 2, msgLen);

        // For strings, the data size is the length of the string (specified by
        // the first byte of data) plus one for the length byte itself.  For
        // everything else, the size is just the size of the data type.
        dataSize = (emberAfIsThisDataTypeAStringType(dataType)
                    ? emberAfStringLength(message + msgIndex + 3) + 1
                    : emberAfGetDataSize(dataType));

        // the data is sent little endian over-the-air, it needs to be
        // inserted into the table big endian for the EM250 and little
        // endian for the EZSP hosts. This means for the EM250 the data
        // needs to be reversed before sending to writeAttributes
#if (BIGENDIAN_CPU)
          // strings go over the air as length byte and then in human
          // readable format. These should not be flipped.
          if (emberAfIsThisDataTypeAStringType(dataType)) {
            MEMCOPY(writeData, message + msgIndex + 3, dataSize);
          } else {
            // the data is sent little endian over-the-air, it needs to be
            // inserted into the table big endian
            int8u i;
            for (i = 0; i < dataSize; i++) {
              writeData[i] = message[msgIndex + 3 + dataSize - i - 1];
            }
          }
#endif //(BIGENDIAN_CPU)

          status = emberAfWriteAttributeExternal(cmd->apsFrame->destinationEndpoint,
                                                 clusterId,
                                                 attrId,
                                                 clientServerMask,
                                                 cmd->mfgCode,
#if (BIGENDIAN_CPU)
                                                 writeData,
#else //(BIGENDIAN_CPU)
                                                 &(message[msgIndex + 3]),
#endif //(BIGENDIAN_CPU)
                                                 dataType);
          emberAfAttributesPrint("WRITE: clus %2x attr %2x ",
                                   clusterId,
                                   attrId);
          if (status == EMBER_ZCL_STATUS_SUCCESS) {
            numSuccess++;
            emberAfAttributesPrintln("OK");
          } else {
            numFailures++;
            // write to the response buffer - status and then attrID
            emberAfPutInt8uInResp(status);
            emberAfPutInt16uInResp(attrId);
            emberAfAttributesPrintln("FAIL %x",  status);
          }
          emberAfCoreFlush();

        // Increment past the attribute id (two bytes), the type (one byte), and
        // the data (N bytes, including the length byte for strings).
        msgIndex += 3 + dataSize;
      }

      if (numFailures == 0) {
        // if no failures and no success this means the packet was too short
        // print an error message but still return TRUE as we consumed the
        // message
        if (numSuccess == 0) {
          emberAfAttributesPrintln("WRITE: too short");
          emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_MALFORMED_COMMAND);
          return TRUE;
        }
        // if no failures and at least one success, write a success status
        // that means everything worked
        else {
          emberAfPutInt8uInResp(EMBER_ZCL_STATUS_SUCCESS);
        }
      }
      // always send a response unless the cmd requested no response
      if (zclCmd == ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID) {
        emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
        return TRUE;
      }
      emberAfSendResponse();
      return TRUE;
    }

  // the format of discover is: [start attr ID:2] [max attr IDs:1]
  // the format of the response is: [done:1] ([attrID:2] [type:1]) * N
  case ZCL_DISCOVER_ATTRIBUTES_COMMAND_ID:
  case ZCL_DISCOVER_ATTRIBUTES_EXTENDED_COMMAND_ID:
    {
      EmberAfAttributeId startingAttributeId;
      int8u numberAttributes;
      boolean *complete;

      emberAfAttributesPrintln("%p%p: clus %2x", "DISC_ATTR",
          (zclCmd == ZCL_DISCOVER_ATTRIBUTES_EXTENDED_COMMAND_ID ? "_EXT" : ""), 
          clusterId);

      // set the cmd byte - this is byte 3 index 2, but since we have
      // already incremented past the 3 byte ZCL header (our index is at 3),
      // this gets written to "-1" since 3 - 1 = 2.
      emberAfPutInt8uInResp(
        (zclCmd == ZCL_DISCOVER_ATTRIBUTES_COMMAND_ID ? 
         ZCL_DISCOVER_ATTRIBUTES_RESPONSE_COMMAND_ID :
         ZCL_DISCOVER_ATTRIBUTES_EXTENDED_RESPONSE_COMMAND_ID));

      // get the attrId to start on and the max count
      startingAttributeId = emberAfGetInt16u(message, msgIndex, msgLen);
      numberAttributes = emberAfGetInt8u(message, msgIndex + 2, msgLen);

      // The response has a one-byte field indicating whether discovery is
      // complete.  We can't populate that field until we've finished going
      // through all the attributes, so save a placeholder, write a temporary
      // value for now (so that the offset moves forward), and write the real
      // value when we're done.
      complete = &(appResponseData[appResponseLength]);
      emberAfPutInt8uInResp(FALSE);
      *complete = emberAfReadSequentialAttributesAddToResponse(
                    cmd->apsFrame->destinationEndpoint,
                    clusterId,
                    startingAttributeId,
                    clientServerMask,
                    cmd->mfgCode,
                    numberAttributes,
                    (zclCmd == ZCL_DISCOVER_ATTRIBUTES_EXTENDED_COMMAND_ID));
      emberAfSendResponse();
      return TRUE;
    }

  case ZCL_CONFIGURE_REPORTING_COMMAND_ID:
    if (emberAfConfigureReportingCommandCallback(cmd)) {
      return TRUE;
    }
    break;

  case ZCL_READ_REPORTING_CONFIGURATION_COMMAND_ID:
    if (emberAfReadReportingConfigurationCommandCallback(cmd)) {
      return TRUE;
    }
    break;

  // ([attribute id:2] [status:1] [type:0/1] [value:0/V])+
  case ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID:
    // The "timesync" command in the CLI sends a Read Attributes command for the
    // Time attribute on another device and then sets a flag.  If that flag is
    // set and a Read Attributes Response command for the time comes in, we set
    // the time to the value in the message.
    if (clusterId == ZCL_TIME_CLUSTER_ID) {
      if (emAfSyncingTime
          && !cmd->mfgSpecific
          && msgLen - msgIndex == 8 // attr:2 status:1 type:1 data:4
          && (emberAfGetInt16u(message, msgIndex, msgLen)
              == ZCL_TIME_ATTRIBUTE_ID)
          && (emberAfGetInt8u(message, msgIndex + 2, msgLen)
              == EMBER_ZCL_STATUS_SUCCESS)
          && (emberAfGetInt8u(message, msgIndex + 3, msgLen)
              == ZCL_UTC_TIME_ATTRIBUTE_TYPE)) {
        emberAfSetTime(emberAfGetInt32u(message, msgIndex + 4, msgLen));
        emberAfDebugPrintln("time sync ok, time: %4x", emberAfGetCurrentTime());
        emAfSyncingTime = FALSE;
      }
#ifdef EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_ESI_DISCOVERY_REQUIRED
      emAfPluginSmartEnergyRegistrationReadAttributesResponseCallback(message + msgIndex,
                                                                      msgLen - msgIndex);
#endif //EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_ESI_DISCOVERY_REQUIRED
    }

#ifdef EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE
    if (clusterId == ZCL_KEY_ESTABLISHMENT_CLUSTER_ID
        && !cmd->mfgSpecific
        && msgLen - msgIndex == 6 // attr:2 status:1 type:1 data:2
        && (emberAfGetInt16u(message, msgIndex, msgLen)
            == ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID)) {
      emAfPluginTrustCenterKeepaliveReadAttributesResponseCallback(message + msgIndex,
                                                                   msgLen - msgIndex);
    }
#endif //EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE

    if (!emberAfReadAttributesResponseCallback(clusterId,
                                               message + msgIndex,
                                               msgLen - msgIndex)) {
      emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    }
    return TRUE;

  // ([status:1] [attribute id:2])+
  case ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID:
    if (!emberAfWriteAttributesResponseCallback(clusterId,
                                                message + msgIndex,
                                                msgLen - msgIndex)) {
      emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    }
    return TRUE;

  // ([status:1] [direction:1] [attribute id:2])+
  case ZCL_CONFIGURE_REPORTING_RESPONSE_COMMAND_ID:
    if (!emberAfConfigureReportingResponseCallback(clusterId,
                                                   message + msgIndex,
                                                   msgLen - msgIndex)) {
      emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    }
    return TRUE;


  // ([status:1] [direction:1] [attribute id:2] [type:0/1] ...
  // ... [min interval:0/2] [max interval:0/2] [reportable change:0/V] ...
  // ... [timeout:0/2])+
  case ZCL_READ_REPORTING_CONFIGURATION_RESPONSE_COMMAND_ID:
    if (!emberAfReadReportingConfigurationResponseCallback(clusterId,
                                                           message + msgIndex,
                                                           msgLen - msgIndex)) {
      emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    }
    return TRUE;

  // ([attribute id:2] [type:1] [data:V])+
  case ZCL_REPORT_ATTRIBUTES_COMMAND_ID:
    if (!emberAfReportAttributesCallback(clusterId,
                                         message + msgIndex,
                                         msgLen - msgIndex)) {
      emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    }
    return TRUE;

  // [command id:1] [status:1]
  case ZCL_DEFAULT_RESPONSE_COMMAND_ID:
    {
      EmberAfStatus status;
      int8u commandId;
      commandId = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      status = (EmberAfStatus)emberAfGetInt8u(message, msgIndex, msgLen);

      emberAfClusterDefaultResponseCallback(cmd->apsFrame->destinationEndpoint,
                                            clusterId,
                                            commandId,
                                            status,
                                            clientServerMask);
      emberAfDefaultResponseCallback(clusterId, commandId, status);
      return TRUE;
    }

  // [discovery complete:1] ([attribute id:2] [type:1])*
  case ZCL_DISCOVER_ATTRIBUTES_RESPONSE_COMMAND_ID:
  case ZCL_DISCOVER_ATTRIBUTES_EXTENDED_RESPONSE_COMMAND_ID:
    {
      boolean discoveryComplete = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      if (!emberAfDiscoverAttributesResponseCallback(clusterId,
                                                     discoveryComplete,
                                                     message + msgIndex,
                                                     msgLen - msgIndex,
												     (zclCmd == 
												      ZCL_DISCOVER_ATTRIBUTES_EXTENDED_RESPONSE_COMMAND_ID))) {
        emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
      }
      return TRUE;
    }

#ifdef EMBER_AF_SUPPORT_COMMAND_DISCOVERY
    // Command discovery takes a bit of flash because we need to add structs 
    // for commands into the generated hader. Hence it's all configurable.
  case ZCL_DISCOVER_COMMANDS_RECEIVED_COMMAND_ID:
  case ZCL_DISCOVER_COMMANDS_GENERATED_COMMAND_ID:
    {
      int8u startCommandIdentifier = emberAfGetInt8u(message, msgIndex, msgLen);
      int8u maximumCommandIdentifiers = emberAfGetInt8u(message, msgIndex+1, msgLen);
      int16u savedIndex;
      boolean flag;
      
      // Ok. This is the command that matters.
      if ( zclCmd == ZCL_DISCOVER_COMMANDS_RECEIVED_COMMAND_ID ) {
        emberAfPutInt8uInResp(ZCL_DISCOVER_COMMANDS_RECEIVED_RESPONSE_COMMAND_ID);
        flag = FALSE;
      } else {
        emberAfPutInt8uInResp(ZCL_DISCOVER_COMMANDS_GENERATED_RESPONSE_COMMAND_ID);
        flag = TRUE;
      }
      savedIndex = appResponseLength;
      flag = emberAfExtractCommandIds(flag,
                                      cmd,
                                      clusterId,
                                      appResponseData + appResponseLength + 1,
                                      EMBER_AF_RESPONSE_BUFFER_LEN - appResponseLength - 1,
                                      &appResponseLength,
                                      startCommandIdentifier,
                                      maximumCommandIdentifiers);
      appResponseData[savedIndex] = ( flag ? 1 : 0 );
      appResponseLength++;
      emberAfSendResponse();
      return TRUE;
    }
  case ZCL_DISCOVER_COMMANDS_RECEIVED_RESPONSE_COMMAND_ID:
    {
      boolean discoveryComplete = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      if(!emberAfDiscoverCommandsReceivedResponseCallback(clusterId,
                                                          cmd->mfgCode,
                                                          discoveryComplete,
                                                          message+msgIndex,
                                                          msgLen-msgIndex)) {
        emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
      }
      return TRUE;
    }
  case ZCL_DISCOVER_COMMANDS_GENERATED_RESPONSE_COMMAND_ID:
    {
      boolean discoveryComplete = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      if(!emberAfDiscoverCommandsGeneratedResponseCallback(clusterId,
                                                           cmd->mfgCode,
                                                           discoveryComplete,
                                                           message+msgIndex,
                                                           msgLen-msgIndex)) {
        emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
      }
      return TRUE;
    }
    
#endif
  }

kickout:
  emberAfSendDefaultResponse(cmd,
                             (cmd->mfgSpecific
                              ? EMBER_ZCL_STATUS_UNSUP_MANUF_GENERAL_COMMAND
                              : EMBER_ZCL_STATUS_UNSUP_GENERAL_COMMAND));
  return TRUE;
}
