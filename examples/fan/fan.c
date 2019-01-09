//Driving Fan + Off Timer

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_system.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "adv_button.h"

#ifndef BUTTON_GPIO
#define BUTTON_GPIO 0
#endif

#define RELAY_GPIO  16
#define LED_GPIO    2

#define DURATION 7200

// The GPIO pin that is connected to D0 pin on the mInI D1 - the relay.
// The GPIO pin that is connected to D4 pin on the mInI D1 - the Built-in LED.
// The GPIO pin that is connected to D3 pin on the mInI D1 - an external button.

// Time duration 3600s = 1 hour
uint32_t rem_duration = 0;

ETSTimer v_timer;

void fan_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void v_off();

void reset_configuration_task() {
    //Flash the LED first before we start the reset
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

void reset_configuration() {
    printf("Resetting mInI configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

homekit_characteristic_t fan_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(fan_on_callback)
);

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    led_write(false);
    relay_write(false);
}

void fan_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    led_write(fan_on.value.bool_value);
    relay_write(fan_on.value.bool_value);
    if (fan_on.value.bool_value == 1) {
      led_write(true);
      relay_write(true);
      printf(">>> Valve ON\n");
      rem_duration = DURATION;
      sdk_os_timer_setfn(&v_timer, v_off, NULL);
      sdk_os_timer_arm(&v_timer, 1000, 1);
    } else {
      sdk_os_timer_disarm(&v_timer);
      led_write(false);
      relay_write(false);
    }
  }

  void v_off() {
    rem_duration--;
    if (rem_duration == 0) {
      sdk_os_timer_disarm(&v_timer);
      led_write(false);
      relay_write(false);
      fan_on.value.bool_value = 0;
      homekit_characteristic_notify(&fan_on, fan_on.value);
    }
  }

  void toggle_switch(const uint8_t gpio) {
      printf("Toggle Switch manual\n");
      fan_on.value.bool_value = !fan_on.value.bool_value;
      led_write(fan_on.value.bool_value);
      relay_write(fan_on.value.bool_value);
      homekit_characteristic_notify(&fan_on, fan_on.value);
  }

void fan_identify_task(void *_args) {
    // We identify the mInI by Flashing it's LED.
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

void fan_identify(homekit_value_t _value) {
    printf("Fan identify\n");
    xTaskCreate(fan_identify_task, "Fan identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Fan");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_fan, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "002A1AVBG05P"),
            HOMEKIT_CHARACTERISTIC(MODEL, "FD1-t"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.4"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, fan_identify),
            NULL
        }),
        HOMEKIT_SERVICE(FAN, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Fan"),
            &fan_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "320-10-125"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Fan-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Fan-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    create_accessory_name();

    wifi_config_init("Fan", NULL, on_wifi_ready);
    gpio_init();
    adv_button_create(BUTTON_GPIO);
    adv_button_register_callback_fn(BUTTON_GPIO, toggle_switch, 1);
    adv_button_register_callback_fn(BUTTON_GPIO, reset_configuration, 5);
}
