// *******************************************************************
// * identify-feedback.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"

#include "app/framework/util/common.h"
#if !defined(EZSP_HOST)
#include "hal/hal.h"
#endif // !defined(EZSP_HOST)

static boolean identifyTable[EMBER_AF_IDENTIFY_CLUSTER_SERVER_ENDPOINT_COUNT];

EmberEventControl emberAfPluginIdentifyFeedbackProvideFeedbackEventControl;

void emberAfPluginIdentifyFeedbackProvideFeedbackEventHandler(void);

void emberAfPluginIdentifyFeedbackProvideFeedbackEventHandler(void)
{
#if !defined(EZSP_HOST)
#ifdef EMBER_AF_PLUGIN_IDENTIFY_FEEDBACK_LED_FEEDBACK
  halToggleLed(BOARDLED0);
  halToggleLed(BOARDLED1);
  halToggleLed(BOARDLED2);
  halToggleLed(BOARDLED3);
#endif // EMBER_AF_PLUGIN_IDENTIFY_FEEDBACK_LED_FEEDBACK

#ifdef EMBER_AF_PLUGIN_IDENTIFY_FEEDBACK_BUZZER_FEEDBACK
  halStackIndicatePresence();
#endif // EMBER_AF_PLUGIN_IDENTIFY_FEEDBACK_BUZZER_FEEDBACK
#endif

  emberEventControlSetDelayMS(emberAfPluginIdentifyFeedbackProvideFeedbackEventControl,
                              MILLISECOND_TICKS_PER_SECOND);
}

void emberAfPluginIdentifyStartFeedbackCallback(int8u endpoint,
                                                int16u identifyTime)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_IDENTIFY_CLUSTER_ID);

  if(ep == 0xFF) {
    emberAfIdentifyClusterPrintln("ERR: invalid endpoint supplied for identification.");
    return;
  }

  emberAfIdentifyClusterPrintln("Starting identifying on endpoint 0x%x, identify time is %d sec", 
                                endpoint, 
                                identifyTime);

  identifyTable[ep] = TRUE;
  emberEventControlSetDelayMS(emberAfPluginIdentifyFeedbackProvideFeedbackEventControl,
                              MILLISECOND_TICKS_PER_SECOND);
}

void emberAfPluginIdentifyStopFeedbackCallback(int8u endpoint)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_IDENTIFY_CLUSTER_ID);
  int8u i;

  if(ep == 0xFF) {
    emberAfIdentifyClusterPrintln("ERR: invalid endpoint supplied for identification.");
    return;
  }

  emberAfIdentifyClusterPrintln("Stopping identifying on endpoint 0x%x", endpoint);

  identifyTable[ep] = FALSE;

  for(i = 0; i < EMBER_AF_IDENTIFY_CLUSTER_SERVER_ENDPOINT_COUNT; i++) {
    if(identifyTable[i]) {
      return;
    }
  }

  emberAfIdentifyClusterPrintln("No endpoints identifying; stopping identification feedback.");
  emberEventControlSetInactive(emberAfPluginIdentifyFeedbackProvideFeedbackEventControl);
}
