/** @file hal/micro/cortexm3/bootloader/bootloader-ccm.h
 * This file contains the declarations of various CCM encryption functions
 * used in the secure bootloader.
 * 
 * <!-- Copyright 2013 by Silicon Laboratories. All rights reserved.    *80* -->
 */

#ifndef BOOTLOADER_CCM_H
#define BOOTLOADER_CCM_H

// Length defines
#define NONCE_LENGTH (12)
#define COUNTER_LENGTH (3)
#define MAC_LENGTH (16)
#define SECURITY_BLOCK_SIZE 16
#define PAYLOAD_SIZE_BYTES 3   // Number of bytes used to hold the payload size

// Offsets of various bits of CCM data after formatting
#define CCM_FLAGS_INDEX (0)
#define CCM_NONCE_INDEX (1)
#define CCM_COUNTER_INDEX (CCM_NONCE_INDEX + NONCE_LENGTH)

// The CCM state struct holds all configuration information needed to
// decrypt and validate a CCM message.
typedef struct ccmState_s
{
  int32u msgLen;                     /* Length of the encrypted data */
  int8u nonce[NONCE_LENGTH];         /* The random nonce for this message */
  int8u mac[SECURITY_BLOCK_SIZE];    /* The full rolling MAC value */
  int32u blockCount;                 /* Current AES block we're processing in this message */
  int8u blockOffset;                 /* Offset within the current AES block [0, SECURITY_BLOCK_SIZE] */
  int8u macOffset;                   /* Last byte written to in the MAC buffer */
} ccmState_t;

// Initialize the CCM state structure
void initCcmState(ccmState_t *ccmState);

// Verify that the secure bootloader key is set and accessbile
boolean validateSecureBootloaderKey(void);

// Functions for creating the MAC
void initCcmMac(ccmState_t *ccmState, int32u aDataLen);
void updateCcmMac(ccmState_t *ccmState, int8u *data, int32u len);
void finalizeCcmMac(ccmState_t *ccmState);
boolean verifyCcmMac(ccmState_t *ccmState, int8u *mac, int8u macLen);
void encryptDecryptCcmMac(ccmState_t *ccmState, int8u *mac);

// Define functions for authenticating unencrypted data. In CCM these
// are really just updating the MAC and finalizing, but use these names
// to make porting easier in the future 
#define processAssociatedData updateCcmMac
#define finalizeAssociatedData finalizeCcmMac

// Counter mode decrypt functions
void decryptCcmBlock(ccmState_t *ccmState, int8u *data, int32u len);

#endif // BOOTLOADER_CCM_H

