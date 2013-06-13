// *****************************************************************************
// * trust-center-backup-cli-posix.c
// *
// * CLI for backing up or restoring TC data to unix filesystem.
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/trust-center-backup/trust-center-backup.h"

#include "app/framework/util/af-main.h"

#include <errno.h>

#if defined(EMBER_TEST)
  #define EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT
#endif

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT)

// *****************************************************************************
// Globals

// This is passed as an argument to emberCopyStringArgument() which only 
// supports 8-bit values.
#define MAX_FILEPATH_LENGTH 255

// *****************************************************************************
// Forward Declarations

static void getFilePathFromCommandLine(int8u* result);

// *****************************************************************************
// Functions

void emAfTcExportCommand(void)
{
  int8u file[MAX_FILEPATH_LENGTH];
  getFilePathFromCommandLine(file);

  emberAfTrustCenterExportBackupToFile(file);
}

void emAfTcImportCommand(void)
{
  int8u file[MAX_FILEPATH_LENGTH];
  getFilePathFromCommandLine(file);

  emberAfTrustCenterImportBackupFromFile(file);
}


static void getFilePathFromCommandLine(int8u* result)
{
  int8u length = emberCopyStringArgument(0, 
                                         result,
                                         MAX_FILEPATH_LENGTH,
                                         FALSE);  // leftpad?
  result[length] = '\0';
}

#endif // defined(EMBER_AF_PLUGIN_POSIX_FILE_BACKUP)
