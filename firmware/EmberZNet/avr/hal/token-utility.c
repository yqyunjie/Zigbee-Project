/**
 * File: token-utility.c
 * Description: Token Utility functions.  This file, in combination with
 * token-utility-framework.c and token-utility-cmd.h, provide a simple user
 * interface to access and manipulate tokens as well as load token defaults.
 *
 * The token manipulation code in this file can be used as example code for
 * an application that wants similar functionality in their application.
 *
 * \b NOTE:  If there are application tokens and they are to be accessed
 * with this utility, don't forget to define APPLICATION_TOKEN_HEADER as
 * the file that contains those defitions.
 * For example: APPLICATION_TOKEN_HEADER="app/my-app/app-tokens.h"
 *
 * \b NOTE:  If a configuration.h file has been defined for your application,
 * don't forget to define CONFIGURATION_HEADER as the configuration file.
 *
 * To add the definition to the token utility app in IAR Embedded
 * Workbench IDE, open the app and chose the menu 'Project -> Options...'
 * then select 'C/C++ compiler -> Preprocessor' and add the definition to
 * the field 'Defined symbols'.
 *
 * Culprit(s): Brooks Barrett
 *
 * Copyright 2005 by Ember Corporation.  All rights reserved.               *80*
 */
#include PLATFORM_HEADER

// The following headers are for the serial interface and dealing with human
// input and interpreting commands.
#include <stdio.h>
#include <string.h>
#ifdef __ICCAVR__
#include <pgmspace.h> // sscanf_P
#else
#define sscanf_P sscanf
#define strcmp_P strcmp
#endif

// The base set of header files needed to operate this application and
// manipulate the tokens.
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"

#define SER232 serialPort
extern int8u serialPort;

// needed to build - see bug 5958
int8u linkMemory[128];
int8u* emPacketBufferLinks = linkMemory;

// A simple method of printing a string and checking for a 'Y or 'N' response.
static boolean choiceIsNo(PGM char string[])
{
  char buff[2];
  do {
    emberSerialPrintf(SER232, string);
    emberSerialReadLine(SER232, buff, sizeof(buff));  // does upcase
  } while ( buff[0] != 'Y' && buff[0] != 'N' );
  return( buff[0] == 'N' );
}



//Called by the TOKMAP command with no parameters.
void tokenMap(int16u param1, int16u param2)
{
  //We need a ptr to keep track of used token space to identify overlaps
  //in token positioning.
  int16u topOfTokensPtr = 0;
  
  emberSerialPrintf(SER232, "Token Memory Map:\r\n");
  emberSerialPrintf(SER232, "Start Address - End Address -- [creator] name\r\n");
  
  //We define DEFINETOKENS so when token-stack.h is included below, only the
  //token definitions are operated on as opposed to the token types.
  #define DEFINETOKENS
  //Treat the manufacturing tokens just like a normal token definition.
  #define TOKEN_MFG TOKEN_DEF
  //Turn each token's definition into a block of code which performs
  //the address check and print of the token's position in memory.
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)       \
    {                                                                  \
      /* Grab an easy copy of the start address */                     \
      int16u start = TOKEN_##name##_ADDRESS;                           \
      /* Grab an easy copy of the end address */                       \
      int16u end = TOKEN_##name##_END;                                 \
      /* When the end is one less than the start, token size is zero */\
      if(start==(end+1)) {                                             \
        emberSerialPrintf(SER232, "(0 size)  -- ");                    \
      } else {                                                         \
        /* Check if this token overlaps with any previous tokens */    \
        if(topOfTokensPtr>start) {                                     \
          emberSerialPrintf(SER232, "ERROR!  Token Overlap!!\r\n");    \
        } else {                                                       \
          /* No token overlap, so record the new top token */          \
          topOfTokensPtr = end;                                        \
        }                                                              \
        /* Show me the addresses of the token */                       \
        emberSerialPrintf(SER232, "%d - %d  -- ", start, end);         \
      }                                                                \
      /* Show me the token's creator and name */                       \
      emberSerialPrintf(SER232, "[%2X] ", creator);                    \
      emberSerialWriteString(SER232, #name );                          \
      emberSerialPrintf(SER232,"\r\n");                                \
    }
    //Now that we've defined the tokens to be a block of code, pull them in
    #include "stack/config/token-stack.h"
  //Release the manufacturing token definition.
  #undef TOKEN_MFG
  //Release the normal token definition.
  #undef TOKEN_DEF
  //Release the token definition blocks.
  #undef DEFINETOKENS
}


