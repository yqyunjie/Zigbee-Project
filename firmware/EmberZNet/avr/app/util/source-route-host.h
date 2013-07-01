/**
 * @file: source-route-host.h
 *
 * Description: Declarations for managing source routes on a host-based gateway.
 *
 * Copyright 2007 by Ember Corporation. All rights reserved.                *80*
 */

/** Search for a source route to the given destination. If one is found, return
 * TRUE and copy the relay list to the given location. If no route is found,
 * return FALSE and don't modify the given location.
 */
boolean emberFindSourceRoute(EmberNodeId destination,
                             int8u *relayCount,
                             int16u *relayList);
