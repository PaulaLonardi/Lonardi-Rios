/*! @mainpage Proyecto final: MateSeguro
 *
 * @section genDesc Descripción General
 *
 * Proyecto de Paula Lonardi y Tomás Ríos. 
 * El sistema MateSeguro permite monitorear y controlar condiciones de apertura en un contenedor mediante un LED RGB, 
 * un buzzer, y un sistema de comunicación BLE para configuraciones y alertas.
 *
 * @section changelog Historial de Cambios
 *
 * |   Fecha      | Descripción                                  |
 * |:------------:|:--------------------------------------------:|
 * | 02/04/2024   | Creación de la documentación                 |
 *
 * @authors
 * - Paula Lonardi: paula.lonardi@ingenieria.uner.edu.ar
 * - Tomás Ríos: tomas.rios@ingenieria.uner.edu.ar
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "timer_mcu.h"
#include "analog_io_mcu.h"

#include "led.h"
#include "neopixel_stripe.h"
#include "ble_mcu.h"

#include "gpio_mcu.h"
#include "led.h"

#include "buzzer.h"
#include "buzzer_melodies.h"

/*==================[macros and definitions]=================================*/
#define PERIODO_SENSADO_US 15000
#define UMBRAL_TEMPORAL 1000
#define TIEMPO_CEBADO 3000
uint16_t senial_medida;
uint16_t umbral = 2000;

//uint16_t estado = "esperando";
enum{esperando, tiempo,cebar, alerta};
bool apertura = false;
uint8_t estado = esperando;
bool lock = false;  
/*==================[internal data definition]===============================*/
TaskHandle_t SensarTask_task_handle = NULL;
TaskHandle_t ValveControlTask_task_handle = NULL;
TaskHandle_t ProcesarTask_task_handle = NULL;

typedef struct
	{
		gpio_t pin;			/*!< GPIO pin number */
		io_t dir;			/*!< GPIO direction '0' IN;  '1' OUT*/
	} gpioConf_t;

/*==================[internal functions declaration]=========================*/

volatile uint8_t config_tiempo_cebado = 0;
/*! @brief Función para recibir y procesar datos de la app bluetooth
 *
 *  Procesa el tiempo de cebado recibido desde la aplicación móvil y envía un mensaje 
 *  de retroalimentación con el valor configurado.
 *
 *  @param[in] data Puntero a los datos recibidos
 *  @param[in] length Longitud de los datos recibidos
 */
void read_data(uint8_t * data, uint8_t length){
	uint8_t i = 1;
    
	char msg[55];

	if(data[0] == 'R'){
        /* El slidebar Rojo envía los datos con el formato "R" + value + "A" */
		config_tiempo_cebado = 0;
		while(data[i] != 'A'){
            /* Convertir el valor ASCII a un valor entero */
			config_tiempo_cebado = config_tiempo_cebado * 10;
			config_tiempo_cebado = config_tiempo_cebado + (data[i] - '0');
			i++;
		}
	}
    /* Se envía una realimentación de los valores actuales de brillo del LED */
    sprintf(msg, "Tiempo de cebado: %d\n", config_tiempo_cebado);
    BleSendString(msg);
}

/*! @brief Tarea de sensado de señal
 *
 *  Lee la señal analógica y notifica a la tarea de procesamiento.
 *
 *  @param[in] pvParameter (void)
 */
static void SensarTask(void *pvParameter){

    while(true){
    	AnalogInputReadSingle(CH3, &senial_medida);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 	
        vTaskNotifyGiveFromISR(ProcesarTask_task_handle, pdFALSE); 

    }
}
/*! @brief Tarea de procesamiento de la señal medida
 *
 *  Procesa la señal y cambia el estado del sistema en función de la condición de apertura.
 * Tiene una máquina de estado que se encarga de esto
 *
 *  @param[in] pvParameter Parámetros de la tarea
 */