// Called by the TOKREAD command which passes in just the creator.
void tokenRead(int16u creator, int16u param2)
{
  int8u i, j, size, arraysize;
  int16u address;
  //We're expecting the smallest scalar token or indexed token element to be
  //no larger than 32 bytes.
  int8u data[32];
  
  //Obtain the basic token parameters from utility functions in token.c
  //These parameters are used below in the calls into the token system.
  size = getTokenSize(creator);
  address = getTokenAddress(creator);
  arraysize = getTokenArraySize(creator);
  //Check if the token exists, exit if it doesn't
  if (size == 0 || address == INVALID_EE_ADDRESS) {
    emberSerialPrintf(SER232, "not found\r\n");
    return;
  }
  
  //Show me the creator code and the size of an element.
  emberSerialPrintf(SER232,"[%2x](%d) =",creator,size);
  
  //If this is an indexed token, we're going to loop over each index.  If this
  //is not an indexed token, the array size should be 1 so we'll only perform
  //this loop once (which is what we want).  The arraysize parameter was
  //obtained above in the getTokenArraySize() call.
  for(j = 0; j < arraysize; j++) {
    if(arraysize == 1) {
      //We're trying to access a non-indexed token (denoted by arraysize = 1).
      //The address of the token was obtained above in the getTokenAddress()
      //call.  The index parameter is zero because as non-indexed token, we do
      //not want to offset.  The size parameter was obtained above in the
      //getTokenSize() call.
      halInternalGetTokenData(data, address, 0, size);
    } else {
      //We're trying to access an indexed token.  The address of the token
      //was obtained above in the getTokenAddress() call.  The index parameter
      //is being looped over with the for loop.  The size parameter was
      //obtained above in the getTokenSize() call.
      halInternalGetTokenData(data, address, j, size);
    }
    //Print out the token data we just obtained.
    for(i = 0; i < size; i++) {
      emberSerialPrintf(SER232, " %X", data[i]);
    }
  }
  emberSerialPrintf(SER232,"\r\n");
}


// Called by the TOKWRITE command which passes in just the creator.
void tokenWrite(int16u creator, int16u param2)
{
  char buff[3];//Each data entry is only 2 bytes plus a terminating character.
  int8u i, j, size, arraysize;
  int16u word;
  int16u address;
  //We're expecting the smallest scalar token or indexed token element to be
  //no larger than 32 bytes.
  int8u data[32];

  //Obtain the basic token parameters from utility functions in token.c
  //These parameters are used below in the calls into the token system.
  size = getTokenSize(creator);
  address = getTokenAddress(creator);
  arraysize = getTokenArraySize(creator);
  //Check if the token exists, exit if it doesn't
  if (size == 0 || address == INVALID_EE_ADDRESS) {
    emberSerialPrintf(SER232, "not found\r\n");
    return;
  }

  //If this is an indexed token, we're going to loop over each index.  If this
  //is not an indexed token, the array size should be 1 so we'll only perform
  //this loop once (which is what we want).  The arraysize parameter was
  //obtained above in the getTokenArraySize() call.
  for(j = 0; j < arraysize; j++) {
    //Gather data bytes for an element from the user and build the data array.
    for(i = 0; i < size; i++) {
      emberSerialPrintf(SER232, "data byte[%d of %d]:",
                                 (((j)*size)+i), ((size*arraysize)-1));
      emberSerialReadLine(SER232, buff, sizeof(buff));
      sscanf_P(buff, "%x", &word);
      data[i] = (word&0xFF);
    }
    if(arraysize == 1) {
      //We're trying to write a non-indexed token (denoted by arraysize = 1).
      //The address of the token was obtained above in the getTokenAddress()
      //call.  The index parameter is zero because as non-indexed token, we do
      //not want to offset.  The size parameter was obtained above in the
      //getTokenSize() call.
      halInternalSetTokenData(address, 0, data, size);
    } else {
      //We're trying to write an indexed token.  The address of the token
      //was obtained above in the getTokenAddress() call.  The index parameter
      //is being looped over with the for loop.  The size parameter was
      //obtained above in the getTokenSize() call.
      halInternalSetTokenData(address, j, data, size);
    }
  }
}


