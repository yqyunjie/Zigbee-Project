// *****************************************************************************
// * button-joining.c
// *
// * Routines for forming/joining using the hardware buttons.
// *
// *   button 0: if not joined: FORM if the device is capable of forming
// *             (a.k.a. a coordinator).  Otherwise form a network.
// *             if joined: broadcast ZDO MGMT Permit Join in network.
// *             Hold for 5 seconds and release: leave network
// *   button 1: Unused (Callback executed)
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "app/framework/plugin/ezmode-commissioning/ez-mode.h"
#include "button-joining-callback.h"

//------------------------------------------------------------------------------
// Forward Declaration

EmberEventControl emberAfPluginButtonJoiningButton0EventControl;
EmberEventControl emberAfPluginButtonJoiningButton1EventControl;

static boolean buttonPress(int8u button, int8u state);
static int32u  buttonPressDurationMs = 0; 

#define BUTTON_HOLD_DURATION_MS 5000

//------------------------------------------------------------------------------
// Globals

#define buttonEvent0 emberAfPluginButtonJoiningButton0EventControl
#define buttonEvent1 emberAfPluginButtonJoiningButton1EventControl

#define PERMIT_JOIN_TIMEOUT EMBER_AF_PLUGIN_BUTTON_JOINING_PERMIT_JOIN_TIMEOUT

//------------------------------------------------------------------------------

void emberAfPluginButtonJoiningButton0EventHandler(void)
{
  emberEventControlSetInactive(buttonEvent0);

	if (buttonPressDurationMs >= BUTTON_HOLD_DURATION_MS) {
    emberAfCorePrintln("Leaving network due to button press.");
		emberLeaveNetwork();
		return;
	}

  if (emberNetworkState() == EMBER_JOINED_NETWORK) {

		emberAfBroadcastPermitJoin(PERMIT_JOIN_TIMEOUT);

  } else if (emberNetworkState() == EMBER_NO_NETWORK) {

#ifdef EMBER_AF_HAS_COORDINATOR_NETWORK
    emberAfCorePrintln("%p: nwk down: do form", "button0");
    emberAfFindUnusedPanIdAndForm();
#else
    emberAfCorePrintln("%p: nwk down: do join", "button0");
    emberAfStartSearchForJoinableNetwork();
#endif

  }
}

void emberAfPluginButtonJoiningButton1EventHandler(void)
{
	emberEventControlSetInactive(buttonEvent1);
	emberAfPluginButtonJoiningButtonEventCallback(1, // button 1 is pressed
                                                buttonPressDurationMs);  
}

void emberAfPluginButtonJoiningPressButton(int8u button)
{
	// We don't bother to check the button press both times
	// since the only reason it fails is invalid button.
	// No point in doing that twice.
  boolean result = buttonPress(button, BUTTON_PRESSED);
	buttonPress(button, BUTTON_RELEASED);
  if (!result) {
    emberAfCorePrintln("Invalid button %d", button);
  }
}

// ISR context functions!!!

// WARNING: these functions are in ISR so we must do minimal
// processing and not make any blocking calls (like printf)
// or calls that take a long time.

static boolean buttonPress(int8u button, int8u state)
{
  // ISR CONTEXT!!!

	static int32u timeMs;
  boolean result = FALSE; 
	EmberEventControl* event;
	
  if (button == BUTTON0) {
    event = &buttonEvent0;
  } else if (button == BUTTON1) {
		event = &buttonEvent1;
  } else {
		return FALSE;
	}
	if (state == BUTTON_PRESSED) {
		buttonPressDurationMs = 0;
		timeMs = halCommonGetInt32uMillisecondTick();
  } else {
		buttonPressDurationMs = elapsedTimeInt32u(timeMs, halCommonGetInt32uMillisecondTick());
		emberEventControlSetActive(*event);
	}

  return TRUE;
}

void emberAfHalButtonIsrCallback(int8u button, int8u state)
{
  // ISR CONTEXT!!!
  buttonPress(button, state);
}

