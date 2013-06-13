// *****************************************************************************
// * ez-mode.c
// *
// * This file provides a function set for initiating ez-mode commissioning
// * as both a client and a server.
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "ez-mode.h"
#include "button-joining-callback.h"

//------------------------------------------------------------------------------
// Forward Declaration

EmberEventControl emberAfPluginEzmodeCommissioningStateEventControl;
static void serviceDiscoveryCallback(const EmberAfServiceDiscoveryResult *result);
static void createBinding(int8u *address);

//------------------------------------------------------------------------------
// Globals

#define stateEvent emberAfPluginEzmodeCommissioningStateEventControl

#define IDENTIFY_TIMEOUT EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_IDENTIFY_TIMEOUT

static int8u currentIdentifyingEndpoint;
static EmberNodeId currentIdentifyingAddress;

static int8u ezmodeClientEndpoint;

static int16u clusterIdsForEzModeMatch[EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_CLUSTER_IDS_FOR_EZ_MODE_MATCH_LENGTH] = 
	EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_CLUSTER_IDS_FOR_EZ_MODE_MATCH;
static int8u clusterIdsForEzModeMatchLength = 
    EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_CLUSTER_IDS_FOR_EZ_MODE_MATCH_LENGTH;
static int16u ezmodeClientCluster;

typedef enum {
  EZMODE_OFF,
  EZMODE_BROAD_PJOIN,
  EZMODE_IDENTIFY,
  EZMODE_MATCH,
  EZMODE_BIND,
  EZMODE_BOUND
} EzModeState;

static EzModeState ezModeState = EZMODE_OFF;

// We assume the first endpoint is the one to use for end-device bind / EZ-Mode
#define ENDPOINT_INDEX 0

//------------------------------------------------------------------------------

// This callback fires when the device is already joined to the network and 
// a button has been pressed.
void emberAfPluginButtonJoiningButtonEventCallback(int8u button, int32u buttonPressDurationMs)
{
	int8u endpoint = emberAfEndpointFromIndex(ENDPOINT_INDEX);
	if (button != 0) {
		return;  
	}
	emberAfCorePrintln("EZ-Mode Commission:%x", endpoint);    
	emberAfEzmodeServerCommission(endpoint);
	emberAfEzmodeClientCommission(endpoint);
}

void emberAfPluginEzmodeCommissioningStateEventHandler(void) {
  EmberStatus status;
  EmberEUI64 add;
  emberEventControlSetInactive(stateEvent);
  switch (ezModeState) {
	case EZMODE_BROAD_PJOIN:
      emberAfCorePrintln("<ezmode bpjoin>");
      emAfPermitJoin(180, TRUE); //Send out a broadcast pjoin
	  ezModeState = EZMODE_IDENTIFY;
      emberEventControlSetDelayMS(stateEvent, MILLISECOND_TICKS_PER_SECOND);
	  break;
    case EZMODE_IDENTIFY:
      emberAfCorePrintln("<ezmode identify>");
	  emAfPermitJoin(180, TRUE); //Send out a broadcast pjoin
      emberAfFillCommandIdentifyClusterIdentifyQuery();
      emberAfSetCommandEndpoints(ezmodeClientEndpoint, EMBER_BROADCAST_ENDPOINT);
      emberAfSendCommandBroadcast(0xffff);
      break;
    case EZMODE_MATCH:
      emberAfCorePrintln("<ezmode match>");
      emberAfFindClustersByDeviceAndEndpoint(currentIdentifyingAddress,
                                             currentIdentifyingEndpoint,
                                             serviceDiscoveryCallback);
      break;
    case EZMODE_BIND:
      emberAfCorePrintln("<ezmode bind>");
  	  status = emberLookupEui64ByNodeId(currentIdentifyingAddress, add);
      if (status == EMBER_SUCCESS) {
		createBinding(add);
		break;
      }    
      status = emberAfFindIeeeAddress(currentIdentifyingAddress, serviceDiscoveryCallback);
	  emberAfCorePrintln("<find ieee status: %d>", status);
      break;
    case EZMODE_BOUND:
      emberAfCorePrintln("<ezmode bound>");
      break;
    default:
      break;
  }
}

/** EZ-MODE CLIENT **/

/**
 * Kicks off the ezmode commissioning process by sending out
 * an identify query command to the given endpoint
 */
