//This is a test code.
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

// The GPIO pin that is connected to D4 pin on the mInI D1 - the Built-in LED.
const int led_gpio = 2;
// The GPIO pin that is connected to D0 pin on the mInI D1 - the relay.
const int relay_gpio = 16;
// The GPIO pin that is oconnected to the button on the Sonoff Basic.
const int button_gpio = 0;

// Global variables
int irr_active = 0;         // Active is 0 or 1 [inactive, active]
int irr_program_mode = 0;   // Program mode is 0, 1 or 2 [no program scheduled, program scheduled, manual mode]
int irr_in_use = 0;         // In use status is 1 or 0 [in use, not in use]
int irr_r_duration = 0;     // Remaining duration is from 0 to 3600 in 1 s steps

void relay_write(bool on) {
    gpio_write(relay_gpio, on ? 1 : 0);
}

void led_write(bool on) {
  gpio_write(led_gpio, on ? 0 : 1);
}

void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    led_write(irr_active);
    relay_write(irr_active);
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

homekit_server_config_t config = {
  .accessories = accessories,
  .password = "111-11-111"     //change it to be valid
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
  gpio_init();
}
