#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"

#include "hal/micro/sim-eeprom.h"

#ifdef CORTEXM3
  //On the Cortex-M3 chips we define a variable that holds the actual
  //SimEE storage and then place this storage at the proper location
  //in the linker.
  #ifdef EMBER_TEST
    int8u simulatedEepromStorage[SIMEE_SIZE_B];
  #else //EMBER_TEST
    __root __no_init int8u simulatedEepromStorage[SIMEE_SIZE_B] @ __SIMEE__;
  #endif //EMBER_TEST
  //sim-eeprom-internal.c uses a set of defines that parameterize its behavior.
  //Since the -internal file is precompiled, we must externally define the
  //parameters (as constants) so an application build can alter them.
  const int8u REAL_PAGES_PER_VIRTUAL = ((SIMEE_SIZE_HW/FLASH_PAGE_SIZE_HW)/2);
  const int16u REAL_PAGE_SIZE = FLASH_PAGE_SIZE_HW;
  const int16u LEFT_BASE = SIMEE_BASE_ADDR_HW;
  const int16u LEFT_TOP = ((SIMEE_BASE_ADDR_HW + (FLASH_PAGE_SIZE_HW *
                          ((SIMEE_SIZE_HW / FLASH_PAGE_SIZE_HW) / 2))) - 1);
  const int16u RIGHT_BASE = (SIMEE_BASE_ADDR_HW + (FLASH_PAGE_SIZE_HW *
                            ((SIMEE_SIZE_HW / FLASH_PAGE_SIZE_HW) / 2)));
  const int16u RIGHT_TOP = (SIMEE_BASE_ADDR_HW + (SIMEE_SIZE_HW - 1));
  const int16u ERASE_CRITICAL_THRESHOLD = (SIMEE_SIZE_HW / 4);
#endif //CORTEXM3

const int16u ID_COUNT = TOKEN_COUNT;

#define DEFINETOKENS
//Manufacturing tokens do not exist in the SimEEPROM -define to nothing
#define TOKEN_MFG(name,creator,iscnt,isidx,type,arraysize,...)

//If the arraysize is exactly 1, we need only a single entry in the ptrCache
//to hold this entry.  If the arraysize is not 1, we need +1 to hold
//the redirection entry in the Primary Table and then arraysize entries in
//the indexed table.  This works for all values of arraysize.
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  + arraysize + ((arraysize==1) ? 0 : 1)
  //value of all index counts added together
  const int16u PTR_CACHE_SIZE = 0
    #include "stack/config/token-stack.h"
  ;
  //the ptrCache definition - used to provide efficient access, based upon
  //ID and index, to the freshest data in the Simulated EEPROM.
  int16u ptrCache[
    #include "stack/config/token-stack.h"
  ];
#undef TOKEN_DEF

//The Size Cache definition.  Links the token to its size for efficient
//calculations.  Indexed by compile ID.  Size is in words.  This Cache should
//be used for all size calculations.  halInternalSimEeInit will guarantee that
//the compiled sizes defining this cache match the stored sizes.
const int8u sizeCache[] = {
#define TOKEN_DEF(name,creator,iscnt,isidx,type,arraysize,...) \
  (BYTES_TO_WORDS(sizeof(type))                                        \
     + (iscnt ? BYTES_TO_WORDS(COUNTER_TOKEN_PAD) : 0)),
    #include "stack/config/token-stack.h"
 };
#undef TOKEN_DEF

#undef TOKEN_MFG
#undef DEFINETOKENS

EmberStatus halInternalSimEeStartupCore(boolean forceRebuildAll, 
                                        int8u *lookupCache);


