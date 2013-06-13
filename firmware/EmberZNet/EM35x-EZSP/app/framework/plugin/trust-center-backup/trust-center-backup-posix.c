// *****************************************************************************
// * trust-center-backup-posix.c
// *
// * API for backing up or restoring TC data from unix filesystem.
// * Files are imported/exported in plain text with the format:
// *   # Comments
// *   Extended Pan ID: <extended-pan-id-big-endian>
// *   Key: <eui64-big-endian>  <key-data>
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/trust-center-backup/trust-center-backup.h"

#include "app/framework/util/af-main.h"

#include <errno.h>

#if defined(EMBER_TEST)
  #define EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT
#endif

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT)

// *****************************************************************************
// Globals

#define MAX_LINE_LENGTH 255

#define MAX_BACKUP_LIST_LENGTH 20  // arbitrary.  Ideally we should use malloc()

// Windows requires the 'b' (binary) as part of the mode so that line endings
// are not truncated.  POSIX ignores this.
#define READ_FLAGS "rb"
#define WRITE_FLAGS "wb"

static const char extendedPanIdKeyWord[] = "Extended PAN ID";
static const char keyKeyWord[] = "Key";


// *****************************************************************************
// Forward Declarations

static boolean writeHexData(FILE* output, 
                            const int8u* data, 
                            int8u length,
                            boolean reverse);
static int8u* readHexDataIntoArray(int8u* result,
                                   int8u hexDataLength,
                                   int8u* line,
                                   int8u lineNumber);
static int8u* skipSpacesInLine(int8u* line);

// *****************************************************************************
// Functions

EmberStatus emberAfTrustCenterExportBackupToFile(const char* filepath)
{
  EmberStatus returnValue = EMBER_ERR_FATAL;
  if (filepath == NULL) {
    return returnValue;
  }

  emberAfSecurityPrintln("Opening file '%p'",
                         filepath);

  FILE* output = fopen(filepath, WRITE_FLAGS);
  if (NULL == output) {
    emberAfSecurityPrintln("Failed to open file: %p",
                           strerror(errno));
    return returnValue;
  }

  EmberAfLinkKeyBackupData exportKeyList[MAX_BACKUP_LIST_LENGTH];
  EmberAfTrustCenterBackupData export;
  EmberStatus status;

  export.maxKeyListLength = MAX_BACKUP_LIST_LENGTH;
  export.keyList = exportKeyList;
  status = emberTrustCenterExportBackupData(&export);
  
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("%p: Failed to get TC backup data.", "Error");
    goto exportEnd;
  }
  
  EmberEUI64 myEui64;
  emberAfGetEui64(myEui64);

  fprintf(output, 
          "# TC Backup File from: (>)%02X%02X%02X%02X%02X%02X%02X%02X\n",
          myEui64[7],
          myEui64[6],
          myEui64[5],
          myEui64[4],
          myEui64[3],
          myEui64[2],
          myEui64[1],
          myEui64[0]);
  int8u mfgString[MFG_STRING_MAX_LENGTH + 1];
  emberAfFormatMfgString(mfgString);
  fprintf(output,
          "# MFG String: %s\n\n", 
          mfgString);

  fprintf(output, "# Format\n");
  fprintf(output, "# ------\n");
  fprintf(output, "# Extended Pan ID: <extended-pan-id-big-endian>\n");
  fprintf(output, "# Key: <EUI64-big-endian>  <Key Data>\n");
  fprintf(output, "# Key: ...\n");
  fprintf(output, "# Key: ...\n\n");

  fprintf(output, 
          "Extended PAN ID: %02X%02X%02X%02X%02X%02X%02X%02X\n\n",
          export.extendedPanId[7],
          export.extendedPanId[6],
          export.extendedPanId[5],
          export.extendedPanId[4],
          export.extendedPanId[3],
          export.extendedPanId[2],
          export.extendedPanId[1],
          export.extendedPanId[0]);

  int8u i = 0;
  while (i < export.keyListLength) {
    fprintf(output, "Key: ");
    writeHexData(output, 
                 export.keyList[i].deviceId, 
                 EUI64_SIZE,
                 TRUE);      // reverse order?
    fprintf(output, "  ");
    writeHexData(output, 
                 emberKeyContents(&(export.keyList[i].key)), 
                 EMBER_ENCRYPTION_KEY_SIZE,
                 FALSE);     // reverse order?
    fprintf(output, "\n");
    fflush(output);
    i++;
  }

  emberAfSecurityPrintln("Wrote %d entries to file.", export.keyListLength);
  returnValue = EMBER_SUCCESS;

 exportEnd:
  fclose(output);
  return returnValue;
}

