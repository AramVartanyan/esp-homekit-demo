/*
 * Temperature and Humidity sensor (device)
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include <dht/dht.h>

#define TEMPERATURE_SENSOR_PIN 5

#include "adv_button.h"


#ifndef BUTTON_GPIO
#define BUTTON_GPIO 0
#endif

#define LED_GPIO    2


void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void reset_configuration_task() {
    //Flash the LED before the reset
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("Resetting Wifi Config\n");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Restarting\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration(const uint8_t gpio) {
    printf("Resetting device configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

void temperature_sensor_task(void *_args) {
    gpio_set_pullup(TEMPERATURE_SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, TEMPERATURE_SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    xTaskCreate(temperature_sensor_task, "Temperatore Sensor", 256, NULL, 2, NULL);
}

void device_identify_task(void *_args) {
    // Identify the device by Flashing it's LED.
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    led_write(false);
    vTaskDelete(NULL);
}

void device_identify(homekit_value_t _value) {
    printf("Sensor identify\n");
    xTaskCreate(device_identify_task, "Sensor identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sensor");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001A1AVBG07P"),
            HOMEKIT_CHARACTERISTIC(MODEL, "TH-D1"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.4"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, device_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
            &humidity,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "320-11-129"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = snprintf(NULL, 0, "Sensor-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Sensor-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();
    wifi_config_init("Sensor", NULL, on_wifi_ready);
    gpio_init();
    adv_button_create(BUTTON_GPIO);
    adv_button_register_callback_fn(BUTTON_GPIO, reset_configuration, 5);
}
