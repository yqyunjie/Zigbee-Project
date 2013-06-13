/*
 * File: micro.c
 * Description: EM3XX micro specific full HAL functions
 *
 *
 * Copyright 2008, 2009 by Ember Corporation. All rights reserved.          *80*
 */
//[[ Author(s): Brooks Barrett, Lee Taylor ]]


#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "include/error.h"

#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "hal/micro/cortexm3/diagnostic.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/flash.h"


// halInit is called on first initial boot, not on wakeup from sleep.
void halInit(void)
{
  //[[ Strip emulator only code from official build
  #ifdef EMBER_EMU_TEST
    //On the emulator, give our fake XTAL reasonable thresholds so the cal
    //algorithm ends up at 4.
    EMU_OSC24M_CTRL =((0x8<<EMU_OSC24M_CTRL_OSC24M_THRESH_H_BIT) |
                      (0x2<<EMU_OSC24M_CTRL_OSC24M_THRESH_L_BIT) |
                      (0x0<<EMU_OSC24M_CTRL_OSC24M_THRESH_STOP_BIT));
  #endif
  //]]
  
  halCommonStartXtal();
  
  halInternalSetRegTrim(FALSE);
  
  GPIO_DBGCFG |= GPIO_DBGCFGRSVD;
  
  #ifndef DISABLE_WATCHDOG
    halInternalEnableWatchDog();
  #endif
  
  halCommonCalibratePads();
  
  halInternalInitBoard();
  
  halCommonSwitchToXtal();
  
  #ifndef DISABLE_RC_CALIBRATION
    halInternalCalibrateFastRc();
  #endif//DISABLE_RC_CALIBRATION
  
  halInternalStartSystemTimer();
  
  #ifdef INTERRUPT_DEBUGGING
    //When debugging interrupts/ATOMIC, ensure that our
    //debug pin is properly enabled and idle low.
    I_OUT(I_PORT, I_PIN, I_CFG_HL);
    I_CLR(I_PORT, I_PIN);
  #endif //INTERRUPT_DEBUGGING
  
  #ifdef USB_CERT_TESTING
  halInternalPowerDownBoard();
  #endif
}


void halReboot(void)
{
  halInternalSysReset(RESET_SOFTWARE_REBOOT);
}

void halPowerDown(void)
{
  halInternalPowerDownUart();
  
  halInternalPowerDownBoard();
}

// halPowerUp is called from sleep state, not from first initial boot.
void halPowerUp(void)
{
  //[[ Strip emulator only code from official build
  #ifdef EMBER_EMU_TEST
    //On the emulator, give our fake XTAL reasonable thresholds so the cal
    //algorithm ends up at 4.
    EMU_OSC24M_CTRL =((0x8<<EMU_OSC24M_CTRL_OSC24M_THRESH_H_BIT) |
                      (0x2<<EMU_OSC24M_CTRL_OSC24M_THRESH_L_BIT) |
                      (0x0<<EMU_OSC24M_CTRL_OSC24M_THRESH_STOP_BIT));
  #endif
  //]]

  halInternalPowerUpKickXtal();

  halCommonCalibratePads();

  halInternalPowerUpBoard();

  halInternalBlockUntilXtal();

  halInternalPowerUpUart();
}


//If the board file defines runtime powerup/powerdown GPIO configuration,
//instantiate the variables and implement halStackRadioPowerDownBoard/
//halStackRadioPowerUpBoard which the stack will use to control the
//power state of radio specific GPIO.  If the board file does not define
//runtime GPIO configuration, the compile time configuration will work as
//it always has.
#if defined(DEFINE_POWERUP_GPIO_CFG_VARIABLES)           && \
    defined(DEFINE_POWERUP_GPIO_OUTPUT_DATA_VARIABLES)   && \
    defined(DEFINE_POWERDOWN_GPIO_CFG_VARIABLES)         && \
    defined(DEFINE_POWERDOWN_GPIO_OUTPUT_DATA_VARIABLES) && \
    defined(DEFINE_GPIO_RADIO_POWER_BOARD_MASK_VARIABLE)


DEFINE_POWERUP_GPIO_CFG_VARIABLES();
DEFINE_POWERUP_GPIO_OUTPUT_DATA_VARIABLES();
DEFINE_POWERDOWN_GPIO_CFG_VARIABLES();
DEFINE_POWERDOWN_GPIO_OUTPUT_DATA_VARIABLES();
DEFINE_GPIO_RADIO_POWER_BOARD_MASK_VARIABLE();
       

