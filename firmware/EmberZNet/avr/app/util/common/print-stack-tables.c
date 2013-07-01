// File: print-stack-tables.c
//
// Description: functions to print stack tables for use in troubleshooting.
//
// Author(s): Matteo Paris, matteo@ember.com
//
// Copyright 2006 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"

#include "print-stack-tables.h"

void printNeighborTable(int8u serialPort)
{
  EmberNeighborTableEntry n;
  int8u i;

  for (i = 0; i < emberNeighborCount(); i++) {
    emberGetNeighbor(i, &n);
    emberSerialPrintf(serialPort, 
            "id:%2X lqi:%d in:%d out:%d age:%d eui:(>)%X%X%X%X%X%X%X%X\r\n",
                      n.shortId,
                      n.averageLqi,
                      n.inCost,
                      n.outCost,
                      n.age,
                      n.longId[7], n.longId[6], n.longId[5], n.longId[4], 
                      n.longId[3], n.longId[2], n.longId[1], n.longId[0]);
    emberSerialWaitSend(serialPort);
    emberSerialBufferTick();
  }
  if (emberNeighborCount() == 0) {
    emberSerialPrintf(serialPort, "empty neighbor table\r\n");
  }
}

PGM_NO_CONST PGM_P zigbeeRouteStatusNames[] =
  {
    "active",
    "discovering",
    "",
    "unused",
  };

void printRouteTable(int8u serialPort)
{
  EmberRouteTableEntry r;
  int8u i;
  boolean hit = FALSE;
  for (i = 0; i < EMBER_ROUTE_TABLE_SIZE; i++) {
    if (emberGetRouteTableEntry(i, &r) == EMBER_SUCCESS) {
      hit = TRUE;
      emberSerialPrintf(serialPort,
                        "id:%2X next:%2X status:%p age:%d\r\n",
                        r.destination,
                        r.nextHop,
                        zigbeeRouteStatusNames[r.status],
                        r.age);
      emberSerialWaitSend(serialPort);
    }
  }
  if (! hit)
    emberSerialPrintf(serialPort, "empty route table\r\n");
}
