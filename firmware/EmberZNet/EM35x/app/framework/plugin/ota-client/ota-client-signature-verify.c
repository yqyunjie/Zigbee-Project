// *****************************************************************************
// * ota-client-signature-verify.c
// *
// * Verification code for the ZigBee over-the-air bootload cluster.
// * This handles retrieving the stored application from storage,
// * calculating the message digest, and checking the signature.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "callback.h"

#if defined(EZSP_HOST)
  #include "stack/include/ember-types.h"
  #include "stack/include/error.h"
  #include "hal/hal.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "stack/include/library.h"
#else
  #include "stack/include/cbke-crypto-engine.h"
  #include "stack/include/library.h"
#endif


#include "app/framework/security/crypto-state.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/plugin/ota-client/ota-client.h"
#include "app/framework/plugin/ota-client-policy/ota-client-policy.h"
#include "ota-client-signature-verify.h"

#if defined(EMBER_TEST) && defined(EMBER_AF_TC_SWAP_OUT_TEST)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT
#endif

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT)


//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

// The size of this buffer is based on the max size that EZSP can handle, 
// including framing.
// This MUST be a multiple of 16, because the emberAesMmmoHashUpdate()
// code requires this.
#define MAX_BLOCK_SIZE_FOR_HASH 96

#define MAX_SIGNERS 3
static const int8u PGM_NO_CONST allowedSignerEuiBigEndian[MAX_SIGNERS][EUI64_SIZE] = {
  EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI0,
  EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI1,
  EMBER_AF_PLUGIN_OTA_CLIENT_SIGNER_EUI2,
};

#define SUBJECT_OFFSET EMBER_PUBLIC_KEY_SIZE
#define ISSUER_OFFSET (SUBJECT_OFFSET + EUI64_SIZE)

#define DIGEST_CALCULATE_PRINT_UPDATE_RATE  5

// TODO:  This is the data that we keep track of while verification is in
// progress.  It consumes a bit of RAM that is rarely used.  It would be
// ideal on the SOC to put this in an Ember message buffer.  However
// we must abstract the data storage since it needs to support both SOC and Host
// based apps.
static EmberAesMmoHashContext context;
static int32u currentOffset = 0;

typedef enum {
  DIGEST_CALCULATE_COMPLETE    = 0,
  DIGEST_CALCULATE_IN_PROGRESS = 1,
  DIGEST_CALCULATE_ERROR       = 2,
} DigestCalculateStatus;


//------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------



#if defined (EZSP_HOST)
// External
void emberReverseMemCopy(int8u* dest, const int8u* src, int16u length);


// For now we declare these here, but they should be moved to a more appropriate
// header.
void emberAesMmoHashInit(EmberAesMmoHashContext* context);
EmberStatus emberAesMmoHashUpdate(EmberAesMmoHashContext* context,
                                  int32u length,
                                  const int8u* data);
EmberStatus emberAesMmoHashFinal(EmberAesMmoHashContext* context,
                                 int32u length,
                                 const int8u* finalData);
#endif // EZSP_HOST

static boolean checkSigner(const int8u* bigEndianSignerEui64);

static void dsaVerifyHandler(EmberStatus status);

static DigestCalculateStatus calculateDigest(int16u maxHashCalculations,
                                             const EmberAfOtaImageId *id, 
                                             EmberMessageDigest* digest);

//#define DEBUG_DIGEST_PRINT
#if defined(DEBUG_DIGEST_PRINT)
  static void debugDigestPrint(const EmberAesMmoHashContext* context);
#else
  #define debugDigestPrint(x)
#endif

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

