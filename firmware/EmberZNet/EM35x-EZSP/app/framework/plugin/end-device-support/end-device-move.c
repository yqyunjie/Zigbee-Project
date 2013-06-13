// *****************************************************************************
// * end-device-move.c
// *
// * Code common to SOC and host to handle moving (i.e. rejoining) to a new
// * parent device.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"

// *****************************************************************************
// Globals

extern EmberEventControl emberAfPluginEndDeviceSupportMoveNetworkEventControls[];

typedef struct {
  int8u moveAttempts;
  int16u totalMoveAttempts;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

#define NEVER_STOP_ATTEMPTING_REJOIN 0xFF
#define MOVE_DELAY_QS (10 * 4)

// *****************************************************************************
// Functions

void emAfScheduleMoveEvent(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  State *state = &states[networkIndex];

  if (EMBER_AF_REJOIN_ATTEMPTS_MAX == NEVER_STOP_ATTEMPTING_REJOIN
      || state->moveAttempts < EMBER_AF_REJOIN_ATTEMPTS_MAX) {
    emberAfAppPrintln("Schedule move nwk %d: %d",
                      networkIndex,
                      state->moveAttempts);
    emberAfNetworkEventControlSetDelayQS(emberAfPluginEndDeviceSupportMoveNetworkEventControls,
                                         (state->moveAttempts == 0
                                          ? 0
                                          : MOVE_DELAY_QS));
  } else {
    emberAfAppPrintln("Max move limit reached nwk %d: %d",
                      networkIndex,
                      state->moveAttempts);
    emberAfStopMoveCallback();
  }
}

boolean emberAfMoveInProgressCallback(void)
{
  return emberAfNetworkEventControlGetActive(emberAfPluginEndDeviceSupportMoveNetworkEventControls);
}

boolean emberAfStartMoveCallback(void)
{
  if (!emberAfMoveInProgressCallback()) {
    emAfScheduleMoveEvent();
    return TRUE;
  }
  return FALSE;
}

void emberAfStopMoveCallback(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  states[networkIndex].moveAttempts = 0;
  emberEventControlSetInactive(emberAfPluginEndDeviceSupportMoveNetworkEventControls[networkIndex]);
}

void emberAfPluginEndDeviceSupportMoveNetworkEventHandler(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  State *state = &states[networkIndex];
  EmberStatus status;
  boolean secure = (state->moveAttempts == 0);
  int32u channels = (state->moveAttempts == 0
                     ? 0 // current channel
                     : EMBER_ALL_802_15_4_CHANNELS_MASK);
  status = emberFindAndRejoinNetworkWithReason(secure, 
                                               channels,
                                               EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE);
  emberAfDebugPrintln("Move attempt %d nwk %d: 0x%x",
                      state->moveAttempts,
                      networkIndex,
                      status);
  if (status == EMBER_SUCCESS) {
    state->moveAttempts++;
    state->totalMoveAttempts++;
  }
  emAfScheduleMoveEvent();
}

void emberAfPluginEndDeviceSupportStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    emberAfStopMoveCallback();
    return;
  }

  if (status == EMBER_NETWORK_DOWN || status == EMBER_MOVE_FAILED) {
    if (!emberStackIsPerformingRejoin()) {
      EmberNetworkStatus state = emberNetworkState();
      if (state == EMBER_JOINED_NETWORK_NO_PARENT) {
        emberAfStartMoveCallback();
      } else if (state == EMBER_NO_NETWORK) {
        emberAfStopMoveCallback();
      }
    }
  }
}
