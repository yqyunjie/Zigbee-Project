/** @file hal/micro/cortexm3/diagnostic.h
 * See @ref diagnostics for detailed documentation.
 *
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.-->
 */

/** @addtogroup diagnostics
 * @brief Crash and watchdog diagnostic functions.
 *
 * See diagnostic.h for source code.
 *@{
 */
 
#ifndef __EM3XX_DIAGNOSTIC_H__
#define __EM3XX_DIAGNOSTIC_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Define the reset reasons that should print out detailed crash data.
#define RESET_CRASH_REASON_MASK ( (1 << RESET_UNKNOWN) |    \
                                  (1 << RESET_WATCHDOG) |   \
                                  (1 << RESET_CRASH) |      \
                                  (1 << RESET_FLASH) |      \
                                  (1 << RESET_FAULT) |      \
                                  (1 << RESET_FATAL) )        

typedef struct
{
  // first two fields must be the same as HalCrashInfoType
  int16u resetReason;       // reason written out just before forcing a reset
  int16u resetSignature; 
  int16u panId;         // PanId that the bootloader will use
  int8u  radioChannel;  // emberGetRadioChannel() - 11
  int8u  radioPower;    // emberGetRadioPower()
  int8u  radioLnaCal;   // Low Noise Amplifier calibration
  int8u  reserved[22]; // (reserved for future use)
} HalBootParamType;

// note that assertInfo and dmaProt are written just before a forced reboot
typedef union
{
  struct { PGM_P file; int line; } assertInfo;
  struct { int32u channel; int32u address; } dmaProt;
} HalCrashSpecificDataType;

// Define crash registers as structs so a debugger can display their bit fields 
typedef union {
  struct
  {
    int32u EXCPT          : 9;  // B0-8
    int32u ICIIT_LOW      : 7;  // B9-15
    int32u                : 8;  // B16-23
    int32u T              : 1;  // B24
    int32u ICIIT_HIGH     : 2;  // B25-26
    int32u Q              : 1;  // B27
    int32u V              : 1;  // B28
    int32u C              : 1;  // B29
    int32u Z              : 1;  // B30
    int32u N              : 1;  // B31
  } bits;
  int32u word;
} HalCrashxPsrType;

typedef union {
  struct
  {
    int32u VECTACTIVE     : 9;  // B0-8
    int32u                : 2;  // B9-10
    int32u RETTOBASE      : 1;  // B11
    int32u VECTPENDING    : 9;  // B12-20
    int32u                : 1;  // B21
    int32u ISRPENDING     : 1;  // B22
    int32u ISRPREEMPT     : 1;  // B23
    int32u                : 1;  // B24
    int32u PENDSTCLR      : 1;  // B25
    int32u PENDSTSET      : 1;  // B26
    int32u PENDSVCLR      : 1;  // B27
    int32u PENDSVSET      : 1;  // B28
    int32u                : 2;  // B29-30
    int32u NMIPENDSET     : 1;  // B31
  } bits;
  int32u word;
} HalCrashIcsrType;

typedef union {
  struct
  {
    int32u Timer1         : 1;  // B0
    int32u Timer2         : 1;  // B1
    int32u Management     : 1;  // B2
    int32u Baseband       : 1;  // B3
    int32u Sleep_Timer    : 1;  // B4
    int32u SC1            : 1;  // B5
    int32u SC2            : 1;  // B6
    int32u Security       : 1;  // B7
    int32u MAC_Timer      : 1;  // B8
    int32u MAC_TX         : 1;  // B9
    int32u MAC_RX         : 1;  // B10
    int32u ADC            : 1;  // B11
    int32u IRQ_A          : 1;  // B12
    int32u IRQ_B          : 1;  // B13
    int32u IRQ_C          : 1;  // B14
    int32u IRQ_D          : 1;  // B15
    int32u Debug          : 1;  // B16
    int32u                : 15; // B17-31
  } bits;
  int32u word;
} HalCrashIntActiveType;

typedef union {
  struct
  {
    int32u MEMFAULTACT    : 1;  // B0   
    int32u BUSFAULTACT    : 1;  // B1 
    int32u                : 1;  // B2 
    int32u USGFAULTACT    : 1;  // B3 
    int32u                : 3;  // B4-6 
    int32u SVCALLACT      : 1;  // B7 
    int32u MONITORACT     : 1;  // B8 
    int32u                : 1;  // B9 
    int32u PENDSVACT      : 1;  // B10
    int32u SYSTICKACT     : 1;  // B11
    int32u USGFAULTPENDED : 1;  // B12
    int32u MEMFAULTPENDED : 1;  // B13
    int32u BUSFAULTPENDED : 1;  // B14
    int32u SVCALLPENDED   : 1;  // B15
    int32u MEMFAULTENA    : 1;  // B16
    int32u BUSFAULTENA    : 1;  // B17
    int32u USGFAULTENA    : 1;  // B18
    int32u                : 13; // B19-31  
  } bits;
  int32u word;
} HalCrashShcsrType;

