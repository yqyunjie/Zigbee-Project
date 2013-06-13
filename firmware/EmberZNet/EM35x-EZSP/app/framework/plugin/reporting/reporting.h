// *******************************************************************
// * reporting.h
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

EmberAfStatus emAfPluginReportingConfigureReportedAttribute(const EmberAfClusterCommand *cmd,
                                                            EmberAfAttributeId attributeId,
                                                            int8u mask,
                                                            EmberAfAttributeType dataType,
                                                            int16u minInterval,
                                                            int16u maxInterval,
                                                            int32u reportableChange);
void emAfPluginReportingGetEntry(int8u index, EmberAfPluginReportingEntry *result);
void emAfPluginReportingSetEntry(int8u index, EmberAfPluginReportingEntry *value);
EmberStatus emAfPluginReportingRemoveEntry(int8u index);
