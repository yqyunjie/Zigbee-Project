// ****************************************************************************
// * ota-bootload-ncp.h
// *
// * SPI/UART Interface to bootloading the NCP from a connected host.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************


boolean emAfStartNcpBootloaderCommunications(void);
void emAfPostNcpBootload(boolean success);
boolean emAfRebootNcpAfterBootload(void);

// These primitives are called by the Xmodem code to pass data via
// the specific mechanism (UART or SPI).
boolean emAfBootloadSendData(const int8u *data, int16u length);
boolean emAfBootloadSendByte(int8u byte);
boolean emAfBootloadWaitChar(int8u *data, boolean expect, int8u expected);