EmberStatus halInternalSimEeStartup(boolean forceRebuildAll)
{
  // lookupCache must be declared here, outside the library so that it can
  //  be appropriately sized based on the number of tokens the application
  //  uses.
  int8u lookupCache[TOKEN_COUNT];
  
  //The value 0xFF is used as a trigger in both caches to indicate an
  //uninitialized entry.  Unitialized entries (i.e. missing tokens) will
  //trigger repairing.
  MEMSET(lookupCache, 0xFF, ID_COUNT);
  MEMSET(ptrCache, 0xFF, PTR_CACHE_SIZE*sizeof(int16u));
  
  //Fundamental limitations of the Simulated EEPROM design:
  //These limitations are forcefully enforced with asserts because
  //exceeding these limitations will cause fatal operation and data
  //corruption.
  //(1) The maximum number of tokens that can be stored is 254.
  //    (Tokens are identified by one-byte values, with 0xFF as invalid.)
  //(2) The maximum size, in bytes, of a token (element) is 254.
  //    (One byte, with 0xFF as invalid)
  //(3) The maximum number of elements in an indexed token is 126.
  //    (One byte, with MSB used as counter flag and 0x7F as non-indexed size.)
  //(4) The maximum total storage for tokens plus management is SIMEE_BTS_SIZE_B/2.
  //    (This limit is due to a Virtual Page only being a limited size and for
  //     the Simulated EEPROM to operate with any kind of efficiency, the
  //     Base Storage must be small enough to leave room for token copies.)
  //NOTE: Sadly, it's not possible to use compile time preprocessor checks on
  //      limitations 1, 2, 3, and 4.  Only runtime asserts can be used.
  assert(TOKEN_COUNT<255); //fundamental limit (1)
  {  
    int8u i;
    int32u totalTokenStorage = TOKEN_COUNT * 2;   // two words per creator
    int32u indexSkip = TOKEN_COUNT;
    for(i = 0; i < TOKEN_COUNT; i++) {
      int32u size = BYTES_TO_WORDS(tokenSize[i]);
      int32u arraySize = tokenArraySize[i];
      assert(tokenSize[i] < 255); //fundamental limit (2)
      assert(tokenArraySize[i] < 127); //fundamental limit (3)
      if(tokenIsCnt[i])
        size += BYTES_TO_WORDS(COUNTER_TOKEN_PAD);


      emberDebugPrintf("Creator: 0x%2X, Words: %d\n",
                       tokenCreators[i],
                       // we cast this to 16-bit to make it readable in decimal
                       // (var args default to 'int' which is 16-bit on xap)
                       // there should be no token who's size is greater than 65k
                       (int16u)(arraySize * (1 + size)));

      //The '1 +' is for the initial info word in each token.
      totalTokenStorage += arraySize * (1 + size);
      //Install the indexed token ptrCache redirection value.  If a token is
      //indexed, set the ptrCache equal to the index number of where the
      //actual token addresses start in the ptrCache.
      if(arraySize != 1) {
        ptrCache[i] = indexSkip;
        indexSkip += arraySize;
      }
    }

    // Again, we force 32-bit into 16-bit for printing purposes.  The var args
    // default to 'int' which is 16-bit on the xap.  We should never exceed
    // 65k words for any of these values, so we are okay.
    emberDebugPrintf("SimEE data: %d words of %d max (%p), tokenCount: %d\n",
                     (int16u)totalTokenStorage,
                     (int16u)(SIMEE_BTS_SIZE_B/2),
                     (totalTokenStorage < (SIMEE_BTS_SIZE_B/2) 
                      ? "ok"
                      : "TOO LARGE"),
                     (int16u)TOKEN_COUNT);

    if(!(totalTokenStorage < (SIMEE_BTS_SIZE_B/2))) { //fundamental limit (4)
      #if !defined(EMBER_ASSERT_OUTPUT_DISABLED)
        halResetWatchdog(); // In case we're close to running out.
        INTERRUPTS_OFF();
        emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, 
                                  "\r\nERROR: SimEE data too large.  Compiling with %d words consumed.  Max allowed is %d.\r\n",
                                  totalTokenStorage,
                                  (SIMEE_BTS_SIZE_B/2));
      #endif
      assert(FALSE);
    }
  }
  
  return halInternalSimEeStartupCore(forceRebuildAll, lookupCache);
}
