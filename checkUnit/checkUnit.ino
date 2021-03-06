﻿/*
 Name:		monitorUnit.ino
 Created:	4/3/2020 17:28:49
 Author:    Helber Carvajal
*/

#include <Arduino.h>
#include <Esp.h>
#include <math.h>
#include <EEPROM.h>
#include <nvs_flash.h>
#include <stdio.h>

//********DEFINICIONES CONDICIONES******
#define TRUE          1
#define FALSE         0

//********DEFINICION DE VERSION*********
#define VERSION_1_0       TRUE
// #define SERIAL_DEVICE     "9GF100007LJD00000"
#define SERIAL_DEVICE     "1A"

//********COMPILACION CONDICIONAL*******
#ifdef VERSION_1_0

// Definiciones para controlar el shiel DFRobot quad motor driver
// Definiciones para controlar el shiel DFRobot quad motor driver
#define EV_INSPIRA      13  // out 7 // Valvula 3/2 de control de la via inspiratoria (pin 3 del shield, velocidad motor 1)
#define EV_ESC_CAM      14  // out 6 // Valvula 3/2 de activaci�n de la camara (pin 6 del shield, velocidad motor 4)
#define EV_ESPIRA       12  // out 8 // Valvula 3/2 de control de presiones PCON y PEEP (pin 11 del shield, velocidad motor 2)

// Definiciones para el manejo del ADC
#define ADC_PRESS_1     26  // ADC 7 // Sensor de presion xx (pin ADC para presion 1)
#define ADC_PRESS_2     35  // ADC 4 // Sensor de presion xx (pin ADC para presion 2)
#define ADC_PRESS_3     33  // ADC 6 // Sensor de presion via aerea del paciente (pin ADC para presion 3)
#define ADC_FLOW_1      39  // ADC 1 // Sensor de flujo linea xx (pin ADC para presion 2)
#define ADC_FLOW_2      36  // ADC 2 // Sensor de flujo linea xx (pin ADC para presion 3)

#elif VERSION_1_1

// Definiciones para controlar el shiel DFRobot quad motor driver
// Definiciones para controlar el shiel DFRobot quad motor driver
#define EV_INSPIRA      5   // out 3 // Valvula 3/2 de control de la via inspiratoria (pin 3 del shield, velocidad motor 1)
#define EV_ESPIRA       4  // out 2 // Valvula 3/2 de control de presiones PCON y PEEP (pin 11 del shield, velocidad motor 2)
#define EV_ESC_CAM      18   // out 1 // Valvula 3/2 de activaci�n de la camara (pin 6 del shield, velocidad motor 4)

// Definiciones para el manejo del ADC
#define ADC_PRESS_1     33  // ADC 6 // Sensor de presion xx (pin ADC para presion 1)
#define ADC_PRESS_2     32  // ADC 5 // Sensor de presion xx (pin ADC para presion 2)
#define ADC_PRESS_3     34  // ADC 4 // Sensor de presion via aerea del paciente (pin ADC para presion 3)
#define ADC_FLOW_1      36  // ADC 1 // Sensor de flujo linea xx (pin ADC para presion 2)
#define ADC_FLOW_2      39  // ADC 2 // Sensor de flujo linea xx (pin ADC para presion 3)

#endif

// Calibracion de los sensores de presion - coeficientes regresion lineal
#define AMP1          0.0262
#define OFFS1         -15.092
#define AMP2          0.0293
#define OFFS2         -20.598
#define AMP3          0.0292
#define OFFS3         -21.429

// Calibracion de los sensores de flujo - coeficientes regresion lineal
// Sensor de flujo Inspiratorio
#define AMP_FI_1      0.116300
#define OFFS_FI_1     -211.888900
#define LIM_FI_1      1599
#define AMP_FI_2      0.610700
#define OFFS_FI_2     -1002.337700
#define LIM_FI_2      1684
#define AMP_FI_3      0.116300
#define OFFS_FI_3     -169.789300

// Sensor de flujo Espiratorio
#define AMP_FE_1      0.091800
#define OFFS_FE_1     -183.289800
#define LIM_FE_1      1714
#define AMP_FE_2      0.381000
#define OFFS_FE_2     -678.984800
#define LIM_FE_2      1850
#define AMP_FE_3      0.091800
#define OFFS_FE_3     -143.815400

// variable para ajustar el nivel cero de flujo y calcular el volumen
#define FLOWUP_LIM        3
#define FLOWLO_LIM        -3
#define FLOW_CONV         16.666666    // conversion de L/min a mL/second
#define DELTA_T           0.05         // delta de tiempo para realizar la integra
#define VOL_SCALE         0.85         // Factor de escala para ajustar el volumen

#define ADC_FAST          3  // muestreo cada 3 ms
#define ADC_SLOW          50  // muestreo cada 50 ms

// Variables de control del protocolo
#define RXD2 16
#define TXD2 17

// Definiciones para State machine
#define CHECK_STATE		      0
#define STANDBY_STATE	      1
#define PCMV_STATE		      2
#define AC_STATE	        	3
#define CPAP_STATE	      	4
#define FAILURE_STATE	      5

// Definiciones para ciclado en mode C-PMV
#define STOP_CYCLING		    	0
#define START_CYCLING		    	1
#define INSPIRATION_CYCLING		2
#define EXPIRATION_CYCLING		3

// Definiciones para ciclado en mode CPAP
#define COMP_FLOW_MAX_CPAP             3  // variable para comparacion de flujo y entrar en modo Inspiratorio en CPAP
#define COMP_FLOW_MIN_CPAP            -3  // variable para comparacion de flujo y entrar en modo Inspiratorio en CPAP
#define COMP_DEL_F_MAX_CPAP            2  // variable para comparacion de flujo y entrar en modo Inspiratorio en CPAP
#define COMP_DEL_F_MIN_CPAP           -2  // variable para comparacion de flujo y entrar en modo Inspiratorio en CPAP
#define CPAP_NONE                      0  // Estado de inicializacion
#define CPAP_INSPIRATION               1  // Entra en modo inspiratorio
#define CPAP_ESPIRATION                2  // Entra en modo espiratorio


//creo el manejador para el semaforo como variable global
SemaphoreHandle_t xSemaphoreTimer = NULL;
SemaphoreHandle_t xSemaphoreAdc = NULL;
//xQueueHandle timer_queue = NULL;

// definicion de los core para ejecucion
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

// Variables y parametros de impresion en raspberry
String idEqupiment;
String patientPress;
String patientFlow;
String patientVolume;
String pressPIP;
String pressPEEP;
String frequency;
String rInspir;
String rEspir;
String volumeT;
String alertPip;
String alertPeep;
String alertDiffPress;
String alertConnPat;
String alertLeak;
String alertConnEquipment;
String alertValve1Fail;
String alertValve2Fail;
String alertValve3Fail;
String alertValve4Fail;
String alertValve5Fail;
String alertValve6Fail;
String alertValve7Fail;
String alertValve8Fail;
String valve1Temp;
String valve2Temp;
String valve3Temp;
String valve4Temp;
String valve5Temp;
String valve6Temp;
String valve7Temp;
String valve8Temp;
String cameraPress;
String bagPress;
String inspFlow;
String inspExp;
String lPresSup;
String lPresInf;
String lFlowSup;
String lFlowInf;
String lVoluSup;
String lVoluInf;

