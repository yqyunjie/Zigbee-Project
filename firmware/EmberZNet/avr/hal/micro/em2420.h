/** @file em2420.h
 * @brief HAL interfaces for the EM2420 radio.
 *
 * The interfaces described in this file should not be called directly
 * by any applications. They are strictly for internal stack use but
 * must be implemented by the HAL to provide an abstraction across
 * differing hardware implementations.
 *
 * See also @ref hal for more documentation.
 *  
 * <!--Author(s): Lee Taylor, lee@ember.com-->
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */
#ifndef __HAL_EM2420_H__
#define __HAL_EM2420_H__

/** @addtogroup hal
 *
 * See also em2420.h.
 *
 */

/** @name Radio Power Functions */
//@{

/** @brief Initializes the SPI and and ports required for 
 * the EM2420.  
 */
void halStack2420Init(void);

/** @brief Powers down the radio by disabling the voltage
 * regulator and setting ports so that they don't leak any current. 
 *
 * @note: Sleeping the radio via radioSleep() is usually preferable 
 * to powering it down because powering the radio back up requires much 
 * more time than waking it from sleep. However, powering down is the 
 * best choice when radio power consumption must be minimized. 
 */ 
void halStack2420PowerDown(void);

//@}  // end of Radio Power Functions


/** @name Pin Functions */
//@{

/** @brief Returns the state of the FIFO pin.
 *   
 * @return TRUE if asserted.
 */
boolean halStack2420GetFifoPin(void);

/** @brief Returns the state of the FIFOP pin.
 *   
 * @return TRUE if asserted.
 */
boolean halStack2420GetFifopPin(void);

/** @brief Returns the state of the CCA pin.
 *   
 * @return TRUE if asserted.
 */
boolean halStack2420GetCcaPin(void);

/** @brief Returns the state of the SFD pin.
 *   
 * @return TRUE if asserted.
 */
#ifdef HALTEST 
   boolean halStack2420GetSfdPin(void);
#endif
//@}  // end of Pin Functions


/** @name Interrupt Functions */
//@{
/** @brief Enables both radio interrupts (FIFOP and SFD). */
void halStack2420EnableInterrupts(void);

/** @brief Enables the FIFOP radio interrupt. */
void halStack2420EnableRxInterrupt(void);

/** @brief Disables both radio interrupts (FIFOP and SFD). */
void halStack2420DisableInterrupts(void);

/** @brief Disables the FIFOP radio interrupt. */
void halStack2420DisableRxInterrupt(void);

/** @brief The HAL should call this whenever the FIFOP interrupt
 * is triggered.
 */
void emberRadioReceiveIsr(void);
//@}  // end of Interrupt Functions


/** @name Radio Interface Functions */
//@{

/** @brief Performs all SPI operations required to set a register
 * with a value.
 * 
 * @param addr  The register to set.
 *   
 * @param data  The value to write.
 * 
 * @return The status from halCommonSpiReadWrite() operation.
 */
int8u halStack2420SetRegister(int16u addr, int16u data);

/** @brief Performs all SPI operations required to get the value stored 
 * in a register.
 *   
 * @param addr  The register to read.
 *   
 * @return The register's value.
 */
int16u halStack2420GetRegister(int8u addr);

/** @brief Performs all SPI operations required to send a strobe command.
 * 
 * @param addr  The strobe to send.
 * 
 * @return The status from halCommonSpiReadWrite() operation.
 */
int8u halStack2420Strobe(int8u addr);

/** @brief Performs all SPI operations required to write data to the 
 * TX FIFO.
 * 
 * @param buff      A pointer to the data to write.
 *   
 * @param addr      The address to write.
 *   
 * @param numbytes  The number of bytes to write.
 *   
 * @return The status from halCommonSpiReadWrite() operation.
 */
int8u halStack2420WriteFifo(int8u* buff, int8u addr, int8u numbytes);

/** @brief Performs all SPI operations required to read a specified 
 * amount of data from the RX FIFO into a buffer.
 * 
 * @param buff      A pointer to the data to read.
 *  
 * @param addr      The address to read.
 *  
 * @param numbytes  The number of bytes to read.
 *  
 * @return The status from halCommonSpiReadWrite() operation.
 */
int8u halStack2420ReadFifo(int8u *buff, int8u addr, int8u numbytes);

/** @brief Performs all the initial operations required to put the EM2420 
 * into a mode to read data from the RX FIFO, and leaves it in that mode
 * so that future calls to halCommonSpiRead() will return data from the FIFO.
 *  
 * @param addr  The address to read.
 *  
 * @return The status from halCommonSpiReadWrite() operation.
 */ 
int8u halStack2420StartReadFifo(int8u addr);

