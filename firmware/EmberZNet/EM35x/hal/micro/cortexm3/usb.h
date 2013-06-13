/** @file hal/micro/cortexm3/usb.h
 * @brief USB header providing some notes and helpful defines to make
 * the code more readable.
 *
 * This USB driver will only work with the Silicon Labs Virtual COM Port protocol.
 * The EM358X works as a Virtual COM Port, analogous to the CP210x VCP device. 
 * Refer to AN571, "CP210X VIRTUAL COM PORT INTERFACE". for more details.
 *
 * <!-- Copyright 2012 by Ember Corporation. All rights reserved.        *80*-->
 */

/*
---- USB buffer offsets ----
A buffers  EP0 IN USB_BUFBASEA + 0x0000
           EP0 OUT USB_BUFBASEA + 0x0008
           EP1 IN USB_BUFBASEA + 0x0010
           EP1 OUT USB_BUFBASEA + 0x0018
           EP2 IN USB_BUFBASEA + 0x0020
           EP2 OUT USB_BUFBASEA + 0x0028
           EP3 IN USB_BUFBASEA + 0x0030
           EP3 OUT USB_BUFBASEA + 0x0070
           EP4 IN USB_BUFBASEA + 0x00b0
           EP4 OUT USB_BUFBASEA + 0x00d0
           EP5 IN USB_BUFBASEA + 0x00f0
           EP5 OUT USB_BUFBASEA + 0x0130
           EP6 IN USB_BUFBASEA + 0x0170
           EP6 OUT USB_BUFBASEA + 0x0370
B buffers  EP0 IN USB_BUFBASEB + 0x0000
           EP0 OUT USB_BUFBASEB + 0x0008
           EP1 IN USB_BUFBASEB + 0x0010
           EP1 OUT USB_BUFBASEB + 0x0008
           EP2 IN USB_BUFBASEB + 0x0020
           EP2 OUT USB_BUFBASEB + 0x0028
           EP3 IN USB_BUFBASEB + 0x0030
           EP3 OUT USB_BUFBASEB + 0x0070
           EP4 IN USB_BUFBASEB + 0x00b0
           EP4 OUT USB_BUFBASEB + 0x00d0
           EP5 IN USB_BUFBASEB + 0x00f0
           EP5 OUT USB_BUFBASEB + 0x0130
           EP6 IN USB_BUFBASEB + 0x0170
           EP6 OUT USB_BUFBASEB + 0x0370
--------
*/


#define USB_EMBER_VID (0x1BA4)
//Ember's USB Link uses product ID 0x0001.
#define USB_VCP_PID (0x0002)


//If the USB is self powered or bus powered the configuration descriptor
//needs to report to the host the powered state.  Additionally, the USB core
//in the chip needs to be directly told.  Set the define USB_SELFPWRD_STATE to
//0 if the device is bus powered, 1 if self powered.
#ifndef USB_SELFPWRD_STATE
  #define USB_SELFPWRD_STATE (1)
#endif

//If the USB has remote wakeup enabled the configuration descriptor
//needs to report this fact to the host.  Additionally, the USB core
//in the chip needs to be directly told.  Set the define USB_REMOTEWKUPEN_STATE
//to 0 if remote wake is disabled or 1 if enabled.
#define USB_REMOTEWKUPEN_STATE (0)

//The bMaxPower parameter in the Configuration Descriptor.  This value is in
//units of 2mA.  Reporting 300mA as max power is a good default starting point
//since it should cover systems, such as with FEMs.  A system in other hardware
//configurations, such as no FEM, could possibly report lower.
#define USB_MAX_POWER (150)


//Define the size of all the endpoints.  Note that all endpoints are
//bidirectional so IN and OUT are the same size.
#define EP0_SIZE (8)   //CTRL endpoint
#define EP1_SIZE (8)   //BULK\INTERRUPT bidirectional
#define EP2_SIZE (8)   //BULK\INTERRUPT bidirectional
#define EP3_SIZE (64)  //BULK\INTERRUPT bidirectional
#define EP4_SIZE (32)  //BULK\INTERRUPT bidirectional
#define EP5_SIZE (64)  //BULK\INTERRUPT bidirectional
#define EP6_SIZE (512) //ISOCHRONOUS bidirectional endpoint
//The USB core in the chip does not distinguish between INTERRUPT and BULK.
//The BULK/INTERRUPT endpoints can be morphed between each other's type by
//simply updating the endpoint descriptor.