static void rmwRadioPowerCfgReg(int16u radioPowerCfg[],
                                int32u volatile * cfgReg,
                                int8u cfgVar)
{
  int32u temp = *cfgReg;
  int8u i;
  
  //don't waste time with a register that doesn't have anything to be done
  if(gpioRadioPowerBoardMask&(0xF<<(4*cfgVar))) {
    //loop over the 4 pins of the cfgReg
    for(i=0; i<4; i++) {
      if((gpioRadioPowerBoardMask>>((4*cfgVar)+i))&1) {
        //read-modify-write the pin's cfg if the mask says it pertains
        //to the radio's power state
        temp &= ~(0xF<<(4*i));
        temp |= (radioPowerCfg[cfgVar]&(0xF<<(4*i)));
      }
    }
  }
  
  *cfgReg = temp;
}


static void rmwRadioPowerOutReg(int8u radioPowerOut[],
                                int32u volatile * outReg,
                                int8u outVar)
{
  int32u temp = *outReg;
  int8u i;
  
  //don't waste time with a register that doesn't have anything to be done
  if(gpioRadioPowerBoardMask&(0xFF<<(8*outVar))) {
    //loop over the 8 pins of the outReg
    for(i=0; i<8; i++) {
      if((gpioRadioPowerBoardMask>>((8*outVar)+i))&1) {
        //read-modify-write the pin's out if the mask says it pertains
        //to the radio's power state
        temp &= ~(0x1<<(1*i));
        temp |= (radioPowerOut[outVar]&(0x1<<(1*i)));
      }
    }
  }
  
  *outReg = temp;
}


void halStackRadioPowerDownBoard(void)
{
  if(gpioRadioPowerBoardMask == 0) {
    //If the mask indicates there are no special GPIOs for the
    //radio that need their power state to be conrolled by the stack,
    //don't bother attempting to do anything.
    return;
  }
  
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PAOUT, 0);
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PBOUT, 1);
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PCOUT, 2);
  
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PACFGL, 0);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PACFGH, 1);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PBCFGL, 2);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PBCFGH, 3);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PCCFGL, 4);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PCCFGH, 5);
}


void halStackRadioPowerUpBoard(void)
{
  if(gpioRadioPowerBoardMask == 0) {
    //If the mask indicates there are no special GPIOs for the
    //radio that need their power state to be conrolled by the stack,
    //don't bother attempting to do anything.
    return;
  }
  
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PAOUT, 0);
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PBOUT, 1);
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PCOUT, 2);
  
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PACFGL, 0);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PACFGH, 1);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PBCFGL, 2);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PBCFGH, 3);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PCCFGL, 4);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PCCFGH, 5);
}

#else

//If the board file uses traditional compile time powerup/powerdown GPIO
//configuration, stub halStackRadioPowerDownBoard/halStackRadioPowerUpBoard
//which the stack would have used to control the power state of radio
//relevant GPIO.  With compile time configuration, only the traditional
//halInternalPowerDownBoard and halInternalPowerUpBoard funtions configure
//the GPIO.

void halStackRadioPowerDownBoard(void) {}
void halStackRadioPowerUpBoard(void) {}

#endif

void halStackProcessBootCount(void)
{
  //Note: Becuase this always counts up at every boot (called from emberInit),
  //and non-volatile storage has a finite number of write cycles, this will
  //eventually stop working.  Disable this token call if non-volatile write
  //cycles need to be used sparingly.
  halCommonIncrementCounterToken(TOKEN_STACK_BOOT_COUNTER);
}


PGM_P halGetResetString(void)
{
  // Table used to convert from reset types to reset strings.
  #define RESET_BASE_DEF(basename, value, string)  string,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  /*nothing*/
  static PGM char resetStringTable[][4] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  return resetStringTable[halGetResetInfo()];
}

// Note that this API should be used in conjunction with halGetResetString
//  to get the full information, as this API does not provide a string for
//  the base reset type
PGM_P halGetExtendedResetString(void)
{
  // Create a table of reset strings for each extended reset type
  typedef PGM char ResetStringTableType[][4];
  #define RESET_BASE_DEF(basename, value, string)   \
                         }; static ResetStringTableType basename##ResetStringTable = {
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  string,
  {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF
  
  // Create a table of pointers to each of the above tables
  #define RESET_BASE_DEF(basename, value, string)  (ResetStringTableType *)basename##ResetStringTable,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  /*nothing*/
  static ResetStringTableType * PGM extendedResetStringTablePtrs[] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  int16u extResetInfo = halGetExtendedResetInfo();
  // access the particular table of extended strings we are interested in
  ResetStringTableType *extendedResetStringTable = 
                    extendedResetStringTablePtrs[RESET_BASE_TYPE(extResetInfo)];

  // return the string from within the proper table
  return (*extendedResetStringTable)[((extResetInfo)&0xFF)];
  
}

