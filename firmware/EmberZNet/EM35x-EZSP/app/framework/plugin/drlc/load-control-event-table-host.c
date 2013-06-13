// *******************************************************************
// * load-control-event-table-host.c
// *
// * Host specific code relade to the event table.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "load-control-event-table.h"
#include "app/framework/security/crypto-state.h"

void ezspDsaSignHandler(EmberStatus status, int8u messageLength, int8u* message)
{
  // Message has been queued by the stack for sending.  Nothing more to do.
  emAfCryptoOperationComplete();

  if (status != EMBER_SUCCESS) {
    emAfNoteSignatureFailure();
  }

  emberAfDemandResponseLoadControlClusterPrintln("ezspDsaSignHandler() returned 0x%x",
                                                 status);
}
