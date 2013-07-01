// File: legacy.c
//
// Description: old APIs to facilitate upgrading applications from EmberZNet 2.x
// to EmberZNet 3.x.
//
// Author(s): Matteo Paris, matteo@ember.com
//
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "stack/include/error.h"

// Forward declaration
static EmberStatus okToSend(EmberMessageBuffer message,
                            int8u bindingIndex,
                            boolean multicast,
                            EmberBindingTableEntry *binding,
                            int16u *profileIdLoc);

EmberStatus emberSendDatagram(int8u bindingIndex,
                              int16u clusterId,
                              EmberMessageBuffer message)
{
  EmberBindingTableEntry binding;
  EmberApsFrame apsFrame;

  if (emberBindingTableSize <= bindingIndex)
    return EMBER_INVALID_BINDING_INDEX;

  {
    EmberStatus status = okToSend(message,
                                  bindingIndex,
                                  FALSE,
                                  &binding,
                                  &(apsFrame.profileId));
    if (status != EMBER_SUCCESS)
      return status;
  }

  apsFrame.options = (EMBER_APS_OPTION_RETRY
                      | EMBER_APS_OPTION_DESTINATION_EUI64
                      | (binding.type == EMBER_MANY_TO_ONE_BINDING
                         ? 0
                         : (EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY
                            | EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY)));
  apsFrame.clusterId = clusterId;
  apsFrame.sourceEndpoint = binding.local;
  apsFrame.destinationEndpoint = binding.remote;

  return emberSendUnicast(EMBER_OUTGOING_VIA_BINDING,
                          bindingIndex,
                          &apsFrame,
                          message);
}

//------------------------------------------------------------------------------

// This checks that the binding and endpoint description are appropriate.  
// The binding and profile ID are passed back to the caller.

static EmberStatus okToSend(EmberMessageBuffer message,
                            int8u bindingIndex,
                            boolean multicast,
                            EmberBindingTableEntry *binding,
                            int16u *profileIdLoc)
{
  EmberEndpointDescription descriptor;
    
  if (emberGetBinding(bindingIndex, binding) != EMBER_SUCCESS)
    return EMBER_INVALID_BINDING_INDEX;
  if (multicast && binding->type != EMBER_MULTICAST_BINDING)
    return EMBER_INVALID_BINDING_INDEX;
  if (! multicast && ! (binding->type == EMBER_UNICAST_BINDING
                        || binding->type == EMBER_MANY_TO_ONE_BINDING))
    return EMBER_INVALID_BINDING_INDEX;
  if (! emberGetEndpointDescription(binding->local, &descriptor))
    return EMBER_INVALID_ENDPOINT;
  *profileIdLoc = descriptor.profileId;
  return EMBER_SUCCESS;
}
