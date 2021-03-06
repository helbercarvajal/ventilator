/*
 * File:   interruption.h
 * Author: GIBIC UdeA
 *
 * Created on July 4, 2020, 13:41 PM
 */

#ifndef INTERRUPTION_H
#define INTERRUPTION_H

#ifdef __cplusplus
extern "C"
{
#endif

  // #include <stdio.h>
  // #include <stdlib.h>
  // #include <string.h>

  /** ****************************************************************************
 ** ************ INCLUDES ******************************************************
 ** ****************************************************************************/
#include <Arduino.h>
#include <Esp.h>
#include <nvs_flash.h>

#include "initializer.h"
#include "encoder.h"
#include "timer.h"

  /** ****************************************************************************
 ** ************ DEFINES *******************************************************
 ** ****************************************************************************/

  /** ****************************************************************************
 ** ************ VARIABLES *****************************************************
 ** ****************************************************************************/

  /** ****************************************************************************
 ** ************ FUNCTIONS *****************************************************
 ** ****************************************************************************/
  void IRAM_ATTR swInterrupt(void);              // Interrupcion por presion del switch
  void IRAM_ATTR encoderInterrupt_A(void);       // Interrupcion por encoder A
  void IRAM_ATTR encoderInterrupt_B(void);       // Interrupcion por encoder B
  void IRAM_ATTR standbyButtonInterrupt(void);   // Interrupcion por boton Standby
  void IRAM_ATTR silenceButtonInterrupt(void);   // Interrupcion por button silence
  void IRAM_ATTR stabilityButtonInterrupt(void); // Interrupcion por boton estabilidad

  void silenceInterruptAttention(void); // Atencion a interrupcion por boton de silencio
  void standbyInterruptAttention(void); // Atencion a interrupcion por boton de standby

  /* *****************************************************************************
 * *****************************************************************************
 * ******************** USO DE MODULO ADS **************************************
 * *****************************************************************************
 * *****************************************************************************/

  /* *****************************************************************************
 * *****************************************************************************
 * ***************** PROTOTYPE DEFINITION **************************************
 * *****************************************************************************
 * *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* INTERRUPTION_H */
