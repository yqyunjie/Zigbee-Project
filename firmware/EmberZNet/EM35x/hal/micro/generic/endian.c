/** @file hal/micro/generic/endian.c
 *  @brief  Generic firmware source for Endian conversions.
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2004-2009 by Ember Corporation. All rights reserved.   *80*-->   
 */

#include PLATFORM_HEADER
#include "hal/micro/endian.h"
//[[TODO: this include is needed for HIGH_LOW_TO_INT, but seems very klunky]]
#include "stack/include/ember-types.h"

// Endian conversion APIs.  
// Network byte order is big endian, so these APIs are only necessary on 
//  platforms which have a natural little endian byte order.  On big-endian
//  platforms, the APIs are macro'd away to nothing

#ifndef NTOHS
// Network to Host Short
int16u NTOHS(int16u val)
{
  int8u *fldPtr;

  fldPtr = (int8u *)&val;
  return (HIGH_LOW_TO_INT(*fldPtr, *(fldPtr+1)));
}
#endif

#ifndef NTOHL
// Network to Host Long
int32u NTOHL(int32u val)
{
  int16u *fldPtr;
  int16u field;
  int16u field2;

  fldPtr = (int16u *)&val;
  field = NTOHS(*fldPtr);
  field2 = NTOHS(*(fldPtr+1));
  return ((int32u)field << 16) | ((int32u)field2);
}
#endif
