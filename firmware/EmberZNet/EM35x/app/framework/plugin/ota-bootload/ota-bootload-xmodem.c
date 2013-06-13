// ****************************************************************************
// * ota-bootload-xmodem.c
// *
// * Routines for sending data via xmodem
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/ota-bootload/ota-bootload-ncp.h"

#ifdef HAL_HOST
  #include "hal/host/crc.h"
#else //HAL_HOST
  #include "hal/micro/crc.h"
#endif //HAL_HOST

//------------------------------------------------------------------------------

//#define DEBUG_PRINT_ON
#if defined DEBUG_PRINT_ON
  #define DBGPRINT(...) emberAfOtaBootloadClusterPrintln(__VA_ARGS__)
#else
  #define DBGPRINT(...)
#endif

#define SOH   (0x01)
#define EOT   (0x04)
#define ACK   (0x06)
#define NAK   (0x15)
#define CAN   (0x18)

#define DATA_SIZE        (128)
#define HEADER_SIZE      (3)
#define FOOTER_SIZE      (2)
#define FULL_BLOCK_SIZE  (HEADER_SIZE + DATA_SIZE + FOOTER_SIZE)

#define CONTROL_OFFSET      (0)
#define BLOCK_NUM_OFFSET    (1)
#define BLOCK_COMP_OFFSET   (2)
#define DATA_OFFSET         (3)
#define CRC_H_OFFSET        (131)
#define CRC_L_OFFSET        (132)

#define UNINITIALIZED       (0)
#define START_TRANSMISSION  (1)
#define SENDING             (2)

static int8u state = UNINITIALIZED;
static int8u buffFinger;
static int8u dataBuff[DATA_SIZE];
static int8u blockNum;

//------------------------------------------------------------------------------

static boolean sendBlock(int8u blockNum, const int8u *data)
{
  int i;
  int retry = 5;
  int16u crc = 0;
  int8u status = NAK;
  int8u fullBlock[FULL_BLOCK_SIZE];
  
  fullBlock[CONTROL_OFFSET] = SOH;
  fullBlock[BLOCK_NUM_OFFSET] = blockNum;
  fullBlock[BLOCK_COMP_OFFSET] = ~blockNum;
  for(i=0; i<DATA_SIZE; i++) {
    crc = halCommonCrc16(data[i],crc);
    fullBlock[DATA_OFFSET + i] = data[i];
  }
  fullBlock[CRC_H_OFFSET] = HIGH_BYTE(crc);
  fullBlock[CRC_L_OFFSET] = LOW_BYTE(crc);
  
  while( (status == NAK) && (retry > 0) ) {
    //print "block %d (" % num,
    //for i in range(0,len(data)):
    //  print "%02x" % ord(data[i]),
    if(!emAfBootloadSendData(fullBlock, FULL_BLOCK_SIZE)) {
      DBGPRINT("sendBlock() fail 1");
      return FALSE;
    }
    retry--;
    if(!emAfBootloadWaitChar(&status, FALSE, 0)) {
      DBGPRINT("sendBlock() fail 2");
      return FALSE;
    }
    while( status == 'C' ) {
      // may have leftover C characters from start of transmission
      if(!emAfBootloadWaitChar(&status, FALSE, 0)) {
        DBGPRINT("sendBlock() fail 3");
        return FALSE;
      }
    } 
  }
  
  return (status == ACK);
}

void emAfInitXmodemState(boolean startImmediately)
{
  if(startImmediately) {
    // skip checking for 'C' characters
    state = SENDING;
  } else {
    state = START_TRANSMISSION;
  }
  
  buffFinger = 0;
  blockNum=1;
}

boolean emAfSendXmodemData(const int8u *data, int16u length, boolean finished)
{
  int8u rxData;
  int16u i;

  if (state == START_TRANSMISSION) {
    if (emAfBootloadWaitChar(&rxData, TRUE, 'C')) {
      DBGPRINT("sending\n");
      state = SENDING;
    } else {
      DBGPRINT("NoC\n");
      return FALSE;
    }
  }
  
  if(state == SENDING) {
    for(i=0; i<length; i++) {
      dataBuff[buffFinger++] = data[i];
      if(buffFinger >= DATA_SIZE) {
        DBGPRINT("block %d\n",blockNum);
        emberAfCoreFlush();
        if(!sendBlock(blockNum, dataBuff)) {
          DBGPRINT("sendblock err\n");
          emberAfCoreFlush();
          return FALSE;
        }
        buffFinger = 0;
        blockNum++;
      }
    }
    if( finished ) {
      if( buffFinger != 0 ) {
        // pad and send final block
        boolean result;
        while(buffFinger < DATA_SIZE) {
          dataBuff[buffFinger++] = 0xFF;
        }
        DBGPRINT("final block %d\n",blockNum);
        result = sendBlock(blockNum, dataBuff);
        if (!result) {
          return FALSE;
        }
        
      }
      DBGPRINT("EOT\n",blockNum);
      emAfBootloadSendByte(EOT);
      if (!emAfBootloadWaitChar(&rxData, TRUE, ACK)){
        DBGPRINT("NoEOTAck\n");
        return FALSE;
      }
    }
    
  } else {
    DBGPRINT("badstate\n");
    return FALSE;
  }
  return TRUE;
}

