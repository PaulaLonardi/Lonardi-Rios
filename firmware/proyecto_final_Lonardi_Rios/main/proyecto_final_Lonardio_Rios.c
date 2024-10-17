/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @authors Lonardi, Paula (paula.lonardi@ingenieria.uner.edu.ar)
 * Rios, Tomas (tomas.rios@ingenieria.uner.edu.ar)
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/
#define umbral 500
#define PERIODO_SENSADO_US 15000
uint16_t global_contador=0;
uint16_t senial_medida;
uint16_t estado="esperando";
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

    	ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        
		AnalogInputReadSingle(CH1, &senial_medida);

        vTaskNotifyGiveFromISR(ProcesarTask_task_handle, pdFALSE); 

    }
}

static void ProcesarTask(void *pvParameter){

    while(true){
    	ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        switch(estado){
            case "esperando":
            if(senial_medida > umbral){
                estado = "tiempo";
            }
            break;

            case "tiempo":
            vTaskDelay(UMBRAL_TEMPORAL/ portTICK_PERIOD_MS);
            if (senial_medida > umbral){
                estado= "cebar";
            }
            else{
                estado="esperando";
            }
            break;

            case  "cebar":
            apertura=true;
            vTaskDelay(TIEMPO_DE_CEBADO/ portTICK_PERIOD_MS);
            apertura=false;
            estado="esperando";

        }
    }
}

static void ValveControlTask(void *pvParameter){
    while (true)
    {
        if(apertura)
        {   global_contador = global_contador+1;

            if(global_contador>=100){
                GPIOOff;
                global_contador=0;
            }

            else{
                if(senial_medida>umbral){
                    OpenValve();
                }

                else{
                    CloseValve();
                    global_contador=0;
                }
            }

        }
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
	
	gpioConf_t pin_relay = {GPIO_01, GPIO_OUTPUT};

	analog_input_config_t analog = {
		.input = CH1, //le paso el canal
		.mode = ADC_SINGLE,//el modo en el que va a operar
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};

	AnalogInputInit(&analog);
	AnalogOutputInit();


	/* Inicialización de timers */
    timer_config_t FuncTimerA = {
        .timer = TIMER_A,
        .period = PERIODO_SENSADO_US,
        .func_p = FuncTimerA,
        .param_p = NULL
    };

    TimerInit(&FuncTimerA);


	xTaskCreate(&ValveControlTask, "control de valvula",2048, NULL,5,&ValveControlTask_task_handle);
	xTaskCreate(&SensarTask, "tarea de sensado",2048, NULL,5,&SensarTask_task_handle);
	xTaskCreate(&ProcesarTask, "procesamiento de la info",2048, NULL,5,&ProcesarTask_task_handle);

	TimerStart(FuncTimerA.timer);
	
}
/*==================[end of file]============================================*/