/** @brief Returns both the SPI interface and the EM2420 to a normal 
 * mode after being in the mode set by halStack2420StartReadFifo().   
*/
void halStack2420EndReadFifo(void);

/** @brief Returns a 1 if we are in the middle of a fifo read.
*/
boolean halStack2420ReadFifoInProgress(void);

/** @brief Performs all initial operations to put the EM2420 into a mode
 * to write data to an arbitrary EM2420 RAM address, and leaves it in that
 * mode so that future calls to halCommonSpiWrite() will write data to the
 * next RAM address.
 * 
 * @param addr  The address to read.
 *  
 * @return The status from halCommonSpiReadWrite() operation.
 */ 
int8u halStack2420StartWriteRam(int16u addr); 

/** @brief Performs all initial operations to put the EM2420 into a mode
 * to read data from an arbitrary EM2420 RAM address, and leaves it in 
 * that mode so that future calls to halCommonSpiRead() will read data from 
 * the next RAM address.
 * 
 * @param addr  The address to read.
 *    
 * @return The status from halCommonSpiReadWrite() operation.
 */
int8u halStack2420StartReadRam(int16u addr);

/** @brief Returns both the SPI interface and the EM2420 to a normal
 * mode after being in the mode set by halStack2420StartReadRam() or 
 * halStack2420StartWriteRam(). 
 */ 
void halStack2420EndRam(void);

/** @brief Performs all SPI operations required to write data to an 
 * EM2420 RAM address.
 * 
 * @param buff      A pointer to the data to read.
 * 
 * @param addr      The address to read.
 *   
 * @param numbytes  The number of bytes to read.
 *   
 * @return The status from halCommonSpiReadWrite() operation.
 */ 
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define halStack2420WriteRam(buff, addr, numbytes) \
  halStack2420WriteRamSpecifyOrder(buff, addr, numbytes, FALSE)
#else
int8u halStack2420WriteRam(int8u* buff, int16u addr, int8u numbytes);
#endif

/** @brief Performs all SPI operations required to write data to an 
 * EM2420 RAM address.  It optionally reverses the byte order for writes
 * to the security RAM page.
 * 
 * @param buff      A pointer to the data to read.
 * 
 * @param addr      The address to read.
 *   
 * @param numbytes  The number of bytes to read.
 *
 * @param reverse   When TRUE, this function writes the data in
 *    reverse order, mapping buff[numbytes - 1] to addr.
 *   
 * @return The status from halCommonSpiReadWrite() operation.
 */ 

int8u halStack2420WriteRamSpecifyOrder(int8u *buff, int16u addr, 
                                  int8u numbytes, boolean reverse);

/** @brief Performs all SPI operations required to read a specified amount
 * of data from an EM2420 RAM address into a buffer.
 * 
 * @param buff      A pointer to the data to read.
 *    
 * @param addr      The address to read.
 *     
 * @param numbytes  The number of bytes to read.
 *   
 * @return The status from halCommonSpiReadWrite() operation.
 */  
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define halStack2420ReadRam(buff, addr, numbytes) \
  halStack2420ReadRamSpecifyOrder(buff, addr, numbytes, FALSE)
#else
int8u halStack2420ReadRam(int8u* buff, int16u addr, int8u numbytes);
#endif

/** @brief Performs all SPI operations required to read a specified amount
 * of data from an EM2420 RAM address into a buffer. It optionally reverses the byte
 * order for reading the security RAM page.
 * 
 * @param buff      A pointer to the data to read.
 *    
 * @param addr      The address to read.
 *     
 * @param numbytes  The number of bytes to read.
 *
 * @param reverse   When TRUE, this function reads the data in
 *    reverse order, mapping buff[numbytes - 1] to addr.
 *
 * @return The status from halCommonSpiReadWrite() operation.
 */  
int8u halStack2420ReadRamSpecifyOrder(int8u* buff, int16u addr, 
                                 int8u numbytes, boolean reverse);

//@}  // end of Radio interface functions


/** @brief On slower systems the FIFO cannot be stuffed as 
 * fast as the radio transmits. This function tells the PHY when to start 
 * stuffing the FIFO.
 * 
 * @param prestuff  Set TRUE for faster systems, to prestuff the FIFO.
 * TRUE is the default choice since it is the most common.
 * This really only needs to be set FALSE for slow systems like 1MHz.
 */  
void halStack2420PreStuffFifo(boolean prestuff);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
enum {
  PACKET_TRACE_TRANSMIT_MODE = 0x00,
  PACKET_TRACE_RECEIVE_MODE  = 0x01
};

void halStack2420StartPacketTrace( int8u direction );
void halStack2420StopPacketTrace( void );
void halStack2420InitPacketTrace( void );

#endif

#endif // __HAL_EM2420_H__
