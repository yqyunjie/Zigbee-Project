// *******************************************************************
// * load-control-event-table-soc.c
// *
// * 250 specific code relade to the event table.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************
#include "../../include/af.h"
#include "../../util/common.h"
#include "load-control-event-table.h"

// necessary for supporting ECDSA
#include "stack/include/cbke-crypto-engine.h"

#include "app/framework/security/crypto-state.h"

void emberDsaSignHandler(EmberStatus status, EmberMessageBuffer message)
{
  // Message has been queued by the stack for sending.  Nothing more to do.
  emAfCryptoOperationComplete();

  if (status != EMBER_SUCCESS) {
    emAfNoteSignatureFailure();
  }
  
  emberAfDemandResponseLoadControlClusterPrintln("emberDsaSignHandler() returned 0x%x",
                                                 status);
}