typedef union {
  struct
  {
    int32u IACCVIOL       : 1;  // B0 
    int32u DACCVIOL       : 1;  // B1
    int32u                : 1;  // B2
    int32u MUNSTKERR      : 1;  // B3
    int32u MSTKERR        : 1;  // B4
    int32u                : 2;  // B5-6
    int32u MMARVALID      : 1;  // B7
    int32u IBUSERR        : 1;  // B8
    int32u PRECISERR      : 1;  // B9
    int32u IMPRECISERR    : 1;  // B10
    int32u UNSTKERR       : 1;  // B11
    int32u STKERR         : 1;  // B12
    int32u                : 2;  // B13-14
    int32u BFARVALID      : 1;  // B15
    int32u UNDEFINSTR     : 1;  // B16
    int32u INVSTATE       : 1;  // B17
    int32u INVPC          : 1;  // B18
    int32u NOCP           : 1;  // B19
    int32u                : 4;  // B20-23
    int32u UNALIGNED      : 1;  // B24
    int32u DIVBYZERO      : 1;  // B25
    int32u                : 6;  // B26-31
  } bits;
  int32u word;
} HalCrashCfsrType;

typedef union {
  struct
  {
    int32u                : 1;  // B0
    int32u VECTTBL        : 1;  // B1
    int32u                : 28; // B2-29
    int32u FORCED         : 1;  // B30
    int32u DEBUGEVT       : 1;  // B31
  } bits;
  int32u word;
} HalCrashHfsrType;

typedef union {
  struct
  {
    int32u HALTED         : 1;  // B0
    int32u BKPT           : 1;  // B1
    int32u DWTTRAP        : 1;  // B2
    int32u VCATCH         : 1;  // B3
    int32u EXTERNAL       : 1;  // B4
    int32u                : 27; // B5-31         
  } bits;
  int32u word;
} HalCrashDfsrType;

typedef union {
  struct
  {
    int32u MISSED         : 1;  // B0
    int32u RESERVED       : 1;  // B1
    int32u PROTECTED      : 1;  // B2
    int32u WRONGSIZE      : 1;  // B3
    int32u                : 28; // B4-31
  } bits;
  int32u word;
} HalCrashAfsrType;

#define NUM_RETURNS     6

// Define the crash data structure
typedef struct
{
  // ***************************************************************************
  // The components within this first block are written by the assembly
  // language common fault handler, and position and order is critical.
  // cstartup-iar-boot-entry.s79 also relies on the position/order here.
  // Do not edit without also modifying that code.
  // ***************************************************************************
  int16u resetReason;    // reason written out just before forcing a reset
  int16u resetSignature; 
  int32u R0;            // processor registers
  int32u R1;
  int32u R2;
  int32u R3;
  int32u R4;
  int32u R5;
  int32u R6;
  int32u R7;
  int32u R8;
  int32u R9;
  int32u R10;
  int32u R11;
  int32u R12;
  int32u LR;
  int32u mainSP;        // main and process stack pointers
  int32u processSP;
  // ***************************************************************************
  // End of the block written by the common fault handler.
  // ***************************************************************************
  
  int32u PC;              // stacked return value (if it could be read)
  HalCrashxPsrType xPSR;  // stacked processor status reg (if it could be read)
  int32u mainSPUsed;      // bytes used in main stack
  int32u processSPUsed;   // bytes used in process stack
  int32u mainStackBottom; // address of the bottom of the stack
  HalCrashIcsrType icsr;  // interrupt control state register
  HalCrashShcsrType shcsr;// system handlers control and state register
  HalCrashIntActiveType intActive;  // irq active bit register
  HalCrashCfsrType cfsr;  // configurable fault status register
  HalCrashHfsrType hfsr;  // hard fault status register
  HalCrashDfsrType dfsr;  // debug fault status register
  int32u faultAddress;    // fault address register (MMAR or BFAR)
  HalCrashAfsrType afsr;  // auxiliary fault status register
  int32u returns[NUM_RETURNS];  // probable return addresses found on the stack       
  HalCrashSpecificDataType data;  // additional data specific to the crash type
} HalCrashInfoType;

typedef union 
{
  HalCrashInfoType crash;
  HalBootParamType boot;
} HalResetInfoType;

#define RESETINFO_WORDS  ((sizeof(HalResetInfoType)+3)/4)

extern HalResetInfoType halResetInfo;

#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief Macro evaluating to TRUE if the last reset was a crash, FALSE
 * otherwise.
 */
#define halResetWasCrash() \
                ( ( (1 << halGetResetInfo()) & RESET_CRASH_REASON_MASK) != 0)

/** @brief Returns the number of bytes used in the main stack.
 *
 * @return The number of bytes used in the main stack.
 */
int32u halGetMainStackBytesUsed(void);

/** @brief Print a summary of crash details.
 *
 * @param port  Serial port number (0 or 1). 
 */
void halPrintCrashSummary(int8u port);

/** @brief Print the complete, decoded crash details.
 *
 * @param port  Serial port number (0 or 1). 
 */
void halPrintCrashDetails(int8u port);

/** @brief Print the complete crash data.
 *
 * @param port  Serial port number (0 or 1). 
 */
void halPrintCrashData(int8u port);

#endif  //__EM3XX_DIAGNOSTIC_H__

/** @} END addtogroup */