//All physical endpoints are packed together and accessed through
//a contiguous block of RAM.  Therefore, define the overall size of the
//buffer by adding all the endpoints together.  The multiply by 2 is
//because each endpoint is bidirection with IN and OUT.
#define USB_BUFFER_SIZE ( (EP0_SIZE*2) + \
                          (EP1_SIZE*2) + \
                          (EP2_SIZE*2) + \
                          (EP3_SIZE*2) + \
                          (EP4_SIZE*2) + \
                          (EP5_SIZE*2) + \
                          (EP6_SIZE*2) )


#define DESCRIPTOR_TYPE_DEVICE            (0x01) //bDescriptorType
#define DESCRIPTOR_TYPE_CONFIGURATION     (0x02) //bDescriptorType
#define DESCRIPTOR_TYPE_STRING            (0x03) //bDescriptorType
#define DESCRIPTOR_TYPE_INTERFACE         (0x04) //bDescriptorType
#define DESCRIPTOR_TYPE_ENDPOINT          (0x05) //bDescriptorType

#define REQUEST_TYPE_DEV_TO_HOST_STANDARD      (0x80) //bmRequestType
#define REQUEST_TYPE_DEV_TO_HOST_VENDOR_DEVICE (0xC0) //bmRequestType
#define REQUEST_TYPE_DEV_TO_HOST_VENDOR_INTF   (0xC1) //bmRequestType
#define REQUEST_TYPE_HOST_TO_DEV_VENDOR_INTF   (0x41) //bmRequestType

#define REQUEST_GET_DESCRIPTOR  (0x06) //bRequest
#define REQUEST_SET_IFC_ENABLE  (0x00) //bRequest
#define REQUEST_GET_PROPS       (0x0F) //bRequest
#define REQUEST_SET_LINE_CTL    (0x03) //bRequest
#define REQUEST_SET_FLOW        (0x13) //bRequest
#define REQUEST_SET_CHARS       (0x19) //bRequest
#define REQUEST_SET_BAUDRATE    (0x1E) //bRequest
#define REQUEST_GET_MDMSTS      (0x08) //bRequest
#define REQUEST_GET_COMM_STATUS (0x10) //bRequest
#define REQUEST_SET_MHS         (0x07) //bRequest
#define REQUEST_SET_BREAK       (0x05) //bRequest
#define REQUEST_PURGE           (0x12) //bRequest
#define REQUEST_VENDOR_SPECIFIC (0xFF) //bRequest


//I didn't use sizeof() for lengths since it's very helpful to have explicitely
//defined lengths when debugging using a MQP GraphicUSB analyzer.
#define LENGTH_DEVICE_DESCRIPTOR          (18) //bLength
#define LENGTH_STRING_LANGUAGE_DESCRIPTOR (4)  //bLength
#define LENGTH_MFG_STRING_DESCRIPTOR      (42) //bLength
#define LENGTH_PRODUCT_STRING_DESCRIPTOR  (46) //bLength
//Make room for the EUI64 as a unicode string descriptor to be populated
//by usbConfigEnumerate().  This is the only descriptor that cannot be const.
#define LENGTH_SER_NUM_STRING_DESCRIPTOR  (2+((8*2)*2))
#define LENGTH_CONFIGURATION_DESCRIPTOR   (9)  //bLength
#define LENGTH_INTERFACE_DESCRIPTOR       (9)  //bLength
#define LENGTH_ENDPOINT_DESCRIPTOR        (7)  //bLength
#define LENGTH_PROPS                      (59) //bLength
#define LENGTH_COMM_STATUS                (20) //bLength


//Total combined size of descriptors.  This is the wTotalLength parameter
//used when responding to get configuration descriptor.
#define W_TOTAL_LENGTH (0            \
  +LENGTH_CONFIGURATION_DESCRIPTOR   \
  +LENGTH_INTERFACE_DESCRIPTOR       \
  +LENGTH_ENDPOINT_DESCRIPTOR        \
  +LENGTH_ENDPOINT_DESCRIPTOR        \
  )


typedef struct {
  int8u  bLength;
  int8u  bDescriptorType;
  int16u bcdUSB;
  int8u  bDeviceClass;
  int8u  bDeviceSubClass;
  int8u  bDeviceProtocol;
  int8u  bMaxPacketSize;
  int16u idVendor;
  int16u idProduct;
  int16u bcdDevice;
  int8u  iManufacturer;
  int8u  iProduct;
  int8u  iSerialNumber;
  int8u  bNumConfigurations;
} DeviceDescriptorType;

typedef struct {
  int8u bLength;
  int8u bDescriptorType;
  int16u wLANGID[1];
} LanguageStringType;

