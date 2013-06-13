// *******************************************************************
// * attribute-size.c
// *
// * Contains storage and function for retrieving attribute size.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER

#include "attribute-type.h"

static PGM int8u attributeSizes[] =
{
#include "attribute-size.h"
};

int8u emberAfGetDataSize(int8u dataType) {
  int8u i;
  for (i = 0; i < sizeof(attributeSizes); i+=2) {
    if (attributeSizes[i] == dataType) {
      return attributeSizes[i + 1];
    }
  }

  return 0;
}

