/** @file hal/micro/cortexm3/memmap-tables.h
 * @brief EM300 series memory map address table definitions
 *
 * <!-- Copyright 2009 by Ember Corporation.             All rights reserved.-->
 */
#ifndef __MEMMAP_TABLES_H__
#define __MEMMAP_TABLES_H__

#include "hal/micro/cortexm3/bootloader/ebl.h"

// The start of any EmberZNet image will always contain the following 
// data in flash:
//    Top of stack pointer            [4 bytes]
//    Reset vector                    [4 bytes]
//    NMI handler                     [4 bytes]
//    Hard Fault handler              [4 bytes]
//    Address Table Type              [2 bytes]
//    Address Table Version           [2 bytes]
//    Pointer to full vector table    [4 bytes]
//  Following this will be additional data depending on the address table type

// The address tables take the place of the standard cortex interrupt vector
// tables.  They are designed such that the first entries are the same as the 
// first entries of the cortex vector table.  From there, the address tables
// point to a variety of information, including the location of the full
// cortex vector table, which is remapped to this location in cstartup

// ****************************************************************************
// If any of these address table definitions ever need to change, it is highly
// desirable to only add new entries, and only add them on to the end of an
// existing address table... this will provide the best compatibility with
// any existing code which may utilize the tables, and which may not be able to 
// be updated to understand a new format (example: bootloader which reads the 
// application address table)



#define IMAGE_INFO_MAXLEN 32

// Description of the Application Address Table (AAT)
// The application address table recieves somewhat special handling, as the
//   first 128 bytes of it are treated as the EBL header with ebl images.
//   as such, any information required by the bootloader must be present in
//   those first 128 bytes.  Other information that is only used by applications 
//   may follow.
//   Also, some of the fields present within those first 128 bytes are filled
//   in by the ebl generation process, either as part of em3xx_convert or
//   when an s37 is loaded by em3xx_load.  An application using these
//   values should take caution in case the image was loaded in some way that
//   did not fill in the auxillary information.
typedef struct {
  HalBaseAddressTableType baseTable;
  // The following fields are used for ebl and bootloader processing.
  //   See the above description for more information.
  int8u platInfo;   // type of platform, defined in micro.h
  int8u microInfo;  // type of micro, defined in micro.h
  int8u phyInfo;    // type of phy, defined in micro.h
  int8u aatSize;    // size of the AAT itself
  int16u softwareVersion;  // EmberZNet SOFTWARE_VERSION
  int16u softwareBuild;  // EmberZNet EMBER_BUILD_NUMBER
  int32u timestamp; // Unix epoch time of .ebl file, filled in by ebl gen
  int8u imageInfo[IMAGE_INFO_MAXLEN];  // string, filled in by ebl generation
  int32u imageCrc;  // CRC over following pageRanges, filled in by ebl gen
  pageRange_t pageRanges[6];  // Flash pages used by app, filled in by ebl gen
  
  void *simeeBottom;
  
  int32u customerApplicationVersion; // a version field for the customer

  void *internalStorageBottom;

  // reserve the remainder of the first 128 bytes of the AAT in case we need
  //  to go back and add any values that the bootloader will need to reference,
  //  since only the first 128 bytes of the AAT become part of the EBL header.
  int32u bootloaderReserved[8];
  
  //////////////
  // Any values after this point are still part of the AAT, but will not
  //   be included as part of the ebl header

  void *debugChannelBottom;
  void *noInitBottom;
  void *appRamTop;
  void *globalTop;
  void *cstackTop;  
  void *initcTop;
  void *codeTop;
  void *cstackBottom;
  void *heapTop;
  void *simeeTop;
  void *debugChannelTop;
} HalAppAddressTableType;

extern const HalAppAddressTableType halAppAddressTable;


