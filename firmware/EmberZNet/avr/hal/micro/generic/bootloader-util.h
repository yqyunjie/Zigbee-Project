/** @file hal/micro/generic/bootloader-util.h
 * 
 * @brief Convenience functions and wrappers for use with the bootloader.
 *
 * The EmberZNet stack does not use these functions, however, many of 
 * the sample applications do use them.
 *
 * Some functions in this file return an EmberStatus value. See 
 * error-def.h for definitions of all EmberStatus return values.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

#ifndef __BOOTLOADER_UTIL_H__
#define __BOOTLOADER_UTIL_H__



/** @brief Reads the image header data from the image beginning at
 *  the segment specified by startSegment.
 *
 *  @param startSegment  The download space segment number where the image
 *  is stored.  This is usually BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT.
 *
 *  @param length         Either a pointer to an int16u type into which
 *  the function can copy the image length in segments, or NULL.  The length
 *  will not be returned if this value is NULL.
 *
 *  @param timestamp: Either a pointer to an int32u type into which
 *  the function can copy the image's unix epoch timestamp, or NULL.  
 *  The timestamp will not be returned if this value is NULL.
 *
 *  @param userData: Either a pointer to an int8u array into which
 *  the function can copy the image's user data, or NULL.  No user data will
 *  be returned if this value is NULL.
 *
 *  @param userDataLength: The length of the data to copy to userData.  This
 *  value is ignored if userData is NULL.
 *
 *  @return TRUE if the image header has a valid signature field.
 *          FALSE if the image header signature field is invalid.
 *
 */

boolean emberGetImageData(int16u startSegment, 
                          int16u *length,
                          int32u *timestamp,
                          int8u  *userData,
                          int8u  userDataLength);


#ifndef DOXYGEN_SHOULD_SKIP_THIS
boolean emberImageIsValidSpecifyingSpace(int16u startSegment,
                                         int8u memorySpace);
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief Calculates the CRC-32 checksum for the image starting at the
 *  specified segment and compares this value to the validity tag found in
 *  the image's header.
 *
 *  @param start: The segment number of the download space segment where the
 *  image starts.  This is usually BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT.
 *
 *  @return TRUE if the calculated CRC-32 checksum and the header tags match.
 *  FALSE if the calculated CRC-32 checksum and the header tags do not match.
 *  
 */
#define emberImageIsValid(start) \
  emberImageIsValidSpecifyingSpace(start, BOOTLOADER_DOWNLOAD_SPACE)

#endif // __BOOTLOADER_UTIL_H__
