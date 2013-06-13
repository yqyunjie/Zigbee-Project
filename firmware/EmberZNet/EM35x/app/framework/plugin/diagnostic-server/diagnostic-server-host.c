// Copyright 2007 - 2012 by Ember Corporation. All rights reserved.
// 
//

#include "app/framework/include/af.h"
#include "diagnostic-server.h"

// This is just a stub for the host processor
boolean emberAfReadDiagnosticAttribute(
                    EmberAfAttributeMetadata *attributeMetadata, 
                    int8u *buffer) {  
  return FALSE;
}