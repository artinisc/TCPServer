#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <dht.h>
#include "esp_event.h"
#include <u8g2_esp32_hal.h>
#include <ultrasonic.h>
#include <string.h>
#include <sys/param.h>
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/event_groups.h"
#include "esp_netif.h"

//TCP
#define PORT CONFIG_EXAMPLE_PORT
//dht
#define DHT_GPIO 16
//oled
#define PIN_SDA 5
#define PIN_SCL 4
//ultrasonico
#define MAX_DISTANCE_CM 500 // 5m max
#define TRIGGER_GPIO 0
#define ECHO_GPIO 2

//variaveis globais
static const char *TAG = "Monitoramento: ";
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = DHT_GPIO;
u8g2_t u8g2;
QueueHandle_t filaTela;
QueueHandle_t filaTcp;

//função temperatura e umidade
void task_dht(void *pvParamters){
    int16_t temperatura = 0;
    int16_t umidade = 0;

    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
    while(1){

        if(dht_read_data(sensor_type, dht_gpio, &umidade, &temperatura) == ESP_OK){
            umidade = umidade / 10;
            temperatura = temperatura / 10;
            //ESP_LOGI(TAG, "Umidade %d%% e Temperatura %d ºC\n", umidade, temperatura);
        }else{
            umidade = 00000;
            temperatura = 00000;
            ESP_LOGE(TAG, "Não foi possivel ler o sensor DHT.\n");
        }
        
        if(temperatura >= 100){
            temperatura = 100;
        }

        xQueueSend(filaTcp,&umidade,pdMS_TO_TICKS(0));
        xQueueSend(filaTcp,&temperatura,pdMS_TO_TICKS(0));
        xQueueSend(filaTela,&temperatura,pdMS_TO_TICKS(0));

        vTaskDelay(3000/portTICK_PERIOD_MS);
    }
}

//função distancia
void task_ultra(void *pvParamters){
    uint32_t distancia = 0;

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };

    ultrasonic_init(&sensor);

    while(1){

        if(ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distancia) == ESP_OK){
            //ESP_LOGI(TAG, "Distancia: %d cm\n", distancia);
        }else{
            distancia = 00000;
            ESP_LOGE(TAG, "Não foi possivel ler o sensor Ultrasonico.\n");
        }

        if(distancia >= 500){
            distancia = 500;
        }

        xQueueSend(filaTcp,&distancia,pdMS_TO_TICKS(0));
        xQueueSend(filaTela,&distancia,pdMS_TO_TICKS(0));

        vTaskDelay(3000/portTICK_PERIOD_MS);
    }
}

void task_oLED(void *pvParameters){

    // initialize the u8g2 hal
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = PIN_SDA;
    u8g2_esp32_hal.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // initialize the u8g2 library
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);
    
    // set the display address
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    
    // initialize the display
    u8g2_InitDisplay(&u8g2);
    
    // wake up the display
    u8g2_SetPowerSave(&u8g2, 0);

    uint16_t temp = 0;
    uint32_t dist = 0;
    char stringTemperatura[10];
    char stringDistancia[10];

    while(1){

        xQueueReceive(filaTela,&dist,pdMS_TO_TICKS(2000));
        xQueueReceive(filaTela,&temp,pdMS_TO_TICKS(2000));

        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawFrame(&u8g2, 0, 0, 128, 64);
        u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
        u8g2_DrawUTF8(&u8g2, 3,15,"Monitoramento");
        u8g2_SendBuffer(&u8g2);
        
        u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
        u8g2_DrawUTF8(&u8g2, 3,45,"D");
        u8g2_DrawFrame(&u8g2, 13, 35, 70, 12);
        u8g2_DrawBox(&u8g2, 13 + 2, 35 + 2, dist / 7.3,  12 - 4);
        sprintf(stringDistancia, "%d", dist);
        u8g2_DrawUTF8(&u8g2, 87,45,stringDistancia);
        u8g2_DrawUTF8(&u8g2, 107,45,"cm");
        
        
        u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
        u8g2_DrawUTF8(&u8g2, 3,30,"T");
        u8g2_DrawFrame(&u8g2, 13, 20, 70, 12);
        u8g2_DrawBox(&u8g2, 13 + 2, 20 + 2, temp / 1.5,  12 - 4);
        sprintf(stringTemperatura, "%d", temp);
        u8g2_DrawUTF8(&u8g2, 87,30,stringTemperatura);
        u8g2_DrawUTF8(&u8g2, 107,30,"C");

        u8g2_DrawUTF8(&u8g2, 3, 60, "Limite: 100C - 500cm");

        u8g2_SendBuffer(&u8g2);

        vTaskDelay(2000/portTICK_PERIOD_MS);
    }

}

//função manda msg ao cliente tcp
static void do_retransmit(const int sock){
    int len;
    char rx_buffer[128];

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

            uint16_t temp = 0;
            uint16_t umid = 0;
            uint32_t dist = 0;

            xQueueReceive(filaTcp,&dist,pdMS_TO_TICKS(2000));
            xQueueReceive(filaTcp,&umid,pdMS_TO_TICKS(2000));
            xQueueReceive(filaTcp,&temp,pdMS_TO_TICKS(2000));

            ESP_LOGI(TAG, "Temperatura: %d ºC\n",temp);
            ESP_LOGI(TAG, "Umidade: %d %%\n",umid);
            ESP_LOGI(TAG, "Distancia: %d cm\n", dist);

            char retorno[30];

            if(rx_buffer[0] == 't'){
                sprintf(retorno, "\n\rTemperatura: %d C", temp);
                write(sock , retorno , strlen(retorno));
            }else if(rx_buffer[0] == 'u'){
                sprintf(retorno, "\n\rUmidade: %d  %%", umid);
                write(sock , retorno , strlen(retorno));
            }else if(rx_buffer[0] == 'd'){
                sprintf(retorno, "\n\rDistancia: %d cm", dist);
                write(sock , retorno , strlen(retorno));
            }else{
                sprintf(retorno, "\n\rComando Invalido!");
                write(sock , retorno , strlen(retorno));
            }

        }
    } while (len > 0);

}

//função cria tcp server
static void tcp_server_task(void *pvParameters){

    char addr_str[128];
    int addr_family;
    int ip_protocol;

#ifdef CONFIG_EXAMPLE_IPV4
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
    struct sockaddr_in6 dest_addr;
    bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
    dest_addr.sin6_family = AF_INET6;
    dest_addr.sin6_port = htons(PORT);
    addr_family = AF_INET6;
    ip_protocol = IPPROTO_IPV6;
    inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.sin6_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        } else if (source_addr.sin6_family == PF_INET6) {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        char pedeT[30];
        char pedeU[30];
        char pedeD[30];

        sprintf(pedeT, "t para Temperatura\n\r");
        sprintf(pedeU, "u para Umidade\n\r");
        sprintf(pedeD, "d Para Distancia\n\r");

        write(sock , pedeT , strlen(pedeT));
        write(sock , pedeU , strlen(pedeU));
        write(sock , pedeD , strlen(pedeD));

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);

    }

    CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void app_main() {

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    filaTela = xQueueCreate(2,sizeof(uint32_t));
    filaTcp = xQueueCreate(3,sizeof(uint32_t));
    ESP_LOGI(TAG, "Inicio...");
    xTaskCreate(task_dht, "task_dht",2048,NULL,5,NULL);
    xTaskCreate(task_ultra, "task_ultra",2048,NULL,5,NULL);
    xTaskCreate(task_oLED, "task_oLED",2048,NULL,1,NULL);
    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 1, NULL);
    
}