EmberStatus emberAfEzmodeClientCommission(int8u endpoint) {
  ezmodeClientEndpoint = endpoint;
  ezModeState = EZMODE_BROAD_PJOIN;
  emberEventControlSetActive(stateEvent);
  return EMBER_SUCCESS;
}

boolean emberAfIdentifyClusterIdentifyQueryResponseCallback(int16u timeout) {
	
  // ignore our own broadcast and only take the first identify
  if (emberGetNodeId() == emberAfCurrentCommand()->source ||
      ezModeState != EZMODE_IDENTIFY)
    return FALSE;
  
  currentIdentifyingAddress = emberAfCurrentCommand()->source;
  currentIdentifyingEndpoint = emberAfCurrentCommand()->apsFrame->sourceEndpoint;
  ezModeState = EZMODE_MATCH;
  emberEventControlSetActive(stateEvent);
  
  return TRUE;
}

static void createBinding(int8u *address) {
  // create binding
  int8u i = 0;
  EmberBindingTableEntry candidate;
  EmberStatus status;
    
	
  // first look for a duplicate binding, we should not add duplicates
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++)
  {
     status = emberGetBinding(i, &candidate);

     if ((status == EMBER_SUCCESS)
           && (candidate.type == EMBER_UNICAST_BINDING)
           && (candidate.local == ezmodeClientEndpoint )
           && (candidate.clusterId == ezmodeClientCluster )
           && (candidate.remote == currentIdentifyingEndpoint )
           && (MEMCOMPARE(candidate.identifier, 
                 address, 
                 EUI64_SIZE) == 0))
     {
       ezModeState = EZMODE_BOUND;
       emberEventControlSetActive(stateEvent);
	   return;
     }
   }
	
   for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
     if (emberGetBinding(i, &candidate) == EMBER_SUCCESS
         && candidate.type == EMBER_UNUSED_BINDING) {
       candidate.type = EMBER_UNICAST_BINDING;
       candidate.local = ezmodeClientEndpoint;
       candidate.remote = currentIdentifyingEndpoint;
       candidate.clusterId = ezmodeClientCluster;
       MEMCOPY(candidate.identifier, address, EUI64_SIZE);
       status = emberSetBinding(i, &candidate);
       if (status == EMBER_SUCCESS) {
         ezModeState = EZMODE_BOUND;
         emberEventControlSetActive(stateEvent);
         break;
       }
     }
   }
}

static void serviceDiscoveryCallback(const EmberAfServiceDiscoveryResult *result)
{ 
	int8u i = 0;
	int8u j = 0;
  if (emberAfHaveDiscoveryResponseStatus(result->status)) {
    if (result->zdoRequestClusterId == SIMPLE_DESCRIPTOR_REQUEST) {
      EmberAfClusterList *list = (EmberAfClusterList*)result->responseData;
      for (i = 0; i < list->inClusterCount; i++) {
        int16u inClus = list->inClusterList[i];
		for (j = 0; j < clusterIdsForEzModeMatchLength; j++) {
          if (inClus == clusterIdsForEzModeMatch[j]) {
            ezModeState = EZMODE_BIND;
			ezmodeClientCluster = inClus;
            emberEventControlSetActive(stateEvent);
            break;
          }
        }
        if (ezModeState == EZMODE_BIND)
			break;
      }
    } else if (result->zdoRequestClusterId == IEEE_ADDRESS_REQUEST) {
	  emberAfCorePrintln("<create binding>");
	  createBinding((int8u *)result->responseData);
    }
  }
}


/** EZ-MODE SERVER **/

/**
 * Puts the device into identify mode for the given endpoint
 * this is all that an ezmode server is responsible for
 */
EmberStatus emberAfEzmodeServerCommission(int8u endpoint) {
  EmberAfStatus afStatus;
  int16u identifyTime = IDENTIFY_TIMEOUT;
  afStatus =  emberAfWriteAttribute(endpoint,
                               ZCL_IDENTIFY_CLUSTER_ID,
                               ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                               CLUSTER_MASK_SERVER,
                               (int8u *)&identifyTime,
                               ZCL_INT16U_ATTRIBUTE_TYPE);
  if (afStatus != EMBER_ZCL_STATUS_SUCCESS)
    return EMBER_BAD_ARGUMENT;
  else
    return EMBER_SUCCESS;
}

