/** 
 * @file hal/micro/token.h
 * @brief Token system for storing non-volatile information.
 * See @ref token for documentation.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved. -->
 */

/** @addtogroup tokens
 * The token system stores such non-volatile information as the manufacturing
 * ID, channel number, transmit power, and various pieces of information
 * that the application needs to be persistent between device power cycles.
 * Tokens are stored in either internal EEPROM, external EEPROM, 
 * or Simulated EEPROM (in Flash).
 */
 
/** @addtogroup token 
 * There are three main types of tokens: 
 * - <b>Manufacturing tokens:</b> Tokens that are set at the factory and 
 *   must not be changed through software operations.
 * - <b>Stack-level tokens:</b> Tokens that can be changed via the 
 *   appropriate stack API calls.  
 * - <b>Application level tokens:</b> Tokens that can be set via the  
 *   token system API calls in this file.
 *
 * The token system API controls writing non-volatile data to and reading
 * non-volatile data from the EEPROM. If an application wishes to use
 * non-volatile memory, it must do so by creating its own token header file
 * similar to token-stack.h. The macro 
 * <code>APPLICATION_TOKEN_HEADER</code> should be defined to
 * equal the name of the header file in which application tokens are defined.
 * 
 * Because the token system is based on memory locations within the EEPROM, 
 * the token information could become out of sync without some kind of
 * version tracking.  The two defines, <code>CURRENT_MFG_TOKEN_VERSION
 * </code> and <code>CURRENT_STACK_TOKEN_VERSION</code>, are used
 * to make sure the stack stays in sync with the proper token set in the proper
 * locations.
 *
 * The most general format of a token definition is:
 *
 * @code
 * #define CREATOR_name 16bit_value
 * #ifdef DEFINETYPES
 *    typedef data_type type
 * #endif //DEFINETYPES
 * #ifdef DEFINETOKENS
 *    DEFINE_*_TOKEN(name, type, ... ,defaults)
 * #endif //DEFINETOKENS
 * @endcode
 *
 * The defined CREATOR is used as a distinct identifier tag for the token.
 * The CREATOR is necessary because the token name is defined differently
 * depending on the hardware platform, so the CREATOR makes sure token
 * definitions and data stay tagged and known.  The only requirement
 * on these creator definitions is that they all must be unique.  A favorite
 * method for picking creator codes is to use two ASCII characters inorder
 * to make the codes more memorable.  The 'name' part of the
 * <code>\#define CREATOR_name</code> must match the 'name' provided in the
 * <code>DEFINE_*_TOKEN</code> because the token system uses this name 
 * to automatically link the two.
 *
 * The typedef provides a convenient and efficient abstraction of the token
 * data.  Since some tokens are structs with multiple pieces of data inside
 * of them, type defining the token type allows more efficient and readable
 * local copies of the tokens throughout the code.
 *
 * The typedef is wrapped with an <code>\#ifdef DEFINETYPES</code> because
 * the typdefs and token defs live in the same file, and DEFINETYPES is used to
 * select only the typedefs when the file is included.  Similarly, the 
 * <code>DEFINE_*_TOKEN</code> is wrapped with an 
 * <code>\#ifdef DEFINETOKENS</code> as a method for selecting only the
 * token definitions when the file is included.
 * 
 * 
 * The abstract definition, 
 * <code>DEFINE_*_TOKEN(name, type, ... ,defaults)</code>, has
 * seven possible complete definitions:<br>
 * <code>
 * <ul>
 *  <li>DEFINE_BASIC_TOKEN(name, type, ...)
 *  <li>DEFINE_INDEXED_TOKEN(name, type, arraysize, ...)
 *  <li>DEFINE_COUNTER_TOKEN(name, type, ...)
 *  <li>DEFINE_FIXED_BASIC_TOKEN(name, type, address, ...)
 *  <li>DEFINE_FIXED_INDEXED_TOKEN(name, type, arraysize, address, ...)
 *  <li>DEFINE_FIXED_COUNTER_TOKEN(name, type, address, ...)
 *  <li>DEFINE_MFG_TOKEN(name, type, address, ...)
 * </ul>
 * </code>
 * The three fields common to all <code>DEFINE_*_TOKEN</code> are:
 * name - The name of the token, which all information is tied to.
 * type - Type of the token which is the same as the typedef mentioned before.
 * ... - The default value to which the token is set upon initialization.
 * 
 * The difference between a FIXED and non-fixed token is the address at which
 * the token is stored.  For a FIXED token, the address is explicitly provided
 * in the token definition in the field:
 * address - Where the token is stored.  Do not conflict with other tokens!
 * A non-fixed token is dynamically placed.  Dynamic token placement begins in
 * the application token region which is directly above stack tokens.
 * Dynamic placement will pack the tokens directly next to eachother, as far
 * down in memory as possible until they hit a FIXED token.<br><br>
 *
 * @note  Be very careful when mixing FIXED tokens with non-fixed tokens!
 * Placing a FIXED token above dynamic tokens, but not far enough above them,
 * could cause token overlap!  Whenever the token set is changed, the Token
 * Utility application should not only be used to load in the new set of
 * default tokens, but it can also be used to view the token memory map for
 * any overlaps or conflicts.
 *
 * \b BASIC_TOKEN is the simplest definition and will be used for the majority
 * of tokens - tokens that are not indexed and are not counters.
 *
 * \b INDEXED_TOKEN should be used on tokens that look like arrays.
 * For example, data storage that looks like:<br>
 * <pre><code>   int32u myData[5]</code></pre><br>
 * can be a token with typedef of int32u and defined as INDEXED with arraysize
 * of 5.  The extra field in this token definition is:
 * arraysize - The number of elements in the indexed token.
 * 
 * \b COUNTER_TOKEN should be used on tokens that are simple numbers.
 * The reason for using COUNTER_TOKEN instead of BASIC_TOKEN is the
 * special support that the token system provides for counters.  The function
 * call <code>halCommonIncrementCounterToken()</code> only operates on
 * counter tokens and is more efficient for incrementing simple numbers in the
 * token system.
 * 
 * \b DEFINE_MFG_TOKEN operates almost identically to DEFINE_FIXED_BASIC_TOKEN,
 * in that it defines a simple scalar token at a specific address.  The major
 * difference is this token is designated manufacturing, which means certain
 * functions and utilities treat it differently from stack or app tokens.  For
 * example, the Token Utility has commands for loading default values, and
 * these commands make a distinction between manufacturing versus non
 * manufacturing tokens (since generally manufacturing tokens should never
 * be changed or reset).
 *
 * Here is an example of two application tokens:
 *
 * @code
 * #define CREATOR_SENSOR_NAME        0x5354
 * #define CREATOR_SENSOR_PARAMETERS  0x5350
 * #ifdef DEFINETYPES
 *   typedef int8u tokTypeSensorName[10];
 *   typedef struct {
 *     int8u initValues[5];
 *     int8u reportInterval;
 *     int16u calibrationValue;
 *   } tokTypeSensorParameters;
 * #endif //DEFINETYPES
 * #ifdef DEFINETOKENS
 *   DEFINE_BASIC_TOKEN(SENSOR_NAME,
 *                      tokTypeSensorName,
 *                      {'U','N','A','M','E','D',' ',' ',' ',' '})
 *   DEFINE_BASIC_TOKEN(SENSOR_PARAMETERS,
 *                      tokTypeSensorParameters,
 *                      {{0x01,0x02,0x03,0x04,0x05},5,0x0000})
 * #endif //DEFINETOKENS
 * @endcode
 *
 * And, here is an example of how to use the two application tokens:
 * @code
 * {
 *   tokTypeSensorName sensor;
 *   tokTypeSensorParameters params;
 *
 *   halCommonGetToken(&sensor, TOKEN_SENSOR_NAME);
 *   halCommonGetToken(&params, TOKEN_SENSOR_PARAMETERS);
 *   if(params.calibrationValue == 0xBEEF) {
 *     params.reportInterval = 5;
 *   }
 *   halCommonSetToken(TOKEN_SENSOR_PARAMETERS, &params);
 * }
 * @endcode
 *
 * See token-stack.h to see the default set of tokens and their values.
 *
 *
 * @note A note on token initialization: The Token Utility should be used
 * for initializing tokens (new or changed) and viewing the layout of tokens
 * in storage (to check for conflicts).
 * It is always possible to create an application that will write the
 * initialized values.  See the Token Utility source for an example.
 *
 * @note Further details on exact implementation can be found in code
 * comments in the stack/config/token-stack.h file, the platform specific
 * token-manufacturing.h file, the platform specific token.h file, and the
 * platform specific token.c file.
 *
 * Some functions in this file return an EmberStatus value. See 
 * stack/include/error-def.h in the <i>EmberZNet Stack API Guide</i>
 * (120-3006-000) for definitions of EmberStatus values.
 *
 * See hal/micro/token.h for source code. 
 *
 *@{ 
 */

