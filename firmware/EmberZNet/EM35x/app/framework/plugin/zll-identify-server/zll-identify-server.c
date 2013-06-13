// *******************************************************************
// * zll-identify-server.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/common.h"

#ifndef EZSP_HOST
#include "hal/hal.h"
#endif

typedef struct {
  boolean active;
  boolean cancel;
  EmberAfIdentifyEffectIdentifier effectId;
  EmberAfIdentifyEffectVariant commandVariant;
  int8u eventsRemaining;
  int16u eventDelay;
} EmAfZllIdentifyState;

void emAfPluginZllIdentifyServerBlinkEffect(int8u endpoint);

void emAfPluginZllIdentifyServerBreatheEffect(int8u endpoint);

void emAfPluginZllIdentifyServerOkayEffect(int8u endpoint);

void emAfPluginZllIdentifyServerChannelChangeEffect(int8u endpoint);

extern EmberEventControl emberAfPluginZllIdentifyServerTriggerEffectEndpointEventControls[];

static EmAfZllIdentifyState stateTable[EMBER_AF_IDENTIFY_CLUSTER_SERVER_ENDPOINT_COUNT];

static EmAfZllIdentifyState *getZllIdentifyState(int8u endpoint);

static void deactivateZllIdentify(EmAfZllIdentifyState *state, int8u endpoint);

static EmAfZllIdentifyState *getZllIdentifyState(int8u endpoint)
{
  int8u index = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_IDENTIFY_CLUSTER_ID);
  return (index == 0xFF ? NULL : &stateTable[index]);  
}

static void deactivateZllIdentify(EmAfZllIdentifyState *state, int8u endpoint)
{
  if (state == NULL) {
    return;
  }

  state->active = FALSE;
  state->cancel = FALSE;

  emberAfEndpointEventControlSetInactive(emberAfPluginZllIdentifyServerTriggerEffectEndpointEventControls, endpoint);
}

void emberAfPluginZllIdentifyServerTriggerEffectEndpointEventHandler(int8u endpoint)
{
  EmAfZllIdentifyState *state = getZllIdentifyState(endpoint);

  if (state == NULL) {
    return;
  }

  switch(state->effectId) {
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK:
      emAfPluginZllIdentifyServerBlinkEffect(endpoint);
      break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE:
      emAfPluginZllIdentifyServerBreatheEffect(endpoint);
      break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY:
      emAfPluginZllIdentifyServerOkayEffect(endpoint);
      break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE:
      emAfPluginZllIdentifyServerChannelChangeEffect(endpoint);
      break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_FINISH_EFFECT: // At this point, these are functionally equivalent
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_STOP_EFFECT:
    default:
      deactivateZllIdentify(state, endpoint);
      return;
  }
  if (state->cancel) {
    deactivateZllIdentify(state, endpoint);
    return;
  }

  if (state->active) {
    emberAfEndpointEventControlSetDelayMS(emberAfPluginZllIdentifyServerTriggerEffectEndpointEventControls,
                                          endpoint,
                                          state->eventDelay);
  }
}

boolean emberAfIdentifyClusterTriggerEffectCallback(int8u effectId,
                                                 int8u effectVariant)
{
  int8u endpoint = emberAfCurrentEndpoint();
  EmAfZllIdentifyState *state = getZllIdentifyState(endpoint);
  EmberAfStatus status;

  if (state == NULL) {
    status = EMBER_ZCL_STATUS_FAILURE;
    goto default_response;
  }

  emberAfIdentifyClusterPrintln("RX identify:trigger effect 0x%x variant 0x%x", effectId, effectVariant);

  if (state->active) {
    switch(state->effectId) {
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_FINISH_EFFECT:
        state->cancel = TRUE;
        break;
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_STOP_EFFECT:
        deactivateZllIdentify(state, endpoint);
        status = EMBER_ZCL_STATUS_SUCCESS;
        goto default_response;
      default:
        status = EMBER_ZCL_STATUS_FAILURE;
        break;
    }
  } else {
    switch(effectId) {
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK:
        state->eventsRemaining = EMBER_AF_PLUGIN_ZLL_IDENTIFY_SERVER_BLINK_EVENTS;
        break;
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE:
        state->eventsRemaining = EMBER_AF_PLUGIN_ZLL_IDENTIFY_SERVER_BREATHE_EVENTS;
        break;
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY:
        state->eventsRemaining = EMBER_AF_PLUGIN_ZLL_IDENTIFY_SERVER_OKAY_EVENTS;
        break;
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE:
        state->eventsRemaining = EMBER_AF_PLUGIN_ZLL_IDENTIFY_SERVER_CHANNEL_CHANGE_EVENTS;
        break;
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_FINISH_EFFECT: // At this point, these are functionally equivalent
      case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_STOP_EFFECT:
        status = EMBER_ZCL_STATUS_SUCCESS;
        goto default_response;
      default:
        status = EMBER_ZCL_STATUS_FAILURE;
        goto default_response;
    }
    state->active = TRUE;
    state->cancel = FALSE;
    state->effectId = effectId;
    state->commandVariant = effectVariant;
    state->eventDelay = EMBER_AF_PLUGIN_ZLL_IDENTIFY_SERVER_EVENT_DELAY;
    emberAfEndpointEventControlSetDelayMS(emberAfPluginZllIdentifyServerTriggerEffectEndpointEventControls,
                                          endpoint,
                                          state->eventDelay);
    status = EMBER_ZCL_STATUS_SUCCESS;
  }

default_response:
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

void emAfPluginZllIdentifyServerBlinkEffect(int8u endpoint)
{
  EmAfZllIdentifyState *state = getZllIdentifyState(endpoint);

  if (state == NULL || state->eventsRemaining == 0) {
    deactivateZllIdentify(state, endpoint);
    return;
  }

#ifndef EZSP_HOST
  halToggleLed(BOARDLED0);
  halToggleLed(BOARDLED1);
  halToggleLed(BOARDLED2);
  halToggleLed(BOARDLED3);
#endif

  state->eventsRemaining = state->eventsRemaining - 1;
}

void emAfPluginZllIdentifyServerBreatheEffect(int8u endpoint)
{
  emAfPluginZllIdentifyServerBlinkEffect(endpoint);
}

void emAfPluginZllIdentifyServerOkayEffect(int8u endpoint)
{
  emAfPluginZllIdentifyServerBlinkEffect(endpoint);
}

void emAfPluginZllIdentifyServerChannelChangeEffect(int8u endpoint)
{
  emAfPluginZllIdentifyServerBlinkEffect(endpoint);
}
