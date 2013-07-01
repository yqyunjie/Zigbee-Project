/*
 * File: em250-ebl.h
 *
 * Author: Don Markuson (ArrAy Inc.)
 *
 * Copyright 2006 by Ember Corporation. All rights reserved.                *80*
 *
 * Description:
 *  Include file that describes .ebl format for EM250.
 *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!! WARNING !!!      Keep in sync with em250-ebl.xai
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */

#ifndef _EM250_EBL_H_
#define _EM250_EBL_H_


// From tool/etrim.c
#define POLYNOMIAL              0xEDB88320L
// This is the working unit of data for the app bootloader.  We want it as
// big as possible, but it must be a factor of the NVM page size and
// fit into a single Zigbee packet.  We choose  2^6 = 64 bytes.
// Note that these literals are also defined in bootloader-interface-app.h
#define BOOTLOADER_SEGMENT_SIZE_LOG2  6 
#define BOOTLOADER_SEGMENT_SIZE       (1 << BOOTLOADER_SEGMENT_SIZE_LOG2)

// use build options to define the image signature. Default to EM250. 
// Default used in em250-load.c due to lack of build options.
#define IMAGE_SIGNATURE_EM250   0xE250
#define IMAGE_SIGNATURE_EM260   0xE260
#define IMAGE_SIGNATURE         IMAGE_SIGNATURE_EM250   // default to EM250
#ifdef XAP2B_EM260
#undef IMAGE_SIGNATURE
#define IMAGE_SIGNATURE         IMAGE_SIGNATURE_EM260
#endif

#define IMAGE_PADDING           0xFF
#define INITIAL_CRC             0xFFFFFFFFL
#define CRC32_START             INITIAL_CRC
#define CRC32_END               0xDEBB20E3L // For CRC32 POLYNOMIAL run LSB-MSB

#define EBLTAG_HEADER           0x0000
#define EBLTAG_PROG             0xFE01
#define EBLTAG_MFGPROG          0x02FE
#define EBLTAG_ERASEPROG        0xFD03
#define EBLTAG_END              0xFC04

// N.B. These structures are defined to be BIG-ENDIAN within EM250 .ebl file
//      except as specifically noted

#define eblTLD(size)                                             \
        struct {                                                 \
          int16u  tag;        /* one of above EBLTAGs         */ \
          int16u  len;        /* = size                       */ \
          int8u   data[size]; /* contents based on tag        */ \
        }

typedef struct      pageRange_s
{
  int8u       begPg;          /* First flash page in range    */
  int8u       endPg;          /* Last  flash page in range    */
}                   pageRange_t;

typedef eblTLD(2)   eblGeneric_t;

typedef eblTLD(60)  eblHdrGeneric_t;
typedef struct      eblHdr_s
{
  int16u      tag;            /* = EBLTAG_HEADER              */
  int16u      len;            /* = 60                         */
  int16u      flashAddr;      /* Location in flash of header  */
  int16u      signature;      /* .ebl signature for EM250     */
  int32u      timestamp;      /* Unix epoch time of .ebl file */
  int32u      imageCrc;       /* CRC over following pageRanges*/
  pageRange_t pageRanges[6];  /* Flash pages used by app      */
  int8u       imageInfo[32];  /* PLAT-MICRO-PHY-BOARD string  */
  int32u      headerCrc;      /* Header CRC    -Little-Endian-*/
}                   eblHdr_t;

typedef eblTLD(4)   eblProgGeneric_t;
typedef struct      eblProg_s
{
  int16u      tag;            /* = EBLTAG_[ERASE|MFG]PROG     */
  int16u      len;            /* = 2..65534                   */
  int16u      flashAddr;      /* Starting addr in flash       */
  int16u      flashData[2/*len/2*/];
}                   eblProg_t;

typedef eblTLD(4)   eblEndGeneric_t;
typedef struct      eblEnd_s
{
  int16u      tag;            /* = EBLTAG_END                 */
  int16u      len;            /* = 4                          */
  int32u      eblCrc;         /* .ebl file CRC -Little-Endian-*/
}                   eblEnd_t;

#endif//_EM250_EBL_H_
