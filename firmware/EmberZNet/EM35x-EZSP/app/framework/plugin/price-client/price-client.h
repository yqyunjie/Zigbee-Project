// *******************************************************************
// * price-client.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE
#define EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE 2
#endif //EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE

#define ZCL_PRICE_CLUSTER_PRICE_ACKNOWLEDGEMENT_MASK 0x01
#define ZCL_PRICE_CLUSTER_RESERVED_MASK              0xFE

#define ZCL_PRICE_CLUSTER_START_TIME_NOW                         0x00000000UL
#define ZCL_PRICE_CLUSTER_END_TIME_NEVER                         0xFFFFFFFFUL
#define ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED                 0xFFFF
#define ZCL_PRICE_CLUSTER_PRICE_RATIO_NOT_USED                   0xFF
#define ZCL_PRICE_CLUSTER_GENERATION_PRICE_NOT_USED              0xFFFFFFFFUL
#define ZCL_PRICE_CLUSTER_GENERATION_PRICE_RATIO_NOT_USED        0xFF
#define ZCL_PRICE_CLUSTER_ALTERNATE_COST_DELIVERED_NOT_USED      0xFFFFFFFFUL
#define ZCL_PRICE_CLUSTER_ALTERNATE_COST_UNIT_NOT_USED           0xFF
#define ZCL_PRICE_CLUSTER_ALTERNATE_COST_TRAILING_DIGIT_NOT_USED 0xFF
#define ZCL_PRICE_CLUSTER_NUMBER_OF_BLOCK_THRESHOLDS_NOT_USED    0xFF
#define ZCL_PRICE_CLUSTER_PRICE_CONTROL_NOT_USED                 0x00

void emAfPluginPriceClientPrintInfo(int8u endpoint);
