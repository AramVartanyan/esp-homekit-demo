
#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "button.h"

// The GPIO pin that is connected to D4 pin on the mInI D1 - the Built-in LED.
const int led_gpio = 2;
// The GPIO pin that is connected to D0 pin on the mInI D1 - the relay.
const int relay_gpio = 16;
// The GPIO pin that is oconnected to the button on the Sonoff Basic.
const int button_gpio = 0;

// Global variables
int irr_active = 0;         // Active is 1 or 0
int irr_program_mode = 0;   // Program mode is 0, 1 or 2
int irr_in_use = 0;         // In use status is 1 or 0
int irr_r_duration = 0;     // Remaining duration is from 0 to 3600 in 1 s steps

void relay_write(bool on) {
    gpio_write(relay_gpio, on ? 1 : 0);
}

void led_write(bool on) {
  gpio_write(led_gpio, on ? 0 : 1);
}

void irr_identify_task(void *_args) {
  //Identify the mInI by Flashing it's LED.
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

void irr_identify(homekit_value_t _value) {
  printf("Irrigating Identify\n");
  xTaskCreate(irr_identify_task, "Irrigation identify", 128, NULL, 2, NULL);
}

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
    printf("Resetting Irrigation configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

//void irr_active_callback(homekit_characteristic_t *_ch, homekit_value_t active, void *context);
//void irr_program_mode_callback(homekit_characteristic_t *_ch, homekit_value_t program_mode, void *context);
//void irr_in_use_callback(homekit_characteristic_t *_ch, homekit_value_t in_use, void *context);
//void irr_r_duration_callback(homekit_characteristic_t *_ch, homekit_value_t irr_r_duration, void *context);

homekit_value_t irr_active_get() {
  return HOMEKIT_FLOAT(irr_active);
}

void irr_active_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    //printf("Invalid Active value format: %d\n", value.format);
    return;
  }
  irr_active = value.int_value;
}

homekit_value_t irr_program_mode_get() {
  return HOMEKIT_FLOAT(irr_program_mode);
}

void irr_program_mode_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    // printf("Invalid Program mode value format: %d\n", value.format);
    return;
  }
  irr_program_mode = value.int_value;
}

homekit_value_t irr_in_use_get() {
  return HOMEKIT_FLOAT(irr_in_use);
}

void irr_in_use_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    // printf("Invalid In use value format: %d\n", value.format);
    return;
  }
  irr_in_use = value.int_value;
}

homekit_value_t irr_r_duration_get() {
  return HOMEKIT_FLOAT(irr_r_duration);
}

void irr_r_duration_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    // printf("Invalid Remaining duration value format: %d\n", value.format);
    return;
  }
  irr_r_duration = value.int_value;
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Irrigation System");

//homekit_accessory_category_sprinkler = 28 - old version

homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_irrigation_system, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      &name,
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001A1AVBG04P"),
      HOMEKIT_CHARACTERISTIC(MODEL, "Garden Irrigating System"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, irr_identify),
      NULL
    }),
    HOMEKIT_SERVICE(IRRIGATION_SYSTEM, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Irrigating System"),
      HOMEKIT_CHARACTERISTIC(
        ACTIVE, 0,
        .getter = irr_active_get,
        .setter = irr_active_set
      ),
      HOMEKIT_CHARACTERISTIC(
        PROGRAM_MODE, 0,
        .getter = irr_program_mode_get,
        .setter = irr_program_mode_set
      ),
      HOMEKIT_CHARACTERISTIC(
        IN_USE, 0,
        .getter = irr_in_use_get,
        .setter = irr_in_use_set
      ),
      HOMEKIT_CHARACTERISTIC(
        REMAINING_DURATION, 3600,
        .getter = irr_r_duration_get,
        .setter = irr_r_duration_set
      ),
      NULL
    }),
    NULL
  }),
  NULL
};



homekit_server_config_t config = {
  .accessories = accessories,
  .password = "320-10-124"     //changed to be valid
};

void on_wifi_ready() {
  homekit_server_init(&config);
}

void create_accessory_name() {
  uint8_t macaddr[6];
  sdk_wifi_get_macaddr(STATION_IF, macaddr);
  int name_len = snprintf(NULL, 0, "Irrigating System%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
  char *name_value = malloc(name_len + 1);
  snprintf(name_value, name_len + 1, "Irrigating System%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
  name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
  uart_set_baud(0, 115200);
  create_accessory_name();

  wifi_config_init("Irrigating System", NULL, on_wifi_ready);
  //Място за инициализиращите подпрограми
}
