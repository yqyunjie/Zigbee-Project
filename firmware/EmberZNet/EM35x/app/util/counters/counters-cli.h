/** 
 * @file: counters-cli.h
 *
 * Used for testing the counters library via a command line interface.
 * For documentation on the counters library see counters.h.
 *
 * Copyright 2007 by Ember Corporation. All rights reserved.                *80*
 */

void printCountersCommand(void);
void simplePrintCountersCommand(void);
void clearCountersCommand(void);

/** Args: destination id, clearCounters (boolean) */
void sendCountersRequestCommand(void);

/** Utility function for printing out the OTA counters response. */
void printCountersResponse(EmberMessageBuffer message);

/** Use this macro in the emberCommandTable for convenience. */
#define COUNTER_COMMANDS \
  emberCommandEntryAction("cnt_print",     printCountersCommand,    "", \
                          "Print all stack counters"),                  \
  emberCommandEntryAction("cnt_clear",     clearCountersCommand,    "" ,\
                          "Clear all stack counters"),

/** Use this macro in the emberCommandTable for convenience.
 *  For applications short on const space. 
 */
#define SIMPLE_COUNTER_COMMANDS                                         \
  emberCommandEntryAction("cnt_print",     simplePrintCountersCommand,  "" , \
                          "Print all stack counters"),                  \
  emberCommandEntryAction("cnt_clear",     clearCountersCommand,        "" , \
                          "Clear all stack counters"),


/** Use this macro in the emberCommandTable for convenience.
 * This command requests counters over the air from a remote node.
 */
#define OTA_COUNTER_COMMANDS \
  emberCommandEntryAction("cnt_req",       sendCountersRequestCommand,  "vu" , \
                          "Request stack counters from remote device"),

