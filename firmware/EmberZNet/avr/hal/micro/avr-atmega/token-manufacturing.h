/** 
* @file token-manufacturing.h
* @brief Definitions for manufacturing tokens.
*
* This file should not be included directly. It is accessed by the other token
* files.
* 
* Please see @ref token for a full explanation of the tokens.
* 
* The tokens listed below are the manufacturing tokens.  This token definitions
* file is included from the master definitions file: stack/config/token-stack.h
* Please see that file for more details.
*
* Since this file contains both the typedefs and the token defs, there are
* two \#defines used to select which one is needed when this file is included.
* \#define DEFINETYPES is used to select the type definitions and
* \#define DEFINETOKENS is used to select the token definitions.
* Refer to token.h and token.c to see how these are used.
* 
* <!--Brooks Barrett-->
* <!-- Copyright 2005 by Ember Corporation. All rights reserved. -->
*/

/** @addtogroup token
 *
 * See also token-manufacturing.h.
 */

//////////////////////////////////////////////////////////////////////////////
// MANUFACTURING DATA
// *the addresses of these tokens must not change*


// MANUFACTURING CREATORS
// The creator codes are here in one list instead of next to their token
// definitions so comparision of the codes is easier.  The only requirement
// on these creator definitions is that they all must be unique.  A favorite
// method for picking creator codes is to use two ASCII characters inorder
// to make the codes more memorable.
#define CREATOR_MFG_NVDATA_VERSION        0xFF09
#define CREATOR_MFG_EUI_64                0xB634
#define CREATOR_MFG_EUI_64_HASH           0xB635
#define CREATOR_MFG_STRING                0xED73
#define CREATOR_MFG_RADIO_BANDS_SUPPORTED 0xF262
#define CREATOR_MFG_RADIO_CRYSTAL_OFFSET  0xE36F
#define CREATOR_MFG_BOARD_NAME            0xC24E // msb+'B'+'N' (Board Name)
#define CREATOR_MFG_MANUF_ID              0xC944 // msb+'I'+'D' (Id)
#define CREATOR_MFG_BOOTLOAD_AES_KEY      0xC24B // msb+'B'+'K' (Bootloader Key)
#define CREATOR_MFG_EZSP_STORAGE          0xCD53
#define CREATOR_MFG_ASH_CONFIG            0xC143 //  msb+'A'+'C' (ASH Config)
// The master define indicating the verions number these definitions work with.
#define CURRENT_MFG_TOKEN_VERSION 0x02FD //MSB is version, LSB is complement


#ifdef DEFINETYPES
typedef int16u tokTypeMfgNvdataVersion;
typedef int8u tokTypeMfgEui64[8];
typedef int8u tokTypeMfgEui64Hash[8];
typedef int8u tokTypeMfgString[16];
typedef int8u tokTypeMfgRadioBandsSupported;
typedef int8s tokTypeMfgRadioCrystalOffset;
typedef int8u tokTypeMfgBoardName[16];
typedef int16u tokTypeMfgManufId;
typedef int8u tokTypeMfgBootloadAesKey[16];
typedef int8u tokTypeMfgEzspStorage[8];
typedef int16u tokTypeMfgAshConfig;
#endif //DEFINETYPES

#ifdef DEFINETOKENS
// The Manufacturing tokens need to be stored at well-defined locations
//  None of these addresses should ever change without extremely great care
// reserved                               0      1  bytes
#define MFG_NVDATA_VERSION_LOCATION       1  //  2  bytes
#define MFG_EUI_64_LOCATION               3  //  8  bytes
#define MFG_EUI_HASH_LOCATION             11 //  8  bytes
#define MFG_STRING_LOCATION               19 //  16 bytes
#define MFG_RADIO_BANDS_SUPPORTED_LOC     35 //  1  bytes
#define MFG_RADIO_CRYSTAL_OFFSET_LOCATION 36 //  1  bytes
#define MFG_BOARD_NAME_LOCATION           37 //  16 bytes
#define MFG_MANUF_ID_LOCATION             53 //  2  bytes
#define MFG_BOOTLOAD_AES_KEY_LOCATION     55 //  16 bytes
#define MFG_ASH_CONFIG_LOCATION           71 //  40 bytes
#define MFG_EZSP_STORAGE_LOCATION         111 // 8  bytes
// reserved                               119    9  bytes
//                                        (do not exceed 128)

// Define the size of indexed token array
#define MFG_ASH_CONFIG_ARRAY_SIZE       20

// NOTE: because of their special handling, the manufacturing tokens
//  cannot use the convenience macros in their definitions, so the full
//  definitions are present here.
TOKEN_NEXT_ADDRESS(MFG_NVDATA_VERSION_ADDR,MFG_NVDATA_VERSION_LOCATION)
TOKEN_MFG(MFG_NVDATA_VERSION, CREATOR_MFG_NVDATA_VERSION,
          0, 0, tokTypeMfgNvdataVersion, 1,
          CURRENT_MFG_TOKEN_VERSION)
          
TOKEN_NEXT_ADDRESS(MFG_EUI_64_ADDR,MFG_EUI_64_LOCATION)
TOKEN_MFG(MFG_EUI_64, CREATOR_MFG_EUI_64,
          0, 0, tokTypeMfgEui64, 1,
          {3,0,0,0,0,0,0,3})
          
TOKEN_NEXT_ADDRESS(MFG_EUI_64_HASH_ADDR,MFG_EUI_HASH_LOCATION)
TOKEN_MFG(MFG_EUI_64_HASH, CREATOR_MFG_EUI_64_HASH,
          0, 0, tokTypeMfgEui64Hash, 1,
          {0,3,3,3,3,3,3,0})
          
TOKEN_NEXT_ADDRESS(MFG_STRING_ADDR,MFG_STRING_LOCATION)
TOKEN_MFG(MFG_STRING, CREATOR_MFG_STRING,
          0, 0, tokTypeMfgString, 1,
          {0,})
          
TOKEN_NEXT_ADDRESS(MFG_RADIO_BANDS_SUPPORTED_ADDR,MFG_RADIO_BANDS_SUPPORTED_LOC)
TOKEN_MFG(MFG_RADIO_BANDS_SUPPORTED, CREATOR_MFG_RADIO_BANDS_SUPPORTED,
          0, 0, tokTypeMfgRadioBandsSupported, 1,
          0x08)
          
TOKEN_NEXT_ADDRESS(MFG_RADIO_CRYSTAL_OFFSET_ADDR,MFG_RADIO_CRYSTAL_OFFSET_LOCATION)
TOKEN_MFG(MFG_RADIO_CRYSTAL_OFFSET, CREATOR_MFG_RADIO_CRYSTAL_OFFSET,
          0, 0, tokTypeMfgRadioCrystalOffset, 1,
          0)
TOKEN_NEXT_ADDRESS(MFG_BOARD_NAME_ADDR,MFG_BOARD_NAME_LOCATION)
TOKEN_MFG(MFG_BOARD_NAME, CREATOR_MFG_BOARD_NAME,
          0, 0, tokTypeMfgBoardName, 1,
          {0,})
          
TOKEN_NEXT_ADDRESS(MFG_MANUF_ID_ADDR,MFG_MANUF_ID_LOCATION)
TOKEN_MFG(MFG_MANUF_ID, CREATOR_MFG_MANUF_ID,
          0, 0, tokTypeMfgManufId, 1,
          0x0000) // default to 0 for ember
          
TOKEN_NEXT_ADDRESS(MFG_BOOTLOAD_AES_KEY_ADDR,MFG_BOOTLOAD_AES_KEY_LOCATION)
TOKEN_MFG(MFG_BOOTLOAD_AES_KEY, CREATOR_MFG_BOOTLOAD_AES_KEY,
          0, 0, tokTypeMfgBootloadAesKey, 1,
          {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})

TOKEN_NEXT_ADDRESS(MFG_ASH_CONFIG_ADDR,MFG_ASH_CONFIG_LOCATION)
TOKEN_MFG(MFG_ASH_CONFIG, CREATOR_MFG_ASH_CONFIG,
          0, 1, tokTypeMfgAshConfig, MFG_ASH_CONFIG_ARRAY_SIZE,
          { 0xFFFF, }) // defaults are all 0xFF's

TOKEN_NEXT_ADDRESS(MFG_EZSP_STORAGE_ADDR,MFG_EZSP_STORAGE_LOCATION)
TOKEN_MFG(MFG_EZSP_STORAGE, CREATOR_MFG_EZSP_STORAGE,
          0, 0, tokTypeMfgEzspStorage, 1,
          { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF })

#endif //DEFINETOKENS


