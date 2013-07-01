/** @file linux-serial.h
 *  @brief  Ember serial functionality specific to a PC with Unix library 
 *    support.
 * 
 * See @ref serial_comm for documentation.
 *
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.       *80*-->
 */

/** @addtogroup serial_comm
 *@{
 */

// The normal CLI is accessible via port 0 while port 1 is usable for
// raw input.  This is often used by applications to receive a 260 
// image for bootloading.
#define SERIAL_PORT_RAW 0
#define SERIAL_PORT_CLI 1

void emberSerialSetPrompt(const char* thePrompt);
void emberSerialCleanup(void);
int emberSerialGetInputFd(int8u port);

// For users of app/util/serial/command-interpreter.h
void emberSerialCommandCompletionInit(EmberCommandEntry* listOfCommands);

// For users of app/util/serial/cli.h
void emberSerialCommandCompletionInitCli(cliSerialCmdEntry* cliCmdList, 
                                         int cliCmdListLength);

/** @} END addtogroup */


