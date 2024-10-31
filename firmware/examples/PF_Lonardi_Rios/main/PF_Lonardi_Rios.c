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

#include "buzzer.h"
#include "buzzer_melodies.h"

/*==================[macros and definitions]=================================*/
//#define umbral 500
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

//cosas de la app bluetooth
volatile uint8_t config_tiempo_cebado = 0;

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


static void SensarTask(void *pvParameter){

    while(true){
    	AnalogInputReadSingle(CH3, &senial_medida);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 	
        vTaskNotifyGiveFromISR(ProcesarTask_task_handle, pdFALSE); 

    }
}

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
                //BuzzerPlayRtttl(songSimpsons);
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


void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR(SensarTask_task_handle, pdFALSE); 
}
//void FuncTimerB(void* param){
//    vTaskNotifyGiveFromISR(ValveControlTask_task_handle, pdFALSE); 
//}

/*==================[external functions definition]==========================*/
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