EmberAfImageVerifyStatus emAfOtaImageSignatureVerify(int16u maxHashCalculations,
                                                     const EmberAfOtaImageId* id, 
                                                     boolean newVerification)
{
  // Smart Energy policy requires verification of the signature of the
  // downloaded file.

  // We only support images that have the embedded certificate in them.
  // However we validate that the certificate only comes from a signer that we
  // know and trust by making sure the hardcoded EUI in the software matches 
  // with the EUI in the certificate.
  // The reason for supporting only images with embedded certificates is that
  // it consumes less flash space to keep track of a signer EUI (8 bytes) 
  // than to keep track of an entire signer ceritficate (48 bytes).
  
  // 1) Extract the certificate of the signer.
  // 2) Verify the signer is known to us.
  // 3) Calculate the message digest of the image.
  // 4) Extract the signature of the image.
  // 5) Validate the signature came from that signer

  EmberMessageDigest digest;
  EmberCertificateData signerCert;
  int32u dataLength;
  int8u signatureTag[SIGNATURE_TAG_DATA_SIZE];
  DigestCalculateStatus digestStatus;
  
  if (newVerification) {
    otaPrintln("Client Verifying Signature.");
    currentOffset = 0;
  }

  if (EMBER_LIBRARY_PRESENT_MASK 
      != (EMBER_LIBRARY_PRESENT_MASK
          & emberGetLibraryStatus(EMBER_CBKE_DSA_VERIFY_LIBRARY_ID))) {
    // We treat this is as a validation error and not a
    // EMBER_AF_OTA_NO_VERIFY_SUPPORT error because we want the software
    // to halt the upgrade and discard the image.  If the software
    // has been configured to perform signature verification then
    // it should NOT accept images because it lacks the required
    // libraries to validate them.  Instead, we want it to always fail to
    // upgrade so that this error case can be detected and fixed by
    // developers.
    otaPrintln("Verify Error: No CBKE library present!");
    return EMBER_AF_IMAGE_VERIFY_ERROR;
  }

  if (EMBER_AF_OTA_STORAGE_SUCCESS
      != emAfOtaStorageGetTagDataFromImage(id,
                                           OTA_TAG_ECDSA_SIGNING_CERTIFICATE,
                                           signerCert.contents,
                                           &dataLength,
                                           EMBER_CERTIFICATE_SIZE)
      || dataLength != EMBER_CERTIFICATE_SIZE) {
    otaPrintln("Verify Error: Could not obtain signing certificate from image.");
    return EMBER_AF_IMAGE_VERIFY_ERROR;
  }

  if (newVerification) {
    otaPrintln("Signer Certificate:");
    emberAfCoreFlush();
    emberAfPrintCert(signerCert.contents);
    emberAfCoreFlush();
  }
  
  if (!checkSigner(signerCert.contents + SUBJECT_OFFSET)) {
    otaPrint("Verify Error: Certificate EUI in image does not "
             "match any known signer EUI: ");
    emberAfPrint8ByteBlocks(1,
                            signerCert.contents + SUBJECT_OFFSET,
                            FALSE);   // cr between blocks?
    otaPrintln("");
    return EMBER_AF_IMAGE_BAD;
  }
  
  if (EMBER_AF_OTA_STORAGE_SUCCESS
      != emAfOtaStorageGetTagDataFromImage(id,
                                           OTA_TAG_ECDSA_SIGNATURE,
                                           signatureTag,
                                           &dataLength,
                                           SIGNATURE_TAG_DATA_SIZE)
      || dataLength != SIGNATURE_TAG_DATA_SIZE) {
    otaPrintln("Verify Error: Could not obtain ECDSA signature from image!");
    return EMBER_AF_IMAGE_BAD;
  }

  // Verify that the signer's EUI64 in the signature tag is the same
  // as the EUI64 in the certificate.  This makes sure no one can have
  // a different device sign than is advertised in the "signer" field
  // of the signature tag sub-element.
  {
    EmberEUI64 subjectFieldEui64LittleEndian;
    emberReverseMemCopy(subjectFieldEui64LittleEndian,
                        signerCert.contents + SUBJECT_OFFSET,
                        EUI64_SIZE);
    if (0 != MEMCOMPARE(subjectFieldEui64LittleEndian,
                        signatureTag,
                        EUI64_SIZE)) {
      otaPrintln("Verify Error: Certificate EUI and signer EUI do not match!\n");
      return EMBER_AF_IMAGE_BAD;
    }
  }

  if (newVerification) {
    EmberCertificateData myCert;
    if (EMBER_SUCCESS != emberGetCertificate(&myCert)) {
      otaPrintln("Verify Error:  Could not extract my local certificate.");
      return EMBER_AF_IMAGE_BAD;
    }

    // This check ultimately prevents a Test Cert device from being used to
    // sign images for a Production cert device.
    // The CBKE DSA verify will fail later, but this prevents us from getting
    // that far.
    if (0 != MEMCOMPARE(myCert.contents + ISSUER_OFFSET,
                        signerCert.contents + ISSUER_OFFSET,
                        EUI64_SIZE)) {
      otaPrintln("Verify Error:  Certificate issuer mismatch.");
      return EMBER_AF_IMAGE_BAD;
    }

    otaPrintln("Signature");
    // This print is 6 bytes too long, but it doesn't matter.  It is just a print
    // The 'emberAfPrintCert()' routines prints 48 bytes but the signature is only 42.
    emberAfOtaBootloadClusterDebugExec(emberAfPrintCert(signatureTag + EUI64_SIZE));
  }

  digestStatus = calculateDigest(maxHashCalculations, id, &digest);
  if (digestStatus == DIGEST_CALCULATE_ERROR) {
    otaPrintln("Digest calculate error.");
    return EMBER_AF_IMAGE_VERIFY_ERROR;
  } else if (digestStatus == DIGEST_CALCULATE_IN_PROGRESS) {
    return EMBER_AF_IMAGE_VERIFY_IN_PROGRESS;
  } // Else, DIGEST_CALCULATE_COMPLETE
    //   Fall through

  if (EMBER_OPERATION_IN_PROGRESS
      == emberDsaVerify(&digest,
                        &signerCert,
                        (EmberSignatureData*)(signatureTag + EUI64_SIZE))) {
    // This indicates that the stack is going to do crypto operations so the
    // application should hold off on normal message sending until that is
    // complete.
    emAfSetCryptoOperationInProgress();
    return EMBER_AF_IMAGE_VERIFY_WAIT;
  }

  return EMBER_AF_IMAGE_VERIFY_ERROR;
}