// Cadena de impresion en raspberry
String RaspberryChain = "";

// contadores de tiempo
int second = 0;
int milisecond = 0;

// Variables de manejo de ADC
volatile int contADC = 0;
volatile int contADCfast = 0;
bool fl_ADC = false;
int ADC1_Value = 0;
int ADC2_Value = 0;
int ADC3_Value = 0;
int ADC4_Value = 0;
int ADC5_Value = 0;
float Pressure1 = 0;
float Pressure2 = 0;
float Pressure3 = 0;
float flow1 = 0;
float flow2 = 0;
float flowZero = 0;
float flowTotal = 0;

// Limites de presion, flujo y volumen para las graficas
float lMaxPres;
float lMinPres;
float lMaxFlow;
float lMinFlow;
float lMaxVolu;
float lMinVolu;

// Variables de maquinas de estado
unsigned int currentStateMachineCycling = START_CYCLING;
int currentStateMachine = STANDBY_STATE;
int newStateMachine = STANDBY_STATE;
int currentVentilationMode = 0;
int newVentilationMode = 0;
int newTrigger = 2;
int newPeepMax = 5;
int maxFR = 30;
int maxVE = 30;
int apneaTime = 10;

// variables para calibracion de sensores
float CalFin = 0; // almacena valor ADC para calibracion
float CalFout = 0; // almacena valor ADC para calibracion
float CalPpac = 0; // almacena valor ADC para calibracion
float CalPin = 0; // almacena valor ADC para calibracion
float CalPout = 0; // almacena valor ADC para calibracion
//- Senales
float SFin = 0; //Senal de flujo inspiratorio
float SFout = 0; //Senal de flujo espiratorio

float SPpac = 0; //Senal de presion en la via aerea del paciente
float SFpac = 0; //Senal de flujo del paciente
float SPin = 0; //Senal filtrada de presion en la camara
float SPout = 0; //Senal filtrada de presion en la bolsa
float SVtidal = 0; // informacion de promedio para Vtidal
float Sfrec = 0; // informacion de promedio para frecuencia

