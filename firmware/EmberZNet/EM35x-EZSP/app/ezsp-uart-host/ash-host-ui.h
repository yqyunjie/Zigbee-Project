/** @file ash-host-ui.h
 * @brief Header for ASH Host user interface functions
 *
 * See @ref ash_util for documentation.
 *
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup ash_util
 *
 * See ash-host-ui.h.
 *
 *@{
 */

#ifndef __ASH_HOST_UI_H__
#define __ASH_HOST_UI_H__

/** @brief Prints usage instructions to stderr.
 *
 * @param name  program name (usually argv[0])
 */
void ashPrintUsage(char *name);

/** @brief Sets host configuration values from command line options.
 *
 * @param argc  number of command line tokens
 * 
 * @param argv  array of pointer to command line tokens
 *
 * @return  TRUE if no errors were detected in the command line
 */
boolean ashProcessCommandOptions(int argc, char *argv[]);

/** @brief Writes a debug trace message, if enabled.
 *
 * @param string  pointer to message string
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_NO_RX_DATA
 */
void ashTraceEvent(const char *string);

/** @brief  Prints host counter data.
 *
 * @param counters  pointer to counters structure
 *
 * @param clear     if TRUE clears counters
 */
void ashPrintCounters(AshCount *counters, boolean clear);

/** @brief  Clears host counter data.
 *
 * @param counters  pointer to counters structure
 */
void ashClearCounters(AshCount *counters);

/** @brief  Converts ASH reset/error code to a string.
 *
 * @param error  error or reset code (from ashError or ncpError)
 *
 * @return  pointer to the string
 */
const int8u* ashErrorString(int8u error);

/** @brief  Converts EZSP-ASH error code to a string.
 *
 * @param error  error code
 *
 * @return  pointer to the string
 */
const int8u* ashEzspErrorString(int8u error);

#define BUMP_HOST_COUNTER(mbr) do {ashCount.mbr++;} while (0)
#define ADD_HOST_COUNTER(op, mbr) do {ashCount.mbr += op;}  while(0)

#endif //__ASH_HOST_UI_H___

/** @} // END addtogroup
 */