static DigestCalculateStatus calculateDigest(int16u maxHashCalculations,
                                             const EmberAfOtaImageId *id, 
                                             EmberMessageDigest* digest)
{
  EmberAfOtaStorageStatus status;
  int32u imageSize;
  int8u block[MAX_BLOCK_SIZE_FOR_HASH];
  int32u readSize;
  int32u dataLeftToRead;
  int32u remainder = 0;
  int16u iterations = 0;

  status
    = emAfOtaStorageGetHeaderLengthAndImageSize(id, 
                                                NULL, // header length (don't care)
                                                &imageSize);
  if (status) {
    return DIGEST_CALCULATE_ERROR;
  }

  dataLeftToRead = imageSize - EMBER_SIGNATURE_SIZE - currentOffset;
  if (currentOffset == 0) {
    otaPrintln("Starting new digest calculation");
    emberAesMmoHashInit(&context);
    emAfPrintPercentageSetStartAndEnd(0, dataLeftToRead);
  }

  readSize = (dataLeftToRead < MAX_BLOCK_SIZE_FOR_HASH
              ? dataLeftToRead
              : MAX_BLOCK_SIZE_FOR_HASH);

  while (dataLeftToRead) {
    int32u returnedLength;
    if ((EMBER_AF_OTA_STORAGE_SUCCESS
         != emberAfOtaStorageReadImageDataCallback(id,
                                                   currentOffset,
                                                   readSize,
                                                   block,
                                                   &returnedLength))
        || (readSize != returnedLength)) {
      return DIGEST_CALCULATE_ERROR;
    }
    if (readSize == MAX_BLOCK_SIZE_FOR_HASH) {
      if (EMBER_SUCCESS != emberAesMmoHashUpdate(&context,
                                                 MAX_BLOCK_SIZE_FOR_HASH,
                                                 block)) {
        return DIGEST_CALCULATE_ERROR;
      }

      debugDigestPrint(&context);

    } else {
      remainder = readSize;
    }

    currentOffset += readSize;
    dataLeftToRead -= readSize;
    readSize = (dataLeftToRead < MAX_BLOCK_SIZE_FOR_HASH
                ? dataLeftToRead
                : MAX_BLOCK_SIZE_FOR_HASH);
    emAfPrintPercentageUpdate("Digest Calculate", 
                              DIGEST_CALCULATE_PRINT_UPDATE_RATE, 
                              currentOffset);
    iterations++;
    if (dataLeftToRead
        && maxHashCalculations != 0
        && iterations >= maxHashCalculations) {
      // Bugzid: 12566.
      // We don't return if there is no dataLeftToRead,
      // since that means we are on the last calculation
      // for the remainder.  The remainder is only stored
      // on the stack so if we return, it will be wiped out
      // for the next execution.
      return DIGEST_CALCULATE_IN_PROGRESS;
    }
  }

  if (EMBER_SUCCESS != emberAesMmoHashFinal(&context,
                                            remainder,
                                            block)) {
    return DIGEST_CALCULATE_ERROR;
  }
  currentOffset += remainder;
  debugDigestPrint(&context);

  emAfPrintPercentageUpdate("Digest Calculate", 
                            DIGEST_CALCULATE_PRINT_UPDATE_RATE, 
                            currentOffset);

  MEMCOPY(digest->contents,
          context.result,
          EMBER_AES_HASH_BLOCK_SIZE);

  emberAfOtaBootloadClusterFlush();
  otaPrintln("Calculated Digest: ");
  emberAfPrintZigbeeKey(digest->contents);
  otaPrintln("");

  return DIGEST_CALCULATE_COMPLETE;
}

