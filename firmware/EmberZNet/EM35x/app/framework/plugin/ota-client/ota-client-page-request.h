// *****************************************************************************
// * ota-client-page-request.h
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading device specific file.
// * This file handles the page request feature for the client.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

enum {
  EM_AF_NO_PAGE_REQUEST              = 0,
  EM_AF_WAITING_PAGE_REQUEST_REPLIES = 1,
  EM_AF_RETRY_MISSED_PACKETS         = 2,
  EM_AF_PAGE_REQUEST_COMPLETE        = 3,
  EM_AF_BLOCK_ALREADY_RECEIVED       = 4,
  EM_AF_PAGE_REQUEST_ERROR           = 0xFF
};
typedef int8u EmAfPageRequestClientStatus;

#define EM_AF_PAGE_REQUEST_BLOCK_SIZE 32

// This routine returns a timer indicating how long we should wait for
// the page request responses to come in.  0 if there was an error.
int32u emAfInitPageRequestClient(int32u offsetForPageRequest, 
                                 int32u totalImageSize);
void emAfPageRequestTimerExpired(void);
boolean emAfHandlingPageRequestClient(void);
EmAfPageRequestClientStatus emAfGetCurrentPageRequestStatus(void);
EmAfPageRequestClientStatus emAfNoteReceivedBlockForPageRequestClient(int32u offset);
EmAfPageRequestClientStatus emAfNextMissedBlockRequestOffset(int32u* nextOffset);

int32u emAfGetPageRequestMissedPacketDelayMs(void);
int32u emAfGetFinishedPageRequestOffset(void);
void emAfAbortPageRequest(void);