//- Filtrado
float Pin[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float Pout[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float Ppac[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float Fin[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float Fout[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float VTidProm[3] = { 0, 0, 0 };
float FreqProm[3] = { 0, 0, 0 };

// Variables de ventilacion umbrales
int currentFrecRespiratoria = 12;
int currentI = 1;
int currentE = 20;
float relI = 0;
float relE = 0;
volatile float inspirationTime = 1.666;
volatile float expirationTime = 3.333;
int frecRespiratoriaCalculada = 0;
int calculatedE = currentE;
int calculatedI = currentI;
int currentVE = 0;
int maxPresion = 30;
//- Mediciones derivadas
float UmbralPpmin = 100;
float UmbralPpico = -100;
float UmbralFmin = 100;
float UmbralFmax = -100;
float UmbralVmin = 100;
float UmbralVmax = -100;
float SFinMax = 50;
float SFoutMax = -50;
float SFinMaxInsp = 50;
float SFtotalMax = -50;
float Pin_max = 0;
float Pout_max = 0;
float Pin_min = 0;
float Pout_min = 0;
float pmin = 0;
float pmax = 0;
float flmin = 0;
float flmax = 0;
float vmin = 0;
float vmax = 0;
unsigned char BandInsp = 0;
unsigned char BandGeneral = 0;
unsigned char ContGeneral = 0;

// Variables recibidas en el Serial
int newFrecRespiratoria = currentFrecRespiratoria;
int newI = currentI;
int newE = currentE;

// mediciones
float Peep = 0;
float Ppico = 0;
float Vtidal = 0;
float VT = 0;
float SUMVin_Ins = 0;
float SUMVout_Ins = 0;
float SUMVin_Esp = 0;
float SUMVout_Esp = 0;
float Vin_Ins = 0;
float Vout_Ins = 0;
float Vin_Esp = 0;
float Vout_Esp = 0;

// variables para calculo de frecuencia y relacion IE en CPAP
float SFant = 0;
int stateFrecCPAP = 0;
int contFrecCPAP = 0;
int contEspCPAP = 0;
int contInsCPAP = 0;

// Variables para el envio y recepcion de alarmas
int alerPresionPIP = 0;
int alerDesconexion = 0;
int alerObstruccion = 0;
int alerPeep = 0;
int alerBateria = 0;
int alerGeneral = 0;
int alerFR_Alta = 0;
int alerVE_Alto = 0;
int estabilidad = 0;
int PeepEstable = 0;

// variables de atencion a interrupcion
volatile uint8_t flagInicio = true;
volatile uint8_t flagTimerInterrupt = false;
volatile uint8_t flagAdcInterrupt = false;
volatile uint8_t flagAlarmPpico = false;
volatile uint8_t flagAlarmGeneral = false;
volatile uint8_t flagAlarmPatientDesconnection = false;
volatile uint8_t flagAlarmObstruccion = false;
volatile uint8_t flagAlarmFR_Alta = false;
volatile uint8_t flagAlarmVE_Alto = false;
volatile uint8_t flagStandbyInterrupt = false;

// variables contadores
volatile unsigned int contDetach = 0;
unsigned int contCiclos = 0;
unsigned int contEscrituraEEPROM = 0;
unsigned int contUpdateData = 0;
unsigned int contStandby = 0;
volatile int contCycling = 0;

// inicializacion del contador del timer
hw_timer_t* timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// definicion de interrupciones
void IRAM_ATTR onTimer(void);  // funcion de interrupcion

/* *********************************************************************
 * **** FUNCIONES DE INICIALIZACION MEMORIA ****************************
 * *********************************************************************/
int eeprom_wr_int(int dataIn = 0, char process = 'r') {
    int dataRead = 0;
    if (process == 'w') {
        byte r1 = (dataIn & 0xff);
        EEPROM.write(0, r1);
        r1 = (dataIn & 0xff00) >> 8;
        EEPROM.write(1, r1);
        r1 = (dataIn & 0xff0000) >> 16;
        EEPROM.write(2, r1);
        r1 = (dataIn & 0xff000000) >> 24;
        EEPROM.write(3, r1);
        EEPROM.commit();
        return dataRead;
    }
    else if (process == 'r') {
        dataRead = EEPROM.read(0);
        dataRead = (EEPROM.read(1) << 8) + dataRead;
        dataRead = (EEPROM.read(2) << 16) + dataRead;
        dataRead = (EEPROM.read(3) << 24) + dataRead;
        return dataRead;
    }
    return dataRead;
}

/************************************************************
 ********** FUNCIONES DE INICIALIZACION *********************
 ***********************************************************/
void init_GPIO() {
    // Configuracion de los pines de conexion con del driver para manejo de electrovalvulas
    pinMode(2, OUTPUT);		// PIN 3   velocidad
    pinMode(4, OUTPUT);		// PIN 6   velocidad
    pinMode(5, OUTPUT);		// PIN 12  velocidad
    pinMode(12, OUTPUT);	    // PIN 3   velocidad
    pinMode(13, OUTPUT);	    // PIN 6   velocidad
    pinMode(14, OUTPUT);	    // PIN 12  velocidad
    pinMode(15, OUTPUT);	    // PIN 3   velocidad
    pinMode(18, OUTPUT);	    // PIN 6   velocidad

    // inicializacion de los pines controladores de las EV como salidas
    pinMode(EV_INSPIRA, OUTPUT);	// PIN 3   velocidad
    pinMode(EV_ESC_CAM, OUTPUT);	// PIN 6   velocidad
    pinMode(EV_ESPIRA, OUTPUT);		// PIN 12  velocidad

    // Inicializacion de los pines de ADC para conversion Analogo-digitalPinToInterrupt
    adcAttachPin(ADC_PRESS_1);
    adcAttachPin(ADC_PRESS_2);
    adcAttachPin(ADC_PRESS_3);
    adcAttachPin(ADC_FLOW_1);
    adcAttachPin(ADC_FLOW_2);

    // desactiva todas las salidas de electrovalvulas
    digitalWrite(2, LOW);	// PIN 3   velocidad
    digitalWrite(4, LOW);	// PIN 6   velocidad
    digitalWrite(5, LOW);   // PIN 12  velocidad
    digitalWrite(12, LOW);  // PIN 3   velocidad
    digitalWrite(13, LOW);  // PIN 6   velocidad
    digitalWrite(14, LOW);  // PIN 12  velocidad
    digitalWrite(15, LOW);  // PIN 3   velocidad
    digitalWrite(18, LOW);  // PIN 6   velocidad

}

void init_TIMER() {
    // Configuracion del timer a 1 kHz
    timer = timerBegin(0, 80, true);                // Frecuencia de reloj 80 MHz, prescaler de 80, frec 1 MHz
    timerAttachInterrupt(timer, &onTimer, true);    // Attach onTimer function to our timer
    timerAlarmWrite(timer, 1000, true);             // Interrupcion cada 1000 conteos del timer, es decir 100 Hz
    timerAlarmEnable(timer);                        // Habilita interrupcion por timer
}

void init_MEMORY() {
    // Inicializacion y consulta de la memoria EEPROM
    EEPROM.begin(512);
    contCiclos = eeprom_wr_int();

    //Inicializacion de los strings comunicacion con la Raspberry
    idEqupiment = String(SERIAL_DEVICE);
    patientPress = String("");
    patientFlow = String("");
    patientVolume = String("");
    pressPIP = String("");
    pressPEEP = String("");
    frequency = String("");
    rInspir = String("");
    rEspir = String("");
    volumeT = String("");
    alertPip = String("");
    alertPeep = String("");
    alertDiffPress = String("");
    alertConnPat = String("");
    alertLeak = String("");
    alertConnEquipment = String("");
    alertValve1Fail = String("");
    alertValve2Fail = String("");
    alertValve3Fail = String("");
    alertValve4Fail = String("");
    alertValve5Fail = String("");
    alertValve6Fail = String("");
    alertValve7Fail = String("");
    alertValve8Fail = String("");
    valve1Temp = String("");
    valve2Temp = String("");
    valve3Temp = String("");
    valve4Temp = String("");
    valve5Temp = String("");
    valve6Temp = String("");
    valve7Temp = String("");
    valve8Temp = String("");
    cameraPress = String("");
    bagPress = String("");
    inspFlow = String("");
    inspExp = String("");
    lPresSup = String("");
    lPresInf = String("");
    lFlowSup = String("");
    lFlowInf = String("");
    lVoluSup = String("");
    lVoluInf = String("");

    RaspberryChain = String("");

}

/* *********************************************************************
 * **** FUNCIONES DE ATENCION A INTERRUPCION ***************************
 * *********************************************************************/
 // Interrupcion por timer
void IRAM_ATTR onTimer(void) {
    portENTER_CRITICAL_ISR(&timerMux);
    flagTimerInterrupt = true;   // asignacion de banderas para atencion de interrupcion
    flagAdcInterrupt = true;
    xSemaphoreGiveFromISR(xSemaphoreTimer, NULL);  // asignacion y liberacion de semaforos
    xSemaphoreGiveFromISR(xSemaphoreAdc, NULL);
    portEXIT_CRITICAL_ISR(&timerMux);
}

/************************************************************
 ***** FUNCIONES DE ATENCION A INTERRUPCION TAREA TIMER *****
 ************************************************************/
void task_Timer(void* arg) {

    while (1) {
        // Se atiende la interrpcion del timer
        if (xSemaphoreTake(xSemaphoreTimer, portMAX_DELAY) == pdTRUE) {
            if (flagTimerInterrupt == true) {
                portENTER_CRITICAL(&timerMux);
                flagTimerInterrupt = false;
                portEXIT_CRITICAL(&timerMux);

                /* *************************************************************
                 * **** SECUENCIA DE FUNCIONAMIENTO, ESTADOS DEL VENTILADOR ****
                 * *************************************************************/
                switch (currentStateMachine) {
                case CHECK_STATE:   // estado de checkeo
                    break;
                case STANDBY_STATE:  // Modo StandBy
                    standbyRoutine();
                    //Serial.println("Standby state on control Unit");
                    break;
                case PCMV_STATE:  // Modo Controlado por presion
                    cycling();
                    // Write the EEPROM each 10 minutes
                    contEscrituraEEPROM++;
                    if (contEscrituraEEPROM > 3600000) {
                        contEscrituraEEPROM = 0;
                        eeprom_wr_int(contCiclos, 'w');
                    }
                    //Serial.println("I am on PCMV_STATE");
                    break;
                case AC_STATE:  // Modo asistido controlado por presion
                    cycling();
                    // Write the EEPROM each 10 minutes
                    contEscrituraEEPROM++;
                    if (contEscrituraEEPROM > 3600000) {
                        contEscrituraEEPROM = 0;
                        eeprom_wr_int(contCiclos, 'w');
                    }
                    //Serial.println("I am on AC_STATE");
                    break;
                case CPAP_STATE:  // Modo CPAP
                    cpapRoutine();
                    //Serial.println("I am on CPAP_STATE");
                    break;
                case FAILURE_STATE:
                    currentStateMachine = STANDBY_STATE;
                    //Serial.println("I am on FAILURE_STATE");
                    break;
                default:
                    currentStateMachine = STANDBY_STATE;
                    //Serial.println("I am on DEFAULT_STATE");
                    break;
                }
            }
        }
    }
    vTaskDelete(NULL);
}

/************************************************************
 ***** FUNCIONES DE ATENCION A INTERRUPCION TAREA ADC *******
 ************************************************************/
void task_Adc(void* arg) {

    while (1) {
        // Se atiende la interrpcion del timer
        if (xSemaphoreTake(xSemaphoreAdc, portMAX_DELAY) == pdTRUE) {
            if (flagAdcInterrupt == true) {
                portENTER_CRITICAL(&timerMux);
                flagAdcInterrupt = false;
                portEXIT_CRITICAL(&timerMux);

                /* *************************************************************
                * **** SECUENCIA DE MEDICION, ADQUISICION DE VARIABLES *********
                * *************************************************************/
                contADC++;
                contADCfast++;
                // muestreo rapido de ADC
                if (contADCfast == ADC_FAST) {
                    contADCfast = 0;

                    // Lectura de valores ADC
                    ADC4_Value = analogRead(ADC_FLOW_1);  // ADC flujo de entrada
                    ADC5_Value = analogRead(ADC_FLOW_2);  // ADC flujo de salida
                    ADC1_Value = analogRead(ADC_PRESS_1);  // ADC presion camara
                    ADC2_Value = analogRead(ADC_PRESS_2);  // ADC presion equipo
                    ADC3_Value = analogRead(ADC_PRESS_3);// ADC presion de la via aerea

                    // Procesamiento senales
                    //- Almacenamiento
                    Fin[39] = ADC4_Value;
                    Fout[39] = ADC5_Value;
                    Ppac[39] = ADC3_Value;
                    Pin[39] = ADC1_Value;
                    Pout[39] = ADC2_Value;

                    //- Corrimiento inicial
                    for (int i = 39; i >= 1; i--) {
                        Fin[39 - i] = Fin[39 - i + 1];
                        Fout[39 - i] = Fout[39 - i + 1];
                        Pin[39 - i] = Pin[39 - i + 1];
                        Pout[39 - i] = Pout[39 - i + 1];
                        Ppac[39 - i] = Ppac[39 - i + 1];
                    }

                    //- Inicializacion
                    SFin = 0;
                    SFout = 0;
                    SPin = 0;
                    SPout = 0;
                    SPpac = 0;

                    //- Actualizacion
                    for (int i = 0; i <= 39; i++) {
                        SFin = SFin + Fin[i];
                        SFout = SFout + Fout[i];
                        SPin = SPin + Pin[i];
                        SPout = SPout + Pout[i];
                        SPpac = SPpac + Ppac[i];
                    }

                    //- Calculo promedio
                    SFin = SFin / 40;
                    SFout = SFout / 40;
                    SPin = SPin / 40;
                    SPout = SPout / 40;
                    SPpac = SPpac / 40;

                    // Actualizacion de valores para realizar calibracion
                    CalFin = SFin;
                    CalFout = SFout;
                    CalPpac = SPpac;
                    CalPin = SPin;
                    CalPout = SPout;

                    //- Conversion ADC-Presion
                    SPin = AMP1 * float(SPin) + OFFS1;
                    SPout = AMP2 * float(SPout) + OFFS2;
                    SPpac = AMP3 * float(SPpac) + OFFS3;// Presion de la via aerea

                    // Conversion ADC Flujo Inspiratorio, ajuste por tramos para linealizacion
                    if (SFin <= LIM_FI_1) {
                        SFin = AMP_FI_1 * float(SFin) + OFFS_FI_1;
                    }
                    else if (SFin <= LIM_FI_2) {
                        SFin = AMP_FI_2 * float(SFin) + OFFS_FI_2;
                    }
                    else {
                        SFin = AMP_FI_3 * float(SFin) + OFFS_FI_3;
                    }

                    // Conversion ADC Flujo Espiratorio, ajuste por tramos para linealizacion
                    if (SFout <= LIM_FE_1) {
                        SFout = AMP_FE_1 * float(SFout) + OFFS_FE_1;
                    }
                    else if (SFout <= LIM_FE_2) {
                        SFout = AMP_FE_2 * float(SFout) + OFFS_FE_2;
                    }
                    else {
                        SFout = AMP_FE_3 * float(SFout) + OFFS_FE_3;
                    }

                }

                // muestreo lento de ADC
                if (contADC == ADC_SLOW) {
                    contADC = 0;

                    // Calculo de relaciones I:E
                    if (currentI != 1) {
                        relI = (float)(currentI / 10.0);
                    }
                    else {
                        relI = (float)(currentI);
                    }
                    if (currentE != 1) {
                        relE = (float)(currentE / 10.0);
                    }
                    else {
                        relE = (float)(currentE);
                    }

                    // Calculo de volumen circulante
                    flowTotal = SFin - SFout - flowZero;
                    SFant = SFpac;
                    SFpac = SFin - SFout;  // flujo del paciente
                    if (alerGeneral == 0) {
                        if ((flowTotal <= FLOWLO_LIM) || (flowTotal >= FLOWUP_LIM)) {
                            Vtidal = Vtidal + (flowTotal * DELTA_T * FLOW_CONV * VOL_SCALE);

                            if (Vtidal < 0) {
                                Vtidal = 0;
                            }
                        }
                    }
                    else {
                        Vtidal = 0;
                    }

                    // Calculo Presiones maximas y minimas en la via aerea
                    if (UmbralPpmin > SPpac) {
                        UmbralPpmin = SPpac;
                    }
                    if (UmbralPpico < SPpac) {
                        UmbralPpico = SPpac;
                    }

                    // Calculo Flujos maximos y minimos en la via aerea
                    if (UmbralFmin > SFpac) {
                        UmbralFmin = SFpac;
                    }
                    if (UmbralFmax < SFpac) {
                        UmbralFmax = SFpac;
                    }

                    // Calculo Volumenes maximos y minimos en la via aerea
                    if (UmbralVmin > Vtidal) {
                        UmbralVmin = Vtidal;
                    }
                    if (UmbralVmax < Vtidal) {
                        UmbralVmax = Vtidal;
                    }

                    // Transmicon serial
                    //- Asignacion de variables
                    //if (alerDesconexion == 1) {
                    //	patientPress = String(0);
                    //}
                    //else {

                    // evaluacion de condicion de desconexion de paciente
                    if (flagAlarmPatientDesconnection == true) {
                        SPpac = 0;
                        SFpac = 0;
                        Vtidal = 0;
                        Ppico = 0;
                        Peep = 0;
                        currentFrecRespiratoria = 0;
                        relI = 0;
                        relE = 0;
                        VT = 0;
                    }

                    // almacenamiento de los datos para envio a la raspberry
                    patientPress = String(SPpac);
                    //}
                    patientFlow = String(SFpac);
                    patientVolume = String(Vtidal);
                    pressPIP = String(int(Ppico));
                    pressPEEP = String(int(Peep));
                    // frequency = String(currentFrecRespiratoria);
                    frequency = String(int(frecRespiratoriaCalculada));
                    if (currentI == 1) {
                        rInspir = String(int(relI));
                    }
                    else {
                        rInspir = String(relI, 1);
                    }
                    if (currentE == 1) {
                        rEspir = String(int(relE));
                    }
                    else {
                        rEspir = String(relE, 1);
                    }

                    volumeT = String(int(VT));
                    alertPip = String(alerPresionPIP);
                    alertPeep = String(alerPeep);
                    alertDiffPress = String(alerObstruccion);
                    alertConnPat = String(alerDesconexion);
                    alertLeak = String(alerGeneral);
                    alertConnEquipment = String(alerBateria);
                    //alertConnEquipment = String(0);
                    alertValve1Fail = String(0);
                    alertValve2Fail = String(0);
                    alertValve3Fail = String(0);
                    alertValve4Fail = String(0);
                    alertValve5Fail = String(0);
                    alertValve6Fail = String(0);
                    alertValve7Fail = String(0);
                    alertValve8Fail = String(0);
                    valve1Temp = String(25.1);
                    valve2Temp = String(25.2);
                    valve3Temp = String(25.3);
                    valve4Temp = String(25.4);
                    valve5Temp = String(24.1);
                    valve6Temp = String(24.2);
                    valve7Temp = String(24.3);
                    valve8Temp = String(24.4);
                    cameraPress = String(SPin);
                    bagPress = String(SPout);
                    inspFlow = String(SFin);
                    inspExp = String(SFout);
                    lPresSup = String(int(pmax));
                    lPresInf = String(int(pmin));
                    lFlowSup = String(int(flmax));
                    lFlowInf = String(int(flmin));
                    lVoluSup = String(int(vmax));
                    lVoluInf = String(int(vmin));

                    //- Composicion de cadena
                    RaspberryChain = idEqupiment + ',' + patientPress + ',' + patientFlow + ',' + patientVolume + ',' +
                        pressPIP + ',' + pressPEEP + ',' + frequency + ',' + rInspir + ',' + rEspir + ',' + volumeT + ',' +
                        alertPip + ',' + alertPeep + ',' + alertDiffPress + ',' + alertConnPat + ',' + alertLeak + ',' +
                        alertConnEquipment + ',' + alertValve1Fail + ',' + alertValve2Fail + ',' + alertValve3Fail + ',' +
                        alertValve4Fail + ',' + alertValve5Fail + ',' + alertValve6Fail + ',' + alertValve7Fail + ',' +
                        alertValve8Fail + ',' + valve1Temp + ',' + valve2Temp + ',' + valve3Temp + ',' + valve4Temp + ',' +
                        valve5Temp + ',' + valve6Temp + ',' + valve7Temp + ',' + valve8Temp + ',' + cameraPress + ',' + bagPress + ',' +
                        inspFlow + ',' + inspExp;

                    // Envio de la cadena de datos (visualizacion Raspberry)
                    Serial.println(RaspberryChain);

                    /* ********************************************************************
                     * **** ENVIO DE VARIABLES PARA CALIBRACION ***************************
                     * ********************************************************************/
                     //  Serial.print(CalFin);
                     //  Serial.print(",");
                     //  Serial.println(CalFout);  // informacion para calibracion de flujo
                     //  Serial.println(CalPpac);
                     //  Serial.println(CalPin);
                     //  Serial.println(CalPout); // informacion para calibracion de presion
                }
            }
        }
    }
    vTaskDelete(NULL);
}

/* ***************************************************************************
 * **** Rutinas de ciclado definidas para el ventilador **********************
 * ***************************************************************************/
 // Rutina de StandBy
void standbyRoutine() {
    if (newStateMachine != currentStateMachine) {  // si hay un cambio de estado de la maquina
        currentStateMachine = newStateMachine;    // actualiza el estado de funcionamiento de la maquina a StandBy
        contCycling = 0;      // detiene el contador de ciclado
    }

    digitalWrite(EV_INSPIRA, LOW);  //Piloto conectado a presion de bloqueo -> Libera valvula piloteada y permite el paso de aire
    digitalWrite(EV_ESC_CAM, LOW);  //Piloto conectado a Camara -> Despresuriza la camara y permite el llenado de la bolsa
    digitalWrite(EV_ESPIRA, LOW);   //Piloto conectado a PEEP -> Limita la presion de la via aerea a la PEEP configurada
}

// Cycling of the Mechanical Ventilator
void cycling() {
    contCycling++;  // contador que incrementa cada ms en la funcion de ciclado

    if ((newFrecRespiratoria != currentFrecRespiratoria) ||
        (newI != currentI) || (newE != !currentE)) {  // condicion implementada para terminar un ciclado normal cada que se cambie de modo o de parametros ventilatorios
        currentFrecRespiratoria = newFrecRespiratoria;
        currentI = newI;
        currentE = newE;
        // Calculo del tiempo I:E
        if (currentI == 1) {  // calculo de la relacion IE en el modo controlado
            inspirationTime = (float)(60.0 / currentFrecRespiratoria) / (1 + (float)(currentE / 10.0));
            expirationTime = (float)(currentE / 10.0) * inspirationTime;
        }
        else {
            expirationTime = (float)(60.0 / currentFrecRespiratoria) / (1 + (float)(currentI / 10.0));
            inspirationTime = (float)(currentI / 10.0) * expirationTime;
        }
    }

    // Maquina de estados del ciclado
    switch (currentStateMachineCycling) {
    case STOP_CYCLING:
        break;
    case START_CYCLING:
        if (contCycling >= 1) {        // Inicia el ciclado abriendo electrovalvula de entrada y cerrando electrovalvula de salida
            BandInsp = 1;// Activa bandera que indica que empezo la inspiracion
            digitalWrite(EV_INSPIRA, LOW);//Piloto conectado a ambiente -> Desbloquea valvula piloteada y permite el paso de aire
            digitalWrite(EV_ESPIRA, HIGH);//Piloto conectado a PIP -> Limita la presion de la via aerea a la PIP configurada
            digitalWrite(EV_ESC_CAM, HIGH);//Piloto conectado a Presion de activacion -> Presiona la camara
            currentStateMachineCycling = INSPIRATION_CYCLING;
        }
        break;
    case INSPIRATION_CYCLING:
        if (contCycling >= int(inspirationTime * 1000)) {
            //Calculo PIP
            if (Ppico < 0) {// Si el valor de Ppico es negativo
                Ppico = 0;// Lo limita a 0
            }
            Ppico = int(Ppico);

            //Mediciones de presion del sistema
            Pin_max = SPin;//Presion maxima de la camara
            Pout_max = SPout;//Presion maxima de la bolsa

            //Medicion de Volumen circulante
            if (Vtidal >= 0) {
                VTidProm[2] = Vtidal;
            }
            else {
                VTidProm[2] = 0;
            }
            // promediado del Vtidal
            for (int i = 2; i >= 1; i--) {
                VTidProm[2 - i] = VTidProm[2 - i + 1];
            }
            //- Inicializacion
            SVtidal = 0;
            //- Actualizacion
            for (int i = 0; i <= 2; i++) {
                SVtidal = SVtidal + VTidProm[i];
            }
            //- Calculo promedio
            VT = SVtidal / 3;

            //Mediciones de flujo cero
            flowZero = SFin - SFout; // nivel cero de flujo para calculo de volumen
            //Rutina de ciclado
            BandInsp = 0;	// Desactiva la bandera, indicando que empezo la espiracion
            digitalWrite(EV_INSPIRA, HIGH);//Piloto conectado a presion de bloqueo -> Bloquea valvula piloteada y restringe el paso de aire
            digitalWrite(EV_ESC_CAM, LOW);//Piloto conectado a PEEP -> Limita la presion de la via aerea a la PEEP configurada
            digitalWrite(EV_ESPIRA, LOW);//Piloto conectado a ambiente -> Despresuriza la camara y permite el llenado de la bolsa
            currentStateMachineCycling = EXPIRATION_CYCLING;
        }
        break;
    case EXPIRATION_CYCLING:
        //Add para el modo A/C
        if ((newVentilationMode == 1) && (SPpac <= Peep - newTrigger) && (contCycling >= int(inspirationTime) * 1000 + (expirationTime * 100))) {
            frecRespiratoriaCalculada = 60.0 / ((float)contCycling / 1000.0);
            calculatedE = (int)((((60.0 / (float)frecRespiratoriaCalculada) / (float)inspirationTime) - 1) * currentI * 10);
            contCycling = 0;

            //Calculo de Peep
            // Peep = SPpac + newTrigger;// Peep como la presion en la via aerea al final de la espiracion

            if (Peep < 0) {// Si el valor de Peep es negativo
                Peep = 0;// Lo limita a 0
            }
            Peep = int(Peep);
            if (estabilidad) {
                PeepEstable = Peep;
                estabilidad = 0;
            }
            else {
                if (Peep <= PeepEstable - 1.5) {
                    alerPeep = 1;
                }
                else {
                    alerPeep = 0;
                }
            }

            //Ajuste del valor de volumen
            Vtidal = 0;
            flowZero = SFin - SFout; // nivel cero de flujo para calculo de volumen

            //Calculos de volumenes
            //- Asignacion
            Vin_Ins = SUMVin_Ins / 1000;
            Vout_Ins = SUMVout_Ins / 1000;
            Vin_Esp = SUMVin_Esp / 1000;
            Vout_Esp = SUMVout_Esp / 1000;

            //- Reinio de acumuladores
            SUMVin_Ins = 0;
            SUMVout_Ins = 0;
            SUMVin_Esp = 0;
            SUMVout_Esp = 0;

            //Mediciones de presion del sistema
            Pin_min = SPin;  //Presion minima de la camara
            Pout_min = SPout;  //Presion minima de la bolsa

            //Asignacion de valores maximos y minimos de presion
            pmin = UmbralPpmin;  //asigna la presion minima encontrada en todo el periodo
            pmax = UmbralPpico;  //asigna la presion maxima encontrada en todo el periodo
            flmin = UmbralFmin;  //asigna el flujo minimo encontrada en todo el periodo
            flmax = UmbralFmax;  //asigna el flujo maximo encontrada en todo el periodo
            vmin = UmbralVmin;  //asigna el volumen minimo encontrada en todo el periodo
            vmax = UmbralVmax;  //asigna el volumen maximo encontrada en todo el periodo
            Ppico = pmax;
            UmbralPpmin = 100;  //Reinicia el umbral minimo de presion del paciente
            UmbralPpico = -100;  //Reinicia el umbral maximo de presion del paciente
            UmbralFmin = 100;  //Reinicia el umbral minimo de flujo del paciente
            UmbralFmax = -100;  //Reinicia el umbral maximo de flujo del paciente
            UmbralVmin = 100;  //Reinicia el umbral minimo de volumen del paciente
            UmbralVmax = -100;  //Reinicia el umbral maximo de volumen del paciente

            //Metodo de exclusion de alarmas
            if (Ppico > 2 && Peep > 2) {
                flagInicio = false;
            }

            currentVE = (int)((VT * frecRespiratoriaCalculada) / 1000.0);  // calculo de la ventilacion minuto

            alarmsDetection();  // se ejecuta la rutina de deteccion de alarmas
            currentStateMachineCycling = START_CYCLING;

            if (newStateMachine != currentStateMachine) {
                currentStateMachine = newStateMachine;
            }
        }
        if ((contCycling >= int(((inspirationTime + expirationTime) * 1000)))) {
            frecRespiratoriaCalculada = 60.0 / ((float)contCycling / 1000.0);
            calculatedE = (int)((((60.0 / (float)frecRespiratoriaCalculada) / (float)inspirationTime) - 1) * currentI * 10);
            contCycling = 0;
            //Calculo de Peep
            Peep = SPpac;// Peep como la presion en la via aerea al final de la espiracion

            if (Peep < 0) {// Si el valor de Peep es negativo
                Peep = 0;// Lo limita a 0
            }
            Peep = int(Peep);
            if (estabilidad) {
                PeepEstable = Peep;
                estabilidad = 0;
            }
            else {
                if (Peep <= PeepEstable - 1.5) {
                    alerPeep = 1;
                }
                else {
                    alerPeep = 0;
                }
            }
            //Ajuste del valor de volumen
            Vtidal = 0;
            flowZero = SFin - SFout; // nivel cero de flujo para calculo de volumen

            //Calculos de volumenes
            //- Asignacion
            Vin_Ins = SUMVin_Ins / 1000;
            Vout_Ins = SUMVout_Ins / 1000;
            Vin_Esp = SUMVin_Esp / 1000;
            Vout_Esp = SUMVout_Esp / 1000;

            //- Reinio de acumuladores
            SUMVin_Ins = 0;
            SUMVout_Ins = 0;
            SUMVin_Esp = 0;
            SUMVout_Esp = 0;

            //Mediciones de presion del sistema
            Pin_min = SPin;//Presion minima de la camara
            Pout_min = SPout;//Presion minima de la bolsa

            //Asignacion de valores maximos y minimos de presion
            pmin = UmbralPpmin;  //asigna la presion minima encontrada en todo el periodo
            pmax = UmbralPpico;  //asigna la presion maxima encontrada en todo el periodo
            flmin = UmbralFmin;  //asigna el flujo minimo encontrada en todo el periodo
            flmax = UmbralFmax;  //asigna el flujo maximo encontrada en todo el periodo
            vmin = UmbralVmin;  //asigna el volumen minimo encontrada en todo el periodo
            vmax = UmbralVmax;  //asigna el volumen maximo encontrada en todo el periodo
            Ppico = pmax;
            UmbralPpmin = 100;  //Reinicia el umbral minimo de presion del paciente
            UmbralPpico = -100;  //Reinicia el umbral maximo de presion del paciente
            UmbralFmin = 100;  //Reinicia el umbral minimo de flujo del paciente
            UmbralFmax = -100;  //Reinicia el umbral maximo de flujo del paciente
            UmbralVmin = 100;  //Reinicia el umbral minimo de volumen del paciente
            UmbralVmax = -100;  //Reinicia el umbral maximo de volumen del paciente

            //Metodo de exclusion de alarmas
            if (Ppico > 2 && Peep > 2) {
                flagInicio = false;
            }

            currentVE = (int)((VT * frecRespiratoriaCalculada) / 1000.0);  // calculo de la ventilacion minuto

            alarmsDetection();  // se ejecuta la rutina de deteccion de alarmas
            currentStateMachineCycling = START_CYCLING;

            if (newStateMachine != currentStateMachine) {
                currentStateMachine = newStateMachine;
                contCycling = 0;
            }
        }
        break;
    default:
        break;
    }

    //Calculo de Volumenes en tiempo inspiratorio y espiratorio
    if (BandInsp == 1) {//Durante el tiempo inspiratorio
        SUMVin_Ins = SUMVin_Ins + SFin;
        SUMVout_Ins = SUMVout_Ins + SFout;
    }
    else {//Durante el tiempo espiratorio
        SUMVin_Esp = SUMVin_Esp + SFin;
        SUMVout_Esp = SUMVin_Esp + SFout;
    }

    milisecond++;
    if (milisecond == 1000) {
        milisecond = 0;
        second++;
        if (second == 60) {
            second = 0;
        }
    }
} // end cycling()

// CPAP of the Mechanical Ventilator
void cpapRoutine() {
    float frecCalcCPAP = 0;
    // esta funcion se ejecuta cada milisegundo
    if (newStateMachine != currentStateMachine) {
        currentStateMachine = newStateMachine;
    }
    contCycling = 0;
    digitalWrite(EV_INSPIRA, LOW);  //Piloto conectado a presion de bloqueo -> Bloquea valvula piloteada y restringe el paso de aire
    digitalWrite(EV_ESC_CAM, LOW);  //Piloto conectado a PEEP -> Limita la presion de la via aerea a la PEEP configurada
    digitalWrite(EV_ESPIRA, LOW);   //Piloto conectado a ambiente -> Despresuriza la camara y permite el llenado de la bolsa

    if ((SFpac > COMP_FLOW_MAX_CPAP) && ((SFpac - SFant) > COMP_DEL_F_MAX_CPAP) && (stateFrecCPAP != CPAP_INSPIRATION)) { // inicio de la inspiracion
      // Inicializa Maquina de estados para que inicie en CPAP
        stateFrecCPAP = CPAP_INSPIRATION;

        // Calculo de la frecuecnia respiratoria en CPAP
        frecCalcCPAP = 60.0 / ((float)contFrecCPAP / 1000.0);
        frecRespiratoriaCalculada = (int)frecCalcCPAP;

        // Calculo de la relacion IE en CPAP
        if (contInsCPAP < contEspCPAP) {
            calculatedI = 1;
            calculatedE = int(10 * ((float)contEspCPAP / 1000.0) / ((60.0 / (float)frecCalcCPAP) - ((float)contEspCPAP) / 1000.0));
        }
        else if (contEspCPAP < contInsCPAP) {
            calculatedE = 1;
            calculatedI = int(10 * ((float)contInsCPAP / 1000.0) / ((60.0 / (float)frecCalcCPAP) - ((float)contInsCPAP) / 1000.0));
        }

        // limita el valor maximo de frecuencia a 35
        if (frecRespiratoriaCalculada > 35) {
            frecRespiratoriaCalculada = 35;
        }

        // // Calculo de PEEP
        // Peep = SPpac;// Peep como la presion en la via aerea al final de la espiracion

        //Ajuste del valor de volumen
        Vtidal = 0;
        // flowZero = SFin - SFout; // nivel cero de flujo para calculo de volumen
        contFrecCPAP = 0;
        contEspCPAP = 0;
        contInsCPAP = 0;

    }
    if ((SFpac < COMP_FLOW_MIN_CPAP) && ((SFpac - SFant) < COMP_DEL_F_MIN_CPAP) && (stateFrecCPAP != CPAP_ESPIRATION)) {  // si inicia la espiracion
        stateFrecCPAP = CPAP_ESPIRATION;

        // // Calculo de PIP
        // Ppico = SPpac;// PIP como la presion en la via aerea al final de la espiracion

        //Medicion de Volumen circulante
        if (Vtidal >= 0) {
            VTidProm[2] = Vtidal;
        }
        else {
            VTidProm[2] = 0;
        }
        // promediado del Vtidal
        for (int i = 2; i >= 1; i--) {
            VTidProm[2 - i] = VTidProm[2 - i + 1];
        }
        //- Inicializacion
        SVtidal = 0;
        //- Actualizacion
        for (int i = 0; i <= 2; i++) {
            SVtidal = SVtidal + VTidProm[i];
            // frecRespiratoriaCalculada = frecRespiratoriaCalculada + FreqProm[i];
        }
        //- Calculo promedio
        VT = SVtidal / 3;
        // frecRespiratoriaCalculada = (int) (frecRespiratoriaCalculada / 3);
    }

    // Maquina de estados para identificar la Inspiracion y la espiracion
    switch (stateFrecCPAP) {
        // Ciclo Inspiratorio
    case CPAP_INSPIRATION:
        contFrecCPAP++;  // Se incrementan los contadore para el calculo de frecuencia y relacion IE
        contInsCPAP++;
        break;
        // Ciclo Espiratorio
    case CPAP_ESPIRATION:
        contFrecCPAP++;
        contEspCPAP++;
        break;
    default:
        break;
    }

    if (contCycling > apneaTime * 1000) {
        newStateMachine = AC_STATE;
        newVentilationMode = 1; // A/C Ventilation Mode
    }
}

/* ***************************************************************************
 * **** Ejecucion de la rutina de comunicacion por serial ********************
 * ***************************************************************************/
 // Function to receive data from serial communication
void task_Receive(void* pvParameters) {

    // Clean Serial buffers
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.flush();
    Serial2.flush();

    while (1) {
        if (Serial2.available() > 5) {
            // if (Serial.available() > 5) {  // solo para pruebas
            String dataIn = Serial2.readStringUntil(';');
            // String dataIn = Serial.readStringUntil(';');  // solo para pruebas
            // Serial.println(dataIn);
            int contComas = 0;
            for (int i = 0; i < dataIn.length(); i++) {
                if (dataIn[i] == ',') {
                    contComas++;
                }
            }
            String dataIn2[40];
            for (int i = 0; i < contComas + 1; i++) {
                dataIn2[i] = dataIn.substring(0, dataIn.indexOf(','));
                dataIn = dataIn.substring(dataIn.indexOf(',') + 1);
            }
            //cargue los datos aqui
            //para entero
            //contCiclos =dataIn2[0].toInt();
            //para float
            newFrecRespiratoria = dataIn2[0].toInt();
            newI = dataIn2[1].toInt();
            newE = dataIn2[2].toInt();
            maxPresion = dataIn2[3].toInt();
            alerBateria = dataIn2[4].toInt();
            estabilidad = dataIn2[5].toInt();
            newStateMachine = dataIn2[6].toInt();
            newVentilationMode = dataIn2[7].toInt();
            Serial2.flush();
            // Serial.flush();  // solo para pruebas
            /*Serial.println("State = " + String(currentStateMachine));
              Serial.println(String(newFrecRespiratoria) + ',' + String(newI) + ',' +
              String(newE) + ',' + String(maxPresion) + ',' +
              String(alerBateria) + ',' + String(estabilidad) + ',' +
              String(newStateMachine) + ',' + String(newVentilationMode));*/

              // Calculo del tiempo I:E
            if (newI == 1) {
                inspirationTime = (float)(60.0 / (float)newFrecRespiratoria) / (1 + (float)((float)newE / 10.0));
                expirationTime = (float)((float)newE / 10.0) * (float)inspirationTime;
                // Serial.println("IC=I = " + String(newI) + ":" + String(newE) + "-" + String(inspirationTime) + " E = " + String(expirationTime));
            }
            else {
                expirationTime = (float)(60.0 / (float)newFrecRespiratoria) / (1 + (float)((float)newI / 10.0));
                inspirationTime = (float)((float)newI / 10.0) * (float)expirationTime;
                // Serial.println("EC=I = " + String(newI) + ":" + String(newE) + "-" + String(inspirationTime) + " E = " + String(expirationTime));
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// Function to rupdate LCD each 200 ms
void task_sendSerialData(void* arg) {
    while (1) {
        // evaluacion de condicion de desconexion de paciente
        if (flagAlarmPatientDesconnection == true) {
            SPpac = 0;
            SFpac = 0;
            Vtidal = 0;
            Ppico = 0;
            Peep = 0;
            frecRespiratoriaCalculada = 0;
            calculatedE = 0;
            currentVE = 0;
            VT = 0;
        }
        String dataToSend = String(Ppico) + ',' + String(Peep) + ',' + String(int(VT)) + ',' +
            String(alerPresionPIP) + ',' + String(alerDesconexion) + ',' +
            String(alerObstruccion) + ',' + String(alerPeep) + ',' + String(alerGeneral) + ',' +
            String(int(frecRespiratoriaCalculada)) + ',' + String(int(calculatedE)) + ',' +
            String(int(alerFR_Alta)) + ',' + String(int(alerVE_Alto)) + ',' + String(currentVE) + ';';

        // Serial.print(dataToSend);  // solo para pruebas
        Serial2.print(dataToSend);

        vTaskDelay(200 / portTICK_PERIOD_MS); // update each 200 ms
    }
    vTaskDelete(NULL);
}

/* ***************************************************************************
 * **** Rutina de deteccion de alarmas ***************************************
 * ***************************************************************************/
void alarmsDetection() {
    // Ppico Alarm
    if (flagInicio == false) { //Si hay habilitacion de alarmas
      // Alarma por Ppico elevada
        if (Ppico > maxPresion) {
            flagAlarmPpico = true;
            alerPresionPIP = 1;
        }
        else {
            flagAlarmPpico = false;
            alerPresionPIP = 0;
        }
        // Fallo general
        if ((Pin_max) < 6) {
            flagAlarmGeneral = true;
            alerGeneral = 1;
            newStateMachine = 1;
        }
        else {
            flagAlarmGeneral = false;
            alerGeneral = 0;
        }

        // Alarma por desconexion del paciente
        if ((Ppico) < 2) {
            flagAlarmPatientDesconnection = true;
            alerDesconexion = 1;
        }
        else {
            flagAlarmPatientDesconnection = false;
            alerDesconexion = 0;
        }

        // Alarma por obstruccion
        if ((Vout_Ins >= 0.5 * Vin_Ins) && (Peep < 2)) {
            flagAlarmObstruccion = true;
            alerObstruccion = 1;
        }
        else {
            flagAlarmObstruccion = false;
            alerObstruccion = 0;
        }

        SFinMax = SFin;
        SFoutMax = SFout;

        if (frecRespiratoriaCalculada > maxFR) {
            flagAlarmFR_Alta = true;
            alerFR_Alta = 1;
        }
        else {
            flagAlarmFR_Alta = false;
            alerFR_Alta = 0;
        }

        if (currentVE > maxVE) {
            flagAlarmVE_Alto = true;
            alerVE_Alto = 1;
        }
        else {
            flagAlarmVE_Alto = false;
            alerVE_Alto = 0;

        }
    }
}

/* ***************************************************************************
 * **** CONFIGURACION ********************************************************
 * ***************************************************************************/
void setup()
{
    init_MEMORY();
    init_GPIO();
    init_TIMER();
    Serial.begin(115200);
    Serial2.begin(115200); // , SERIAL_8N1, RXD2, TXD2);
    Serial2.setTimeout(10);

    // nvs_flash_init();

    // se crea el semaforo binario
    // xSemaphoreEncoder = xSemaphoreCreateBinary();
    xSemaphoreTimer = xSemaphoreCreateBinary();
    xSemaphoreAdc = xSemaphoreCreateBinary();

    // // creo la tarea task_pulsador
    // xTaskCreatePinnedToCore(task_Encoder, "task_Encoder", 2048, NULL, 4, NULL, taskCoreOne);
    // // xTaskCreatePinnedToCore(task_Encoder_B, "task_Encoder_B", 10000, NULL, 1, NULL, taskCoreZero);

    xTaskCreatePinnedToCore(task_Timer, "task_Timer", 4096, NULL, 5, NULL, taskCoreOne);
    xTaskCreatePinnedToCore(task_Adc, "task_Adc", 2048, NULL, 4, NULL, taskCoreOne);
    // xTaskCreatePinnedToCore(task_Display, "task_Display", 2048, NULL, 3, NULL, taskCoreOne);  // se puede colocar en el core cero
    xTaskCreatePinnedToCore(task_Receive, "task_Receive", 2048, NULL, 1, NULL, taskCoreOne);
    xTaskCreatePinnedToCore(task_sendSerialData, "task_sendSerialData", 2048, NULL, 1, NULL, taskCoreOne);

    // Clean Serial buffers
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // delay(1000);
    // Serial.flush();
    // Serial2.flush();

}

/* ***************************************************************************
 * **** LOOP MAIN_MENU *******************************************************
 * ***************************************************************************/
void loop() {

}

/* ***************************************************************************
 * **** FIN DEL PROGRAMA *****************************************************
 * ***************************************************************************/

