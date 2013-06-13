// *******************************************************************
// * tunneling-server-cli.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "tunneling-server.h"

static void transfer(void);

EmberCommandEntry emberAfPluginTunnelingServerCommands[] = {
  emberCommandEntryAction("transfer", transfer,                                      "vb",
                          "Transfer data through the tunnel"),
  emberCommandEntryAction("busy",     emberAfPluginTunnelingServerToggleBusyCommand, "",
                          "Toggly the busy status of the tunnel"),
  emberCommandEntryAction("print",    emAfPluginTunnelingServerPrint,                "",
                          "Print the list of tunnels"),
  emberCommandEntryTerminator(),
};

// plugin tunneling-server transfer <tunnel id:2> <data>
static void transfer(void)
{
  int16u tunnelId = (int16u)emberUnsignedCommandArgument(0);
  int8u data[255];
  int16u dataLen = emberCopyStringArgument(1, data, sizeof(data), FALSE);
  EmberAfStatus status = emberAfPluginTunnelingServerTransferData(tunnelId,
                                                                  data,
                                                                  dataLen);
  emberAfTunnelingClusterPrintln("%p 0x%x", "transfer", status);
}
