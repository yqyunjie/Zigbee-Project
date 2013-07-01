/** @file ash-host-ui.c
 *  @brief  ASH Host user interface functions
 *
 *  This includes command option parsing, trace and counter functions.
 * 
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.       *80*-->
 */

#include PLATFORM_HEADER
#include <string.h> 
#ifndef WIN32
  #include <unistd.h>
  #include <time.h>
#endif
#include <stdlib.h>
#include "stack/include/ember-types.h"
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "hal/micro/system-timer.h"
#include "app/util/ezsp/ezsp-enum-decode.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-priv.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-queues.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

#include "app/util/gateway/backchannel.h"

//------------------------------------------------------------------------------
// Preprocessor definitions

#define ERR_LEN               128   // max length error message

#define txControl (txBuffer[0])     // more descriptive aliases
#define rxControl (rxBuffer[0])

//------------------------------------------------------------------------------
// Local Variables

static const char usage[] = 
" {ncp type} {options}\n"
"  ncp type:\n"
"    -n 0,1,2          0=EM2xx @ 115200 bps, RTS/CTS\n"
"                      1=EM2xx @ 57600 bps, XON/XOFF\n"                    
"                      2=AVR/2420 @ 38400 bps, XON/XOFF\n"                    
"                      (if present must be the first option)\n"
"  options:\n"
"    -b <baud rate>    9600, 19200, 38400, 57600, 115200, etc.\n"
"    -f r,x            flow control: r=RST/CTS, x=XON/XOFF\n"
"    -h                display usage information\n"
"    -i 0,1            enable/disable input buffering\n"
"    -o 0,1            enable/disable output buffering\n"
"    -p <port>         serial port name or number (eg, COM1, ttyS0, or 1)\n"
"    -r d,r,c          ncp reset method: d=DTR, r=RST frame, c=custom\n"
"    -s 1,2            stop bits\n"
"    -t <trace flags>  trace B0=frames, B1=verbose frames, B2=events, B3=EZSP\n"
"    -v                enable virtual ISA support; CLI available via telnet\n"
"                      on port 4901\n"
"    -x 0,1            enable/disable data randomization\n";

static const AshCount zeroAshCount = {0};

//------------------------------------------------------------------------------
// Command line option parsing

void ashPrintUsage(char *name)
{
  char *shortName = strrchr(name, '/');
  shortName = shortName ? shortName + 1 : name;
  fprintf(stderr, "Usage: %s", shortName);
  fprintf(stderr, usage);
}

