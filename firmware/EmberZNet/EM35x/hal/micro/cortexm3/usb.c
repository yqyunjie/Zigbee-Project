/** @file hal/micro/cortexm3/usb.c
 * @brief USB driver to support a SiLabs Virtual COM Port.
 *
 * This USB driver will only work with the Silicon Labs Virtual COM Port protocol.
 * The EM358X works as a Virtual COM Port, analogous to the CP210x VCP device. 
 * Refer to AN571, "CP210X VIRTUAL COM PORT INTERFACE". for more details.
 *
 * NOTE- the USB cannot be used when oschf is off
 *
 * <!-- Copyright 2012 by Ember Corporation. All rights reserved.        *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "hal/micro/cortexm3/usb.h"

//[[ TODO: This following #if, a shameful expedient, is only here so AppBuilder
//         can indiscriminately include this source file in the project files
//         for all 35x variants.  Once this shortcoming is addressed, the #if
//         should be removed.  See bug 14947.
//]]
#if defined (CORTEXM3_EM35X_USB)

//Define the A buffer.
#pragma data_alignment=8 //USB's buffering needs 8 byte alignment
EndPointUnion usbBufferA = {0,};

#ifdef USB_DOUBLE_BUFFER
  //Define the B buffer.
  #pragma data_alignment=8 //USB's buffering needs 8 byte alignment
  EndPointUnion usbBufferB = {0,};
#endif

//Receiving getCommStatus from the host indicates the device has enumerated
//and the COM port has been opened.  This is used as a means for the
//serial driver to know when it can begin using serial port 3.
boolean receivedGetCommStatus=FALSE;


const int8u getDeviceDescriptor[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x00, 0x01, //wValue - Device Index = 0
  0x00, 0x00, //wIndex - Zero
  //I ignore wLength
  //0x??, 0x?? //wLength - Length Requested
};
//Descriptor sent back to the host 
const DeviceDescriptorType deviceDescriptor[] = {
  LENGTH_DEVICE_DESCRIPTOR,
  DESCRIPTOR_TYPE_DEVICE,
  0x0200,        //bcdUsb - spec number: 2.0
  0x00,          //bDeviceClass - this device is of no particular device class
  0x00,          //bDeviceSubClass - there is no particular subclass
  0x00,          //bDeviceProtocol - no device-specific protocol
  8,             //bMaxPacketSize - Ember's fixed this as 8 bytes for EP0
  USB_EMBER_VID, //idVendor - Issued by usb.org; Ember's VID
  USB_VCP_PID,   //idProduct - Issued by Ember; PID for Virtual COM Port
  0x0100,        //bcdDevice - release number;
  1,             //iManufacturer - mfg string descriptor
  2,             //iProduct - product string descriptor
  3,             //iSerialNumber - serial number string descriptor
  1,             //bNumConfigurations - only 1 Configuration (Descriptor)
};               


const int8u getLanguageStringDescriptor[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x00, 0x03, //wValue - Index = 0, String Language
};
//Descriptor sent back to the host
const LanguageStringType languageStringDescriptor[] = {
  LENGTH_STRING_LANGUAGE_DESCRIPTOR,
  DESCRIPTOR_TYPE_STRING,
  {0x0409},
};


const int8u getMfgStringDescriptor[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x01, 0x03, //wValue - Index = 1, MFG String
  0x09, 0x04, //wIndex - English (United States)
};
const MfgStringType mfgStringDescriptor[] = {
  LENGTH_MFG_STRING_DESCRIPTOR,
  DESCRIPTOR_TYPE_STRING,
  'S','i','l','i','c','o','n',' ',
  'L','a','b','o','r','a','t','o','r','i','e','s',
};


const int8u getProductStringDescriptor[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x02, 0x03, //wValue - Index = 2, Product String
  0x09, 0x04, //wIndex - English (United States)
};
const ProdStringType productStringDescriptor[] = {
  LENGTH_PRODUCT_STRING_DESCRIPTOR,
  DESCRIPTOR_TYPE_STRING,
  'E','M','3','5','8',' ',
  'V','i','r','t','u','a','l',' ',
  'C','O','M',' ',
  'P','o','r','t',
};


const int8u getSerNumStringDescriptor[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x03, 0x03, //wValue - Index = 3, Serial Number String
  0x09, 0x04, //wIndex - English (United States)
};
//Descriptor sent back to the host
//The EUI64 of the chip is used as the serial number string descriptor.
//This is the only descriptor that cannot be const.  It is populated
//by usbConfigEnumerate.
SerNumStringType serNumStringDescriptor[] = {
  LENGTH_SER_NUM_STRING_DESCRIPTOR,
  DESCRIPTOR_TYPE_STRING,
  {0,},
};


//Get descriptor request sent from the host.  "Combined Descriptor" doesn't
//actually exist.  It's just the name I've given to all the non-Device
//descriptors so they can be stored together in the same array.
const int8u getCombinedDescriptors[] = {
  REQUEST_TYPE_DEV_TO_HOST_STANDARD,
  REQUEST_GET_DESCRIPTOR,
  0x00, 0x02, //wValue - CONFIGURATION Index = 0
  0x00, 0x00, //wIndex - Zero
  //--, --    //wLength - Will be processed to send back the requested amount.
};
//combinedDescriptors[] combines together all the non-Device descriptors      
const int8u combinedDescriptors[] = {

//>> Configuration Descriptor
  LENGTH_CONFIGURATION_DESCRIPTOR,
  DESCRIPTOR_TYPE_CONFIGURATION,
  W_TOTAL_LENGTH, //wTotalLength
  0,
  0x01, //bNumInterfaces - SiLabs protocol only uses 1 interface
  0x01, //bConfigurationValue - Ember only has 1 configuration
  0x00, //iConfiguration - Zero = no string descriptor
  //bmAttributes
    ((1 << 7)  //D7: Reserved (set to one)
    |(USB_SELFPWRD_STATE << 6)  //D6: self-powered? (0=bus powered)
    |(USB_REMOTEWKUPEN_STATE << 5)  //D5: remote wakeup? (1=supported)
    |(0 << 4)  //D4: Reserved (reset to zero)
    |(0 << 3)  //D3: Reserved (reset to zero)
    |(0 << 2)  //D2: Reserved (reset to zero)
    |(0 << 1)  //D1: Reserved (reset to zero)
    |(0 << 0)),//D0: Reserved (reset to zero)
  USB_MAX_POWER, //bMaxPower

//>> Default Interface Descriptor
  LENGTH_INTERFACE_DESCRIPTOR,
  DESCRIPTOR_TYPE_INTERFACE,
  0x00, //bInterfaceNumber
  0x00, //bAlternateSetting
  2,    //bNumEndpoints - Number of physical EPs enabled excluding EP0
  0xFF, //bInterfaceClass - Indicates that this is a vendor-specific interface.
  0x00, //bInterfaceSubClass - No particular subclass.
  0x00, //bInterfaceProtocol - The interface uses no specific protocol.
  0x00, //iInterface - Zero = no string descriptor.


//>> Data Endpoint Descriptor (EP3 - bulk - IN)
  LENGTH_ENDPOINT_DESCRIPTOR,
  DESCRIPTOR_TYPE_ENDPOINT,
    ((1<<7)|  //bit 7, direction, 1=IN
     (3<<0)), //bit 3:0, endpoint number ; EP3 as bulk
  2, //transfer type, bulk
  EP3_SIZE, 0, //wMaxPacketSize - 64 bytes
  0, //bInterval - ignored for bulk

  
//>> Data Endpoint Descriptor (EP3 - bulk - OUT)
  LENGTH_ENDPOINT_DESCRIPTOR,
  DESCRIPTOR_TYPE_ENDPOINT,
    ((0<<7)|  //bit 7, direction, 0=OUT
     (3<<0)), //bit 3:0, endpoint number ; EP3 as bulk
  2, //transfer type, bulk
  EP3_SIZE, 0, //wMaxPacketSize - 64 bytes
  0, //bInterval - ignored for bulk

};


//BEGIN definition of SiLabs Protocol specific transactions (control commands).

const int8u getVendorSpecific[] = {
  REQUEST_TYPE_DEV_TO_HOST_VENDOR_DEVICE,
  REQUEST_VENDOR_SPECIFIC,
};


const int8u getProps[] = {
  REQUEST_TYPE_DEV_TO_HOST_VENDOR_INTF,
  REQUEST_GET_PROPS,
  0x0000, //wValue
  0x0000, //wIndex
  //wLength is variable. don't bother looking for it
};
//Refer to AN572, "CP210X VIRTUAL COM PORT INTERFACE" for
//Table 7. Communication Properties Response.
const int8u props[] = {
  LENGTH_PROPS, 0x00,     //wLength - LENGTH_PROPS
  0x00, 0x01,             //bcdVersion - 0x0100
  0x01, 0x00, 0x00, 0x00, //ulServiceMask - 0x00000001
  0x00, 0x00, 0x00, 0x00, //reserved1
  0x40, 0x00, 0x00, 0x00, //ulMaxTxQueue - EP3 size - 64
  0x40, 0x00, 0x00, 0x00, //ulMaxRxQueue - EP3 size - 64
  0x00, 0xC2, 0x01, 0x00, //ulMaBaud - 115200
  0x01, 0x00, 0x00, 0x00, //ulProvSubType - RS-232
  0x00, 0x00, 0x00, 0x00, //ulProvCapabilities - no flow control
  0x00, 0x00, 0x00, 0x00, //ulSettableParams - can't set any parameters
  0x00, 0x00, 0x00, 0x00, //ulSettableBaud - can't be set (n/a for EM358X)
  0x0F, 0x00,             //wSettableData - 8 data bits
  0x40, 0x00, 0x00, 0x00, //ulCurrentTxQueue - EP3 size - 64
  0x40, 0x00, 0x00, 0x00, //ulCurrentRxQueue - EP3 size - 64
  0x00, 0x00, 0x00, 0x00, //reserved2
  0x00, 0x00, 0x00, 0x00, //reserved3
  '\0'                    //uniProvName - NULL, no name
};


const int8u setLineCtl[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_LINE_CTL,
  //wValue ???
  //wIndex ???
  //wLength = 0
};

const int8u setFlow[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_FLOW,
  //wValue ???
  //wIndex ???
  //wLength = 0x0010
};

const int8u setChars[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_CHARS,
  //wValue ???
  //wIndex ???
  //wLength = 0x0006
};

const int8u setBaudrate[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_BAUDRATE,
  //wValue ???
  //wIndex ???
  //wLength = 4
};

const int8u setIfcEnable[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_IFC_ENABLE,
  //wValue ???
  //wIndex ???
  //wLength = 0
};

const int8u getMdmsts[] = {
  REQUEST_TYPE_DEV_TO_HOST_VENDOR_INTF,
  REQUEST_GET_MDMSTS,
  //wValue ???
  //wIndex ???
  //wLength ???
};
const int8u mdmdsts[] = {
  0,
};

const int8u getCommStatus[] = {
  REQUEST_TYPE_DEV_TO_HOST_VENDOR_DEVICE,
  REQUEST_GET_COMM_STATUS,
  //wValue ???
  //wIndex ???
  //wLength ???
};
const int8u commStatus[] = {
  LENGTH_COMM_STATUS,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
};

const int8u setMhs[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_MHS,
  //wValue ???
  //wIndex ???
  //wLength = 0
};

const int8u setBreak[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_SET_BREAK,
  //wValue ???
  //wIndex ???
  //wLength = 0
};

const int8u purge[] = {
  REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF,
  REQUEST_PURGE,
  //wValue ???
  //wIndex ???
  //wLength = 0
};

//END definition of SiLabs Protocol specific transactions (control commands).



int8u *ep0TxPtr;
int8u ep0TxSize;
//This function assumes ep0TxPtr and ep0TxSize have been set for EP0 TX.
//This function will only ever be called in interrupt context so
//it should not be necessary to use ATOMIC.
boolean finalEp0TxSent = TRUE; //at startup we need to claim TX is done
void driveEp0Tx(void)
{
  int8u txSize = (ep0TxSize > EP0_SIZE) ? EP0_SIZE : ep0TxSize;
  ep0TxSize -= txSize;
  MEMCOPY(usbBufferA.eps.ep0IN, ep0TxPtr, txSize);
  ep0TxPtr += txSize;
  
  if((txSize == 0) && finalEp0TxSent) {
    return;
  }
  if(txSize < EP0_SIZE) {
    finalEp0TxSent = TRUE;
  } else {
    finalEp0TxSent = FALSE;
  }
  
  USB_TXBUFSIZEEP0A = txSize;
  USB_TXLOAD = USB_TXLOADEP0A;
}


//If driveEp3Tx() is called outside of interrupt/atomic context it
//will most likely generate corrupted out.
boolean finalEp3TxSent = TRUE;

int8u dequeueTxIntoBuffer(int8u * ep3INPtr)
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[3];
  int8u txSize=0;
  
  //If there are bytes in the Q and txSize hasn't maxed out, pull more
  //bytes off the Q into the DMA buffer
  while((q->used > 0) && (txSize < EP3_SIZE)) {
    *ep3INPtr = FIFO_DEQUEUE(q, emSerialTxQueueWraps[3]);
    ep3INPtr++;
    txSize++;
  }
  
  return txSize;
}

void driveEp3Tx(void)
{
  //It is not necessary to service the SOF interrupt.  It is necessary to
  //know when a SOF has occurred in the confines of the while loop waiting
  //for TX to unload.  A SOF occurring twice while waiting for TX to unload
  //indicates the host has stopped sending IN transactions to obtain the
  //loaded TX data.  Therefore, return out of trying to TX the data so
  //the system doesn't get stuck in the while loop.  Leaving the buffer
  //loaded with data will allow the host to resume consuming the data.
  boolean sofOccurred = FALSE;
  INT_USBFLAG = INT_USBSOF;

  //Wait for both DMA buffers to be unloaded before modifying DMA's RAM.
  while(USB_TXLOAD & (USB_TXLOADEP3A|USB_TXLOADEP3B)) {
    if(INT_USBFLAG & INT_USBSOF) {
      INT_USBFLAG = INT_USBSOF;
      if(sofOccurred) {
        sofOccurred = FALSE;
        return;
      }
      sofOccurred = TRUE;
    }
  }
  
  //handle buffer A
  {
    int8u txSize = dequeueTxIntoBuffer(usbBufferA.eps.ep3IN);
    
    if((txSize == 0) && finalEp3TxSent) {
      return;
    }
    if(txSize < EP3_SIZE) {
      finalEp3TxSent = TRUE;
    } else {
      finalEp3TxSent = FALSE;
    }
  
    USB_TXBUFSIZEEP3A = txSize;
    USB_TXLOAD = USB_TXLOADEP3A;
  }
  
  #ifdef USB_DOUBLE_BUFFER
    //handle buffer B
    //Bug 14584 "USB double buffering selects the wrong buffer size" describes
    //what happens when trying to load buffer B while A is loaded.  Ideally
    //B would be loaded while A is loaded so that the transmits would be as
    //quick as possible.  Unfortunately, the bug necessitates waiting for
    //A and B to be unloaded before trying to use B.  Therefore, the use
    //of buffer B (double buffering) in this incarnation of EP3/COM port is
    //actually slightly slower than single buffer. (About 10-15% slower as
    //measured with pin toggles around the full platformtest help menu
    //over USB COM port.)  More than just being slower, the while()
    //loop waiting to unload is longer than our 200us ISR handler allowance.
    //
    //POSSIBLE IMPROVEMENTS:
    //(1) The use of EP3 as a COM port can be re-architected such that
    //the data to be transmitted is placed into buffer B while waiting for A
    //to unload.
    //(2) If the size of B is the same as the size of A it is not necessary
    //to wait for A to unload.
    while(USB_TXLOAD & (USB_TXLOADEP3A|USB_TXLOADEP3B)) {}
    {
      int8u txSize = dequeueTxIntoBuffer(usbBufferB.eps.ep3IN);
      
      if((txSize == 0) && finalEp3TxSent) {
        return;
      }
      if(txSize < EP3_SIZE) {
        finalEp3TxSent = TRUE;
      } else {
        finalEp3TxSent = FALSE;
      }
    
      USB_TXBUFSIZEEP3B = txSize;
      USB_TXLOAD |= USB_TXLOADEP3B;
    }
  #endif
}

//The macros SUSPEND_TIMING_PRINT_ENTRY and SUSPEND_TIMING_PRINT_EXIT print
//the MAC Timer and System Timer around USB suspend to show the system
//clock is divided by 4.  The System Timer is the only clock not affected
//by USB suspend mode so it should track with real time.  The MAC Timer comes
//from the main clock and will be slowed by a factor or 4.  System Timer and
//MAC Timer*4 are not guranteed to match, but they should be close to
//each other.
//NOTE:  The MAC Timer is a microsecond tick that wraps every second.
#define SUSPEND_TIMING_PRINT_ENTRY()                               \
  int32u systemTimeEnteringSuspend = 0;                            \
  int32u systemTimeExitingSuspend = 0;                             \
  int32u macTimerEnteringSuspend = 0;                              \
  int32u macTimerExitingSuspend = 0;                               \
  MAC_TIMER_CTRL |= MAC_TIMER_CTRL_MAC_TIMER_EN;                   \
  systemTimeEnteringSuspend = halCommonGetInt32uMillisecondTick(); \
  macTimerEnteringSuspend = MAC_TIMER;

#define SUSPEND_TIMING_PRINT_EXIT()                                \
   macTimerExitingSuspend = MAC_TIMER;                             \
   systemTimeExitingSuspend = halCommonGetInt32uMillisecondTick(); \
   emberSerialPrintf(1, "MAC raw %d ms, *4 = %d ms\r\n",           \
     ((macTimerExitingSuspend-macTimerEnteringSuspend)/1000),      \
     (((macTimerExitingSuspend-macTimerEnteringSuspend)/1000)*4)); \
   emberSerialPrintf(1, "system raw %d ms\r\n",                    \
     systemTimeExitingSuspend-systemTimeEnteringSuspend);


//By default out of reset USB is not allowed to suspend.  Not suspending
//will not break USB functionality, but is a violation of the USB
//specification.  Applications that formally support USB have to specifically
//be capable of shutting down and restarting peripherals around usbSuspendDsr().
boolean usbAllowedToSuspend = FALSE;
boolean usbNeedsToSuspend = FALSE;
void usbSuspendDsr(void)
{
#if defined(SER232_3) && defined(USB_CERT_TESTING)
  usbAllowedToSuspend = TRUE;
#endif

  if(usbNeedsToSuspend && usbAllowedToSuspend) {
    
    //SUSPEND_TIMING_ENTRY()
    
    //USBTODO- The Stack and HAL needs to be powerdown and up when
    //using USB sleep since USB sleep divides the clocks by 4 which
    //affects all peripherals except the Sleep Timer/System Timer.
    
    //In my testing (on 2013-02-15), to achieve the lowst current I could
    //while suspended, all peripherals except USB needed to be disabled.
    //I did not investigate what would need to be done to properly shutdown
    //and restart them.  Disabling was the simplest technique I could invoke.
    //I've limited this disabling to USB certification testing for the time
    //being.  It should be revisited later.
    #ifdef USB_CERT_TESTING
    PERIPHERAL_DISABLE = (0 |
                          PERIDIS_RSVD7|
                          PERIDIS_RSVD6|
                          PERIDIS_RSVD |
                          PERIDIS_ADC  |
                          PERIDIS_TIM2 |
                          PERIDIS_TIM1 |
                          PERIDIS_SC1  |
                          PERIDIS_SC2  );
    #endif
    
    //Turn idle sleep into USB sleep which divides down all the chip clocks,
    //by 4, except system timer.
    CPU_CLKSEL |= USBSUSP_CLKSEL_FIELD;
    halSleep(SLEEPMODE_IDLE);
    
    //USBTODO- Track resume state to know when to resume versus trying to
    //return to suspend.
    
    //SUSPEND_TIMING_EXIT()
    
    USB_RESUME = 1;
    usbNeedsToSuspend = FALSE;
  }
}

#ifndef USB_ELECTRICAL_TEST

boolean expectingLineCoding = FALSE;
int16u wLength = 0;
int16u numEp0OutBytesToIgnore = 0;

void halUsbIsr(void)
{
  boolean rxValidEP0Handled = FALSE;
  int32u flag = INT_USBFLAG;
  INT_USBFLAG = flag;
  
  //Ensure EP0 IN is not stalled.  Refer to receiving getQualifierDescriptor
  //for more information.
  USB_STALLIN &= ~USB_STALLINEP0;

  if(flag & INT_USBRESUME) {
    //The INT_USBRESUME interrupt pulls us out of suspend while the USB
    //core auto clears the USBSUSP_CLKSEL bit (used when entering suspend).
    usbNeedsToSuspend = FALSE;
    
    //USBTODO-  Properly support remote wakeup.  The host can disable
    //  the remote wakeup feature.  This is not visible to software.
    //  When software tries to perform a remote wakeup it must use a
    //  timeout to determine if the request was denied (which means we
    //  would not receive INT_USBRESUME response).  We need to hold a
    //  state variable about the host denying a remote wakeup so we don't
    //  have to keep trying at other times.  The state variable would be
    //  cleared if the host resumes us for any reason.
    //
  }
  
  if(flag & INT_USBSUSPEND) {
    //If suspend if being supported, we now have 7ms to get down to
    //suspend current.  Setting usbNeedsToSuspend=TRUE will allow
    //the usbSuspendDsr() function to place us in the appropriate low
    //power clocking and idle mode.
    //
    //NOTE:  The stack and HAL should be shutdown like deep sleeping before
    //       suspend is entered.  USB sleep involves slowing down peripheral
    //       clocking, but it does not stop the clocks.  This means
    //       peripherals will keep running if they're enabled.  Being at
    //       a slower clock speed can severely disrupt application behavior
    //       that depends on clean peripherial behavior at a desired clock
    //       rate.  For example, problems with the UART would be very obvious.
    
    usbNeedsToSuspend = TRUE;
    
    //USBTODO- Software needs to remember when INT_USBSUSPEND fires so that
    //  it can return to suspend if a non-USB event pulls us out of
    //  the USB sleep mode.  Only INT_USBRESUME is allowed to keep us
    //  out of suspend.
  }
  
  if(flag & INT_USBRESET) {
    //When a USB reset occurs it resets the core but not the DMA.  To ensure
    //transactions after a reset are fresh all buffers (DMA) need to be
    //cleared.
    USB_BUFCLR = (USB_BUFCLRINEP6 |
                  USB_BUFCLRINEP5 |
                  USB_BUFCLRINEP4 |
                  USB_BUFCLRINEP3 |
                  USB_BUFCLRINEP2 |
                  USB_BUFCLRINEP1 |
                  USB_BUFCLRINEP0 );
  }

  //==== TX functionality ====
  //NOTE: INT_USBTXACTIVEEPx interrupts fire on USB_TXACTIVEEPxy falling edge.

  if(flag & INT_USBTXACTIVEEP0) {
    driveEp0Tx();
  }

  if(flag & INT_USBTXACTIVEEP3) {
    driveEp3Tx();
  }

  //==== RX functionality ====
  //Process ep0OUT to learn how much the host wants back (wLength).
  #define EP0OUT_WLENGTH() HIGH_LOW_TO_INT(usbBufferA.eps.ep0OUT[7],  \
                                           usbBufferA.eps.ep0OUT[6])
  
  if(flag & INT_USBRXVALIDEP0) {
    //Blindly extract wLength from everything that comes in on EP0.
    //While wLength isn't actually present on every transaction that comes in,
    //save it early so other pieces in the RX processing can use it if they
    //want/need to.
    //NOTE: wLength should only be consumed in very tightly controlled
    //  situations where wLength is known to be valid.
    wLength = EP0OUT_WLENGTH();
    
    //If the device is waiting for bytes to accept but not take action
    //on, simply decrement that number by the amount of bytes that have
    //shown up on EP0.  Indicate that the RX has been handled so the end
    //of this ISR wont STALL the transaction thinking there was an unknown
    //transaction.
    if(numEp0OutBytesToIgnore) {
      numEp0OutBytesToIgnore -= USB_RXBUFSIZEEP0A;
      rxValidEP0Handled = TRUE;
    }

    //Clear the EP0 TX buffer when receiving the 0 byte status phase (which
    //indicates a transaction is done from the host's perspective so the
    //host will not accept anymore IN data from a prior transaction).
    if(USB_RXBUFSIZEEP0A == 0) {
      rxValidEP0Handled = TRUE;
      USB_BUFCLR = USB_BUFCLRINEP0;
    }
    
    if(RECEIVED(getDeviceDescriptor, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getDeviceDescriptor\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)deviceDescriptor;
      ep0TxSize = deviceDescriptor[0].bLength;
      driveEp0Tx();
    }
    
    if(RECEIVED(getLanguageStringDescriptor, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getLanguageStringDescriptor\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)languageStringDescriptor;
      ep0TxSize = languageStringDescriptor[0].bLength;
      driveEp0Tx();
    }
    
    if(RECEIVED(getMfgStringDescriptor, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getMfgStringDescriptor\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)mfgStringDescriptor;
      ep0TxSize = wLength;
      driveEp0Tx();
    }
    
    if(RECEIVED(getProductStringDescriptor, usbBufferA.eps.ep0OUT)){
      //give the host as much length as it wants, as long as it doesn't
      //exceed the total length of productStringDescriptor.
      wLength = (wLength > LENGTH_PRODUCT_STRING_DESCRIPTOR) ?
                  LENGTH_PRODUCT_STRING_DESCRIPTOR : wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getProductStringDescriptor\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)productStringDescriptor;
      ep0TxSize = wLength;
      driveEp0Tx();
    }
    
    if(RECEIVED(getSerNumStringDescriptor, usbBufferA.eps.ep0OUT)){
      //give the host as much length as it wants, as long as it doesn't
      //exceed the total length of serNumStringDescriptor.
      wLength = (wLength > LENGTH_SER_NUM_STRING_DESCRIPTOR) ?
                  LENGTH_SER_NUM_STRING_DESCRIPTOR : wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getSerNumStringDescriptor\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)serNumStringDescriptor;
      ep0TxSize = wLength;
      driveEp0Tx();
    }
    
    if(RECEIVED(getCombinedDescriptors, usbBufferA.eps.ep0OUT)){
      //The combinedDescriptors array has a max size of W_TOTAL_LENGTH.
      wLength = (wLength > W_TOTAL_LENGTH) ? W_TOTAL_LENGTH : wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getCombinedDescriptors\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)combinedDescriptors;
      ep0TxSize = wLength;
      driveEp0Tx();
    }
    
    
    //BEGIN handling reception of SiLabs Protocol specific transactions
    //(control commands).
    //The transactions are defined here in the order they will be received
    //during enumeration and opening of a COM port.
    //  NOTE: VENDOR_SPECIFIC is specifically STALLed
    //  getProps -- necessary for device to enumerate
    //  setLineCtl -- necessary to open COM port
    //  setFlow -- necessary to open COM port
    //  setChars -- necessary to open COM port
    //  setBaudrate -- necessary to open COM port
    //  setIfcEnable -- necessary to open COM port
    //  getMdmsts -- necessary to open COM port
    //  getCommStatus -- necessary to open COM port
    //  NOTE: getMdmsts is the transaction used every ~100ms
    //        to maintain a COM port connection.
    
    if(RECEIVED(getVendorSpecific, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      USB_STALLIN |= USB_STALLINEP0;
    }
    
    if(RECEIVED(getProps, usbBufferA.eps.ep0OUT)){
      wLength = (wLength > LENGTH_PROPS) ? LENGTH_PROPS : wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getProps\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)props;
      ep0TxSize = wLength;
      driveEp0Tx();
    }

    if(RECEIVED(setLineCtl, usbBufferA.eps.ep0OUT)){
      //emberSerialPrintf(1, "setLineCtl\r\n");
      rxValidEP0Handled = TRUE;
      //The EM358X device doesn't care about line control settings (stop bits,
      //parity settings, or word length), so the device driver simply
      //acknowledges receipt with a zero length packet.  (Stalling the
      //transaction would indicate an invalid setting.)
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }

    if(RECEIVED(setFlow, usbBufferA.eps.ep0OUT)){
      //Not supported.  Simply acknowledge receipt and remember how many
      //data bytes need to be ignored.
      //wLength should be 0x0010.  Use the received value just in case
      //it is different.
      numEp0OutBytesToIgnore = wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setFlow\r\n");
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }

    if(RECEIVED(setChars, usbBufferA.eps.ep0OUT)){
      //Not supported.  Simply acknowledge receipt and remember how many
      //data bytes need to be ignored.
      //wLength should be 0x0006.  Use the received value just in case
      //it is different.
      numEp0OutBytesToIgnore = wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setChars\r\n");
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }
    
    if(RECEIVED(setBaudrate, usbBufferA.eps.ep0OUT)){
      //Not supported.  Simply acknowledge receipt and remember how many
      //data bytes need to be ignored.
      //wLength should be 4.  Use the received value just in case
      //it is different.
      numEp0OutBytesToIgnore = wLength;
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setBaudrate\r\n");
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }

    if(RECEIVED(setIfcEnable, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setIfcEnable\r\n");
      //Not supported.  Simply acknowledge receipt.
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }

    if(RECEIVED(getMdmsts, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "getMdmsts\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)mdmdsts;
      ep0TxSize = 1;
      driveEp0Tx();
    }
    
    if(RECEIVED(getCommStatus, usbBufferA.eps.ep0OUT)){
      //give the host as much length as it wants, as long as it doesn't
      //exceed the total length of commStatus.
      wLength = (wLength > LENGTH_COMM_STATUS) ? LENGTH_COMM_STATUS : wLength;
      rxValidEP0Handled = TRUE;
      
      //The upper layers need to know when USB has received getCommStatus
      //indicating the host has opened a COM port.
      receivedGetCommStatus=TRUE;
      
      //emberSerialPrintf(1, "getCommStatus\r\n");
      //kickoff first transaction.  let INT_USBTXACTIVEEP0 drive the rest
      ep0TxPtr = (int8u *)commStatus;
      ep0TxSize = wLength;
      driveEp0Tx();
    }

    if(RECEIVED(setMhs, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setMhs\r\n");
      //Not supported.  Simply acknowledge receipt.
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }
    
    if(RECEIVED(setBreak, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "setBreak\r\n");
      //Not supported.  Simply acknowledge receipt.
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }
    
    if(RECEIVED(purge, usbBufferA.eps.ep0OUT)){
      rxValidEP0Handled = TRUE;
      //emberSerialPrintf(1, "purge\r\n");
      //Not supported.  Simply acknowledge receipt.
      USB_TXBUFSIZEEP0A = 0;
      USB_TXLOAD = USB_TXLOADEP0A;
    }
    
    //END handling reception of SiLabs Protocol specific transactions
    //(control commands).
    
    //If we haven't handled the received EP0 activity, stall EP0 indicating
    //to the host that the requested functionality is not supported.
    if(!rxValidEP0Handled) {
      USB_STALLIN |= USB_STALLINEP0;
    }
    
    //Just some debug code to print what ended up in the OUT buffers.
    #if PRINT_OUT_BUFFS_TO_PORT1
    {
      int8u i;
      for(i=0;i<8;i++) {
        emberSerialPrintf(1, "%X", usbBufferA.eps.ep0OUT[i]);
      }
      emberSerialPrintf(1, "\r\n");
    }
    #endif
    
    MEMSET(usbBufferA.eps.ep0OUT, 0, EP0_SIZE);
    //If we don't clear RXVALID it causes EP0 reception to stall.
    USB_RXVALID = USB_RXVALIDEP0A;
  }

  if(flag & INT_USBRXVALIDEP3) {
    #if (EMBER_SERIAL3_MODE != EMBER_SERIAL_UNUSED)
      #ifdef USB_DOUBLE_BUFFER
        //Refer to "handle buffer B" in the driveEp3Tx() function above
        //to learn why double buffering is not enabled by default.
        //RX with double buffering does not have the same problem as TX,
        //but it is cleaner to have TX and RX use the same buffering scheme.
        //
        //Since double buffering is enabled, this logic will simply process
        //A and B in sequence as expected.  Clearing both valid bits after
        //processing B will enforce incoming data ends up in the proper
        //"next" buffer (A) so there are no race conditions.
        static boolean rxAIsNext = TRUE;
        if(rxAIsNext) {
          halInternalUart3RxIsr(usbBufferA.eps.ep3OUT, USB_RXBUFSIZEEP3A);
          MEMSET(usbBufferA.eps.ep3OUT, 0, EP3_SIZE);
          rxAIsNext = FALSE;
        } else {
          halInternalUart3RxIsr(usbBufferB.eps.ep3OUT, USB_RXBUFSIZEEP3B);
          MEMSET(usbBufferB.eps.ep3OUT, 0, EP3_SIZE);
          //Now that A and B have both been processed, clear the valid bits so
          //reception can begin again with A.
          USB_RXVALID = (USB_RXVALIDEP3A | USB_RXVALIDEP3B);
          rxAIsNext = TRUE;
        }
      #else //single buffer
        halInternalUart3RxIsr(usbBufferA.eps.ep3OUT, USB_RXBUFSIZEEP3A);
        MEMSET(usbBufferA.eps.ep3OUT, 0, EP3_SIZE);
        USB_RXVALID = (USB_RXVALIDEP3A | USB_RXVALIDEP3B);
      #endif
    #endif
  }
}
#endif //USB_ELECTRICAL_TEST


void usbTxData(void)
{
  //Pretend we got the falling edge of INT_USBTXACTIVEEP3 to start TX.
  ATOMIC(
    //Because bytes are pulled from the serial Q and placed into USB's DMA
    //the code must be ATOMIC to safely know when the DMA has finished, move
    //the new bytes off the Q into the DMA, and then load the DMA.  If the
    //driveEp3Tx() function call is not ATOMIC the output will become very
    //corrupted due to modifying bytes in RAM that DMA is pulling from.
    //NOTE:  The only other location that calls driveEp3Tx() doesn't need
    //to be ATOMIC() since it is already in halUsbIsr().
    if(receivedGetCommStatus) {
      driveEp3Tx();
    } else {
      //If the COM port didn't open, this driver would get stuck waiting for
      //activity on USB_TXLOAD.  The safest action is to simply drop the
      //data.
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[3];
      ATOMIC_LITE(
        q->used = 0;
        q->head = 0;
        q->tail = 0;
      )
    }
  )
}


int8u *forceEp3TxPtr;
//This function assumes forceEp3TxPtr has been set for EP3 TX
void forceDriveEp3TX(int8u *length)
{
  int8u txSize = (*length > EP3_SIZE) ? EP3_SIZE : *length;
  
  //Wait for DMA to be unloaded before modifying DMA's RAM.
  while((USB_TXLOAD & USB_TXLOADEP3A) == USB_TXLOADEP3A) {}
  
  *length -= txSize;
  MEMCOPY(usbBufferA.eps.ep3IN, forceEp3TxPtr, txSize);
  forceEp3TxPtr += txSize;
  
  if((txSize == 0) && finalEp3TxSent) {
    return;
  }
  if(txSize < EP3_SIZE) {
    finalEp3TxSent = TRUE;
  } else {
    finalEp3TxSent = FALSE;
  }

  USB_TXBUFSIZEEP3A = txSize;
  USB_TXLOAD = USB_TXLOADEP3A;
  
  //Ensure DMA has unloaded before releasing control.
  while((USB_TXLOAD & USB_TXLOADEP3A) == USB_TXLOADEP3A) {}
}

void usbForceTxData(int8u *data, int8u length)
{
  //Force TX data is designed to work without interrupts.  Therefore
  //interrupts are specifically disabled to ensure an other USB activity
  //does not interfere.
  //driveEp3Tx uses the serial Q.  This function receives the data
  //directly.

  ATOMIC(
    //If the COM port didn't open, this driver would get stuck waiting for
    //activity on USB_TXLOAD.  The safest action is to simply drop the
    //data.
    if(receivedGetCommStatus) {
      forceEp3TxPtr = data;
      
      while(length) {
        forceDriveEp3TX(&length);
      }
    }
  )
}


#if USB_SELFPWRD_STATE==1 && defined(VBUSMON)

boolean usbReadyToEnumerate = FALSE;
boolean usbVbusPresent = FALSE;

void VBUSMON_ISR(void)
{
  //PA3 is used for sensing VBUS indicating if USB is physically connected.
  //An IRQ is used to monitor for any edge change on PA3.
  //For self-powered devices, The USB specification requires that the pull-up
  //resistor for enumerating, on PA2, is disconnected if VBUS is not connected.
  
  //clear int before read to avoid potential of missing interrupt
  INT_MISS = VBUSMON_MISS_BIT;     //clear missed interrupt flag
  INT_GPIOFLAG = VBUSMON_FLAG_BIT; //clear top level interrupt flag
  
  //Because this ISR/IRQ triggers on both edges, usbVbusPresent is used
  //to track the state of VBUS.  Simply toggle that state whenever this
  //ISR fires.
  //Falling edge: VBUS removed, must set PA2 to IN-low
  //Rrising edge: VBUS applied, if usbConfigEnumerate'ed, PA2 allowed to OUT-high
  usbVbusPresent = !usbVbusPresent;
  
  if(usbVbusPresent && usbReadyToEnumerate) {
    //Only attempt enumerate if usbConfigEnumerate has already been called
    //CFG PA2 as ouput-high to enumerate.
    SET_REG_FIELD(GPIO_PACFGL, PA2_CFG, GPIOCFG_OUT);
    GPIO_PASET = PA2;
  } else {
    //CFG PA2 as an input so the device de-enumurates and the pin does not put
    //any load on the bus.
    SET_REG_FIELD(GPIO_PACFGL, PA2_CFG, GPIOCFG_IN);
    //Still set PA2 low to be ready for the next time enumerating.
    GPIO_PACLR = PA2;
  }
}

//Use a selectable IRQ for monitoring VBUS on PA3.
void vbusMonCfg(void)
{
  //PA3 just needs to be a simple input.
  SET_REG_FIELD(GPIO_PACFGL, PA3_CFG, GPIOCFG_IN);

  //start from a fresh state, just in case
  VBUSMON_INTCFG = 0;              //disable triggering
  INT_CFGCLR = VBUSMON_INT_EN_BIT; //clear top level int enable
  INT_GPIOFLAG = VBUSMON_FLAG_BIT; //clear stale interrupt
  INT_MISS = VBUSMON_MISS_BIT;     //clear stale missed interrupt
  //configure
  VBUSMON_SEL();             //point IRQ at the desired pin
  VBUSMON_INTCFG  = (0 << GPIO_INTFILT_BIT); //no filter
  VBUSMON_INTCFG |= (3 << GPIO_INTMOD_BIT);  //3 = both edges
  
  usbVbusPresent = ((GPIO_PAIN & (1 << PA3_BIT)) != 0);
  
  INT_CFGSET = VBUSMON_INT_EN_BIT; //set top level interrupt enable
}

#endif


#define nibble2Ascii(n) ((n) + (((n) < 10) ? '0'  : 'A' - 10));

void usbConfigEnumerate(void)
{
  //Every USB device needs a distinct serial number.  Conveniently,
  //all Ember chips have a distinct EUI64.  serNumStringDescriptor is the
  //only non-const descriptor since it's the only descriptor that changes
  //chip to chip.
  int8u i = 0;
  int8u j = 0;
  tokTypeMfgEui64 tokEui64;
  halCommonGetMfgToken(&tokEui64, TOKEN_MFG_EUI_64);
  for(j = 0; j<8; j++) {
    serNumStringDescriptor[0].string[i++] = nibble2Ascii((tokEui64[j]>>4)&0xF);
    serNumStringDescriptor[0].string[i++] = nibble2Ascii((tokEui64[j]>>0)&0xF);
  }

  //Ensure no bytes are ignored until EP0 transactions have learned how
  //many should be ignored.
  numEp0OutBytesToIgnore = 0;

  //The upper layers need to know when USB has received getCommStatus
  //indicating the host has opened a COM port.
  receivedGetCommStatus = FALSE;

  //At startup we need to claim TX is done so the final, transaction
  //ending packet isn't unnecessarily sent (before it even makes sense).
  finalEp0TxSent = TRUE;
  finalEp3TxSent = TRUE;

  //Configure PA0 and PA1 in analog mode for USB
  SET_REG_FIELD(GPIO_PACFGL, PA0_CFG, GPIOCFG_ANALOG);
  SET_REG_FIELD(GPIO_PACFGL, PA1_CFG, GPIOCFG_ANALOG);
  //Set EMU_USE_IO_BD_TCH = 0 to use the old 0607 "Cap Touch" board, if needed.
  
  
  USB_BUFBASEA = (int32u)usbBufferA.allEps;
  MEMSET(usbBufferA.allEps, 0, USB_BUFFER_SIZE);
  #ifdef USB_DOUBLE_BUFFER
    USB_BUFBASEB = (int32u)usbBufferB.allEps;
    MEMSET(usbBufferB.allEps, 0, USB_BUFFER_SIZE);
  #endif
  
  
  //The SiLabs VCP protocol only uses one endpoint, beyond the
  //usual EP0, as bulk for all transfers.
  USB_ENABLEIN |= USB_ENABLEINEP3;
  USB_ENABLEOUT |= USB_ENABLEOUTEP3;
  
  #ifdef USB_DOUBLE_BUFFER
    //Refer to "handle buffer B" in the driveEp3Tx() function above
    //to learn why double buffering is not enabled by default.
    //    
    //Enable double buffering bulk EP3.
    USB_CTRL |= (USB_ENBUFOUTEP3B | USB_ENBUFINEP3B);
  #endif
  
  //Use the SELFPWDRD and REMOTEWKUPEN state defines to set the appropriate
  //bits in USB_CTRL
  USB_CTRL = (USB_CTRL & (~(USB_SELFPWRD_MASK | USB_REMOTEWKUPEN_MASK))) |
             ((USB_SELFPWRD_STATE<<USB_SELFPWRD_BIT) |
              (USB_REMOTEWKUPEN_STATE<<USB_REMOTEWKUPEN_BIT));
  
  INT_USBFLAG = 0xFFFFFFFF;
  
  //RESET, EP0, and EP3 are the only interrupts needed to
  //support a COM port.  The other interrupts are not necessary for a simple
  //COM port and can saturate the NVIC if enabled.
  INT_USBCFG = (INT_USBRESUME      |
                INT_USBSUSPEND     |
                INT_USBRESET       |
                INT_USBRXVALIDEP3  |
                INT_USBRXVALIDEP0  |
                INT_USBTXACTIVEEP3 |
                INT_USBTXACTIVEEP0 );
  
  INT_CFGSET = INT_USB;


#ifdef  EMBER_EMU_TEST
  //Select which GPIO output is used for connect/disconnect. Any
  //value > 47, selects disconnected state.
  EMU_USB_DISCONNECT = PORTA_PIN(2);
#endif
  
#if USB_SELFPWRD_STATE==1 && defined(VBUSMON)
  usbReadyToEnumerate = TRUE;

  //For self-powered devices, The USB specification requires that the pull-up
  //resistor is disconnected if VBUS is not connected.
  vbusMonCfg();
  if(usbVbusPresent) {
    //Use PA2 for enumeration control.  Drive it high to connect.
    SET_REG_FIELD(GPIO_PACFGL, PA2_CFG, GPIOCFG_OUT);
    GPIO_PASET = PA2;
  }
#else
  //Use PA2 for enumeration control.  Drive it high to connect.
  SET_REG_FIELD(GPIO_PACFGL, PA2_CFG, GPIOCFG_OUT);
  GPIO_PASET = PA2;
#endif
}

boolean comPortActive(void)
{
  return receivedGetCommStatus;
}

#endif // CORTEXM3_EM35X_USB
