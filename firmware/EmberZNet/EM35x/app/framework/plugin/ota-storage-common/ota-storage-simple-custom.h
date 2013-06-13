// *****************************************************************************
// * ota-storage-custom.h
// *
// * This defines the custom storage interface used by ota-storage-simple.c
// * to interact with the actual hardware where the images are stored.
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
