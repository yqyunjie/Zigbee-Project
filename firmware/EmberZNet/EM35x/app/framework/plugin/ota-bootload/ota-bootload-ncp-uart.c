// ****************************************************************************
// * ota-bootload-ncp.c
// *
// * Routines for bootloading from a linux host connected via UART to an NCP
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/util/common.h"
#include "app/framework/util/attribute-storage.h"
#include "enums.h"
#include "app/framework/util/af-main.h"

#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "hal/micro/bootloader-interface-standalone.h"

#include "ota-bootload-ncp.h"

#if defined(GATEWAY_APP)

#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include "app/ezsp-uart-host/ash-host-io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>

//------------------------------------------------------------------------------
// Globals

#define NULL_FILE_DESCRIPTOR  (-1)
static const char returnChar = 0x0A; // line feed 
static const char runChar =  '2';
static const char beginDownload = '1';
static const char xmodemStartChar = 'C';

#define BOOTLOADER_DELAY  4     // seconds
#define MAX_ERROR_LENGTH  255

// This is setup to be greater than the time between 'C' characters
// spit out by the bootloader to indicate that it is ready for an Xmodem
// transfer.
#define READ_TIMEOUT_SEC  3     

#define MAX_BYTES_WITHOUT_PROMPT 200

// We store the last few bytes so that we can check whether we received
// the expected bootloader prompt "BL >"
#define MAX_RECEIVED_BYTES  4
static int8u receivedBytesCache[MAX_RECEIVED_BYTES];
static const char menuPrompt[] = "BL >";

// This is sized to be big enough to read "\r\nbegin upload\r\n"
// with some additional extra fuzz.
#define MAX_BYTES_WITHOUT_XMODEM_START  20

//------------------------------------------------------------------------------
// Forward Declarations

static void delay(void);
static int checkFdForData(void);
static void storeReceivedByte(int8u byte);
static boolean checkForBootloaderMenu(void);
static boolean checkForXmodemStart(void);

#define errorPrint(...) fprintf(stderr, __VA_ARGS__)
#define messagePrint(...) printf(__VA_ARGS__)

static int serialFd = NULL_FILE_DESCRIPTOR;

//------------------------------------------------------------------------------

boolean emAfStartNcpBootloaderCommunications(void)
{
  serialFd = NULL_FILE_DESCRIPTOR;
  char errorString[MAX_ERROR_LENGTH];

  delay();
  messagePrint("Setting up serial port\n");

  if (EZSP_SUCCESS != ashSetupSerialPort(&serialFd,
                                         errorString,
                                         MAX_ERROR_LENGTH,
                                         TRUE)) {          // bootloader mode?
    errorString[MAX_ERROR_LENGTH - 1] = '\0';
    errorPrint("Error: Could not setup serial port: %s", errorString);
  }

  if (!checkForBootloaderMenu()) {
    return FALSE;
  }

  if (!emAfBootloadSendByte(beginDownload)) {
    errorPrint("Failed to set bootloader in download mode.\n");
    return FALSE;
  }

  return checkForXmodemStart();
}

static boolean checkForXmodemStart(void)
{
  int8u bytesSoFar = 0;
  while (bytesSoFar < MAX_BYTES_WITHOUT_XMODEM_START) {
    int status = checkFdForData();
    if (status <= 0) {
      // Timeout or error
      return FALSE;
    }

    int8u data;
    ssize_t bytes = read(serialFd, &data, 1);
    if (bytes < 0) {
      errorPrint("Read failed: %s\n", strerror(errno));
      return FALSE;
    }

    // debug
    //    printf("%c", (char)data);

    if (data == xmodemStartChar) {
      return TRUE;
    }
    bytesSoFar++;
  }

  errorPrint("Failed to get Xmodem start message from bootloader.\n");
  return FALSE;
}

boolean emAfBootloadSendData(const int8u *data, int16u length)
{
  if (length != write(serialFd, 
                      data, 
                      length)) {         // count
    errorPrint("Error: Failed to write to serial port: %s",
               strerror(errno));
    return FALSE;
  }
  fsync(serialFd);
  return TRUE;
}

boolean emAfBootloadSendByte(int8u byte)
{
  return emAfBootloadSendData(&byte, 1);
}

