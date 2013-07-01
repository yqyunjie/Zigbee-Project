// File: ezsp-protocol.h
// 
// Description: Definitions useful for creating and decoding EZSP frames. The
// frame format is defined in the EM260 datasheet and looks like this:
//   <sequence:1>
//   <frame control:1>
//   <frame ID:1>
//   <parameters:0-120>
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_PROTOCOL_H__
#define __EZSP_PROTOCOL_H__

//------------------------------------------------------------------------------
// Protocol Definitions

#define EZSP_PROTOCOL_VERSION 0x03

#define EZSP_MIN_FRAME_LENGTH     3
#define EZSP_MAX_FRAME_LENGTH     128
#define EZSP_SEQUENCE_INDEX       0
#define EZSP_FRAME_CONTROL_INDEX  1
#define EZSP_FRAME_ID_INDEX       2
#define EZSP_PARAMETERS_INDEX     3

#define EZSP_STACK_TYPE_MESH 0x02

#include "ezsp-enum.h"

//------------------------------------------------------------------------------
// Frame Control Definitions

// The high bit of the frame control byte indicates the direction of the
// message. Commands are sent from the Host to the EM260. Responses are sent
// from the EM260 to the Host.
#define EZSP_FRAME_CONTROL_DIRECTION_MASK 0x80
#define EZSP_FRAME_CONTROL_COMMAND        0x00
#define EZSP_FRAME_CONTROL_RESPONSE       0x80

// Command Frame Control Fields

// The EM260 enters the sleep mode specified by the command frame control once
// it has sent its response.
#define EZSP_FRAME_CONTROL_SLEEP_MODE_MASK 0x03
#define EZSP_FRAME_CONTROL_IDLE            0x00 // Processor idle.
#define EZSP_FRAME_CONTROL_DEEP_SLEEP      0x01 // Wake on interrupt or timer.
#define EZSP_FRAME_CONTROL_POWER_DOWN      0x02 // Wake on interrupt only.
// Sleep mode 0x03 is reserved.

// Response Frame Control Fields

// The overflow flag in the response frame control indicates to the Host that
// one or more callbacks occurred since the previous response and there was not
// enough memory available to report them to the Host.
#define EZSP_FRAME_CONTROL_OVERFLOW_MASK 0x01
#define EZSP_FRAME_CONTROL_NO_OVERFLOW   0x00
#define EZSP_FRAME_CONTROL_OVERFLOW      0x01

// The truncated flag in the response frame control indicates to the Host that
// the response has been truncated. This will happen if there is not enough
// memory available to complete the response or if the response would have
// exceeded the maximum EZSP frame length.
#define EZSP_FRAME_CONTROL_TRUNCATED_MASK 0x02
#define EZSP_FRAME_CONTROL_NOT_TRUNCATED  0x00
#define EZSP_FRAME_CONTROL_TRUNCATED      0x02

#endif // __EZSP_PROTOCOL_H__
