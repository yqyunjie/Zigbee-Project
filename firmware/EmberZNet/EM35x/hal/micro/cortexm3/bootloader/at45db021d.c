/*
 * File: at45db021d.c
 * Description: SPI Interface to Atmel AT45DB021D Serial Flash Memory
 * containing 264kBytes of memory.
 *
 * This file provides an interface to the AT45DB021D flash memory to allow
 * writing, reading and status polling.
 *
 * The write function uses command 82 to write data to the flash buffer
 * which then erases the page and writes the memory.
 *
 * The read function uses command D2 to read directly from memory without
 * using the buffer.
 *
 * The Ember remote storage code operates using 128 byte blocks of data. This
 * interface will write two 128 byte blocks to each remote page utilizing
 * 256 of the 264 bytes available per page. This format effectively uses
 * 256kBytes of memory.
 *
 * Copyright 2010 by Ember Corporation. All rights reserved.                *80*
 *
 */

/*
 * When EEPROM_USES_SHUTDOWN_CONTROL is defined in the board header, 
 * logic is enabled in the EEPROM driver which drives PB7 high upon EEPROM 
 * initialization.  In Ember reference designs, PB7 acts as an EEPROM enable 
 * pin and therefore must be driven high in order to use the EEPROM.  
 * This option is intended to be enabled when running app-bootloader on 
 * designs based on current Ember reference designs.
 */

#include PLATFORM_HEADER
#include "hal/micro/bootloader-eeprom.h"
#include "bootloader-common.h"
#include "bootloader-serial.h"
#include "hal/micro/micro.h"
#include BOARD_HEADER
#include "hal/micro/cortexm3/memmap.h"

#define THIS_DRIVER_VERSION (0x0109)

#if !(BAT_VERSION == THIS_DRIVER_VERSION)
  #error External Flash Driver must be updated to support new API requirements
#endif


//
// ~~~~~~~~~~~~~~~~~~~~~~~~~~ Generic SPI Routines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
static int8u halSpiReadWrite(int8u txData)
{
  int8u rxData;

  SC2_DATA = txData;
  while( (SC2_SPISTAT&SC_SPITXIDLE) != SC_SPITXIDLE) {} //wait to finish
  if ((SC2_SPISTAT&SC_SPIRXVAL) != SC_SPIRXVAL)
    rxData = 0xff;
  else
    rxData = SC2_DATA;

  return rxData;
}

static void halSpiWrite(int8u txData)
{
  SC2_DATA = txData;
  while( (SC2_SPISTAT&SC_SPITXIDLE) != SC_SPITXIDLE) {} //wait to finish
  (void) SC2_DATA;
}

static int8u halSpiRead(void)
{
  return halSpiReadWrite(0xFF);
}

//
// ~~~~~~~~~~~~~~~~~~~~~~~~ Device Specific Interface ~~~~~~~~~~~~~~~~~~~~~~~~~~
//

#define DEVICE_SIZE       (256ul * 1024ul)   // 256 kBytes

#define DEVICE_PAGE_SZ     256
#define DEVICE_PAGE_MASK   255
#define DEVICE_WORD_SIZE   (1)

#define AT_MANUFACTURER_ID 0x1F

// Atmel Op Codes for SPI mode 0 or 3
#define AT_OP_RD_MFG_ID 0x9F //read manufacturer ID
#define AT_OP_RD_STATUS_REG 0xD7   // status register read
#define AT_OP_RD_MEM_PG 0xD2     // memory page read
#define AT_OP_WR_TO_BUF_TO_MEM_W_ERASE 0x82   // write into buffer, then to memory, erase first
#define AT_OP_RD_MEM_TO_BUF 0x53   // memory to buffer read
// other commands not used by this driver:
//#define AT_OP_RD_BUF 0xD4        // buffer read
//#define AT_OP_ERASE_PG 0x81      // page erase
//#define AT_OP_ERASE_BL 0x50      // block erase
//#define AT_OP_COMPARE_MEM_TO_BUF 0x60   // memory to buffer compare

#define CHIP_RDY 0x80   // ready bit

#define EEPROM_CS_ACTIVE()   do { GPIO_PACLR |= 0x0008; } while (0) // CS pin, activate chip; PA3
#define EEPROM_CS_INACTIVE() do { GPIO_PASET |= 0x0008; } while (0) // CS pin, deactivate chip; PA3

// could be optionally added
#define EEPROM_WP_ON()  do { ; } while (0)  // WP pin, write protection on
#define EEPROM_WP_OFF() do { ; } while (0)  // WP pin, write protection off


//This function reads the manufacturer ID to verify this driver is
//talking to the chip this driver is written for.
static int8u halAT45DB021DVerifyMfgId(void)
{
  int8u mfgId;

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();
  halSpiWrite(AT_OP_RD_MFG_ID);
  mfgId = halSpiRead();
  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  //If this assert triggers, this driver is being used to talk to
  //the wrong chip.
  return (mfgId==AT_MANUFACTURER_ID)? EEPROM_SUCCESS : EEPROM_ERR_INVALID_CHIP;
}


#ifdef EEPROM_USES_SHUTDOWN_CONTROL
  // Define PB7 as the EEPROM_ENABLE_PIN
  #define EEPROM_ENABLE_PIN PORTB_PIN(7)
  #define SET_EEPROM_ENABLE_PIN() GPIO_PBSET = PB7
  #define CLR_EEPROM_ENABLE_PIN() GPIO_PBCLR = PB7
#endif //EEPROM_USES_SHUTDOWN_CONTROL

// Initialization constants.  For more detail on the resulting waveforms,
// see the EM35x datasheet.
#define SPI_ORD_MSB_FIRST (0<<SC_SPIORD_BIT) // Send the MSB first
#define SPI_ORD_LSB_FIRST (1<<SC_SPIORD_BIT) // Send the LSB first

#define SPI_PHA_FIRST_EDGE (0<<SC_SPIPHA_BIT)  // Sample on first edge
#define SPI_PHA_SECOND_EDGE (1<<SC_SPIPHA_BIT) // Sample on second edge

#define SPI_POL_RISING_LEAD  (0<<SC_SPIPOL_BIT) // Leading edge is rising
#define SPI_POL_FALLING_LEAD (1<<SC_SPIPOL_BIT) // Leading edge is falling

// configure for fastest (12MHz) line rate.
// rate = 24MHz / (2 * (LIN + 1) * (2^EXP))
#if defined(CORTEXM3_EM350) || defined(CORTEXM3_STM32W108)
  // limited to 6MHz
  #define SPI_LIN  (1)  // 6Mhz
  #define SPI_EXP  (0)
#else
  #define SPI_LIN  (0)    // 12Mhz - FOR EM35x
  #define SPI_EXP  (0)
#endif
//#define SPI_LIN  (2)  // 1Mhz
//#define SPI_EXP  (2)

int8u halEepromInit(void)
{
  // GPIO assignments
  // PA0: SC2MOSI
  // PA1: SC2MISO
  // PA2: SC2SCLK
  // PA3: SC2 chip select

  // Set EEPROM_ENABLE_PIN high as part of EEPROM init
  #ifdef EEPROM_USES_SHUTDOWN_CONTROL
    halGpioConfig(EEPROM_ENABLE_PIN, GPIOCFG_OUT);
    SET_EEPROM_ENABLE_PIN();
  #endif //EEPROM_USES_SHUTDOWN_CONTROL

  //-----SC2 SPI Master GPIO configuration
  #if defined(CORTEXM3_EM350) || defined(CORTEXM3_STM32W108)
    // requires special mode for CLK
    GPIO_PACFGL =   (GPIOCFG_OUT_ALT  << PA0_CFG_BIT)|  // PA0 MOSI
                    (GPIOCFG_IN       << PA1_CFG_BIT)|  // PA1 MISO
                    (0xb              << PA2_CFG_BIT)|  // PA2 SCLK
                    (GPIOCFG_OUT      << PA3_CFG_BIT);  // PA3 nSSEL
  #else
    GPIO_PACFGL =   (GPIOCFG_OUT_ALT  << PA0_CFG_BIT)|  // PA0 MOSI
                    (GPIOCFG_IN       << PA1_CFG_BIT)|  // PA1 MISO
                    (GPIOCFG_OUT_ALT  << PA2_CFG_BIT)|  // PA2 SCLK
                    (GPIOCFG_OUT      << PA3_CFG_BIT);  // PA3 nSSEL
  #endif

  SC2_RATELIN = SPI_LIN;
  SC2_RATEEXP = SPI_EXP;
  SC2_SPICFG  =  0;
  SC2_SPICFG =  (1 << SC_SPIMST_BIT)|  // 4; master control bit
                (SPI_ORD_MSB_FIRST | SPI_PHA_FIRST_EDGE | SPI_POL_RISING_LEAD);
  SC2_MODE   =  SC2_MODE_SPI;
  #if defined(CORTEXM3_EM350) || defined(CORTEXM3_STM32W108)
    // required to enable high speed SCLK
    SC2_TWICTRL2 |= SC_TWIACK_BIT;
  #endif

  //The datasheet describes timing parameters for powerup.  To be
  //the safest and impose no restrictions on further operations after
  //powerup/init, delay worst case of 20ms.  (I'd much rather worry about
  //time and power consumption than potentially unstable behavior).
  //The Atmel AT45DB021D datasheet says that 20ms is the max "Power-Up Device
  //Delay before Write Allowed", and 1ms is the delay "required before the
  //device can be selected in order to perform a read operation."
  halCommonDelayMicroseconds(20000);
  
  //Make sure this driver is talking to the correct chip
  return halAT45DB021DVerifyMfgId();
}

static const HalEepromInformationType partInfo = {
  EEPROM_INFO_VERSION,
  0,  // no specific capabilities
  0,  // page erase time (not suported or needed in this driver)
  0,  // part erase time (not suported or needed in this driver)
  DEVICE_PAGE_SZ,  // page size
  DEVICE_SIZE,  // device size
  "AT45DB021D",
  DEVICE_WORD_SIZE // word size in bytes
};

const HalEepromInformationType *halEepromInfo(void)
{
  return &partInfo;
}

boolean halEepromBusy(void)
{
  // This driver doesn't support busy detection
  return FALSE;
}

// halAT45DB021DReadStatus
//
// Read the status register, return value read
//
// return: status value
//
static int8u halAT45DB021DReadStatus(void)
{
  int8u retVal;

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();
  halSpiWrite(AT_OP_RD_STATUS_REG);  // cmd D7
  retVal = halSpiRead();
  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  return retVal;
}

void halEepromShutdown(void)
{
  // wait for any outstanding operations to complete before pulling the plug
  while (halAT45DB021DReadStatus() < CHIP_RDY) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");
  #ifdef EEPROM_USES_SHUTDOWN_CONTROL
    CLR_EEPROM_ENABLE_PIN();
  #endif //EEPROM_USES_SHUTDOWN_CONTROL
}

// halAT45DB021DReadBytes
//
// Reads directly from memory, non-buffered
// address: byte address within device
// data: write the data here
// len: number of bytes to read
//
// return: EEPROM_SUCCESS
//
// This routine requires [address .. address+len-1] to fit within a device
// page; callers must enforce this, we do not check here for violators.
//
static int8u halAT45DB021DReadBytes(int32u address, int8u *data, int16u len)
{
  BLDEBUG_PRINT("ReadBytes: address:len ");
//BLDEBUG(serPutHex((int8u)(address >> 24)));
  BLDEBUG(serPutHex((int8u)(address >> 16)));
  BLDEBUG(serPutHex((int8u)(address >>  8)));
  BLDEBUG(serPutHex((int8u)(address      )));
  BLDEBUG_PRINT(":");
  BLDEBUG(serPutHexInt(len));
  BLDEBUG_PRINT("\r\n");

  // make sure chip is ready
  while (halAT45DB021DReadStatus() < CHIP_RDY) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();

  // write opcode for main memory page read
  halSpiWrite(AT_OP_RD_MEM_PG);  // cmd D2

  // write 24 addr bits -- converting address to Atmel 264-byte page addressing
  halSpiWrite((int8u)(address >> 15));
  halSpiWrite((int8u)(address >>  8) << 1);
  halSpiWrite((int8u)(address      ));

  // initialize with 32 don't care bits
  halSpiWrite(0);
  halSpiWrite(0);
  halSpiWrite(0);
  halSpiWrite(0);

  // loop reading data
  BLDEBUG_PRINT("ReadBytes: data: ");
  while(len--) {
    halInternalResetWatchDog();
    *data = halSpiRead();
    BLDEBUG(serPutHex(*data));
    BLDEBUG(serPutChar(' '));
    data++;
  }
  BLDEBUG_PRINT("\r\n");
  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  return EEPROM_SUCCESS;
}

