/** @file ash-host-priv.h
 * @brief Private header for ASH Host functions
 * 
 * This file should be included only by ash-host-ui.c and ash-host.c.
 *
 * See @ref ash_util for documentation.
 *
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup ash_util
 *
 * See ash-host-priv.h.
 *
 *@{
 */

#ifndef __ASH_HOST_PRIV_H__
#define __ASH_HOST_PRIV_H__

// Defined in ash-host-ui.c
void ashTraceFrame(boolean sent);
void ashTraceEventRecdFrame(const char *string);
void ashTraceEventTime(const char *string);
void ashTraceDisconnected(int8u error);
void ashTraceArray(int8u *name, int8u len, int8u *data);
void ashTraceEzspFrameId(const char *message, int8u *ezspFrame);
void ashTraceEzspVerbose(char *format, ...);
void ashCountFrame(boolean sent);

// Defined in ash-host.c
int8u readTxControl(void);
int8u readRxControl(void);
int8u readAckRx(void);
int8u readAckTx(void);
int8u readFrmTx(void);
int8u readFrmReTx(void);
int8u readFrmRx(void);
int8u readAshTimeouts(void);

#endif //__ASH_HOST_PRIV_H___

/** @} // END addtogroup
 */
