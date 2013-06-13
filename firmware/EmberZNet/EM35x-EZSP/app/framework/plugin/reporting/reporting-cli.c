// *******************************************************************
// * reporting-cli.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "reporting.h"

static void print(void);
static void clear(void);
static void remov(void); // "remov" to avoid a conflict with "remove" in stdio
static void add(void);

EmberCommandEntry emberAfPluginReportingCommands[] = {
  emberCommandEntryAction("print",  print, "", "Print the reporting table"),
  emberCommandEntryAction("clear",  clear, "", "Clear the reporting tabel"),
  emberCommandEntryAction("remove", remov, "u","Remove an entry from the reporting table"),
  emberCommandEntryAction("add",    add,   "uvvuuvvw", "Add an entry to the reporting table"),
  emberCommandEntryTerminator(),
};

// plugin reporting print
static void print(void)
{
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE ; i++) {
    EmberAfPluginReportingEntry entry;
    emAfPluginReportingGetEntry(i, &entry);
    emberAfReportingPrint("%x:", i);
    if (entry.endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID) {
      emberAfReportingPrint("ep %x clus %2x attr %2x svr %c",
                            entry.endpoint,
                            entry.clusterId,
                            entry.attributeId,
                            (entry.mask == CLUSTER_MASK_SERVER ? 'y' : 'n'));
      if (entry.manufacturerCode != EMBER_AF_NULL_MANUFACTURER_CODE) {
        emberAfReportingPrint(" mfg %x", entry.manufacturerCode);
      }
      if (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED) {
        emberAfReportingPrint(" report min %2x max %2x rpt-chg %4x",
                              entry.data.reported.minInterval,
                              entry.data.reported.maxInterval,
                              entry.data.reported.reportableChange);
        emberAfReportingFlush();
      } else {
        emberAfReportingPrint(" receive from %2x ep %x timeout %2x",
                              entry.data.received.source,
                              entry.data.received.endpoint,
                              entry.data.received.timeout);
      }
    }
    emberAfReportingPrintln("");
    emberAfReportingFlush();
  }
}

// plugin reporting clear
static void clear(void)
{
  EmberStatus status = emberAfClearReportTableCallback();
  emberAfReportingPrintln("%p 0x%x", "clear", status);
}

// plugin reporting remove <index:1>
static void remov(void)
{
  EmberStatus status = emAfPluginReportingRemoveEntry((int8u)emberUnsignedCommandArgument(0));
  emberAfReportingPrintln("%p 0x%x", "remove", status);
}

// plugin reporting add <endpoint:1> <cluster id:2> <attribute id:2> ...
// ... <mask:1> <type:1> <min interval:2> <max interval:2> ...
// ... <reportable change:4>
static void add(void)
{
  EmberAfAttributeType type;
  EmberAfClusterCommand cmd;
  EmberAfStatus status;
  EmberApsFrame apsFrame;
  int32u reportableChange = 0;

  apsFrame.clusterId = (EmberAfClusterId)emberUnsignedCommandArgument(1);
  apsFrame.destinationEndpoint = (int8u)emberUnsignedCommandArgument(0);
  cmd.apsFrame = &apsFrame;
  cmd.mfgSpecific = FALSE;
  cmd.mfgCode = EMBER_AF_NULL_MANUFACTURER_CODE;

  type = (EmberAfAttributeType)emberUnsignedCommandArgument(4);
  if (emberAfGetAttributeAnalogOrDiscreteType(type)
      == EMBER_AF_DATA_TYPE_ANALOG) {
    reportableChange = emberUnsignedCommandArgument(7);
  }

  status = emAfPluginReportingConfigureReportedAttribute(&cmd,
                                                         (EmberAfAttributeId)emberUnsignedCommandArgument(2),
                                                         (emberUnsignedCommandArgument(3) == 0
                                                          ? CLUSTER_MASK_CLIENT
                                                          : CLUSTER_MASK_SERVER),
                                                         type,
                                                         (int16u)emberUnsignedCommandArgument(5), // min interval
                                                         (int16u)emberUnsignedCommandArgument(6), // max interval
                                                         reportableChange);
  emberAfReportingPrintln("%p 0x%x", "add", status);
}