//Called by the TOKDUMP command with no parameters.
void tokenDump(int16u param1, int16u param2)
{
  emberSerialPrintf(SER232,"(data shown little endian)\r\n");
  
  //We define DEFINETOKENS so when token-stack.h is included below, only the
  //token definitions are operated on as opposed to the token types.
  #define DEFINETOKENS
  //Treat the manufacturing tokens just like a normal token definition.
  #define TOKEN_MFG TOKEN_DEF
  //Turn each token's definition into a block of code which performs
  //an internal GetTokenData call to read out that token and print it.
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)         \
    {                                                                    \
      int8s j;                                                           \
      /* Create an exact size array for the local copy of the data. */   \
      int8u i, dst[sizeof(type)];                                        \
      /* Tell me the creator code and name of the token. */              \
      emberSerialPrintf(SER232, "[%2X]", creator);                       \
      emberSerialWriteString(SER232, #name );                            \
      /* If this is an indexed token, we're going to loop over each */   \
      /* index.  If this is not an indexed token, the array size    */   \
      /* should be 1 so we'll only perform this loop once.          */   \
      for(j = 0; j < arraysize; j++) {                                   \
        if(arraysize == 1) {                                             \
          /* We're trying to access a non-indexed token (denoted by  */  \
          /* arraysize = 1).  The index parameter is zero because as */  \
          /* a non-indexed token, we do not want to offset.          */  \
          halInternalGetTokenData(dst, TOKEN_##name, 0, sizeof(type));   \
        } else {                                                         \
          /* We're trying to access an indexed token.  The index */      \
          /* parameter is being looped over with the for loop.   */      \
          halInternalGetTokenData(dst, TOKEN_##name, j, sizeof(type));   \
        }                                                                \
        /* Print out the token data we just obtained. */                 \
        for(i = 0; i < sizeof(type); i++) {                              \
          emberSerialPrintf(SER232, " %X", dst[i]);                      \
        }                                                                \
      }                                                                  \
      emberSerialPrintf(SER232,"\r\n");                                  \
    }
    //Now that we've defined the tokens to be a block of code, pull them in
    #include "stack/config/token-stack.h"
  //Release the manufacturing token definition.
  #undef TOKEN_MFG
  //Release the normal token definition.
  #undef TOKEN_DEF
  //Release the token definition blocks.
  #undef DEFINETOKENS
}


//Called by the LOADTOKS command with no parameters.
void loadStackAppTokens(int16u foo1, int16u foo2)
{
  //Ask the user to verify they want to erase current values and load defaults
  if ( choiceIsNo("Write default stack and app tokens?(y/n)") )
    return;

  emberSerialPrintf(SER232,"Writing Default Stack and App Tokens... wait.");

  //We define DEFINETOKENS so when token-stack.h is included below, only the
  //token definitions are operated on as opposed to the token types.
  #define DEFINETOKENS
  //Guarantee the normal token definitions are released first
  #undef TOKEN_DEF
  //Guarantee the manufacturing token definitions are released first
  #undef TOKEN_MFG
  //Define manufacturing tokens as nothing since we're not operating on them
  #define TOKEN_MFG(name,creator,iscnt,isidx,type,arraysize,...)
  //Turn each token's definition into a block of code which performs
  //an internal SetTokenData call to write the default value.
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)        \
    {                                                                   \
      int8s i;                                                          \
      /* Create a local copy of the token's default data */             \
      type data = __VA_ARGS__;                                          \
      if(arraysize==1)                                                  \
        /* We're trying to access a non-indexed token (denoted by  */   \
        /* arraysize = 1).  The index parameter is zero because as */   \
        /* a non-indexed token, we do not want to offset.          */   \
        halInternalSetTokenData(TOKEN_##name, 0, &data, sizeof(type));  \
      else {                                                            \
        /* This is an indexed token, we're going to loop over each */   \
        /* index.                                                  */   \
        for(i=0;i<arraysize;i++) {                                      \
          /* We're trying to access an indexed token.  The index */     \
          /* parameter is being looped over with the for loop.   */     \
          halInternalSetTokenData(TOKEN_##name, i, &data, sizeof(type));\
        }                                                               \
      }                                                                 \
    }
    //Now that we've defined the tokens to be a block of code, pull them in
    #include "stack/config/token-stack.h"
  //Release the normal token definition.
  #undef TOKEN_DEF
  //Release the manufacturing token definition.
  #undef TOKEN_MFG
  //Release the token definition blocks.
  #undef DEFINETOKENS
  
  emberSerialPrintf(SER232,"done.");
}


//Called by the LOADMFGTOKS command with no parameters.
void loadMfgTokens(int16u foo1, int16u foo2)
{
  //Ask the user to verify they want to erase current values and load defaults
  if ( choiceIsNo("Write default mfg tokens?(y/n)") )
    return;

  emberSerialPrintf(SER232,"Writing Default Manufacturing Tokens... wait.");

  //We define DEFINETOKENS so when token-stack.h is included below, only the
  //token definitions are operated on as opposed to the token types.
  #define DEFINETOKENS
  //Guarantee the normal token definitions are released first
  #undef TOKEN_DEF
  //Guarantee the manufacturing token definitions are released first
  #undef TOKEN_MFG
  //Define normal tokens as nothing since we're not operating on them
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)
  //Turn each token's definition into a block of code which performs
  //an internal SetTokenData call to write the default value.
  #define TOKEN_MFG(name,creator,iscnt,isidx,type,arraysize,...)        \
    {                                                                   \
      int8s i;                                                          \
      /* Create a local copy of the token's default data */             \
      type data = __VA_ARGS__;                                          \
      if(arraysize==1)                                                  \
        /* We're trying to access a non-indexed token (denoted by  */   \
        /* arraysize = 1).  The index parameter is zero because as */   \
        /* a non-indexed token, we do not want to offset.          */   \
        halInternalSetTokenData(TOKEN_##name, 0, &data, sizeof(type));  \
      else {                                                            \
        /* This is an indexed token, we're going to loop over each */   \
        /* index.                                                  */   \
        for(i=0;i<arraysize;i++) {                                      \
          /* We're trying to access an indexed token.  The index */     \
          /* parameter is being looped over with the for loop.   */     \
          halInternalSetTokenData(TOKEN_##name, i, &data, sizeof(type));\
        }                                                               \
      }                                                                 \
    }
    //Now that we've defined the tokens to be a block of code, pull them in
    #include "stack/config/token-stack.h"
  //Release the normal token definition.
  #undef TOKEN_DEF
  //Release the manufacturing token definition.
  #undef TOKEN_MFG
  //Release the token definition blocks.
  #undef DEFINETOKENS
  
  emberSerialPrintf(SER232,"done.");
}


//Called by the LOADMFGTOKS command with no parameters.
void loadAllTokens(int16u param1, int16u param2)
{
  //Ask the user to verify they want to erase current values and load defaults
  if ( choiceIsNo("Write all default tokens?(y/n)") )
    return;

  //"Erase" the EEPROM by setting it to all zeroes.
  {
    int16u i;
    int8u fill = 0x00;
    emberSerialPrintf(SER232,"Erasing EEPROM... wait.");
    for (i = 0; i < 0x400; i++) {  //There is a 1kByte limit on token space.
      //The internal SetToken function translates directly into an address
      //so we can pull a little trick here and walk the address space writing
      //the fill value;
      halInternalSetTokenData(i, 0, &fill, 1);
    }
    emberSerialPrintf(SER232,"done.\r\n");
  }

  emberSerialPrintf(SER232,"Writing Default Tokens... wait.");

  //We define DEFINETOKENS so when token-stack.h is included below, only the
  //token definitions are operated on as opposed to the token types.
  #define DEFINETOKENS
  //Guarantee the normal token definitions are released first
  #undef TOKEN_DEF
  //Guarantee the manufacturing token definitions are released first
  #undef TOKEN_MFG
  //Treat the manufacturing tokens just like a normal token definition.
  #define TOKEN_MFG TOKEN_DEF
  //Turn each token's definition into a block of code which performs
  //an internal SetTokenData call to write the default value.
  #define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...)        \
    {                                                                   \
      int8s i;                                                          \
      /* Create a local copy of the token's default data */             \
      type data = __VA_ARGS__;                                          \
      if(arraysize==1)                                                  \
        /* We're trying to access a non-indexed token (denoted by  */   \
        /* arraysize = 1).  The index parameter is zero because as */   \
        /* a non-indexed token, we do not want to offset.          */   \
        halInternalSetTokenData(TOKEN_##name, 0, &data, sizeof(type));  \
      else {                                                            \
        /* This is an indexed token, we're going to loop over each */   \
        /* index.                                                  */   \
        for(i=0;i<arraysize;i++) {                                      \
          /* We're trying to access an indexed token.  The index */     \
          /* parameter is being looped over with the for loop.   */     \
          halInternalSetTokenData(TOKEN_##name, i, &data, sizeof(type));\
        }                                                               \
      }                                                                 \
    }
    //Now that we've defined the tokens to be a block of code, pull them in
    #include "stack/config/token-stack.h"
  //Release the manufacturing token definition.
  #undef TOKEN_MFG
  //Release the normal token definition.
  #undef TOKEN_DEF
  //Release the token definition blocks.
  #undef DEFINETOKENS

  emberSerialPrintf(SER232,"done.");
}