#ifndef __TOKEN_H__
#define __TOKEN_H__

#if defined(AVR_ATMEGA)
  #include "avr-atmega/token.h"
#elif defined(MSP430)
  #include "msp430/token.h"
#elif defined(XAP2B)
  #include "xap2b/token.h"
#elif defined(EMBER_TEST)
  #include "generic/token-ram.h"
#else
  #error invalid platform
#endif


/**
 * @brief Initializes and enables the token system. Checks if the
 * manufacturing and stack non-volatile data versions are correct.
 *
 * @return An EmberStatus value indicating the success or
 *  failure of the command.
 */
EmberStatus halStackInitTokens(void);

// NOTE:
// The following API is actually macros redefined in the platform
// specific token files and provide a bit of abstraction that can allow
// easy and efficient access to tokens in different implementations

//The actual defintions for the defines below are in the platform specific
//token.h file.  The defines below are for documentation/doxygen purposes.
#ifdef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief Macro that copies the token value from non-volatile storage into a RAM
 * location.  This macro can only be used with tokens that are defined using
 * either DEFINE_BASIC_TOKEN or DEFINE_FIXED_BASIC_TOKEN.
 * For indexed tokens, use <code>halCommonGetIndexedToken()</code>.
 *
 * @note  To better understand the parameters of this macro, refer to the
 *           example of token usage above.
 *
 * @param data  A pointer to where the token data should be placed.
 *
 * @param token  The token name used in <code>DEFINE_*_TOKEN</code>,
 *  prepended with <code>TOKEN_</code>.
 */