typedef struct {
  int16u wLength;
  int16u bcdVersion;
  int32u ulServiceMask;
  int32u reserved1;
  int32u ulMaxTxQueue;
  int32u ulMaxRxQueue;
  int32u ulMaBaud;
  int32u ulProvSubType;
  int32u ulProvCapabilities;
  int32u ulSettableParams;
  int32u ulSettableBaud;
  int16u wSettableData;
  int32u ulCurrentTxQueue;
  int32u ulCurrentRxQueue;
  int32u reserved2;
  int32u reserved3;
  int16u uniProvName[15];
} PropsStructType;

typedef struct {
  int8u length;
  int8u descriptor;
  int16u string[(LENGTH_SER_NUM_STRING_DESCRIPTOR-1-1)/2];
} SerNumStringType;

typedef struct {
  int8u length;
  int8u descriptor;
  //The string is UTF-16.  Adjust LENGTH_MFG_STRING_DESCRIPTOR to not
  //include length or descriptor bytes and allocate 16bits per character.
  int16u string[(LENGTH_MFG_STRING_DESCRIPTOR-1-1)/2];
} MfgStringType;

typedef struct {
  int8u length;
  int8u descriptor;
  //The string is UTF-16.  Adjust LENGTH_PRODUCT_STRING_DESCRIPTOR to not
  //include length or descriptor bytes and allocate 16bits per character.
  int16u string[(LENGTH_PRODUCT_STRING_DESCRIPTOR-1-1)/2];
} ProdStringType;


//Helper macro to make the ISR more readable.  Turns the code into a simple
//if(RECEIVED(<array>, <endpoint>){<do something>}
#define RECEIVED(array, endpoint) \
  MEMCOMPARE(array, endpoint, sizeof(array))==0


//Define all of the endpoints as byte arrays.
typedef struct {
  int8u ep0IN[EP0_SIZE];
  int8u ep0OUT[EP0_SIZE];
  
  int8u ep1IN[EP1_SIZE];
  int8u ep1OUT[EP1_SIZE];
  
  int8u ep2IN[EP2_SIZE];
  int8u ep2OUT[EP2_SIZE];
  
  int8u ep3IN[EP3_SIZE];
  int8u ep3OUT[EP3_SIZE];
  
  int8u ep4IN[EP4_SIZE];
  int8u ep4OUT[EP4_SIZE];
  
  int8u ep5IN[EP5_SIZE];
  int8u ep5OUT[EP5_SIZE];
  
  int8u ep6IN[EP6_SIZE];
  int8u ep6OUT[EP6_SIZE];
} EndPointStruct;

//Overlay the endpoint structure on top of a flat byte array of the entire
//memory block that USB will be accessing.  This way an enpoint can be
//accessed specifically by name or the entire block can be accessed.
typedef union {
  int8u allEps[USB_BUFFER_SIZE];
  EndPointStruct eps;
} EndPointUnion;

extern EndPointUnion usbBufferA;
extern EndPointUnion usbBufferB;


//halInternalUart3RxIsr is called from within halUsbIsr() and is defined
//in uart.c along side the other Uart ISRs.
void halInternalUart3RxIsr(int8u *rxData, int8u length);


//The USB driver operates on serial port 3's Q for data TX/RX.
void usbTxData(void);


//When performing guaranteed printing (such as emberSerialGuaranteedPrintf)
//the USB driver accepts the data directly.  The key difference between
//usbForceTxData and usbTxData is that usbForceTxData does not use interrupts.
//This function will block until done sending all the data.
void usbForceTxData(int8u *data, int8u length);


//configure the GPIO
//assign the buffers
//zero out the buffers
//enable the interfaces and endpoints
//enable the interrupts
//using the GPIO, connect to begin enumeration
void usbConfigEnumerate(void);


//The USB peforming getCommStatus indicates that the chip's USB has
//enumerated and the host has activated COM port functionality.
boolean comPortActive(void);

//halUsbIsr() will handle a USB suspend interrupt indicating the host
//wants this device to suspend.  Performing an actual suspend should not
//be in interrupt context; especially since the chip needs to powerdown
//systems like it does with deep sleep.  The suspend and resume interrupts
//are used to simply set a flag that usbSuspendDsr() will use to know what
//the USB bus is requesting.
void usbSuspendDsr(void);

//Configure the GPIO used for VBUS Monitoring in self powered builds.
void vbusMonCfg(void);


//This is a hook for the higher (application) layers to enable suspend/resume
//functionality.  By default suspended is not enabled.  Not allowing it will
//not break USB but will not allow USB to pass certification.
extern boolean usbAllowedToSuspend;

