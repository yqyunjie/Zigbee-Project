/**
 * File: token.c
 * Description:  See token.h for details.
 *
 * Author(s): Brooks Barrett
 *
 * Copyright 2005 by Ember Corporation. All rights reserved.                *80*
 */
#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "stack/include/error.h"
#include "stack/include/packet-buffer.h"
#include "hal/hal.h" 

#define DEFINETOKENS
#define TOKEN_MFG TOKEN_DEF
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  const int16u TOKEN_##name = TOKEN_##name##_ADDRESS;
  #include "stack/config/token-stack.h"
#undef TOKEN_DEF
#undef TOKEN_MFG
#undef DEFINETOKENS

#if defined(DEBUG)
#if !defined(HALTEST) && !defined(BOOTLOADER)
void resetSystemTokens(void)
{
/*
  this routine will prove useful when the developer changes the format of the
  stack related non-volatile data. for example while the developer is working
  on the code base.
  please note however, that this functionality isn't necessarily required in
  the embedded space, since an external script could easily handle the process
  of carefully reading, modifying, and re-uploading the critical manufacturing
  and/or stack related non-volatile data. thus - why it's only in the debug
  builds.
*/
// Must be careful here not to reset the manufacturing tokens to their
// default values.
  #define DEFINETOKENS
  #undef TOKEN_MFG
  #define TOKEN_MFG(name,creator,iscnt,isidx,type,arraysize,...)
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)        \
    {                                                                   \
      int8s i;                                                          \
      type data = __VA_ARGS__;                                          \
      if(arraysize==1)                                                  \
        halInternalSetTokenData(TOKEN_##name, 0, &data, sizeof(type));  \
      else {                                                            \
        for(i=0;i<arraysize;i++) {                                      \
          halInternalSetTokenData(TOKEN_##name, i, &data, sizeof(type));\
        }                                                               \
      }                                                                 \
    }
    #include "stack/config/token-stack.h"
  #undef TOKEN_MFG
  #undef TOKEN_DEF
  #undef DEFINETOKENS
}
#endif //!defined(HALTEST) && !defined(BOOTLOADER)
#endif //defined(DEBUG)


EmberStatus halStackInitTokens(void)
{
  tokTypeMfgNvdataVersion tokMfg;
  tokTypeStackNvdataVersion tokStack;
  
  // Due to hardware limitations, our maximum allowable storage for the token
  // system is 4096 bytes.  Forcefully enforce this limitation, since going
  // above the address 4096 could cause a hardware wrap of the address and
  // corrupt the manufacturing data at the bottom of token storage.
  assert(TOKEN_MAXIMUM_SIZE<0x1000);
  
  halCommonGetToken(&tokMfg, TOKEN_MFG_NVDATA_VERSION);
  halCommonGetToken(&tokStack, TOKEN_STACK_NVDATA_VERSION);

  // Applications are never supposed to touch the mfg tokens,
  // and I don't think we want to require customers to run 
  // another "token-upgrade" sort of thing, so we wind up needing
  // to put a small backward compatibility blip in halStackInitTokens()
  // where the version numbers are checked.
  if(CURRENT_MFG_TOKEN_VERSION != tokMfg) {
    if( tokMfg == 0x01FE ) {
      tokTypeMfgRadioCrystalOffset tokXtal = 0; //resonable default value
      tokMfg = 0x02FD;                       //up the version
      halCommonSetToken(TOKEN_MFG_RADIO_CRYSTAL_OFFSET, &tokXtal);
      halCommonSetToken(TOKEN_MFG_NVDATA_VERSION, &tokMfg);
    }
  }
  if(TOKEN_STACK_NVDATA_VERSION != tokStack) {
    if( tokStack == 0x02FD ) {
      #ifndef UPGRADE_NODE_AND_CLASSIC_TOKENS
        // STACK_NVDATA_VERSION 0x02FD is now out of date.  The difference
        // between 0x02FD and 0x03FC is the collection of a couple smaller
        // tokens into two larger, structured tokens which are in different
        // locations.  To perform an automatic, one-way upgrade from 0x02FD
        // to 0x03FC, simply #define UPGRADE_NODE_AND_CLASSIC_TOKENS above.
        assert(0);
      #else //UPGRADE_NODE_AND_CLASSIC_TOKENS
        int8u i;
        int8u EEPROM *ee;
        int8u ram[26];
        //move NODE_DATA
        ee = (int8u EEPROM *)130;
        ram[0] = *ee;
        ram[1] = *(ee+1);
        ee = (int8u EEPROM *)133;
        ram[2] = *ee;
        ee = (int8u EEPROM *)134;
        ram[3] = *ee;
        ee = (int8u EEPROM *)169;
        ram[4] = *ee;
        ee = (int8u EEPROM *)189;
        ram[5] = *ee;
        ee = (int8u EEPROM *)187;
        ram[6] = *ee;
        ram[7] = *(ee+1);
        halCommonSetToken(TOKEN_STACK_NODE_DATA, ram);      
        //move CLASSIC_DATA
        ee = (int8u EEPROM *)132;
        ram[0] = *ee;
        ee = (int8u EEPROM *)135;
        for( i = 0; i < 16; i++ )
          ram[1+i] = ee[i];
        ee = (int8u EEPROM *)151;
        ram[17] = *ee;
        ee = (int8u EEPROM *)154;
        ram[18] = *ee;
        ram[19] = *(ee+1);
        ee = (int8u EEPROM *)163;
        for( i = 0; i < 6; i++ )
          ram[20+i] = ee[i];
        halCommonSetToken(TOKEN_STACK_CLASSIC_DATA, ram);
        tokStack = 0x03FC;  //up the stack version
        halCommonSetToken(TOKEN_STACK_NVDATA_VERSION, &tokStack);
      #endif //UPGRADE_NODE_AND_CLASSIC_TOKENS
    }
  }


#if !defined(HALTEST) && !defined(BOOTLOADER)
  if (CURRENT_MFG_TOKEN_VERSION != tokMfg
      || CURRENT_STACK_TOKEN_VERSION != tokStack) {
    EmberStatus status;
    if((CURRENT_MFG_TOKEN_VERSION != tokMfg)
       && (CURRENT_STACK_TOKEN_VERSION != tokStack)) {
      status = EMBER_EEPROM_MFG_STACK_VERSION_MISMATCH;
    } else if(CURRENT_MFG_TOKEN_VERSION != tokMfg) {
      status = EMBER_EEPROM_MFG_VERSION_MISMATCH;
    } else if(CURRENT_STACK_TOKEN_VERSION != tokStack) {
      status = EMBER_EEPROM_STACK_VERSION_MISMATCH;
    }
    #if defined(DEBUG)
      // It would be nice if there were an emDebugError call.
      emberDebugPrintf("Err %x", status);
      resetSystemTokens();
    #else //  node release image
      // we have no other options
      return status;
    #endif
  }
#endif

  return EMBER_SUCCESS;
}

void halInternalGetTokenData(void *data, int16u address, int8u index, int8u len)
{
  int8u i;
  int8u *ram = (int8u*)data;
  int8u EEPROM *ee = (int8u EEPROM *)address;

  //base token address loaded.  offset to the index
  ee += (index * len);
  
  for( i = 0; i < len; i++ )
    ram[i] = ee[i];
}

void halInternalSetTokenData(int16u address, int8u index, void *data, int8u len)
{
  int8u i;
  int8u *ram = (int8u*)data;
  int8u EEPROM *ee = (int8u EEPROM *)address;

  //base token address loaded.  offset to the index
  ee += (index * len);
  
  for ( i = 0; i < len; i++ )
    ee[i] = ram[i];
  halResetWatchdog();
}



// The following interfaces are admittedly code space hogs but serve
// as glue interfaces to link creator codes to tokens (primarily for haltest)

int16u getTokenAddress(int16u creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
      case creator: return TOKEN_##name##_ADDRESS;
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  };
  #undef DEFINETOKENS
  return INVALID_EE_ADDRESS;
}

int8u getTokenSize(int16u creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
      case creator: return sizeof(type);
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  };
  #undef DEFINETOKENS
  return 0;  
}

int8u getTokenArraySize(int16u creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
      case creator: return arraysize;
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  };
  #undef DEFINETOKENS
  return 0;  
}

