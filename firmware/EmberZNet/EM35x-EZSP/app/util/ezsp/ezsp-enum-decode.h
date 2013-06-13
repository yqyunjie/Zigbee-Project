// File: ezsp-enum-decode.h
// 
// Description: 
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_ENUM_DECODE_H__
#define __EZSP_ENUM_DECODE_H__

const char *decodeEzspConfigId(int8u value);
const char *decodeEzspPolicyId(int8u value);
const char *decodeEzspDecisionId(int8u value);
const char *decodeEzspMfgTokenId(int8u value);
const char *decodeEzspStatus(int8u value);
const char *decodeFrameId(int8u value);

#endif // __EZSP_H__
