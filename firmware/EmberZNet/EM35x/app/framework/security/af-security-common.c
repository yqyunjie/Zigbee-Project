// *******************************************************************
// * af-security-common.c
// *
// * Security code common to both the Trust Center and the normal node.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/security/af-security.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/cli/security-cli.h"

//------------------------------------------------------------------------------
boolean emberAfClearLinkKeyTableUponFormingOrJoining = TRUE;

const EmberAfSecurityProfileData emAfSecurityProfileData[] = {
  #include "security-profile-data.h"
};

static PGM EmberKeyData unSetKey = DUMMY_KEY;

// This routine sets the keys from values previously set on the CLI.
// If none are set via the CLI, then the default keys for the security profile
// are used.
static void getKeyFromCli(EmberKeyData* returnData, boolean linkKey)
{
  int8u* keyPtr = (linkKey
                   ? emberKeyContents(&cliPreconfiguredLinkKey)
                   : emberKeyContents(&cliNetworkKey));
  if (0 != halCommonMemPGMCompare(keyPtr, 
                                  emberKeyContents(&unSetKey), 
                                  EMBER_ENCRYPTION_KEY_SIZE)) {
    MEMCOPY(emberKeyContents(returnData), keyPtr, EMBER_ENCRYPTION_KEY_SIZE);
  }
}

void getLinkKeyFromCli(EmberKeyData* returnData)
{
  getKeyFromCli(returnData, TRUE);
}

void getNetworkKeyFromCli(EmberKeyData* returnData)
{
  getKeyFromCli(returnData, FALSE);
}

void emAfClearLinkKeyTable(void) {
#if EMBER_KEY_TABLE_SIZE
  if (emberAfClearLinkKeyTableUponFormingOrJoining) {
    emberClearKeyTable();
  }
  emberAfClearLinkKeyTableUponFormingOrJoining = TRUE;
#endif
}

const EmberAfSecurityProfileData *emAfGetCurrentSecurityProfileData(void)
{
  int8u i;
  for (i = 0; i < COUNTOF(emAfSecurityProfileData); i++) {
    if (emAfCurrentNetwork->securityProfile
        == emAfSecurityProfileData[i].securityProfile) {
      return &emAfSecurityProfileData[i];
    }
  }
  return NULL;
}

boolean emberAfIsCurrentSecurityProfileSmartEnergy(void)
{
#ifdef EMBER_AF_HAS_SECURITY_PROFILE_SE
  return ((emAfCurrentNetwork->securityProfile
           == EMBER_AF_SECURITY_PROFILE_SE_TEST)
          || (emAfCurrentNetwork->securityProfile
              == EMBER_AF_SECURITY_PROFILE_SE_FULL));
#else
  return FALSE;
#endif
}
