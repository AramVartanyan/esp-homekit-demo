/*
 * Switch + Motion sensor HC-SR501 (device Sonoff redesign)
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

#include "adv_button.h"

#define MOTION_SENSOR_GPIO 14
#define RELAY_GPIO 12
#define LED_GPIO 2
#ifndef BUTTON_GPIO
#define BUTTON_GPIO     0
#endif

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

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

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));

homekit_characteristic_t motion_detected  = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);

void motion_sensor_callback(uint8_t gpio) {
  if (gpio == MOTION_SENSOR_GPIO){
    motion_detected.value.bool_value = gpio_read(MOTION_SENSOR_GPIO);
    homekit_characteristic_notify(&motion_detected, motion_detected.value);
    printf("Motion Detected on %d\n", gpio);
    for (int i=0; i<2; i++) {
        led_write(true);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
  }
  else {
    printf("Interrupt on %d", gpio);
  }
}

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    gpio_enable(MOTION_SENSOR_GPIO, GPIO_INPUT);
    gpio_set_interrupt(MOTION_SENSOR_GPIO, GPIO_INTTYPE_EDGE_ANY, motion_sensor_callback);
    led_write(false);
    relay_write(false);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    led_write(switch_on.value.bool_value);
    relay_write(switch_on.value.bool_value);
}

void toggle_switch(const uint8_t gpio) {
    printf("RC >>> Toggle Switch manual\n");
    switch_on.value.bool_value = !switch_on.value.bool_value;
    led_write(switch_on.value.bool_value);
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
}

void switch_identify_task(void *_args) {
    // Device identification
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

void switch_identify(homekit_value_t _value) {
    printf("Switch identify\n");
    xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "007A1AVBG02P"),
            HOMEKIT_CHARACTERISTIC(MODEL, "S-MS"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.3"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch"),
            &switch_on,
            NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=false, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
            &motion_detected,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "320-11-122"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Switch-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Switch-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();
    wifi_config_init("Switch", NULL, on_wifi_ready);
    gpio_init();
    adv_button_create(BUTTON_GPIO);
    adv_button_register_callback_fn(BUTTON_GPIO, toggle_switch, 1);
    adv_button_register_callback_fn(BUTTON_GPIO, reset_configuration, 5);
}
