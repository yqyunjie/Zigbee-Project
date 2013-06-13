// File: print-stack-tables.h
//
// Description: declarations for functions to print stack tables 
// for use in troubleshooting.
//
// The print-stack-tables library is deprecated and will be removed in a future
// release.  Similar functionality is available in the Stack Diagnostics plugin
// in Application Framework.
//
// Author(s): Matteo Paris, matteo@ember.com
//
// Copyright 2006 by Ember Corporation. All rights reserved.                *80*

// Prints the neighbor table to the serial port passed in. See the 
// EmberNeighborTableEntry struct in stack/include/ember-types.h for a 
// description of the fields printed.  Note that neighbor EUI64s are not 
// available on insecure networks. This function uses emberNeighborCount and
// emberGetNeighbor from stack/include/ember.h to read the neighbor table.
 
void printNeighborTable(int8u serialPort);

// Prints the route table to the serial port passed in.  See the
// EmberRouteTableEntry struct in stack/include/ember-types.h for a
// description of the fields printed.

void printRouteTable(int8u serialPort);
