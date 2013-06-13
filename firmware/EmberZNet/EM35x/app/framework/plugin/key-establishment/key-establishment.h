// *******************************************************************
// * key-establishment.h
// *
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


// Init - bytes: suite (2), key gen time (1), derive secret time (1), cert (48)
#define EM_AF_KE_INITIATE_SIZE (2 + 1 + 1 + EMBER_CERTIFICATE_SIZE)

#define EM_AF_KE_EPHEMERAL_SIZE EMBER_PUBLIC_KEY_SIZE
#define EM_AF_KE_SMAC_SIZE      EMBER_SMAC_SIZE      

  // Terminate - bytes: status (1), wait time (1), suite (2)
#define EM_AF_KE_TERMINATE_SIZE (1 + 1 + 2)

#define APS_ACK_TIMEOUT_SECONDS 1

// These values reported to the remote device as to how long the local
// device takes to execute these operations.
#define DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS   (10 + APS_ACK_TIMEOUT_SECONDS)
#define DEFAULT_GENERATE_SHARED_SECRET_TIME_SECONDS    (15 + APS_ACK_TIMEOUT_SECONDS)

extern PGM int8u emAfKeyEstablishMessageToDataSize[];

#ifdef EZSP_HOST
  #define emAfPluginKeyEstablishmentGenerateCbkeKeysHandler ezspGenerateCbkeKeysHandler
  #define emAfPluginKeyEstablishmentCalculateSmacsHandler   ezspCalculateSmacsHandler
#else
  #define emAfPluginKeyEstablishmentGenerateCbkeKeysHandler emberGenerateCbkeKeysHandler
  #define emAfPluginKeyEstablishmentCalculateSmacsHandler   emberCalculateSmacsHandler
#endif

#define TERMINATE_STATUS_STRINGS { \
    "Success",                     \
    "Unknown Issuer",              \
    "Bad Key Confirm",             \
    "Bad Message",                 \
    "No resources",                \
    "Unsupported suite",           \
    "???",                         \
   }
#define UNKNOWN_TERMINATE_STATUS 6