static void ProcesarTask(void *pvParameter){
    uint16_t global_contador = 0;
    
    while(true){
    	ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        switch(estado){
            case esperando:
                LedOff(LED_2);
                LedOff(LED_3);
                LedOn(LED_1);

                if(senial_medida > umbral ){
                estado = tiempo;
                }

            break;

            case tiempo:
                LedOn(LED_2);
                LedOff(LED_1);
                LedOff(LED_3);
                vTaskDelay(UMBRAL_TEMPORAL/ portTICK_PERIOD_MS);
                if (senial_medida > umbral){
                    estado = cebar;
                }
                else{
                
                        estado = esperando;    
                }
            break;

            case cebar:
                LedOn(LED_3);
                LedOff(LED_2);
                LedOff(LED_1);

                //Verifico que se haya configurado de forma externa el tiempo de cebado
                if (config_tiempo_cebado > 0 ){
                    //Alarma de apertura
                    BuzzerPlayTone(1840, 150);
                    BuzzerPlayTone(1940, 150);
                    BuzzerPlayTone(2040, 150);
                    GPIOOn(GPIO_1);
                }
                global_contador++;

                if (global_contador >= 2) {
                        estado = alerta; 
                    } else {
                        if (config_tiempo_cebado > 0 ){
                        vTaskDelay((config_tiempo_cebado*1000) / portTICK_PERIOD_MS);
                        //apertura = false;
                        GPIOOff(GPIO_1);
                        
                        //alarma de cierre
                        BuzzerPlayTone(2040, 150);
                        BuzzerPlayTone(1940, 150);
                        BuzzerPlayTone(1840, 150);

                        estado = esperando;
                        }
                        else {
                        vTaskDelay(TIEMPO_CEBADO / portTICK_PERIOD_MS);
                        //apertura = false;
                        estado = esperando;
                        }
                    }
                break;
            case alerta:
                LedOn(LED_1);
                LedOn(LED_2);
                LedOn(LED_3);
                 // Cierra la válvula como medida de seguridad
                GPIOOff(GPIO_1);

                //alarma de alerta
                BuzzerPlayTone(1440, 150);
                BuzzerPlayTone(1340, 150);
                BuzzerPlayTone(1240, 150);
            
               

                vTaskDelay(1000 / portTICK_PERIOD_MS);  
                if(senial_medida < umbral ){
                estado = esperando;
                global_contador = 0;
                }
            break;
        }
    }
}

/*! @brief Función de timer para activar SensarTask
 *
 *  Activa la tarea de sensado cada periodo de tiempo definido.
 *
 *  @param[in] param Parámetros de la función
 */
void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR(SensarTask_task_handle, pdFALSE); 
}

/*==================[external functions definition]==========================*/
/*! @brief Función principal del programa
 *
 *  Inicializa GPIOs, LED, Buzzer, BLE y crea las tareas necesarias para el funcionamiento.
 *  Inicia el timer a
 */
void app_main(void){

    GPIOInit(GPIO_2,GPIO_OUTPUT);
    BuzzerInit(GPIO_2);

    ble_config_t ble_configuration = {
        "MateSeguro",
        read_data
    };
    BleInit(&ble_configuration);
	//gpioConf_t pin_relay = {GPIO_1, GPIO_OUPUT};
    GPIOInit(GPIO_1,GPIO_OUTPUT);
    LedsInit();

	analog_input_config_t analog = {
		.input = CH3, //le paso el canal
		.mode = ADC_SINGLE,//el modo en el que va a operar
		//.func_p = NULL,
		//.param_p = NULL,
		//.sample_frec = 0
	};

	AnalogInputInit(&analog);
	AnalogOutputInit();


	/* Inicialización de timers */
    timer_config_t timer_sensar = {
        .timer = TIMER_A,
        .period = PERIODO_SENSADO_US,
        .func_p = FuncTimerA,
        .param_p = NULL
    };

    TimerInit(&timer_sensar);


	//xTaskCreate(&ValveControlTask, "control de valvula",2048, NULL,5,&ValveControlTask_task_handle);
	xTaskCreate(&SensarTask, "tarea de sensado",2048, NULL,5,&SensarTask_task_handle);
	xTaskCreate(&ProcesarTask, "procesamiento de la info",2048, NULL,5,&ProcesarTask_task_handle);

	TimerStart(timer_sensar.timer);
	
}
/*==================[end of file]============================================*/


