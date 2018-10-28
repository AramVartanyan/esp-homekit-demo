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

// Valve type is 0, 1, 2 or 3 [generic, irrigation, shower head, water faucet]
#define V_TYPE 1

// The GPIO pin that is connected to D4 pin on the mInI D1 - the Built-in LED.
const int led_gpio = 2;
// The GPIO pin that is connected to D0 pin on the mInI D1 - the relay.
const int relay_gpio = 16;

// Active is 1 or 0 [active, inactive]
uint8_t vactive = 0;
// In use status is 1 or 0 [in use, not in use]
uint8_t vin_use = 0;
// Time duration is from 0 to 3600 in 1 s steps
uint32_t vset_duration = 900;
uint32_t vremaining_duration = 0;

ETSTimer v_timer;

void relay_write(bool on) {
    gpio_write(relay_gpio, on ? 1 : 0);
}

void led_write(bool on) {
  gpio_write(led_gpio, on ? 0 : 1);
}

homekit_value_t v_active_get();
void v_active_set();
homekit_value_t v_in_use_get();
void v_off();
homekit_value_t v_set_duration_get();
void v_set_duration_set();
homekit_value_t v_rem_duration_get();

void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    led_write(false);
    relay_write(false);
}

void v_identify_task(void *_args) {
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

void v_identify(homekit_value_t _value) {
  printf("Irrigating Identify\n");
  xTaskCreate(v_identify_task, "Irrigation identify", 128, NULL, 2, NULL);
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

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Irrigation System");
homekit_characteristic_t valve_type = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, V_TYPE);
homekit_characteristic_t active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0, .getter=v_active_get, .setter=v_active_set);
homekit_characteristic_t in_use = HOMEKIT_CHARACTERISTIC_(IN_USE, 0, .getter=v_in_use_get);
homekit_characteristic_t set_duration = HOMEKIT_CHARACTERISTIC_(SET_DURATION, 900, .getter=v_set_duration_get, .setter=v_set_duration_set);
homekit_characteristic_t remaining_duration = HOMEKIT_CHARACTERISTIC_(REMAINING_DURATION, 0, .getter=v_rem_duration_get);

homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_valve, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      &name,
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001A1AVBG04P"),
      HOMEKIT_CHARACTERISTIC(MODEL, "WS-D1"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.6"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, v_identify),
      NULL
    }),
    HOMEKIT_SERVICE(VALVE, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Watering System"),
      &active,
      &valve_type,
      &in_use,
      &set_duration,
      &remaining_duration,
      NULL
    }),
    NULL
  }),
  NULL
};

homekit_value_t v_active_get() {
  return HOMEKIT_UINT8(vactive);
}

void v_active_set(homekit_value_t value) {
  if (value.format != homekit_format_uint8) {
    printf("Invalid Active value format: %d\n", value.format);
    return;
  }
  vactive = value.int_value;
  if (vactive == 0) {
    vin_use = 0;
  } else {
    vin_use = 1;
  }
  if (vactive == 1) {
    led_write(true);
    relay_write(true);
    printf(">>> Valve ON\n");
    if (vset_duration > 0) {
      vremaining_duration = vset_duration;
      sdk_os_timer_setfn(&v_timer, v_off, NULL);
      sdk_os_timer_arm(&v_timer, 1000, 1);
    }
  } else {
    sdk_os_timer_disarm(&v_timer);
    led_write(false);
    relay_write(false);
    printf(">>> Valve manual OFF\n");
    vremaining_duration = 0;
  }
  in_use.value.int_value = vin_use;
  remaining_duration.value.int_value = vremaining_duration;
  homekit_characteristic_notify(&in_use, in_use.value);
  homekit_characteristic_notify(&remaining_duration, remaining_duration.value);
}

homekit_value_t v_in_use_get() {
  return HOMEKIT_UINT8(vin_use);
}

void v_off() {
  vremaining_duration--;
  if (vremaining_duration == 0) {
    sdk_os_timer_disarm(&v_timer);
    led_write(false);
    relay_write(false);
    vactive = 0;
    vin_use = 0;
    active.value.int_value = vactive;
    in_use.value.int_value = vin_use;
    homekit_characteristic_notify(&active, active.value);
    homekit_characteristic_notify(&in_use, in_use.value);
  }
}

homekit_value_t v_set_duration_get() {
  return HOMEKIT_UINT32(vset_duration);
}

void v_set_duration_set(homekit_value_t value) {
  if (value.format != homekit_format_uint32) {
    printf("Invalid Set Duration value format: %d\n", value.format);
    return;
  }
  vset_duration = value.int_value;
}

homekit_value_t v_rem_duration_get() {
  return HOMEKIT_UINT32(vremaining_duration);
}

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
  int name_len = snprintf(NULL, 0, "Watering System%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
  char *name_value = malloc(name_len + 1);
  snprintf(name_value, name_len + 1, "Watering System%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
  name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
  uart_set_baud(0, 115200);
  create_accessory_name();

  wifi_config_init("Watering System", NULL, on_wifi_ready);
  gpio_init();
}