boolean ashProcessCommandOptions(int argc, char *argv[])
{

  int c;
  char port[ASH_PORT_LEN];
  char devport[ASH_PORT_LEN];
  char errStr[ERR_LEN] = "";
  int32u baud;
  int8u stops;
  int8u trace;
  int8u enable;
  int8u portnum;
  int8u cfg;
  int blksize;
  int optionCount = 0;

  while (TRUE) {
    c = getopt(argc, argv, "b:f:hvi:n:o:p:r:s:t:x:");
    if (c == -1) {
      if (optind != argc ) {
        snprintf(errStr, ERR_LEN, "Invalid option %s.\n", argv[optind]);
      } 
      break;
    }

    optionCount++;
    switch (c) {
    case 'b':
      if (sscanf(optarg, "%u", &baud) != 1) {
        snprintf(errStr, ERR_LEN, "Invalid baud rate %s.\n", optarg);
      } else {
        ashWriteConfig(baudRate, baud);
      }
      break;
    case 'f':
      switch (*optarg) {
      case 'r':
        ashWriteConfig(rtsCts, TRUE);
        break;    
      case 'x':
        ashWriteConfig(rtsCts, FALSE);
        break;    
      default:
        snprintf(errStr, ERR_LEN, "Invalid flow control choice %s.\n", optarg);
      }
      break;
    case 'h':
    case '?':
      snprintf(errStr, ERR_LEN, "\n");
      break; 
    case 'i':
      if ( (sscanf(optarg, "%hhu", &enable) != 1) || (enable > 1) ) {
        snprintf(errStr, ERR_LEN, "Invalid input buffer choice %s.\n", optarg);
      } else {
        blksize = enable ? 256 : 1;
        ashWriteConfig(inBlockLen, blksize);
      }
      break;
    case 'n':
      if (optionCount != 1) {
        snprintf(errStr, ERR_LEN, "Ncp option, if present, must be first.\n");
      } else if ( (sscanf(optarg, "%hhu", &cfg) != 1) || (cfg > 2) ) {
        snprintf(errStr, ERR_LEN, "Invalid ncp config choice %s.\n", optarg);
      } else {
        ashSelectHostConfig(cfg);
      }
      break;
    case 'o':
      if ( (sscanf(optarg, "%hhu", &enable) != 1) || (enable > 1) ) {
        snprintf(errStr, ERR_LEN, "Invalid output buffer choice %s.\n", optarg);
      } else {
        blksize = enable ? 256 : 1;
        ashWriteConfig(outBlockLen, blksize);
      }
      break;
    case 'p':
      sscanf(optarg, "%39s", port);
      if (strlen(port) >= ASH_PORT_LEN-1) {
        snprintf(errStr, ERR_LEN, "Serial port name %s too long.\n", port);
      } else {
        // Handle some common variations specifying a serial port
        strcpy(devport, "/dev/");
        if ( strncmp(devport, port, 5) == 0) {
          strcat(devport, port + 5);
#ifdef __CYGWIN__
        } else if ( ((strncmp("COM", port, 3) == 0) ||
                    (strncmp("com", port, 3) == 0) ) &&
                    (sscanf(port + 3, "%hhu", &portnum) == 1) && 
                    (portnum > 0) ) {
          sprintf(devport, "/dev/ttyS%hhu", portnum-1);
        } else if ( (sscanf(port, "%hhu", &portnum) == 1) && portnum ) {
          sprintf(devport, "/dev/ttyS%hhu", portnum-1);
#else
        } else if (sscanf(port, "%hhu", &portnum) == 1) {
          sprintf(devport, "/dev/ttyS%hhu", portnum);
#endif
        } else {
          strcat(devport, port);
        }
        strncpy(ashHostConfig.serialPort, devport, ASH_PORT_LEN);
      }
      break;
    case 'r':
      switch (*optarg) {
      case 'r':
        ashWriteConfig(resetMethod, ASH_RESET_METHOD_RST);
        break;    
      case 'd':
        ashWriteConfig(resetMethod, ASH_RESET_METHOD_DTR);
        break;    
      case 'c':
        ashWriteConfig(resetMethod, ASH_RESET_METHOD_CUSTOM);
        break;    
      default:
        snprintf(errStr, ERR_LEN, "Invalid reset method %s.\n", optarg);
      }
      break;
    case 's':
      if ( (sscanf(optarg, "%hhu", &stops) != 1) || (stops < 1) || (stops > 2) ) {
        snprintf(errStr, ERR_LEN, "Invalid number of stop bits %s.\n", optarg);
      } else {
        ashWriteConfig(stopBits, stops);
      }
      break;
    case 't':
      if (sscanf(optarg, "%hhu", &trace) != 1) {
        snprintf(errStr, ERR_LEN, "Invalid trace flag value %s.\n", optarg);
      } else {
        ashWriteConfig(traceFlags, trace);
      }
      break;
    case 'v':
      if (!backchannelSupported) {
        fprintf(stderr, "Error: Backchannel support not compiled into this application.\n");
        exit(1);
      }
      backchannelEnable = TRUE;
      break;
    case 'x':
      if ( (sscanf(optarg, "%hhu", &enable) != 1) || (enable > 1) ) {
        snprintf(errStr, ERR_LEN, "Invalid randomization choice %s.\n", optarg);
      } else {
        ashWriteConfig(randomize, enable);
      }
      break;
    default:
      assert(1);
      break;
    }   // end of switch (c)
  }
  if (*errStr != '\0') {
    fprintf(stderr, "%s", errStr);
    ashPrintUsage(argv[0]);
  }
  return (*errStr == '\0');
}


//------------------------------------------------------------------------------
// Counter functions

