/*
 * File: ebl.h
 * Description: em35x common ebl processing code
 *
 * Author(s): David Iacobone, diacobone@ember.com
 *            Lee Taylor, lee@ember.com
 *
 * Copyright 2009 by Ember Corporation. All rights reserved.                *80*
 */

#ifndef __EBL_H__
#define __EBL_H__

#include "bootloader-common.h"  // for BL_Status
#include "bootloader-ccm.h"

/////////////////////
// Definitions related to the EBL file format

#define IMAGE_SIGNATURE 0xE350

#define EBL_MAX_TAG_SIZE         (2048+32)
#define EBL_MIN_TAG_SIZE         (128)

#define EBLTAG_HEADER           0x0000
#define EBLTAG_PROG             0xFE01
#define EBLTAG_MFGPROG          0x02FE
#define EBLTAG_ERASEPROG        0xFD03
#define EBLTAG_END              0xFC04
// Tags for encrypted EBL files
#define EBLTAG_ENC_HEADER       0xFB05
#define EBLTAG_ENC_INIT         0xFA06
#define EBLTAG_ENC_EBL_DATA     0xF907  // Encrypted EBL data
#define EBLTAG_ENC_MAC          0xF709

typedef struct {
	int16u tagId;
  int16u structSize;
  int16u minSerializedLength;
  boolean canBeBigger;
	const char* tagName;
} EblTagInfo;

// Definitions to control the encrypted EBL state machine
#define EBL_STATE_ENC_MASK              (0x80)
#define EBL_STATE_ENC_START             (EBL_STATE_ENC_MASK | 0)
#define EBL_STATE_ENC_INIT              (EBL_STATE_ENC_MASK | 1)
#define EBL_STATE_ENC_RCVDATA           (EBL_STATE_ENC_MASK | 2)
#define EBL_STATE_ENC_RCVDATA_RESUME    (EBL_STATE_ENC_MASK | 3)
#define EBL_STATE_ENC_MAC_DONE          (EBL_STATE_ENC_MASK | 4)
#define EBL_STATE_ENC_DONE              (EBL_STATE_ENC_MASK | 5)
#define EBL_STATE_ENC_INVALID           (EBL_STATE_ENC_MASK | 0xF)

// Current version of the ebl format.   The minor version (LSB) should be
//  incremented for any changes that are backwards-compatible.  The major
//  version (MSB) should be incremented for any changes that break 
//  backwards compatibility. The major version is verified by the ebl 
//  processing code of the bootloader.
#define EBL_VERSION              (0x0201)
#define EBL_MAJOR_VERSION        (0x0200)
#define EBL_MAJOR_VERSION_MASK   (0xFF00)

// EBL data fields are stored in network (big) endian order
//  however, fields referenced within the aat of the ebl header are stored
//  in native (little) endian order.
typedef struct      pageRange_s
{
  int8u       begPg;          /* First flash page in range    */
  int8u       endPg;          /* Last  flash page in range    */
}                   pageRange_t;

#define AAT_MAX_SIZE 128

typedef struct eblHdr3xx_s
{
  int16u      tag;            /* = EBLTAG_HEADER              */
  int16u      len;            /* =                            */
  int16u      version;        /* Version of the ebl format    */
  int16u      signature;      /* Magic signature: 0xE350      */
  int32u      flashAddr;      /* Address where the AAT is stored */
  int32u      aatCrc;         /* CRC of the ebl header portion of the AAT */
  // aatBuff is oversized to account for the potential of the AAT to grow in
  //  the future.  Only the first 128 bytes of the AAT can be referenced as
  //  part of the ebl header, although the AAT itself may grow to 256 total
  int8u       aatBuff[AAT_MAX_SIZE];   /* buffer for the ebl portion of the AAT    */
} eblHdr3xx_t;

typedef struct      eblProg3xx_s
{
  int16u      tag;            /* = EBLTAG_[ERASE|MFG]PROG     */
  int16u      len;            /* = 2..65534                   */
  int32u      flashAddr;      /* Starting addr in flash       */
  int8u*      flashData;      /* must be multiple of 2 */
}                   eblProg3xx_t;

typedef struct      eblEnd_s
{
  int16u      tag;            /* = EBLTAG_END                 */
  int16u      len;            /* = 4                          */
  int32u      eblCrc;         /* .ebl file CRC -Little-Endian-*/
}                   eblEnd_t;

#define EBL_ENCRYPT_HEADER_LENGTH 6  // does not include tag/length overhead
typedef struct eblEncHdr3xx_s
{
  int16u      tag;            /* = EBLTAG_ENC_HEADER          */
  int16u      len;            /* =   6                         */
  int16u      version;        /* Version of the EBL format    */
  int16u      encType;        /* Type of encryption used      */
  int16u      signature;      /* Magic signature: 0xE350      */
}                   eblEncHdr3xx_t;

#define EBL_ENCRYPT_INIT_LENGTH (4 + NONCE_LENGTH)  // does not include tag/length overhead
typedef struct eblEncInit3xx_s
{
  int16u      tag;            /* = EBLTAG_ENC_INIT            */
  int16u      len;            /* =                            */
  int32u      msgLen;         /* Length of the cipher text in bytes */
  int8u       nonce[NONCE_LENGTH];  /* Random nonce used for this message */
  int8u*      associatedData; /* Data that is authenticated but unencrypted
															   (must be multiple of 2) */
}                   eblEncInit3xx_t;