static void print8BytePgmData(PGM_PU data)
{
  // Ideally I want to use a convenience function like emberAfPrint8ByteBlocks
  // to print the eui64 but I can't because on the AVR the variable
  // 'allowedSignerEuiBigEndian' has memory attributes that the function
  // emberAfPrint8ByteBlocks doesn't have.  It would cause enormous pain
  // to fix this for all our platforms.  So Instead we just use a local
  // print routine.
  otaPrintln("%X %X %X %X %X %X %X %X",
             data[0],
             data[1],
             data[2],
             data[3],
             data[4],
             data[5],
             data[6],
             data[7]);
  emberAfCoreFlush();
}

static boolean checkSigner(const int8u* bigEndianSignerEui64)
{
  int8u i;
  for (i = 0; i < MAX_SIGNERS; i++) {
    int8u j;
    boolean nullEui64 = TRUE;
//    otaPrint("Considering Signer EUI: ");
//    print8BytePgmData(allowedSignerEuiBigEndian[i]);
    for (j = 0; nullEui64 && j < EUI64_SIZE; j++) {
      if (allowedSignerEuiBigEndian[i][j] != 0) {
        nullEui64 = FALSE;
      }
    }
    if (nullEui64) {
      continue;
    }

    if (0 == MEMPGMCOMPARE(bigEndianSignerEui64,
                           allowedSignerEuiBigEndian[i],
                           EUI64_SIZE)) {
      return TRUE;
    }
  }
  return FALSE;
}

void emAfOtaClientSignatureVerifyPrintSigners(void)
{
  int8u i;
  emberAfCoreFlush();
  otaPrintln("Allowed Signers of images, big endian (NULL EUI64 is invalid)");
  emberAfCoreFlush();
  for (i = 0; i < MAX_SIGNERS; i++) {
    emberAfCoreFlush();
    otaPrint("%d: ", i);
    print8BytePgmData(allowedSignerEuiBigEndian[i]);
  }
}

#if defined DEBUG_DIGEST_PRINT
static void debugDigestPrint(const EmberAesMmoHashContext* context)
{
  emberAfOtaBootloadClusterPrint("Current Digest for length 0x%4X: ",
                                 context->length);
  emberAfPrintZigbeeKey(context->result);
}
#endif 

static void dsaVerifyHandler(EmberStatus status)
{
  otaPrintln("DSA Verify returned: 0x%x (%p)", 
             status,
             (status == EMBER_SUCCESS
              ? "Success"
              : (status == EMBER_SIGNATURE_VERIFY_FAILURE
                 ? "Invalid Signature" 
                 : "ERROR")));

  // This notes that the stack is done doing crypto and has
  // resumed normal operations.  The application's normal
  // behavior will no longer be held off.
  emAfCryptoOperationComplete();

  // Any error status is treated as an image verification failure.
  // One could argue that we could retry an operation after a transient
  // failure (out of buffers) but for now we don't.
  emAfOtaVerifyStoredDataFinish(status == EMBER_SUCCESS
                                ? EMBER_AF_IMAGE_GOOD
                                : EMBER_AF_IMAGE_BAD);
                            
}

#if defined (EZSP_HOST)

  void ezspDsaVerifyHandler(EmberStatus status)
  {
    dsaVerifyHandler(status);
  }

#else

  void emberDsaVerifyHandler(EmberStatus status)
  {
    dsaVerifyHandler(status);
  }

#endif

//------------------------------------------------------------------------------
#else //  !defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT)

EmberAfImageVerifyStatus emAfOtaImageSignatureVerify(int16u maxHashCalculations,
                                                     const EmberAfOtaImageId* id, 
                                                     boolean newVerification)
{
  return EMBER_AF_NO_IMAGE_VERIFY_SUPPORT;
}

#endif 

//------------------------------------------------------------------------------

