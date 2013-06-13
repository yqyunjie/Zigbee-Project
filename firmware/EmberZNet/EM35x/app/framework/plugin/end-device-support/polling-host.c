#include "app/framework/include/af.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"

// *****************************************************************************
// Globals

typedef struct {
  int32u pollIntervalTimeQS;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

// *****************************************************************************
// Functions

// This is called to scheduling polling events for the network(s).  We only
// care about end device networks.  For each of those, the NCP will be told to
// poll for joined networks or not to poll otherwise.
void emberAfSchedulePollEventCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    if (EMBER_END_DEVICE <= emAfNetworks[i].nodeType) {
      State *state = &states[i];
      int32u lastPollIntervalTimeQS = state->pollIntervalTimeQS;
      emberAfPushNetworkIndex(i);
      if (emberNetworkState() == EMBER_JOINED_NETWORK) {
        state->pollIntervalTimeQS = emberAfGetCurrentPollIntervalQsCallback();
      } else {
        state->pollIntervalTimeQS = 0;
      }
      if (state->pollIntervalTimeQS != lastPollIntervalTimeQS) {
        EmberStatus status;
        if (state->pollIntervalTimeQS != 0) {
          emberAfDebugPrintln("Next poll nwk %d in %l ms",
                              i,
                              (state->pollIntervalTimeQS
                               * MILLISECOND_TICKS_PER_QUARTERSECOND));
        }
        status = ezspPollForData(state->pollIntervalTimeQS,
                                 EMBER_EVENT_QS_TIME,
                                 EMBER_AF_NUM_MISSED_POLLS_TO_TRIGGER_MOVE);
        if (status != EMBER_SUCCESS) {
          emberAfCorePrintln("poll nwk %d: 0x%x", i, status);
        }
      }
      emberAfPopNetworkIndex();
    }
  }
}

// The NCP schedules and manages polling, so we do not schedule our own events
// and therefore this handler should never fire.
void emberAfPluginEndDeviceSupportPollingNetworkEventHandler(void)
{
}

// This function is called when a poll completes and explains what happend with
// the poll.  If no ACKs are received from the parent, we will try to find a
// new parent.
void ezspPollCompleteHandler(EmberStatus status)
{
  int8u networkIndex;

  emberAfPushCallbackNetworkIndex();
  networkIndex = emberGetCurrentNetwork();
  (void)networkIndex;

  switch (status) {
  case EMBER_SUCCESS:
    emberAfDebugPrintln("poll nwk %d: got data", networkIndex);
    break;
  case EMBER_MAC_NO_DATA:
    emberAfDebugPrintln("poll nwk %d: no data", networkIndex);
    break;
  case EMBER_DELIVERY_FAILED:
    emberAfDebugPrintln("poll nwk %d: delivery failed", networkIndex);
    break;
  case EMBER_MAC_NO_ACK_RECEIVED:
    // If we are performing key establishment, we can ignore this since the
    // parent could go away for long periods of time while doing ECC processes.
    if (emberAfPerformingKeyEstablishment()) {
      break;
    }
    emberAfStartMoveCallback();
    break;
  default:
    emberAfDebugPrintln("poll nwk %d: 0x%x", networkIndex, status);
  }

  emberAfPopNetworkIndex();
}

void emberAfPreNcpResetCallback(void)
{
  // Reset the poll intervals so the NCP will be instructed to poll if
  // necessary.
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    states[i].pollIntervalTimeQS = 0;
  }
}