static boolean writeHexData(FILE* output, 
                            const int8u* data, 
                           int8u length, 
                            boolean reverseOrder)
{
  int8u i;
  for (i = 0; i < length; i++) {
    int8u j = (reverseOrder
               ? (length - i - 1)
               : i);
    if (0 > fprintf(output, "%02X", data[j])) {
      return FALSE;
    }
  }
  fflush(output);
  return TRUE;
}


// I miss Perl.  Something to make me feel at home.  Remove trailing '\n' if 
// present.  Depending on format that could be CR, LF, or both. 
static int chomp(char* line)
{
  int length = strnlen(line, MAX_LINE_LENGTH);
  int newLength = length;
  int8u i;
  for (i = 1; i <= 2; i++) {
    char lastChar = line[length - i];
    if (lastChar == (char)0x0A        // LF
        || lastChar == (char)0x0D) {  // CR
      line[length -i ] = '\0';
      newLength--;
    }
  }
  return newLength;
}


EmberStatus emberAfTrustCenterImportBackupFromFile(const char* filepath)
{
  EmberStatus returnValue = EMBER_ERR_FATAL;
  if (filepath == NULL) {
    return returnValue;
  }

  emberAfSecurityPrintln("Opening file '%p'",
                         filepath);

  FILE* input = fopen(filepath, READ_FLAGS);
  if (input == NULL) {
    emberAfSecurityPrintln("Failed to open file: %p",
                           strerror(errno));
    return returnValue;
  }

  EmberAfTrustCenterBackupData import;
  MEMSET(&import, 0, sizeof(EmberAfTrustCenterBackupData));

  EmberAfLinkKeyBackupData importKeyList[MAX_BACKUP_LIST_LENGTH];
  import.maxKeyListLength = MAX_BACKUP_LIST_LENGTH;
  import.keyList = importKeyList;

  int8u keyListIndex = 0;
  int lineNumber = 0;

  int8u line[MAX_LINE_LENGTH];

  while (NULL != fgets(line, MAX_LINE_LENGTH, input)) {
    const char* keyWordPtr;
    int8u* linePtr = line;
    lineNumber++;
    chomp(linePtr);
    linePtr = skipSpacesInLine(linePtr);
    if (strnlen(line, MAX_LINE_LENGTH) == 0) {
      continue;
    }
    if (*linePtr == '#') { // comment line
      continue;
    }
    int8u* endToken;
    endToken = strchr(linePtr, ':');
    if (endToken == NULL) {
      emberAfSecurityPrintln("Error: Invalid format on line %d, must be <token>: <value>\n",
                             lineNumber);
      goto importEnd;
    }
    // truncate the line so that we can compare this token to known values.
    *endToken = '\0';
    if (0 == strncasecmp(linePtr, 
                         extendedPanIdKeyWord, 
                         strlen(extendedPanIdKeyWord) + 1)) { // +1 for '\0'
      if (0 != MEMCOMPARE(import.extendedPanId,
                          emberAfNullEui64,
                          EUI64_SIZE)) {
        emberAfSecurityPrintln("Error: Duplicate Extended PAN ID token on line %d.\n",
                               lineNumber);
        goto importEnd;
      }
      keyWordPtr = extendedPanIdKeyWord;

    } else if (0 == strncasecmp(linePtr, 
                                keyKeyWord,
                                strlen(keyKeyWord) + 1)) { // +1 for '\0'
      keyWordPtr = keyKeyWord;

    } else {
      emberAfSecurityPrintln("Error: Unknown token '%p' on line %d\n",
                             linePtr,
                             lineNumber);
      goto importEnd;
    }

    int8u temp[EUI64_SIZE];
    int8u* data = ((keyWordPtr == extendedPanIdKeyWord)
                   ? import.extendedPanId
                   : import.keyList[keyListIndex].deviceId);

    linePtr = skipSpacesInLine(endToken + 1);
    linePtr = readHexDataIntoArray(temp,
                                   EUI64_SIZE,
                                   linePtr,
                                   lineNumber);
    if (NULL == linePtr) {
      emberAfSecurityPrintln("Error: Failed to parse %p on line %d.", 
                             (keyWordPtr == extendedPanIdKeyWord
                              ? "Extended PAN ID"
                              : "key data"),
                             lineNumber);
      goto importEnd;
    }
    
    // Device EUI64 and PAN ID are written out in big-endian.  reverse
    // them so they are little-endian in the struct.
    emberReverseMemCopy(data, temp, EUI64_SIZE);


    if (keyWordPtr == extendedPanIdKeyWord) {
      emberAfSecurityPrintln("Imported extended PAN ID: (>)%02X%02X%02X%02X%02X%02X%02X%02X\n",
                             import.extendedPanId[7],
                             import.extendedPanId[6],
                             import.extendedPanId[5],
                             import.extendedPanId[4],
                             import.extendedPanId[3],
                             import.extendedPanId[2],
                             import.extendedPanId[1],
                             import.extendedPanId[0]);
      continue;
    }

    linePtr = skipSpacesInLine(linePtr);
    if (*linePtr == '\0') {
      emberAfSecurityPrintln("Missing key-data on line %d", lineNumber);
      goto importEnd;
    }

    if (NULL == readHexDataIntoArray(emberKeyContents(&(import.keyList[keyListIndex].key)),
                                     EMBER_ENCRYPTION_KEY_SIZE,
                                     linePtr,
                                     lineNumber)) {
      emberAfSecurityPrintln("Error: Failed to parse key-data on line %d.", lineNumber);
      goto importEnd;
    }
    emberAfSecurityPrint("Imported data for EUI64 ");
    emberAfPrintBigEndianEui64(import.keyList[keyListIndex].deviceId);
    emberAfSecurityPrint(", key ");
    emberAfPrintZigbeeKey(emberKeyContents(&(import.keyList[keyListIndex].key)));
    emberAfSecurityPrintln("");
    keyListIndex++;
  }
  import.keyListLength = keyListIndex;

  EmberStatus status = emberTrustCenterImportBackupAndStartNetwork(&import);
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("%p: Failed to import backup data and form network.",
                           "Error");
  } else {
    emberAfSecurityPrintln("Import from file successful.");
    returnValue = EMBER_SUCCESS;
  }

 importEnd:
  fclose(input);
  return returnValue;
}

static int8u* skipSpacesInLine(int8u* line)
{
  while (*line == ' '
         || *line == '\t') {
    line++;
  }
  return line;
}

static int8u* readHexDataIntoArray(int8u* result,
                                   int8u hexDataLength,
                                   int8u* line,
                                   int8u lineNumber)
{
  int8u* ptr = result;
  int8u i;
  for (i = 0; i < hexDataLength * 2; i++) {
    int8u nibble;
    char data[2];
    int temp;

    if (line[i] == '\0') {
      emberAfSecurityPrintln("Error: hex data too short on line %d.",
                             lineNumber);
      return NULL;
    }

    data[0] = line[i];
    data[1] = '\0';

    if (1 != sscanf(data, "%x", &temp)) {
      emberAfSecurityPrintln("Error: Invalid character found on line %d.",
                             lineNumber);
      return NULL;
    }

    nibble = (int8u)temp;

    if (i % 2 == 0) {
      *ptr = (nibble << 4); 
    } else {
      *ptr |= nibble;
      ptr++;
    }
 }
  return &(line[i]);
}


#endif // EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT
