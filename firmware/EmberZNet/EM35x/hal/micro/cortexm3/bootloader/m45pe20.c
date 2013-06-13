/*
 * File: m45pe20.c
 * Description: SPI Interface to Micron/Numonyx M45PE20 Serial Flash Memory
 * containing 256kBytes of memory.
 *
 * This file provides an interface to the M45PE20 flash memory to allow
 * writing, reading and status polling.
 *
 * The write function uses command 0x0A to write data to the flash buffer
 * which then erases the page and writes the memory.
 *
 * The read function uses command 0x03 to read directly from memory without
 * using the buffer.
 *
 * The Ember remote storage code operates using 256 byte blocks of data. This
 * interface will write one 256 byte block to each remote page utilizing
 * 256 of the 256 bytes available per page. This format effectively uses
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

#if BAT_VERSION != THIS_DRIVER_VERSION
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

#define MICRON_MANUFACTURER_ID 0x20

// Micron Op Codes for SPI mode 0 or 3
#define MICRON_OP_RD_MFG_ID 0x9F //read manufacturer ID
#define MICRON_OP_WRITE_ENABLE 0x06     // write enable
#define MICRON_OP_RD_STATUS_REG 0x05   // status register read
#define MICRON_OP_RD_MEM 0x03     // memory read
#define MICRON_OP_PAGE_WRITE 0x0A     // page write
// other commands not used by this driver:
//#define MICRON_OP_FAST_RD_MEM 0x0B     // fast memory read
//#define MICRON_OP_PAGE_ERASE 0xDB     // page erase
//#define MICRON_OP_SECTOR_ERASE 0xD8     // sector erase

// Write in progress bit mask for status register
#define MICRON_SR_WIP_MASK 0x01

#define EEPROM_CS_ACTIVE()   do { GPIO_PACLR |= 0x0008; } while (0) // CS pin, activate chip; PA3
#define EEPROM_CS_INACTIVE() do { GPIO_PASET |= 0x0008; } while (0) // CS pin, deactivate chip; PA3

// could be optionally added
#define EEPROM_WP_ON()  do { ; } while (0)  // WP pin, write protection on
#define EEPROM_WP_OFF() do { ; } while (0)  // WP pin, write protection off


//This function reads the manufacturer ID to verify this driver is
//talking to the chip this driver is written for.
static int8u halM45PE20VerifyMfgId(void)
{
  int8u mfgId;

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();
  halSpiWrite(MICRON_OP_RD_MFG_ID);
  mfgId = halSpiRead();
  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  //If this assert triggers, this driver is being used to talk to
  //the wrong chip.
  return (mfgId==MICRON_MANUFACTURER_ID)?EEPROM_SUCCESS:EEPROM_ERR_INVALID_CHIP;
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
  //powerup/init, delay worst case of 10ms.  (I'd much rather worry about
  //time and power consumption than potentially unstable behavior).
  halCommonDelayMicroseconds(10000);
  
  //Make sure this driver is talking to the correct chip
  return halM45PE20VerifyMfgId();
}

static const HalEepromInformationType partInfo = {
  EEPROM_INFO_VERSION,
  0,  // no specific capabilities
  0,  // page erase time (not suported or needed in this driver)
  0,  // part erase time (not suported or needed in this driver)
  DEVICE_PAGE_SZ,  // page size
  DEVICE_SIZE,  // device size
  "M45PE20",
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


// halM45PE20ReadStatus
//
// Read the status register, return value read
//
// return: status value
//
static int8u halM45PE20ReadStatus(void)
{
  int8u retVal;

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();
  halSpiWrite(MICRON_OP_RD_STATUS_REG);  // cmd 0x05
  retVal = halSpiRead();
  EEPROM_CS_INACTIVE();
  EEPROM_WP_ON();

  return retVal;
}

void halEepromShutdown(void)
{
  // wait for any outstanding operations to complete before pulling the plug
  while(halM45PE20ReadStatus() & MICRON_SR_WIP_MASK) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");
  #ifdef EEPROM_USES_SHUTDOWN_CONTROL
    CLR_EEPROM_ENABLE_PIN();
  #endif //EEPROM_USES_SHUTDOWN_CONTROL
}

// halM45PE20ReadBytes
//
// Reads directly from memory, non-buffered
// address: byte address within device
// data: write the data here
// len: number of bytes to read
//
// return: EEPROM_SUCCESS
//
// This routine requires [address .. address+len-1] to fit within the device;
// callers must enforce this, we do not check here for violators.
// This device handles reading unaligned blocks
//
static int8u halM45PE20ReadBytes(int32u address, int8u *data, int16u len)
{
  BLDEBUG_PRINT("ReadBytes: address:len ");
//BLDEBUG(serPutHex((int8u)(address >> 24)));
  BLDEBUG(serPutHex((int8u)(address >> 16)));
  BLDEBUG(serPutHex((int8u)(address >>  8)));
  BLDEBUG(serPutHex((int8u)(address      )));
  BLDEBUG_PRINT(":");
  BLDEBUG(serPutHexInt(len));
  BLDEBUG_PRINT("\r\n");

  // Make sure EEPROM is not in a write cycle
  while(halM45PE20ReadStatus() & MICRON_SR_WIP_MASK) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");

  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();

  // write opcode for main memory read
  halSpiWrite(MICRON_OP_RD_MEM);  // cmd 0x03

  // write 24 addr bits
  halSpiWrite((int8u)(address >> 16));
  halSpiWrite((int8u)(address >>  8));
  halSpiWrite((int8u)(address      ));

  // loop reading data
  BLDEBUG_PRINT("ReadBytes: data: ");
  while(len--) {
    halResetWatchdog();
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

// halM45PE20WriteBytes
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
static int8u halM45PE20WriteBytes(int32u address, const int8u *data, int16u len)
{
  BLDEBUG_PRINT("WriteBytes: address:len ");
//BLDEBUG(serPutHex((int8u)(address >> 24)));
  BLDEBUG(serPutHex((int8u)(address >> 16)));
  BLDEBUG(serPutHex((int8u)(address >>  8)));
  BLDEBUG(serPutHex((int8u)(address      )));
  BLDEBUG_PRINT(":");
  BLDEBUG(serPutHexInt(len));
  BLDEBUG_PRINT("\r\n");

  // Make sure EEPROM is not in a write cycle
  while(halM45PE20ReadStatus() & MICRON_SR_WIP_MASK) {
    BLDEBUG_PRINT("Poll ");
  }
  BLDEBUG_PRINT("\r\n");

  // activate memory
  EEPROM_WP_OFF();
  EEPROM_CS_ACTIVE();

  // Send write enable op
  halSpiWrite(MICRON_OP_WRITE_ENABLE);

  // Toggle CS (required for write enable to take effect)
  EEPROM_CS_INACTIVE();
  EEPROM_CS_ACTIVE();

  // send page write command
  halSpiWrite(MICRON_OP_PAGE_WRITE);  // cmd 0x0A

  // write 24 addr bits
  halSpiWrite((int8u)(address >> 16));
  halSpiWrite((int8u)(address >>  8));
  halSpiWrite((int8u)(address      ));

  // loop reading data
  BLDEBUG_PRINT("WriteBytes: data: ");
  while(len--) {
    halResetWatchdog();
    halSpiWrite(*data);
    BLDEBUG(serPutHex(*data));
    BLDEBUG(serPutChar(' '));
    data++;
  }
  BLDEBUG_PRINT("\r\n");

  EEPROM_CS_INACTIVE();

  //Wait until EEPROM finishes write cycle
  while(halM45PE20ReadStatus() & MICRON_SR_WIP_MASK);

  EEPROM_WP_ON();

  return EEPROM_SUCCESS;
}

void halEepromTest(void)
{
  // No test function for this driver.
}

//
// ~~~~~~~~~~~~~~~~~~~~~~~~~ Standard EEPROM Interface ~~~~~~~~~~~~~~~~~~~~~~~~~
//

// halEepromRead
// address: the address in EEPROM to start reading
// data: write the data here
// len: number of bytes to read
//
// return: result of halM45PE20ReadBytes() call(s) or EEPROM_ERR_ADDR
//
// NOTE: We don't need to worry about handling unaligned blocks in this
// function since the M45PE20 can read continuously through its memory.
//
int8u halEepromRead(int32u address, int8u *data, int16u totalLength)
{
  if( address > DEVICE_SIZE || (address + totalLength) > DEVICE_SIZE)
    return EEPROM_ERR_ADDR;

  return halM45PE20ReadBytes( address, data, totalLength);
}

// halEepromWrite
// address: the address in EEPROM to start writing
// data: pointer to the data to write
// len: number of bytes to write
//
// return: result of halM45PE20WriteBytes() call(s) or EEPROM_ERR_ADDR
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
    if( (status=halM45PE20WriteBytes(address, data, len)) != EEPROM_SUCCESS) {
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
