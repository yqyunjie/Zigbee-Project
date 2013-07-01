/**
 * @file library.h
 * @brief Interface for querying library status.
 *
 * <!-- Author(s): Rob Alexander, <ralexander@ember.com>  -->
 * 
 * <!--Copyright 2009 by Ember Corporation. All rights reserved.        *80*-->
 */

#ifndef __EMBER_LIBRARY_H__
#define __EMBER_LIBRARY_H__

// A library's status is an 8-bit value with information about it.
// The high bit indicates whether the library is present (1), or if it is a 
// stub (0).  The lower 7-bits can be used for codes specific to the library.  
// This allows a library, like the security library, to specify what additional
// features are present.
// A value of 0xFF is reserved, it indicates an error in retrieving the
// library status.

#define EMBER_LIBRARY_PRESENT_MASK  0x80
#define EMBER_LIBRARY_IS_STUB       0x00
#define EMBER_LIBRARY_ERROR         0xFF

#define EMBER_FIRST_LIBRARY_ID              0x00
#define EMBER_ZIGBEE_PRO_LIBRARY_ID         0x00
#define EMBER_BINDING_LIBRARY_ID            0x01
#define EMBER_END_DEVICE_BIND_LIBRARY_ID    0x02
#define EMBER_SECURITY_CORE_LIBRARY_ID      0x03
#define EMBER_SECURITY_LINK_KEYS_LIBRARY_ID 0x04
#define EMBER_ALARM_LIBRARY_ID              0x05
#define EMBER_CBKE_LIBRARY_ID               0x06
#define EMBER_CBKE_DSA_SIGN_LIBRARY_ID      0x07
#define EMBER_ECC_LIBRARY_ID                0x08

#define EMBER_NUMBER_OF_LIBRARIES           0x09
#define EMBER_NULL_LIBRARY_ID               0xFF

#define EMBER_LIBRARY_NAMES   \
    "Zigbee Pro",             \
    "Binding",                \
    "End Device Bind",        \
    "Security Core",          \
    "Security Link Keys",     \
    "Alarm",                  \
    "CBKE",                   \
    "CBKE DSA",               \
    "ECC",

#if !defined(EZSP_HOST)
// This will be defined elsewhere for the EZSP Host applications.
EmberLibraryStatus emberGetLibraryStatus(int8u libraryId);
#endif

// The ZigBee Pro library uses the following to indicate additional
// functionality:
//   Bit 0 indicates whether the device supports router capability.
//     A value of 0 means it is an end device only stack,
//     while a value of 1 means it has router capability.
#define EMBER_ZIGBEE_PRO_LIBRARY_END_DEVICE_ONLY        0x00
#define EMBER_ZIGBEE_PRO_LIBRARY_HAVE_ROUTER_CAPABILITY 0x01

// The security library uses the following to indicate additional 
// funtionality:
//   Bit 0 indicates whether the library is end device only (0)
//     or can be used for routers / trust centers (1)
#define EMBER_SECURITY_LIBRARY_END_DEVICE_ONLY      0x00
#define EMBER_SECURITY_LIBRARY_HAVE_ROUTER_SUPPORT  0x01

#endif // __EMBER_LIBRARY_H__

