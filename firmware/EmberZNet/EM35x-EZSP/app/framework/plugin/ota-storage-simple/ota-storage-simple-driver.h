// *****************************************************************************
// * ota-storage-simple-driver.h
// *
// * The Simple Storage Module driver interface.  In other words, primitives
// * for reading / writing and storing data about the OTA file that is stored,
// * or is in the process of being downloaded and stored.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

boolean emberAfCustomStorageInitCallback(void);

boolean emberAfCustomStorageReadCallback(int32u offset, 
                                         int32u length,
                                         int8u* returnData);

boolean emberAfCustomStorageWriteCallback(const int8u* dataToWrite,
                                          int32u offset, 
                                          int32u length);


#if defined(EMBER_TEST)
void emAfOtaLoadFileCommand(void);
#endif