void ashPrintCounters(AshCount *counters, boolean clear)
{
  AshCount a = *counters;

  if (clear) {
    ashClearCounters(counters);
  }
  printf("Host Counts        Received Transmitted\n");
  printf("Total bytes      %10d  %10d\n",  a.rxBytes,       a.txBytes);
  printf("DATA bytes       %10d  %10d\n",  a.rxData,        a.txData);
  printf("I/O blocks       %10d  %10d\n\n",a.rxBlocks,      a.txBlocks);
  printf("Total frames     %10d  %10d\n",  a.rxAllFrames ,  a.txAllFrames);
  printf("DATA frames      %10d  %10d\n",  a.rxDataFrames,  a.txDataFrames);
  printf("ACK frames       %10d  %10d\n",  a.rxAckFrames,   a.txAckFrames);
  printf("NAK frames       %10d  %10d\n",  a.rxNakFrames,   a.txNakFrames);
  printf("Retry frames     %10d  %10d\n",  a.rxReDataFrames,a.txReDataFrames);
  printf("Cancelled        %10d  %10d\n",  a.rxCancelled,   a.txCancelled);
  printf("nRdy frames      %10d  %10d\n",  a.rxN1Frames,    a.txN1Frames);

  printf("\nHost Receive Errors\n");
  printf("CRC errors       %10d\n",  a.rxCrcErrors);
  printf("Comm errors      %10d\n",  a.rxCommErrors);
  printf("Length < minimum %10d\n",  a.rxTooShort);
  printf("Length > maximum %10d\n",  a.rxTooLong);
  printf("Bad controls     %10d\n",  a.rxBadControl);
  printf("Bad lengths      %10d\n",  a.rxBadLength);
  printf("Bad ACK numbers  %10d\n",  a.rxBadAckNumber);
  printf("Out of buffers   %10d\n",  a.rxNoBuffer);
  printf("Retry dupes      %10d\n",  a.rxDuplicates);
  printf("Out of sequence  %10d\n",  a.rxOutOfSequence);
  printf("ACK timeouts     %10d\n",  a.rxAckTimeouts);
}

void ashCountFrame(boolean sent)
{
  int8u control;

  if (sent) {
    control = readTxControl();
    BUMP_HOST_COUNTER(txAllFrames);
  } else {
    control = readRxControl();
    BUMP_HOST_COUNTER(rxAllFrames);
  }
  if ( (control & ASH_DFRAME_MASK) == ASH_CONTROL_DATA) {
    if (sent) {
      if (control & ASH_RFLAG_MASK) {
        BUMP_HOST_COUNTER(txReDataFrames);
      } else {
        BUMP_HOST_COUNTER(txDataFrames);
      }
    } else {
      if (control & ASH_RFLAG_MASK) {
        BUMP_HOST_COUNTER(rxReDataFrames);
      } else {
        BUMP_HOST_COUNTER(rxDataFrames);
      }
    }
  } else if ( (control & ASH_SHFRAME_MASK) == ASH_CONTROL_ACK) {
    if (sent) {
      BUMP_HOST_COUNTER(txAckFrames);
      if (control & ASH_NFLAG_MASK) {
        BUMP_HOST_COUNTER(txN1Frames);
      } else {
        BUMP_HOST_COUNTER(txN0Frames);
      }
    } else {
      BUMP_HOST_COUNTER(rxAckFrames);
      if (control & ASH_NFLAG_MASK) {
        BUMP_HOST_COUNTER(rxN1Frames);
      } else {
        BUMP_HOST_COUNTER(rxN0Frames);
      }
    }
  } else if ( (control & ASH_SHFRAME_MASK) == ASH_CONTROL_NAK) {
    if (sent) {
      BUMP_HOST_COUNTER(txNakFrames);
      if (control & ASH_NFLAG_MASK) {
        BUMP_HOST_COUNTER(txN1Frames);
      } else {
        BUMP_HOST_COUNTER(txN0Frames);
      }
    } else {
      BUMP_HOST_COUNTER(rxNakFrames);
      if (control & ASH_NFLAG_MASK) {
        BUMP_HOST_COUNTER(rxN1Frames);
      } else {
        BUMP_HOST_COUNTER(rxN0Frames);
      }
    }
  }
}

