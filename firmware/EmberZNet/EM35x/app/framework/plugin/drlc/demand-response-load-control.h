// *******************************************************************
// * ami-demand-response-load-control.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "load-control-event-table.h"

// needed for EmberSignatureData
#include "stack/include/ember-types.h"

#define EVENT_OPT_IN_DEFAULT  0x01

void afReportEventStatusHandler(int32u eventId,
                                     int8u eventCode,
                                     int32u startTime,
                                     int8u criticalityLevelApplied,
                                     int16s coolingTempSetPointApplied,
                                     int16s heatingTempSetPointApplied,
                                     int8s avgLoadAdjPercent,
                                     int8u dutyCycle,
                                     int8u eventControl,
                                     int8u messageLength,
                                     int8u* message,
                                     EmberSignatureData* signature);
