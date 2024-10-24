/*! @mainpage ejemplo Bluetooth LED-RGB
 *
 * @section genDesc General Description
 *
 * Este proyecto ejemplifica el uso del módulo de comunicación Bluetooth Low Energy (BLE) 
 * junto con el manejo de tiras de LEDs RGB. 
 * Permite manejar la tonalidad e intensidad del LED RGB incluído en la placa ESP-EDU, 
 * mediante una aplicación móvil.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 02/04/2024 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
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

/*==================[macros and definitions]=================================*/
//#define umbral 500
#define PERIODO_SENSADO_US 15000
#define UMBRAL_TEMPORAL 1000
#define TIEMPO_CEBADO 3000
uint16_t global_contador = 0;
uint16_t senial_medida;
uint16_t umbral = 2400;

//uint16_t estado = "esperando";
enum{esperando, tiempo,cebar};
bool apertura = false;
uint8_t estado = esperando;

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
static void SensarTask(void *pvParameter){

    while(true){
        //LedOn(LED_1);
        //LedOn(LED_2);
        //LedOn(LED_3);

    	AnalogInputReadSingle(CH3, &senial_medida);

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 	

        vTaskNotifyGiveFromISR(ProcesarTask_task_handle, pdFALSE); 

    }
}

static void ProcesarTask(void *pvParameter){
//    uint8_t estado = esperando;
    while(true){
    	ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        switch(estado){
            case esperando:
                LedOff(LED_2);
                LedOff(LED_3);
                LedOn(LED_1);
                if(senial_medida > umbral){
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
                if (senial_medida < umbral){
                    estado = esperando;
                }
                
            }
            break;

            case cebar:
            LedOn(LED_3);
            LedOff(LED_2);
            LedOff(LED_1);
            apertura = true;
            vTaskDelay(TIEMPO_CEBADO/ portTICK_PERIOD_MS);
            apertura = false;
            estado = esperando;

        }
    }
}

static void ValveControlTask(void *pvParameter){
    while (true)
    {
        if(apertura)
        {   global_contador = global_contador+1;

            if(global_contador >= 3){
                GPIOOff(GPIO_1);
                global_contador = 0;
            }

            else{
                if(senial_medida > umbral){
                GPIOOn(GPIO_1);
                }

                else{
                    GPIOOff(GPIO_1);
                    global_contador = 0;
                }
            }

        }
        else{
            GPIOOff(GPIO_1);
            global_contador = 0;
        }
        //global_contador = 0;
        vTaskDelay(500/ portTICK_PERIOD_MS);

    }
    
}

void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR(SensarTask_task_handle, pdFALSE); 
}
//void FuncTimerB(void* param){
//    vTaskNotifyGiveFromISR(ValveControlTask_task_handle, pdFALSE); 
//}

/*==================[external functions definition]==========================*/
void app_main(void){
    
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


	xTaskCreate(&ValveControlTask, "control de valvula",2048, NULL,5,&ValveControlTask_task_handle);
	xTaskCreate(&SensarTask, "tarea de sensado",2048, NULL,5,&SensarTask_task_handle);
	xTaskCreate(&ProcesarTask, "procesamiento de la info",2048, NULL,5,&ProcesarTask_task_handle);

	TimerStart(timer_sensar.timer);
	
}
/*==================[end of file]============================================*/