static boolean checkForBootloaderMenu(void)
{
  MEMSET(receivedBytesCache, 0, MAX_RECEIVED_BYTES);

  // Send a CR to the bootloader menu to trigger the prompt to echo back.
  if (!emAfBootloadSendByte(returnChar)) {
    return FALSE;
  }

  boolean done = FALSE;
  int totalBytes = 0;
  while (!done) {
    int status = checkFdForData();

    if (status <= 0) {
      return FALSE;
    }

    int8u data;
    ssize_t bytes = read(serialFd, &data, 1);
    if (bytes < 0) {
      errorPrint("Error: read() failed: %s\n", strerror(errno));
      return FALSE;
    } else if (bytes == 0) {
      continue;
    }
    totalBytes++;
    storeReceivedByte(data);

    // debug
    // printf("%c", data);

    if (0 == MEMCOMPARE(receivedBytesCache, menuPrompt, MAX_RECEIVED_BYTES)) {
      done = TRUE;
      continue;
    }
    
    if (totalBytes > MAX_BYTES_WITHOUT_PROMPT) {
      errorPrint("Got %d bytes without seeing the bootloader menu prompt.\n",
                 MAX_BYTES_WITHOUT_PROMPT);
      return FALSE;
    }
  }

  return TRUE;
}

boolean emAfRebootNcpAfterBootload(void)
{
  messagePrint("Rebooting NCP\n");

  if (!checkForBootloaderMenu()) {
    errorPrint("Failed to get bootloader menu prompt.\n");
    return FALSE;
  }

  if (!emAfBootloadSendByte(runChar)) {
    return FALSE;
  }

  delay();        // arbitrary delay to give NCP time to reboot.
  close(serialFd);
  serialFd = NULL_FILE_DESCRIPTOR;
  return TRUE;
}

static void storeReceivedByte(int8u newByte)
{
  // We right shift all the bytes in the array.  The first byte on the array
  // will be dumped, and the new byte will become the last byte.
  int8u i;
  for (i = 0; i < MAX_RECEIVED_BYTES - 1; i++) {
    receivedBytesCache[i] = receivedBytesCache[i+1];
  }
  receivedBytesCache[i] = newByte;
}

static void delay(void)
{
  // Empirically I have found that we have to delay to give the bootloader
  // time to launch before we see the LEDs flash and the bootloader menu
  // is available.  Not sure if this could be improved.
  messagePrint("Delaying %d seconds\n", BOOTLOADER_DELAY);
  sleep(BOOTLOADER_DELAY);
}

// CYGWIN NOTE
//   Cygwin does NOT properly handle select() with regard to serial ports.
//   I am not sure exactly what will happen in that case but I believe
//   it will not timeout.  If all goes well we will never get a timeout,
//   but if things go south we won't handle them gracefully.
static int checkFdForData(void)
{
  fd_set readSet;
  struct timeval timeout = { 
    READ_TIMEOUT_SEC,
    0,                  // ms. timeout value
  };
  
  FD_ZERO(&readSet);
  FD_SET(serialFd, &readSet);

  int fdsWithData = select(serialFd + 1,         // per the select() manpage
                                                 //   highest FD + 1
                           &readSet,             // read FDs
                           NULL,                 // write FDs
                           NULL,                 // exception FDs
                           (READ_TIMEOUT_SEC > 0 // passing NULL means wait 
                            ? &timeout           //   forever
                            : NULL));

  // Ideally we should check for EINTR and retry the operation.
  if (fdsWithData < 0) {
    errorPrint("FATAL: select() returned error: %s\n",
               strerror(errno));
    return -1;
  }

  if (fdsWithData == 0) {
    errorPrint("Error: Timeout occurred waiting for read data.\n");
  }
  return (fdsWithData);  // If timeout has occurred, this will be 0,
                         // otherwise it will be the number of FDs
                         // with data.
}

// TODO: would be better to make this work based on any qty of characters
//  being allowed to be received until the timeout is reached.  Current
//  behavior only looks at one character and waits up to the timeout for it to
//  arrive
boolean emAfBootloadWaitChar(int8u *data, boolean expect, int8u expected)
{
  int status = checkFdForData();
  if (status <= 0) {
    // Timeout or error
    return FALSE;
  }

  ssize_t bytes = read(serialFd, data, 1);
  if (bytes < 0) {
    errorPrint("Read failed: %s\n", strerror(errno));
    return FALSE;
  }

  // debug
  //printf("<%c,%x>", (char)*data, (char)*data);

  if(expect) {
    return ((*data)==expected);
  } else {
    return TRUE;
  }
}

void emAfPostNcpBootload(boolean success)
{
  // TODO:  Call an application callback.

  messagePrint("Rebooting host\n");
  halReboot();
}

#endif // Gateway app.