// halAT45DB021DWriteBytes
// address: byte address within device
// data: data to write
// len: number of bytes to write
//
// Create the flash address from page and byte address params. Writes to
// memory thru buffer with page erase.
// This routine requires [address .. address+len-1] to fit within a device
// page; callers must enforce this, we do not check here for violators.
//
// return: EEPROM_SUCCESS
//
static int8u halAT45DB021DWriteBytes(int32u address, const int8u *data,
                                     int16u len)
{
  BLDEBUG_PRINT("WriteBytes: address:len ");
//BLDEBUG(serPutHex((int8u)(address >> 24)));
  BLDEBUG(serPutHex((int8u)(address >> 16)));
  BLDEBUG(serPutHex((int8u)(address >>  8)));
  BLDEBUG(serPutHex((int8u)(address      )));
  BLDEBUG_PRINT(":");
  BLDEBUG(serPutHexInt(len));
  BLDEBUG_PRINT("\r\n");

  // make sure chip is ready
  while (halAT45DB021DReadStatus() < CHIP_RDY) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");

  if( len < DEVICE_PAGE_SZ ) { // partial flash page write
    // read current contents of page into on-chip buffer
    EEPROM_CS_ACTIVE();

    // write opcode
    halSpiWrite(AT_OP_RD_MEM_TO_BUF);  // cmd 53

    // write 24 addr bits -- converting address to Atmel 264-byte page addressing
    halSpiWrite((int8u)(address >> 15));
    halSpiWrite((int8u)(address >>  8) << 1);
    halSpiWrite((int8u)(address      ));

    EEPROM_CS_INACTIVE();

    // wait for completion
    while (halAT45DB021DReadStatus() < CHIP_RDY) {
      BLDEBUG_PRINT("BufPoll\r\n");
    }
  }

  // activate memory
  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();

  // write opcode
  halSpiWrite(AT_OP_WR_TO_BUF_TO_MEM_W_ERASE);  // cmd 82

  // write 24 addr bits -- converting address to Atmel 264-byte page addressing
  halSpiWrite((int8u)(address >> 15));
  halSpiWrite((int8u)(address >>  8) << 1);
  halSpiWrite((int8u)(address      ));

  // loop reading data
  BLDEBUG_PRINT("WriteBytes: data: ");
  while(len--) {
    halInternalResetWatchDog();
    halSpiWrite(*data);
    BLDEBUG(serPutHex(*data));
    BLDEBUG(serPutChar(' '));
    data++;
  }
  BLDEBUG_PRINT("\r\n");

  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  return EEPROM_SUCCESS;
}