// Translate EM3xx reset codes to the codes previously used by the EM2xx.
// If there is no corresponding code, return the EM3xx base code with bit 7 set.
int8u halGetEm2xxResetInfo(void)
{
  int8u reset = halGetResetInfo();

  // Any reset with an extended value field of zero is considered an unknown
  // reset, except for FIB resets.
  if ( (RESET_EXTENDED_FIELD(halGetExtendedResetInfo()) == 0) && 
       (reset != RESET_FIB) ) {
     return EM2XX_RESET_UNKNOWN;
  }

 switch (reset) {
  case RESET_UNKNOWN:
    return EM2XX_RESET_UNKNOWN;
  case RESET_BOOTLOADER:
    return EM2XX_RESET_BOOTLOADER;
  case RESET_EXTERNAL:      // map pin resets to poweron for EM2xx compatibility
//    return EM2XX_RESET_EXTERNAL;  
  case RESET_POWERON:
    return EM2XX_RESET_POWERON;
  case RESET_WATCHDOG:
    return EM2XX_RESET_WATCHDOG;
  case RESET_SOFTWARE:
    return EM2XX_RESET_SOFTWARE;
  case RESET_CRASH:
    return EM2XX_RESET_ASSERT;
  default:
    return (reset | 0x80);      // set B7 for all other reset codes
  }
}

static boolean rhoActive;

boolean halRadioHoldOffIsActive(void)
{
  return rhoActive;
}

#ifdef  RHO_GPIO // BOARD_HEADER supports Radio HoldOff
int8u WAKE_ON_LED_RHO_VAR = WAKE_ON_LED_RHO;

extern void emRadioHoldOffIsr(boolean active);

static boolean rhoEnabled;

boolean halGetRadioHoldOff(void)
{
  return rhoEnabled;
}

// Return active state of Radio HoldOff GPIO pin
boolean halInternalRhoPinIsActive(void)
{
  return (!!(RHO_IN & BIT(RHO_GPIO&7)) == !!RHO_ASSERTED);
}

void RHO_ISR(void)
{
  boolean rhoStateNow;

  //clear int before read to avoid potential of missing interrupt
  INT_MISS = RHO_MISS_BIT;     //clear missed RHO interrupt flag
  INT_GPIOFLAG = RHO_FLAG_BIT; //clear top level RHO interrupt flag

  if (rhoEnabled) { //Ignore if not enabled
    rhoStateNow = halInternalRhoPinIsActive();
    if (rhoActive != rhoStateNow) {
      //state changed
      rhoActive = rhoStateNow;
      emRadioHoldOffIsr(rhoActive); //Notify Radio land of state change
    } else {
      //state unchanged -- probably a glitch or too quick to matter
    }
  }
}

void halSetRadioHoldOff(boolean enabled)
{
  rhoEnabled = enabled;

  //start from a fresh state just in case
  RHO_INTCFG = 0;              //disable RHO triggering
  INT_CFGCLR = RHO_INT_EN_BIT; //clear RHO top level int enable
  INT_GPIOFLAG = RHO_FLAG_BIT; //clear stale RHO interrupt
  INT_MISS = RHO_MISS_BIT;     //clear stale missed RHO interrupt

  // Reconfigure GPIOs for desired state
  ADJUST_GPIO_CONFIG_LED_RHO(enabled);

  if (enabled) {
    RHO_SEL();                   //point IRQ at the desired pin
    RHO_INTCFG  = (0 << GPIO_INTFILT_BIT)    //0 = no filter
                  | (3 << GPIO_INTMOD_BIT);  //3 = both edges
    rhoActive = halInternalRhoPinIsActive();//grab state before enable int
  } else {
    // No need to change the IRQ selector
    RHO_INTCFG  = (0 << GPIO_INTFILT_BIT)    //0 = no filter
                  | (0 << GPIO_INTMOD_BIT);  //0 = disabled
    rhoActive = FALSE; // Force state off when disabling
  }

  emRadioHoldOffIsr(rhoActive); //Notify Radio land of configured state

  if (enabled) {
    INT_CFGSET = RHO_INT_EN_BIT; //set top level interrupt enable
    // Interrupt on now, ISR will maintain rhoActive
  } else {
    // Interrupt already disabled above, leave it off
  }
}
#endif//RHO_GPIO // Board header supports Radio HoldOff
