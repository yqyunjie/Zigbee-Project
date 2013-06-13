

extern EmberEventControl emberAfPluginTrustCenterNwkKeyUpdateUnicastMyEventControl;
void emberAfPluginTrustCenterNwkKeyUpdateUnicastMyEventHandler(void);

#define TC_KEY_UPDATE_EVENT \
  { &emAfTcKeyUpdateUnicastEvent, emAfTcKeyUpdateUnicastEventHandler },


#if defined EMBER_TEST
  void zdoDiscoveryCallback(const EmberAfServiceDiscoveryResult* result);
#endif

#if defined(EMBER_AF_PLUGIN_TEST_HARNESS) || defined(EMBER_SCRIPTED_TEST)
  // For testing, we need to support a single application that can do
  // unicast AND broadcast key updates.  This function is actually
  // emberAfTrustCenterStartNetworkKeyUpdate() but is renamed.
  EmberStatus emberAfTrustCenterStartUnicastNetworkKeyUpdate(void);
#endif

// Because both the unicast and broadcast plugins for Trust Center NWK Key update
// define this function, we must protect it to eliminate the redudandant
// function declaration.  Unicast and broadcast headers may be included together
// since the code then doesn't need to determine which plugin (unicast or 
// broadcast) is being used and thus which header it should inclued.
#if !defined(EM_AF_TC_START_NETWORK_KEY_UPDATE_DECLARATION)
  #define EM_AF_TC_START_NETWORK_KEY_UPDATE_DECLARATION
  EmberStatus emberAfTrustCenterStartNetworkKeyUpdate(void);
#endif
