#ifndef CUSTOM_CLI_H
 #define CUSTOM_CLI_H
 #ifdef EMBER_AF_ENABLE_CUSTOM_COMMANDS
   extern EmberCommandEntry emberAfCustomCommands[];
   #define CUSTOM_COMMANDS   emberCommandEntrySubMenu("custom", emberAfCustomCommands, "Custom commands defined by the developer"),
 #else
   #ifndef CUSTOM_COMMANDS
     #define CUSTOM_COMMANDS
   #endif
 #endif
#endif
