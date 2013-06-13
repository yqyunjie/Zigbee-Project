// *****************************************************************************
// * crypto-state.h
// *
// * This file records the state of crypto operations so that the application
// * can defer processing until after crypto operations have completed.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// For the host applications, if ECC operations are underway then
// the NCP will be completely consumed doing the processing for 
// SECONDS.  Therefore the application should not expect it to be
// very responsive.  Normal operations (cluster and app. ticks) will 
// not be fired during that period.

#ifndef CRYPTO_OPERATION_TIMEOUT_MS
#define CRYPTO_OPERATION_TIMEOUT_MS MILLISECOND_TICKS_PER_SECOND * 5
#endif //CRYPTO_OPERATION_TIMEOUT_MS

enum {
  EM_AF_NO_CRYPTO_OPERATION,
  EM_AF_CRYPTO_OPERATION_IN_PROGRESS,
};
typedef int8u EmAfCryptoStatus;

#define EM_AF_CRYPTO_STATUS_TEXT \
  {                                 \
    "No operation",                 \
    "Operation in progress",        \
    NULL                            \
  }

EmAfCryptoStatus emAfGetCryptoStatus(void);
void emAfSetCryptoStatus(EmAfCryptoStatus newStatus);

#define emAfSetCryptoOperationInProgress() \
  (emAfSetCryptoStatus(EM_AF_CRYPTO_OPERATION_IN_PROGRESS))

#define emAfIsCryptoOperationInProgress() \
  (EM_AF_CRYPTO_OPERATION_IN_PROGRESS == emAfGetCryptoStatus())

#define emAfCryptoOperationComplete() \
  (emAfSetCryptoStatus(EM_AF_NO_CRYPTO_OPERATION))

