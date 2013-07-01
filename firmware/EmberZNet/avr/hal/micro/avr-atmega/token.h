/** 
 * @file hal/micro/avr-atmega/token.h
 * @brief AVR-ATMEGA Token system for storing non-volatile information.
 *
 * Please see @ref token for a full explanation of the tokens.
 * 
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved. -->
 */

/** @addtogroup token
 *
 * See also hal/micro/avr-atmega/token.h.
 */

#ifndef __PLAT_TOKEN_H__
#define __PLAT_TOKEN_H__

#ifndef __TOKEN_H__
#error do not include this file directly - include micro/token.h
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS

//no special handling for the manufacturing tokens
#define TOKEN_MFG TOKEN_DEF



//-- Build structure defines
/**
 * @brief Simple declarations of all of the token types so that they can
 * be referenced from anywhere in the code base.
 */
#define DEFINETYPES
  #include "stack/config/token-stack.h"
#undef DEFINETYPES



//-- Build parameter links
#define DEFINETOKENS
/**
 * @brief Macro for translating token definitions into size variables.
 * This provides a convenience for abstracting the 'sizeof(type)' anywhere.
 *
 * @param name  The name of the token.
 *
 * @param type  The token type.  The types are found in token-stack.h.
 */
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  TOKEN_##name##_SIZE = sizeof(type),
  enum {
    #include "stack/config/token-stack.h"
  };
#undef TOKEN_DEF




#define DEFINEADDRESSES
/**
 * @brief Macro for creating a 'region' element in the enum below.  This
 * creates an element in the enum that provides a starting point (address) for
 * subsequent tokens to align against.  ( See hal/micro/avr-atmega/token.c for
 * the instances of TOKEN_NEXT_ADDRESS() );
 *
 * @param region   The name to give to the element in the address enum..
 *
 * @param address  The address in EEPROM where the region begins.
 */
#define TOKEN_NEXT_ADDRESS(region, address)      \
  TOKEN_##region##_NEXT_ADDRESS = ((address) - 1),

/**
 * @brief Macro for creating ADDRESS and END elements for each token in
 * the enum below.  The ADDRESS element is linked to from the the normal
 * TOKEN_##name macro and provides the value passed into the internal token
 * system calls.  The END element is a placeholder providing the starting
 * point for the ADDRESS of the next dynamically positioned token.
 *
 * @param name       The name of the token.
 *
 * @param arraysize  The number of elements in an indexed token (arraysize=1
 * for scalar tokens).
 */
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  TOKEN_##name##_ADDRESS,                                      \
  TOKEN_##name##_END = TOKEN_##name##_ADDRESS +                \
                       (TOKEN_##name##_SIZE * arraysize) - 1,

/**
 * @brief The enum that operates on the two macros above.  Also provides
 * an indentifier so the address of the top of the token system can be known.
 */
enum {
  #include "stack/config/token-stack.h"
  TOKEN_MAXIMUM_SIZE
};
#undef TOKEN_DEF
#undef TOKEN_NEXT_ADDRESS
#undef DEFINEADDRESSES




/**
 * @brief Macro for translating token defs into address variables
 * that point to the correct location in the EEPROM.  (This is the extern,
 * the actual definition is found in hal/micro/avr-atmega/token.c)
 *
 * @param name     The name of the token.
 *
 * @param TOKEN_##name##_ADDRESS  The address in EEPROM at which the token
 * will be stored.  This parameter is generated with a macro above.
 */
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  extern const int16u TOKEN_##name;
    #include "stack/config/token-stack.h"
#undef TOKEN_DEF

/**
 * @brief Macro for typedef'ing the CamelCase token type found in
 * token-stack.h to a capitalized TOKEN style name that ends in _TYPE.
 * This macro allows other macros below to use 'token##_TYPE' to declare
 * a local copy of that token.
 *
 * @param name  The name of the token.
 *
 * @param type  The token type.  The types are found in token-stack.h.
 */
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  typedef type TOKEN_##name##_TYPE;
  #include "stack/config/token-stack.h"
#undef TOKEN_DEF
#undef DEFINETOKENS

#endif   //DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief Copies the token value from non-volatile storage into a RAM
 * location.  This is the internal function that the two exposed APIs
 * (::halCommonGetToken and ::halCommonGetIndexedToken) expand out to.  The
 * API simplifies the access into this function by hiding the size parameter
 * and hiding the value 0 used for the index parameter in scalar tokens.
 *
 * @param data   A pointer to where the data being read should be placed.
 *
 * @param token  The name of the token to get data from.  On this platform
 * that name is defined as an address.
 *
 * @param index  The index to access.  If the token being accessed is not an
 * indexed token, this parameter is set by the API to be 0.
 *
 * @param len    The length of the token being worked on.  This value is
 * automatically set by the API to be the size of the token.
 */
void halInternalGetTokenData(void *data, int16u token, int8u index, int8u len);

/**
 * @brief Sets the value of a token in non-volatile storage.  This is
 * the internal function that the two exposed APIs (::halCommonSetToken and
 * ::halCommonSetIndexedToken) expand out to.  The API simplifies the access
 * into this function by hiding the size parameter and hiding the value 0
 * used for the index parameter in scalar tokens.
 *
 * @param token  The name of the token to get data from.  On this platform
 * that name is defined as an address.
 *
 * @param index  The index to access.  If the token being accessed is not an
 * indexed token, this parameter is set by the API to be 0.
 *
 * @param data   A pointer to the data being written.
 *
 * @param len    The length of the token being worked on.  This value is
 * automatically set by the API to be the size of the token.
 */
void halInternalSetTokenData(int16u token, int8u index, void *data, int8u len);


#ifndef DOXYGEN_SHOULD_SKIP_THIS

// See hal/micro/token.h for the full explanation of the token API as
// instantiated below.

#define halCommonGetToken( data, token )                    \
  halInternalGetTokenData(data, token, 0, token##_SIZE)

#define halCommonGetIndexedToken( data, token, index )      \
  halInternalGetTokenData(data, token, index, token##_SIZE)
    
#define halStackGetIndexedToken( data, token, index, size ) \
  halInternalGetTokenData(data, token, index, size)
    
#define halCommonSetToken( token, data )                    \
  halInternalSetTokenData(token, 0, data, token##_SIZE)

#define halCommonSetIndexedToken( token, index, data )      \
  halInternalSetTokenData(token, index, data, token##_SIZE)

#define halStackSetIndexedToken( token, index, data, size ) \
  halInternalSetTokenData(token, index, data, size)

#define halCommonIncrementCounterToken( token )             \
  {                                                         \
    token##_TYPE data;                                      \
    halInternalGetTokenData(&data, token, 0, token##_SIZE); \
    data++;                                                 \
    halInternalSetTokenData(token, 0, &data, token##_SIZE); \
  }

#endif   //DOXYGEN_SHOULD_SKIP_THIS

// For use only by the AVR-ATmega 128 EZSP UART protocol
#ifdef EZSP_UART
  #ifdef AVR_ATMEGA_128
    #define halInternalMfgIndexedToken( type, address, index )  \
      *((type EEPROM *)(address) + index)
  #endif
#endif

#undef TOKEN_MFG

#endif // __PLAT_TOKEN_H__

