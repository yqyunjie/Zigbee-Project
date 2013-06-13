// File: library.h
// 
// Description: Code to display or retrieve the presence or absence of
//   Ember stack libraries on the device.
//
// Copyright 2009 by Ember Corporation. All rights reserved.                *80*

void printAllLibraryStatus(void);
boolean isLibraryPresent(int8u libraryId);

#define LIBRARY_COMMANDS \
  emberCommandEntryAction("libs", printAllLibraryStatus, "" , \
                          "Prints the status of all Ember stack libraries"),