// Description of the Bootloader Address Table (BAT)
typedef struct {
  HalBaseAddressTableType baseTable;
  int16u bootloaderType;
  int16u bootloaderVersion;  
  const HalAppAddressTableType *appAddressTable;
  
  // plat/micro/phy info added in version 0x0104
  int8u platInfo;   // type of platform, defined in micro.h
  int8u microInfo;  // type of micro, defined in micro.h
  int8u phyInfo;    // type of phy, defined in micro.h
  int8u reserved;   // reserved for future use
  
  // moved to this location after plat/micro/phy info added in version 0x0104
  void (*eblProcessInit)(EblConfigType *config,
                         void *dataState,
                         int8u *tagBuf,
                         int16u tagBufLen,
                         boolean returnBetweenBlocks);  
  BL_Status (*eblProcess)(const EblDataFuncType *dataFuncs, 
                          EblConfigType *config,
                          const EblFlashFuncType *flashFuncs);
  EblDataFuncType *eblDataFuncs; 
 
  // these eeprom routines happen to be app bootloader specific
  // added in version 0x0105
  int8u (*eepromInit)(void);
  int8u (*eepromRead)( int32u address, int8u *data, int16u len);
  int8u (*eepromWrite)( int32u address, int8u const *data, int16u len);
  void (*eepromShutdown)(void);
  const void *(*eepromInfo)(void);
  int8u (*eepromErase)(int32u address, int32u len);
  boolean (*eepromBusy)(void);

  // These variables hold extended version information
  // added in version 0x0109
  int16u bootloaderBuild; // the build number associated with bootloaderVersion
  int16u reserved2;       // reserved for future use
  int32u customerBootloaderVersion; // hold a customer specific bootloader version
  
  //pointer to reset info area?

  // Left for reference.  These items were exposed on the 2xx. Do we want
  //  to add them to the 3xx?
  //void *        bootCodeBaseW;                  // $??CODE_LO
  //int16u        bootCodeSizeW;                  // $??CODE_SIZEW
  //void *        bootConstBaseW;                 // $??CONST_LO Relocated
  //int16u        bootConstSize;                  // $??CONST_SIZE
  //int8u *       bootName;                       //=>const int8u bootName[];

} HalBootloaderAddressTableType;

extern const HalBootloaderAddressTableType halBootloaderAddressTable;



// Description of the Ramexe Address Table (RAT)
typedef struct {
  HalBaseAddressTableType baseTable;
  void *startAddress;
  void *endAddress;
} HalRamexeAddressTableType;

extern const HalRamexeAddressTableType halRamAddressTable; 


#define APP_ADDRESS_TABLE_TYPE          (0x0AA7)
#define BOOTLOADER_ADDRESS_TABLE_TYPE   (0x0BA7)
#define RAMEXE_ADDRESS_TABLE_TYPE       (0x0EA7)

// The current versions of the address tables.
// Note that the major version should be updated only when a non-backwards
// compatible change is introduced (like removing or rearranging fields)
// adding new fields is usually backwards compatible, and their presence can
// be indicated by incrementing only the minor version
#define AAT_VERSION                     (0x0108)
#define AAT_MAJOR_VERSION               (0x0100)
#define AAT_MAJOR_VERSION_MASK          (0xFF00)

// *** AAT Version history: ***
//0x0108 - Add the simeeTop to the AAT so that we can place the simee wherever
//         we want to. This change also adds a pointer to the internalStorage
//         region to the beginning of the AAT so that bootloaders can confirm
//         that they have the same concept of internalStorage as the app.
//         Lastly, this version now includes the top of the debug channel so
//         that we can support unused RAM living above the debug channel.
//0x0107 - Changed a reserved field to the software build number and added a
//         customer application version to the first 128 bytes of the AAT.
//0x0106 - Moved everything from APP_RAM into the heap. Now the value stored
//         in appRamTop is not the APP_RAM's top. Convert needs to know this
//         to correctly compute the space being used for configuration data.
//0x0105 - Added stack and heap information now that we have a stack and
//         heap that can grow towards each other.
//0x0104 - Flash/ram usage information added for em3xx_convert
//0x0103 - Added debugchannel shared memory pointer to AAT
//0x0102 - Combined AAT and EBL header
//0x0101 - Initial version

#define BAT_NULL_VERSION                (0x0000)
#define BAT_VERSION                     (0x0109)
#define BAT_MAJOR_VERSION               (0x0100)
#define BAT_MAJOR_VERSION_MASK          (0xFF00)

// *** BAT Version history: ***
// 0x0109 - Added the bootloader build number and a customer bootloader version
//          field to help track their versions as well.
// 0x0108 - Added halEepromInfo(), halEepromErase(), and halEepromBusy() APIs
//          Extended halEepromInit to return a status in case of failure
// 0x0107 - Added halEepromShutdown() API, Added support for reading/writing 
//          arbitrary addresses in halEepromRead/Write() APIs
// 0x0106 - Standalone bootloader ota support aded
// 0x0105 - Add function pointers for shared app bootloader dataflash drivers
// 0x0104 - PLAT/MICRO/PHY type information added
// 0x0103 - Add function pointers for shared ebl processing code part 2
// 0x0102 - Add function pointers for shared ebl processing code part 1
// 0x0101 - Initial version


#define RAT_VERSION                     (0x0101)

#include "memmap-fat.h"

#endif //__MEMMAP_TABLES_H__
