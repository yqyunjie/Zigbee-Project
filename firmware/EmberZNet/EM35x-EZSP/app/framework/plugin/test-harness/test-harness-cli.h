
#if defined(EMBER_AF_PLUGIN_TEST_HARNESS)
  extern EmberCommandEntry emAfTestHarnessCommands[];

  #define TEST_HARNESS_CLI_COMMANDS \
    { "test-harness", NULL, (PGM_P)emAfTestHarnessCommands },
#else
  #define TEST_HARNESS_CLI_COMMANDS
#endif 