typedef struct      eblEncData3xx_s
{
  int16u      tag;            /* = EBLTAG_ENC_EBL_DATA */
  int16u      len;            /* = 2..65534                   */
  int8u*      data;           /* = encrypted data (must be multiple of 2) */
}                   eblEncData3xx_t;

typedef struct      eblEncMac_s
{
  int16u      tag;            /* = EBLTAG_ENC_MAC             */
  int16u      len;            /* = 16                          */
  int8u       eblMac[MAC_LENGTH]; /* = Message Authenticity Check of the data */
}                   eblEncMac_t;

// Define the types of encryption known for EBL files
#define ENC_TYPE_NONE  (0x0000)
#define ENC_TYPE_CCM   (0x0001)

#define EBL_SIZE_CAN_BE_BIGGER TRUE
#define EBL_SIZE_IS_EXACT      FALSE

#define EBL_HEADER_MIN_SIZE  12
#define EBL_PROGRAM_MIN_SIZE 6
#define EBL_END_MIN_SIZE     4
#define EBL_ENC_HDR_SIZE     6
#define EBL_INIT_HDR_SIZE    (4 + NONCE_LENGTH)
#define EBL_ENC_DATA_SIZE    2
#define EBL_ENC_MAC_SIZE     (MAC_LENGTH)

#define EBLTAG_INFO_STRUCT_CONTENTS                                     \
  { EBLTAG_HEADER,         sizeof(eblHdr3xx_t),     EBL_HEADER_MIN_SIZE,  EBL_SIZE_CAN_BE_BIGGER, "EBL Header" }, \
	{ EBLTAG_PROG,           sizeof(eblProg3xx_t),    EBL_PROGRAM_MIN_SIZE, EBL_SIZE_CAN_BE_BIGGER, "Program Data" }, \
  { EBLTAG_MFGPROG,        sizeof(eblProg3xx_t),    EBL_PROGRAM_MIN_SIZE, EBL_SIZE_CAN_BE_BIGGER, "MFG Program Data" }, \
  { EBLTAG_ERASEPROG,      sizeof(eblProg3xx_t),    EBL_PROGRAM_MIN_SIZE, EBL_SIZE_CAN_BE_BIGGER, "Erase then Program Data" }, \
  { EBLTAG_END,            sizeof(eblEnd_t),        EBL_END_MIN_SIZE,     EBL_SIZE_IS_EXACT,      "EBL End Tag" }, \
	{ EBLTAG_ENC_HEADER,     sizeof(eblEncHdr3xx_t),  EBL_ENC_HDR_SIZE,     EBL_SIZE_IS_EXACT,      "EBL Header for Encryption" }, \
	{ EBLTAG_ENC_INIT,       sizeof(eblEncInit3xx_t), EBL_INIT_HDR_SIZE,    EBL_SIZE_CAN_BE_BIGGER, "EBL Init for Encryption" }, \
	{ EBLTAG_ENC_EBL_DATA,   sizeof(eblEncData3xx_t), EBL_ENC_DATA_SIZE,    EBL_SIZE_CAN_BE_BIGGER, "Encrypted EBL Data" }, \
  { EBLTAG_ENC_MAC,        sizeof(eblEncMac_t),     EBL_ENC_MAC_SIZE,     EBL_SIZE_IS_EXACT,      "MAC Data for Encryption"}, \
  { 0xFFFF,                0,                       0,                    FALSE,                  NULL },


/////////////////////
// ebl.c APIs


typedef struct {
  BL_Status (*eblGetData)(void *state, int8u *dataBytes, int16u len);
  BL_Status (*eblDataFinalize)(void *state);
} EblDataFuncType;

typedef struct {
  int32u fileCrc;
  boolean headerValid;
  eblHdr3xx_t eblHeader;
  int16u encType;
  int8u encEblStateMachine;
  boolean decryptEnabled;
  int32u byteCounter;
  ccmState_t encState;
} EblProcessStateType;

typedef struct {
  void *dataState;
  int8u *tagBuf;
  int16u tagBufLen;
  boolean returnBetweenBlocks;
  EblProcessStateType eblState;
} EblConfigType;

typedef int8u (*flashReadFunc)(int32u address);

// int8u is used as the return type below to avoid needing to include 
//   ember-types.h for EmberStatus
typedef struct {
  int8u (*erase)(int8u eraseType, int32u address);
  int8u (*write)(int32u address, int16u * data, int32u length);
  flashReadFunc read;
} EblFlashFuncType;


// passed in tagBuf/tagBufLen must be at least EBL_MIN_TAG_SIZE, need not be
//  larger than EBL_MAX_TAG_SIZE
void eblProcessInit(EblConfigType *config,
                    void *dataState,
                    int8u *tagBuf,
                    int16u tagBufLen,
                    boolean returnBetweenBlocks);

BL_Status eblProcess(const EblDataFuncType *dataFuncs, 
                     EblConfigType *config,
                     const EblFlashFuncType *flashFuncs);

#endif //__EBL_H__