void ashClearCounters(AshCount *counters)
{
  *counters = zeroAshCount;
}

//------------------------------------------------------------------------------
// Error/status code to string conversion

const int8u* ashErrorString(int8u error)
{
  switch (error) {
  case RESET_UNKNOWN:
    return (int8u *) "unknown reset";
  case RESET_EXTERNAL:
    return (int8u *) "external reset";
  case RESET_POWERON:
    return (int8u *) "power on reset";
  case RESET_WATCHDOG:
    return (int8u *) "watchdog reset";
  case RESET_BROWNOUT:
    return (int8u *) "brownout reset";
  case RESET_JTAG:
    return (int8u *) "JTAG reset";
  case RESET_ASSERT:
    return (int8u *) "assert reset";
  case RESET_RSTACK:
    return (int8u *) "R stack reset";
  case RESET_CSTACK:
    return (int8u *) "C stack reset";
  case RESET_BOOTLOADER:
    return (int8u *) "bootloader reset";
  case RESET_PC_ROLLOVER:
    return (int8u *) "PC rollover reset";
  case RESET_SOFTWARE:
    return (int8u *) "software reset";
  case RESET_PROTFAULT:
    return (int8u *) "protection fault reset";
  case RESET_FLASH_VERIFY_FAIL:
    return (int8u *) "flash verify failed reset";
  case RESET_FLASH_WRITE_INHIBIT:
    return (int8u *) "flash write inhibited reset";
  case RESET_BOOTLOADER_IMG_BAD:
    return (int8u *) "bootloader bad EEPROM image reset";
  default:
    return (int8u *) decodeEzspStatus(error);
  }
} // end of ashErrorString()

const int8u* ashEzspErrorString(int8u error)
{
  return (int8u *) decodeEzspStatus(error);
} // end of ashEzspErrorString()

//------------------------------------------------------------------------------
// Trace output functions

static void ashPrintFrame(int8u c)
{
  if ((c & ASH_DFRAME_MASK) == ASH_CONTROL_DATA) {
    if (c & ASH_RFLAG_MASK) {
      ashDebugPrintf("DATA(%d,%d)", ASH_GET_FRMNUM(c), ASH_GET_ACKNUM(c));
    } else {
      ashDebugPrintf("data(%d,%d)", ASH_GET_FRMNUM(c), ASH_GET_ACKNUM(c));
    }
  } else if ((c & ASH_SHFRAME_MASK) == ASH_CONTROL_ACK) {
    ashDebugPrintf("ack(%d", ASH_GET_ACKNUM(c));
    if (ASH_GET_NFLAG(c)) {
      ashDebugPrintf(")-  ");
    } else {
      ashDebugPrintf(")+  ");
    }
  } else if ((c & ASH_SHFRAME_MASK) == ASH_CONTROL_NAK) {
    ashDebugPrintf("NAK(%d", ASH_GET_ACKNUM(c));
    if (ASH_GET_NFLAG(c)) {
      ashDebugPrintf(")-  ");
    } else {
      ashDebugPrintf(")+  ");
    }
  } else if (c == ASH_CONTROL_RST) {
    ashDebugPrintf("RST      ");
  } else if (c == ASH_CONTROL_RSTACK) {
    ashDebugPrintf("RSTACK   ");
  } else if (c == ASH_CONTROL_ERROR) {
    ashDebugPrintf("ERROR    ");
  } else {
    ashDebugPrintf("???? 0x%02X", c);
  } 
}

static void ashPrintStatus(void)
{
  ashDebugPrintf(" Ar=%d At=%d Ft=%d Fr=%d Frt=%d Ql=%d To=%d Tp=%d Bf=%d", 
    readAckRx(), readAckTx(), readFrmTx(), readFrmRx(), readFrmReTx(), 
    ashQueueLength(&reTxQueue), 
    readAshTimeouts(), ashGetAckPeriod(), ashFreeListLength(&rxFree) );
}

#define DATE_MAX 11 //10/10/2000 = 11 characters including NULL
#define TIME_MAX 13 //10:10:10.123 = 13 characters including NULL

