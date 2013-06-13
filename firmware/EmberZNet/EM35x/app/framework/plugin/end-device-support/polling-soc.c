#include "app/framework/include/af.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"

// *****************************************************************************
// Globals

extern EmberEventControl emberAfPluginEndDeviceSupportPollingNetworkEventControls[];

typedef struct {
  int32u pollIntervalTimeQS;
  int8u numPollsFailing;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

// *****************************************************************************
// Functions

// This is called to scheduling polling events for the network(s).  We only
// care about end device networks.  For each of those, a polling event will be
// scheduled for joined networks or canceled otherwise.
void emberAfSchedulePollEventCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    if (EMBER_END_DEVICE <= emAfNetworks[i].nodeType) {
      emberAfPushNetworkIndex(i);
      if (emberNetworkState() == EMBER_JOINED_NETWORK) {
        State *state = &states[i];
        int32u lastPollIntervalTimeQS = state->pollIntervalTimeQS;
        state->pollIntervalTimeQS = emberAfGetCurrentPollIntervalQsCallback();
        if (state->pollIntervalTimeQS != lastPollIntervalTimeQS
            || !emberAfNetworkEventControlGetActive(emberAfPluginEndDeviceSupportPollingNetworkEventControls)) {
          emberAfDebugPrintln("Next poll nwk %d in %l ms",
                              i,
                              (state->pollIntervalTimeQS
                               * MILLISECOND_TICKS_PER_QUARTERSECOND));
          emberAfNetworkEventControlSetDelay(emberAfPluginEndDeviceSupportPollingNetworkEventControls,
                                             (state->pollIntervalTimeQS
                                              * MILLISECOND_TICKS_PER_QUARTERSECOND));
        }
      } else {
        emberAfNetworkEventControlSetInactive(emberAfPluginEndDeviceSupportPollingNetworkEventControls);
      }
      emberAfPopNetworkIndex();
    }
  }
}

// Whenever the polling event fires for a network, a MAC data poll is sent.
void emberAfPluginEndDeviceSupportPollingNetworkEventHandler(void)
{
  EmberNetworkStatus state = emberNetworkState();
  if (state == EMBER_JOINED_NETWORK) {
    EmberStatus status = emberPollForData();
    if (status != EMBER_SUCCESS) {
      emberAfCorePrintln("poll nwk %d: 0x%x", emberGetCurrentNetwork(), status);
    }
  }
}

// This function is called when a poll completes and explains what happend with
// the poll.  If the number of sequential data polls not ACKed by the parent
// exceeds the threshold, we will try to find a new parent.
void emberPollCompleteHandler(EmberStatus status)
{
  State *state;
  int8u networkIndex;

  emberAfPushCallbackNetworkIndex();
  networkIndex = emberGetCurrentNetwork();
  state = &states[networkIndex];

  switch (status) {
  case EMBER_SUCCESS:
    emberAfAddToCurrentAppTasks(EMBER_AF_LAST_POLL_GOT_DATA);
    emberAfDebugPrintln("poll nwk %d: got data", networkIndex);
    state->numPollsFailing = 0;
    break;
  case EMBER_MAC_NO_DATA:
    emberAfRemoveFromCurrentAppTasks(EMBER_AF_LAST_POLL_GOT_DATA);
    emberAfDebugPrintln("poll nwk %d: no data", networkIndex);
    state->numPollsFailing = 0;
    break;
  case EMBER_DELIVERY_FAILED:
    // this means the air was busy - don't count as a failure
    emberAfRemoveFromCurrentAppTasks(EMBER_AF_LAST_POLL_GOT_DATA);
    emberAfDebugPrintln("poll nwk %d: delivery failed", networkIndex);
    break;
  case EMBER_MAC_NO_ACK_RECEIVED:
    // If we are performing key establishment, we can ignore this since the
    // parent could go away for long periods of time while doing ECC processes.
    if (emberAfPerformingKeyEstablishment()) {
      break;
    }
    // Count failures until we hit EMBER_AF_NUM_MISSED_POLLS_TO_TRIGGER_MOVE,
    // then we try a rejoin. If rejoin fails, it will trigger a move.
    state->numPollsFailing++;
    if (state->numPollsFailing >= EMBER_AF_NUM_MISSED_POLLS_TO_TRIGGER_MOVE) {
      emberAfStartMoveCallback();
    }
    emberAfRemoveFromCurrentAppTasks(EMBER_AF_LAST_POLL_GOT_DATA);
    break;
  default:
    emberAfDebugPrintln("poll nwk %d: 0x%x", networkIndex, status);
  }

  emberAfPopNetworkIndex();
}
