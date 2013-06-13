

void emAfOtaClientStop(void);
void emAfOtaClientPrintState(void);
void emAfSendImageBlockRequestTest(void);

void emAfSetPageRequest(boolean pageRequest);
boolean emAfUsingPageRequest(void);

void emAfOtaBootloadClusterClientResumeAfterErase(boolean success);

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS)
  // How often the client will ask for a piece of an upgrade image being 
  // actively downloaded.  A rate of 0 means the client will pull down the data
  // as fast as it can.
  #define EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS  0L
#endif

// How often the OTA client looks for an OTA server when there is NOT
// one present in the network.  Once it has found one, it queries the
// same one forever (or until it reboots).
#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_SERVER_DISCOVERY_DELAY_MINUTES)
  #define EMBER_AF_OTA_SERVER_DISCOVERY_DELAY_MS \
    (EMBER_AF_PLUGIN_OTA_CLIENT_SERVER_DISCOVERY_DELAY_MINUTES * MINUTES_IN_MS)
#else
  #define EMBER_AF_OTA_SERVER_DISCOVERY_DELAY_MS     (2 * MINUTES_IN_MS)
#endif

// How often the OTA client asks the OTA server if there is a new image 
// available
#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_DELAY_MINUTES)
  #define EMBER_AF_OTA_QUERY_DELAY_MS  \
    (EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_DELAY_MINUTES * MINUTES_IN_MS)
#else
  #define EMBER_AF_OTA_QUERY_DELAY_MS (5 * MINUTES_IN_MS)
#endif

// The number of query errors before re-discovery of an OTA
// server is discovered.
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_ERROR_THRESHOLD)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_ERROR_THRESHOLD 10
#endif

// The maximum number of sequential errors when downloading that will trigger
// the OTA client to abort the download.
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_ERROR_THRESHOLD)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_ERROR_THRESHOLD  10
#endif

// The delay between attempts to request to initiate the bootload
// of a successfully downloaded file.
#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_RUN_UPGRADE_REQUEST_DELAY_MINUTES)
  #define EMBER_AF_RUN_UPGRADE_REQUEST_DELAY_MS \
    (EMBER_AF_PLUGIN_OTA_CLIENT_RUN_UPGRADE_REQUEST_DELAY_MINUTES * MINUTES_IN_MS)
#else
  #define EMBER_AF_RUN_UPGRADE_REQUEST_DELAY_MS (10 * MINUTES_IN_MS)
#endif

// The maximum number of sequential errors when asking the OTA Server when to
// upgrade that will cause the OTA client to apply the upgrade without the 
// server telling it to do so.
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_UPGRADE_WAIT_THRESHOLD)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_UPGRADE_WAIT_THRESHOLD  10
#endif

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE 1024
#endif

// The spacing requested by the client between the image blocks sent by the 
// server to the client during a page request.
#if !defined(EMBER_AF_OTA_CLIENT_PAGE_REQUEST_SPACING_MS)
  #define EMBER_AF_OTA_CLIENT_PAGE_REQUEST_SPACING_MS 50L
#endif

// This is the time delay between calls to emAfOtaImageDownloadVerify().
// Verification can take a while (especially in the case of signature
// checking for Smart Energy) so this provides the ability for other
// parts of the system to run.
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_VERIFY_DELAY_MS)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_VERIFY_DELAY_MS    10L
#endif

#define NULL_EUI64 { 0, 0, 0, 0, 0, 0, 0, 0 }

// A NULL eui64 is an invalid signer.  It will never match.
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI0)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI0 NULL_EUI64
#endif

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI1)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI1 NULL_EUI64
#endif

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI2)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI2 NULL_EUI64
#endif


#if defined(EMBER_TEST)
extern int8u emAfOtaClientStopDownloadPercentage;
#endif

