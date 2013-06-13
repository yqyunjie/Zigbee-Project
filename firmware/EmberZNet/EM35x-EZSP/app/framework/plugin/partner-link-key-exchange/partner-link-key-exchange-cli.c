// *******************************************************************
// * partner-link-key-exchange-cli.c
// *
// *
// * Copyright 2013 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/partner-link-key-exchange/partner-link-key-exchange.h"

static void cbkeAllowPartner(void);
static void cbkePartnerCommand(void);

#if defined(EZSP_HOST)
extern EzspStatus emberAfSetEzspPolicy(EzspPolicyId policyId,
                                       EzspDecisionId decisionId,
                                       PGM_P policyName,
                                       PGM_P decisionName);
#endif 

extern void emAfPrintStatus(PGM_P task,
                            EmberStatus status);

EmberCommandEntry emberAfPluginPartnerLinkKeyExchangeCommands[] = {
  emberCommandEntryAction("partner",  cbkePartnerCommand, "vu", ""),
  emberCommandEntryAction("allow-partner",  cbkeAllowPartner, "u", ""),
  emberCommandEntryTerminator(),
};

static void cbkeAllowPartner(void)
{
  emAfAllowPartnerLinkKey = (boolean)emberUnsignedCommandArgument(0);

#if defined(EZSP_HOST)
  emberAfSetEzspPolicy(EZSP_APP_KEY_REQUEST_POLICY,
                       (emAfAllowPartnerLinkKey
                        ? EZSP_ALLOW_APP_KEY_REQUESTS
                        : EZSP_DENY_APP_KEY_REQUESTS),
                       "App Link Key Request Policy",
                       (emAfAllowPartnerLinkKey
                        ? "Allow"
                        : "Deny"));

#endif
}

static void cbkePartnerCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u endpoint     = (int8u)emberUnsignedCommandArgument(1);
  EmberStatus status = emberAfInitiatePartnerLinkKeyExchange(target,
                                                             endpoint,
                                                             NULL); // callback
  emberAfCoreDebugExec(emAfPrintStatus("partner link key request", status));
}