#define halCommonGetToken( data, token )

/**
 * @brief Macro that copies the token value from non-volatile storage into a RAM
 * location.  This macro can only be used with tokens that are defined using
 * either DEFINE_INDEXED_TOKEN or DEFINE_FIXED_INDEXED_TOKEN.
 * For non-indexed tokens use halCommonGetToken().
 *
 * @note To better understand the parameters of this macro, refer to the
 *           example of token usage above.
 *
 * @param data   A pointer to where the token data should be placed.
 *
 * @param token  The token name used in <code>DEFINE_*_TOKEN</code>,
 *               prepended with <code>TOKEN_</code>.
 * @param index  The index to access in the array token.
 */
#define halCommonGetIndexedToken( data, token, index )

/**
 * @brief Macro that sets the value of a token in non-volatile storage.  This
 * macro can only be used with tokens that are defined using either
 * DEFINE_BASIC_TOKEN or DEFINE_FIXED_BASIC_TOKEN. For indexed
 * tokens use halCommonSetIndexedToken().
 *
 * @note  To better understand the parameters of this macro, refer to the
 *           example of token usage above.
 *
 * @param token The token name used in <code>DEFINE_*_TOKEN</code>,
 * prepended with <code>TOKEN_</code>.
 * 
 * @param data  A pointer to the data being written.
 */
#define halCommonSetToken( token, data )

/**
 * @brief Macro that sets the value of a token in non-volatile storage.  This
 * macro can only be used with tokens that are defined using either
 * DEFINE_INDEXED_TOKEN or DEFINE_FIXED_INDEXED_TOKEN.
 * For non-indexed tokens, use halCommonSetToken().
 *
 * @note  To better understand the parameters of this macro, refer to the
 *           example of token usage above.
 *
 * @param token  The token name used in <code>DEFINE_*_TOKEN</code>,
 * prepended with <code>TOKEN_</code>.
 *
 * @param index  The index to access in the array token.
 *
 * @param data   A pointer to where the token data should be placed.
 */
#define halCommonSetIndexedToken( token, index, data )

 /**
 * @brief Macro that increments the value of a token that is a counter.  This
 * macro can only be used with tokens that are defined using either
 * DEFINE_COUNTER_TOKEN or DEFINE_FIXED_COUNTER_TOKEN.
 *
 * @note  To better understand the parameters of this macro, refer to the
 *           example of token usage above.
 *
 * @param token  The token name used in <code>DEFINE_*_TOKEN</code>,
 * prepended with <code>TOKEN_</code>.
 */
#define halCommonIncrementCounterToken( token )

#endif //DOXYGEN_SHOULD_SKIP_THIS



// the interfaces that follow serve only as a glue layer
// to link creator codes to tokens (primarily for *test code)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #define INVALID_EE_ADDRESS 0xFFFF
  int16u getTokenAddress(int16u creator);
  int8u getTokenSize(int16u creator );
  int8u getTokenArraySize(int16u creator);
#endif //DOXYGEN_SHOULD_SKIP_THIS


#endif // __TOKEN_H__

/**@} // END token group
 */
  

