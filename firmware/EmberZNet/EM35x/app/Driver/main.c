// *******************************************************************
//  main.c
//  driver app for emberznet
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor/common.h"
#include "app/sensor/lcd.h"
#include <stdio.h>
#include "app/Driver/PWM.h"
#include "app/Driver/TWI.h"

// *******************************************************************
// Ember endpoint and interface configuration

int8u emberEndpointCount = 1;
EmberEndpointDescription PGM endpointDescription = { PROFILE_ID, 0, };
EmberEndpoint emberEndpoints[] = {
  { ENDPOINT, &endpointDescription },
};

// End Ember endpoint and interface configuration
// *******************************************************************

// *******************************************************************
// Application specific constants and globals

// Indicates if we are in MFG_MODE

#ifdef USE_MFG_CLI
boolean mfgMode = FALSE;
boolean  mfgCmdProcessing(void);
#endif

//
// *******************************************************************

// *******************************************************************
// Begin main application loop

void main(void)
{
  int16u time;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }

  // emberInit must be called before other EmberNet stack functions
  emberInit();

  // print the reason for the reset
  emberSerialGuaranteedPrintf(APP_SERIAL, "reset: %p\r\n",
                              (PGM_P)halGetResetString());
#ifdef  PHY_BRIDGE
  #ifdef CORTEXM3
    if (halResetWasCrash()) {
      halPrintCrashSummary(APP_SERIAL);
      halPrintCrashDetails(APP_SERIAL);
      halPrintCrashData(APP_SERIAL);
    }
  #endif
#endif//PHY_BRIDGE

  emberSerialGuaranteedPrintf(APP_SERIAL,
							  "PWM Driver Example \r\nBuild on: "__TIME__" "__DATE__"\r\n");

  #ifdef USE_BOOTLOADER_LIB
    bootloadUtilInit(APP_SERIAL, APP_SERIAL);
  #endif

  /** timer initial. */
  {
	const PWM_TypeDef pwm = {
		.chTMR = 1,
        .chCCR = 4,
		.clkSel = timerPCK12MHZ,
		.prescale = timerPrescale1,
        .mod.freq = 2000,
		.mod.duty = 90,
	};
    //PWM_Init(&pwm);
	//pwm_init(1000, 50);   //freq = 1000hz duty = 80%
  }

  twi_init();

  // event loop
  while(TRUE) {

    halResetWatchdog();
    emberTick();
    emberFormAndJoinTick();
	time = halCommonGetInt16uMillisecondTick();
	if( 0 == ( time % 1000) ) {
	  	twi_wr( LM73_DEVICE_ADDRESS, 0 );
		twi_rd( LM73_DEVICE_ADDRESS );
    	twi_wr( LM73_DEVICE_ADDRESS, 7 );
		twi_rd( LM73_DEVICE_ADDRESS );
		//twi_wr( TSL2550_DEVICE_ADDRESS, 0X83 );
		//twi_rd( TSL2550_DEVICE_ADDRESS );
	}
    #ifdef DEBUG
      emberSerialBufferTick();   // Needed for debug which uses buffered serial
    #endif
  }
}

// end main application loop
// *******************************************************************


// Called when a message has completed transmission --
// status indicates whether the message was successfully
// transmitted or not.
void emberMessageSentHandler(EmberOutgoingMessageType type,
                      int16u indexOrDestination,
                      EmberApsFrame *apsFrame,
                      EmberMessageBuffer message,
                      EmberStatus status)
{

}

#ifdef USE_BOOTLOADER_LIB
// When a device sends out a bootloader query, the bootloader
// query response messages are parsed by the bootloader-util
// libray and and handled to this function. This application
// simply prints out the EUI of the device sending the query
// response.
void bootloadUtilQueryResponseHandler(boolean bootloaderActive,
                                      int16u manufacturerId,
                                      int8u *hardwareTag,
                                      EmberEUI64 targetEui,
                                      int8u bootloaderCapabilities,
                                      int8u platform,
                                      int8u micro,
                                      int8u phy,
                                      int16u blVersion)
{

}

// This function is called by the bootloader-util library
// to ask the application if it is ok to start the bootloader.
// This happens when the device is meant to be the target of
// a bootload. The application could compare the manufacturerId
// and/or hardwareTag arguments to known values to enure that
// the correct image will be bootloaded to this device.
boolean bootloadUtilLaunchRequestHandler(int16u manufacturerId,
                                         int8u *hardwareTag,
                                         EmberEUI64 sourceEui) {
  // TODO: Compare arguments to known values.

  // TODO: Check for minimum required radio signal strength (RSSI).

  // TODO: Do not agree to launch the bootloader if any of the above conditions
  // are not met.  For now, always agree to launch the bootloader.
  return TRUE;
}
#endif // USE_BOOTLOADER_LIB


void emberIncomingMessageHandler(EmberIncomingMessageType type,
                                 EmberApsFrame *apsFrame,
                                 EmberMessageBuffer message)
{

}

void emberPollHandler(EmberNodeId childId, boolean transmitExpected)
{

}

void emberChildJoinHandler(int8u index, boolean joining)
{

}

// this is called when the stack status changes
void emberStackStatusHandler(EmberStatus status)
{

}

void emberSwitchNetworkKeyHandler(int8u sequenceNumber)
{
  emberSerialPrintf(APP_SERIAL, "EVENT: network key switched, new seq %x\r\n",
                    sequenceNumber);
}

void emberScanErrorHandler(EmberStatus status)
{

}

void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{

}

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
}

// end Ember callback handlers
// *******************************************************************


// *******************************************************************
// Functions that use EmberNet

void checkButtonEvents(void) {

}


//
// *******************************************************************

// *******************************************************************
// Callback from the HAL when a button state changes
// WARNING: this callback is an ISR so the best approach is to set a
// flag here when an action should be taken and then perform the action
// somewhere else. In this case the actions are serviced in the
// applicationTick function
#if     (defined(BUTTON0) || defined(BUTTON1))
void halButtonIsr(int8u button, int8u state)
{

}
#endif//(defined(BUTTON0) || defined(BUTTON1))

// *******************************************************************
