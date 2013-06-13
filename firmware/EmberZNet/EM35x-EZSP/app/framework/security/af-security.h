// *******************************************************************
// * af-security.h
// *
// * Header file for App. Framework security code 
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

void getLinkKeyFromCli(EmberKeyData* returnData);
void getNetworkKeyFromCli(EmberKeyData* returnData);

// If this flag is TRUE we clear the link key table before forming or joining.
// If FALSE, we skip clearing the link key once and we set this flag back to
// TRUE.
extern boolean emberAfClearLinkKeyTableUponFormingOrJoining;

extern const EmberAfSecurityProfileData emAfSecurityProfileData[];
const EmberAfSecurityProfileData *emAfGetCurrentSecurityProfileData(void);

void emAfClearLinkKeyTable(void);


#ifdef EMBER_AF_HAS_SECURITY_PROFILE_NONE
  // For no security, simply #define the security init routines to no-ops.
  #ifndef USE_REAL_SECURITY_PROTOTYPES
    #define zaNodeSecurityInit()
    #define zaTrustCenterSecurityInit()
    #define zaTrustCenterSetJoinPolicy(policy)
    #define zaTrustCenterSecurityPolicyInit()
  #endif
#else // All other security profiles.
  EmberStatus zaNodeSecurityInit(void);
  EmberStatus zaTrustCenterSecurityInit(void);
  EmberStatus zaTrustCenterSetJoinPolicy(EmberJoinDecision decision);
  EmberStatus zaTrustCenterSecurityPolicyInit(void);
#endif
