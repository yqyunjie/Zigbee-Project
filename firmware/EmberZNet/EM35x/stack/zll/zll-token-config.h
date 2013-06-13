// File: zll-token-config.h
//
// Description: ZigBee Light Link token definitions used by the stack.
//
// Copyright 2011 by Ember Corporation.  All rights reserved.               *80*


#if defined(DEFINETYPES)

typedef struct {
  int32u bitmask;
  int16u freeNodeIdMin;
  int16u freeNodeIdMax;
  int16u myGroupIdMin;
  int16u freeGroupIdMin;
  int16u freeGroupIdMax;
  int8u rssiCorrection;
} tokTypeStackZllData;

typedef struct {
  int32u bitmask;
  int8u keyIndex;
  int8u encryptionKey[16];
  int8u preconfiguredKey[16];
} tokTypeStackZllSecurity;

#endif //DEFINETYPES

#ifdef DEFINETOKENS

DEFINE_BASIC_TOKEN(STACK_ZLL_DATA,
                   tokTypeStackZllData,
                   { 
                     EMBER_ZLL_STATE_FACTORY_NEW, // bitmask
                     0x0000,                      // freeNodeIdMin
                     0x0000,                      // freeNodeIdMax
                     0x0000,                      // myGroupIdMin
                     0x0000,                      // freeGroupIdMin
                     0x0000,                      // freeGroupIdMax
                     0                            // rssiCorrection
                   })

DEFINE_BASIC_TOKEN(STACK_ZLL_SECURITY,
                   tokTypeStackZllSecurity,
                   {
                     0x00000000,
                     0x00,
                     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                   })

#endif
