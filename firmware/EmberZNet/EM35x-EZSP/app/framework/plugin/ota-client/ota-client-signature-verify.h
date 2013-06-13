

// Hashing a file can be expensive for small processors. The maxHashCalculations
// determines how many iterations are made before returning back to the caller.  
// A value of 0 indicates to completely calculate the digest before returning.
// A value greater than 0 means that a number of hashes will be performed and
// then the routine will return EMBER_AF_OTA_VERIFICATION_IN_PROGRESS.  In that
// case it is expected that this function must be called repeatedly until it
// returns another status code.  
// When EMBER_AF_OTA_VERIFICATION_WAIT is returned then no further calls
// are necessary.  The verify code will fire the callback 
// emAfOtaVerifyStoredDataFinish() when it is has a result.
EmberAfImageVerifyStatus emAfOtaImageSignatureVerify(int16u maxHashCalculations,
                                                     const EmberAfOtaImageId* id, 
                                                     boolean newVerification);

// This is the maximum number of digest calculations we perform per call to
// emAfOtaImageSignatureVerify().  Arbitrary chosen value to limit
// time spent in this routine.  A value of 0 means we will NOT return
// until we are completely done with our digest calculations.
// Empirically waiting until digest calculations are complete can
// take quite a while for EZSP hosts (~40 seconds for a UART connected host).
// So we want to make sure that other parts of the framework can run during
// this time.  On SOC systems a similar problem occurs.  If we set this to 0
// then emberTick() will not fire and therefore the watchdog timer will not be
// serviced.
#define MAX_DIGEST_CALCULATIONS_PER_CALL 5

void emAfOtaVerifyStoredDataFinish(EmberAfImageVerifyStatus status);

void emAfOtaClientSignatureVerifyPrintSigners(void);
