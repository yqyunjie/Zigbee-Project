/***************************************************************************//**
 * @file
 * @brief TWI Peripheral API for EM35x from Silicon Labs.
 * @author kaiser.ren, renkaikaiser@163.com
 *******************************************************************************
 * @section License
 * <b>(C) Copyright by kaiser.ren, All rights reserved.</b>
 *******************************************************************************
 ******************************************************************************/
#ifndef _TWI_H_
#define _TWI_H_

/*----------------------------------------------------------------------------
 *        Macro
 *----------------------------------------------------------------------------*/
// device address on i2c bus
#define LM73_DEVICE_ADDRESS  (0x92 >> 1)
#define TSL2550_DEVICE_ADDRESS (0X39)
#define AD7414_DEVICE_ADDRESS (0x48)

// SCx block base address
#define SC1_BASE_ADDR 0x4000C800u
#define SC2_BASE_ADDR 0x4000C000u

// SCx REV wait period when 1000Hz TWI speed
#define SCX_WAIT_WHEN_1KHZ 300000ul

/**
 * @brief
 *   Indicate plain write sequence: S+ADDR(W)+DATA0+P.
 * @details
 *   @li S - Start
 *   @li ADDR(W) - address with W/R bit cleared
 *   @li DATA0 - Data taken from buffer with index 0
 *   @li P - Stop
 */
#define I2C_FLAG_WRITE          0x0001

/**
 * @brief
 *   Indicate plain read sequence: S+ADDR(R)+DATA0+P.
 * @details
 *   @li S - Start
 *   @li ADDR(R) - address with W/R bit set
 *   @li DATA0 - Data read into buffer with index 0
 *   @li P - Stop
 */
#define I2C_FLAG_READ           0x0002

/**
 * @brief
 *   Indicate combined write/read sequence: S+ADDR(W)+DATA0+Sr+ADDR(R)+DATA1+P.
 * @details
 *   @li S - Start
 *   @li Sr - Repeated start
 *   @li ADDR(W) - address with W/R bit cleared
 *   @li ADDR(R) - address with W/R bit set
 *   @li DATAn - Data written from/read into buffer with index n
 *   @li P - Stop
 */
#define I2C_FLAG_WRITE_READ     0x0004

/**
 * @brief
 *   Indicate write sequence using two buffers: S+ADDR(W)+DATA0+DATA1+P.
 * @details
 *   @li S - Start
 *   @li ADDR(W) - address with W/R bit cleared
 *   @li DATAn - Data written from buffer with index n
 *   @li P - Stop
 */
#define I2C_FLAG_WRITE_WRITE    0x0008

/*----------------------------------------------------------------------------
 *        Typedef
 *----------------------------------------------------------------------------*/
/**************************************************************\
   TWI bus frequency.
\**************************************************************/
typedef enum{
  	twiSCLK_1000HZ = 0,
	twiSCLK_6000HZ,
  	twiSCLK_12KHZ,
	twiSCLK_25KHZ,
   twiSCLK_50KHZ,
   twiSCLK_100KHZ,
   twiSCLK_375KHZ,
	twiSCLK_400KHZ
}TWI_BusFreq_TypeDef;

/**************************************************************\
   TWI channel.
\**************************************************************/
typedef enum{
   twiSC1 = SC1_BASE_ADDR,
   twiSC2 = SC2_BASE_ADDR
}TWI_SCx_TypeDef;

/**************************************************************\
   TWI bus Read/Write buffer.
\**************************************************************/
typedef struct{
   int8u addr;
	int8u flag;
	struct{
		int8u len;
   	int8u* data;
	}buf[2];
}TWI_WRBuf_TypeDef;

/*----------------------------------------------------------------------------
 *        Prototype
 *----------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief
 *   Set current configured TWI bus frequency.
 *
 * @details
 *   This frequency is only of relevance when acting as master.
 *
 * @param[in] twi
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_BusFreqSet(TWI_SCx_TypeDef ch, TWI_BusFreq_TypeDef speed);

/***************************************************************************//**
 * @brief
 *   Initialize TWI.
 *
 * @param[in] i2c
 *   TBD.
 *
 * @param[in] init
 *   NULL
 ******************************************************************************/
void TWI_Init(TWI_SCx_TypeDef ch);

/***************************************************************************//**
 * @brief
 *  TWI Write (single master mode only).
 *
 * @details
 *
 * @param[in] TWI
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_Wr(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data);

/***************************************************************************//**
 * @brief
 *  TWI Read (single master mode only).
 *
 * @details
 *
 * @param[in] TWI
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_Rd(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data);

#endif /*_TWI_H_*/