static void ashPrintElapsedTime(void)
{
  static boolean first = TRUE;
  static int32u base;
  int32u now;

  struct tm tmToday;
  time_t currentTime;
  char dateString[DATE_MAX];
  char timeString[TIME_MAX];
  time(&currentTime);
#ifdef WIN32
  localtime_s(&tmToday, &currentTime);
#else
  localtime_r(&currentTime, &tmToday);
#endif
  strftime(dateString, DATE_MAX, "%m/%d/%Y", &tmToday);
  strftime(timeString, TIME_MAX, "%H:%M:%S", &tmToday);

  if (first) {
    base = halCommonGetInt32uMillisecondTick();
    first = FALSE;
  }
  now = halCommonGetInt32uMillisecondTick() - base;
  ashDebugPrintf("%s %s: %d.%03d ", dateString, timeString, now/1000, now%1000);
}

void ashTraceFrame(boolean sent)
{
  int8u flags;

  ashCountFrame(sent);
  flags = ashReadConfig(traceFlags);
  if (flags & (TRACE_FRAMES_BASIC | TRACE_FRAMES_VERBOSE)) {
    ashPrintElapsedTime();
    if (sent) {
      ashDebugPrintf("Tx ");
      ashPrintFrame(readTxControl());
      if (flags & TRACE_FRAMES_VERBOSE) {
        ashPrintStatus();
      }
      ashDebugPrintf("   \r\n");
    } else {
      ashDebugPrintf("   ");
      ashPrintFrame(readRxControl());
      if (flags & TRACE_FRAMES_VERBOSE) {
        ashPrintStatus();
      }
      ashDebugPrintf(" Rx\r\n");
    }
  }
}

void ashTraceEventRecdFrame(const char *string)
{
  if (ashReadConfig(traceFlags) & TRACE_EVENTS) {
    ashPrintElapsedTime();
    ashDebugPrintf("Rec'd frame: ");
    ashDebugPrintf(string);
    ashDebugPrintf("\r\n");
  }
}

void ashTraceEventTime(const char *string)
{
  if (ashReadConfig(traceFlags) & TRACE_EVENTS) {
    ashPrintElapsedTime();
    ashDebugPrintf(string);
    ashDebugPrintf("\r\n");
  }
}

void ashTraceDisconnected(int8u error)
{
  if (ashReadConfig(traceFlags) & TRACE_EVENTS) {
    ashPrintElapsedTime();
    ashDebugPrintf("ASH disconnected: %s\r\n", ashErrorString(error));
    ashDebugPrintf("    NCP status: %s\r\n", ashErrorString(ncpError));
    ashDebugFlush();
  }
}

void ashTraceEvent(const char *string)
{
  if (ashReadConfig(traceFlags) & TRACE_EVENTS) {
    ashDebugPrintf(string);
    ashDebugPrintf("\r\n");
  }
}

void ashTraceArray(int8u *name, int8u len, int8u *data)
{
  if (ashReadConfig(traceFlags) & TRACE_EVENTS) {
    ashDebugPrintf("%s ", name);
    while(len--) {
      ashDebugPrintf(" %02X", *data++);
    }
    ashDebugPrintf("\r\n");
  }
}

void ashTraceEzspFrameId(const char *message, int8u *ezspFrame)
{
  if (ashReadConfig(traceFlags) & TRACE_EZSP) {
    int8u frameId = ezspFrame[EZSP_FRAME_ID_INDEX];
    ashPrintElapsedTime();
    ashDebugPrintf("EZSP: %s %s (%02X)\r\n",
                   message, decodeFrameId(frameId), frameId);
  }
}

void ashTraceEzspVerbose(char *format, ...)
{
  if (ashReadConfig(traceFlags) & TRACE_EZSP_VERBOSE) {
    va_list argPointer;
    ashPrintElapsedTime();
    ashDebugPrintf("EZSP: ");
    va_start(argPointer, format);
    ashDebugVfprintf(format, argPointer);
    va_end(argPointer);
    ashDebugPrintf("\r\n");
  }
}
