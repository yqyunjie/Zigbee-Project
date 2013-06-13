// *****************************************************************************
// * trust-center-backup.h
// *
// * Header for backing up and restoring a trust center.
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

EmberStatus emberTrustCenterExportBackupData(EmberAfTrustCenterBackupData* backup);
EmberStatus emberTrustCenterImportBackupAndStartNetwork(const EmberAfTrustCenterBackupData* backup);

// Available only for EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT
void emAfTcExportCommand(void);
void emAfTcImportCommand(void);
EmberStatus emberAfTrustCenterImportBackupFromFile(const char* filepath);
EmberStatus emberAfTrustCenterExportBackupToFile(const char* filepath);