// test writing and reading two pages
static int8u halAT45DB021DTest(void)
{
  int8u i;
  int8u wrData[] = {0,1,2,3,4,5,6,7,8,9};
  int8u rdData[] = {0,0,0,0,0,0,0,0,0,0};

  BLDEBUG_PRINT("AT test: read status\r\n");
  BLDEBUG(serPutHex(halAT45DB021DReadStatus()));
  BLDEBUG_PRINT("\r\n");

  BLDEBUG_PRINT("AT test: write data\r\n");
  halAT45DB021DWriteBytes(0x0000, wrData, 10);
  halAT45DB021DWriteBytes(0x0080, wrData, 10);
  halAT45DB021DWriteBytes(0x0300, wrData, 10);
  halAT45DB021DWriteBytes(0x0380, wrData, 10);
  halAT45DB021DReadBytes(0x0000, rdData, 10);
  halAT45DB021DReadBytes(0x0080, rdData, 10);
  halAT45DB021DReadBytes(0x0300, rdData, 10);
  halAT45DB021DReadBytes(0x0380, rdData, 10);
  BLDEBUG_PRINT("AT test: read data ");
  for (i=0;i<10;i++)
    BLDEBUG(serPutHex(rdData[i]));
  BLDEBUG_PRINT("\r\n");

  return 0;
}

void halEepromTest(void)
{
  halAT45DB021DTest();
}

//
// ~~~~~~~~~~~~~~~~~~~~~~~~~ Standard EEPROM Interface ~~~~~~~~~~~~~~~~~~~~~~~~~
//

// halEepromRead
// address: the address in EEPROM to start reading
// data: write the data here
// len: number of bytes to read
//
// return: result of halAT45DB021DReadBytes() call(s) or EEPROM_ERR_ADDR
//
int8u halEepromRead(int32u address, int8u *data, int16u totalLength)
{
  int32u nextPageAddr;
  int16u len;
  int8u status;

  if( address > DEVICE_SIZE || (address + totalLength) > DEVICE_SIZE)
    return EEPROM_ERR_ADDR;

  if( address & DEVICE_PAGE_MASK) {
    // handle unaligned first block
    nextPageAddr = (address & (~DEVICE_PAGE_MASK)) + DEVICE_PAGE_SZ;
    if((address + totalLength) < nextPageAddr){
      // fits all within first block
      len = totalLength;
    } else {
      len = (int16u) (nextPageAddr - address);
    }
  } else {
    len = (totalLength>DEVICE_PAGE_SZ)? DEVICE_PAGE_SZ : totalLength;
  }
  while(totalLength) {
    if( (status=halAT45DB021DReadBytes(address, data, len)) != EEPROM_SUCCESS) {
      return status;
    }
    totalLength -= len;
    address += len;
    data += len;
    len = (totalLength>DEVICE_PAGE_SZ)? DEVICE_PAGE_SZ : totalLength;
  }
  return EEPROM_SUCCESS;
}

// halEepromWrite
// address: the address in EEPROM to start writing
// data: pointer to the data to write
// len: number of bytes to write
//
// return: result of halAT45DB021DWriteBytes() call(s) or EEPROM_ERR_ADDR
//
int8u halEepromWrite(int32u address, const int8u *data, int16u totalLength)
{
  int32u nextPageAddr;
  int16u len;
  int8u status;

  if( address > DEVICE_SIZE || (address + totalLength) > DEVICE_SIZE)
    return EEPROM_ERR_ADDR;

  if( address & DEVICE_PAGE_MASK) {
    // handle unaligned first block
    nextPageAddr = (address & (~DEVICE_PAGE_MASK)) + DEVICE_PAGE_SZ;
    if((address + totalLength) < nextPageAddr){
      // fits all within first block
      len = totalLength;
    } else {
      len = (int16u) (nextPageAddr - address);
    }
  } else {
    len = (totalLength>DEVICE_PAGE_SZ)? DEVICE_PAGE_SZ : totalLength;
  }
  while(totalLength) {
    if( (status=halAT45DB021DWriteBytes(address, data, len)) != EEPROM_SUCCESS) {
      return status;
    }
    totalLength -= len;
    address += len;
    data += len;
    len = (totalLength>DEVICE_PAGE_SZ)? DEVICE_PAGE_SZ : totalLength;
  }
  return EEPROM_SUCCESS;
}

int8u halEepromErase(int32u address, int32u totalLength)
{
  // This driver doesn't support or need erasing
  return EEPROM_ERR_NO_ERASE_SUPPORT;
}
