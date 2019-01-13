/*
 * Switch firmware for Shelly 1
 * Reset sequence:
 * - Toggle three times;
 * - Wait for 3 sec;
 * - Toggle one more time;
 *
 */

 #include <stdio.h>
 #include <espressif/esp_wifi.h>
 #include <espressif/esp_sta.h>
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

 #include "shelly_toggle.h"

 #define TOGGLE_GPIO     5
 #define RELAY_GPIO      4

 int t_count=0;

 ETSTimer reset_tmr, toggle_tmr;

 /*
void sdk_ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void sdk_ets_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag);
void sdk_ets_timer_disarm(ETSTimer *ptimer);
*/

 void toggle_count();
 void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

 void relay_write(bool on, int gpio) {
     gpio_write(gpio, on ? 1 : 0);
 }

 void reset_configuration_task() {
     vTaskDelay(1000 / portTICK_PERIOD_MS);
     printf("Resetting HomeKit Config\n");
     homekit_server_reset();
     vTaskDelay(1000 / portTICK_PERIOD_MS);
     printf("Resetting Wifi Config\n");
     wifi_config_reset();
     printf("Restarting\n");
     sdk_system_restart();
     vTaskDelete(NULL);
 }

 void reset_configuration() {
     relay_write(false, RELAY_GPIO);
     gpio_disable(RELAY_GPIO);
     printf("Resetting device configuration\n");
     xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
 }

 homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));

 void gpio_init() {
     gpio_enable(TOGGLE_GPIO, GPIO_INPUT);
     //Disable the internal pullup resistor 47k
     gpio_set_pullup(TOGGLE_GPIO, false, false);
     gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
     relay_write(false, RELAY_GPIO);
 }

 void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
     relay_write(switch_on.value.bool_value, RELAY_GPIO);
 }

 void toggle_switch(const uint8_t gpio) {
     printf("Toggle Switch manual\n");
     switch_on.value.bool_value = !switch_on.value.bool_value;
     relay_write(switch_on.value.bool_value, gpio);
     homekit_characteristic_notify(&switch_on, switch_on.value);
     t_count++;
     toggle_count();
 }

 void stop_count() {
   t_count=0;
   sdk_os_timer_disarm(&toggle_tmr);
   printf("Stop toggle counting\n");
 }

 void reset_seq() {
   t_count=13;
   sdk_os_timer_disarm(&reset_tmr);
   sdk_os_timer_setfn(&toggle_tmr, stop_count, NULL);
   sdk_os_timer_arm(&toggle_tmr, 2000, 0);
   printf("You have 2 s to reset\n");
 }

 void toggle_count() {
   switch (t_count) {
     case 1:
     sdk_os_timer_setfn(&toggle_tmr, stop_count, NULL);
     sdk_os_timer_arm(&toggle_tmr, 5000, 0);
     printf("Toggle task running\n");
     break;

     case 2:
     printf("Toggle one more time\n");
     break;

     case 3:
     sdk_os_timer_disarm(&toggle_tmr);
     sdk_os_timer_setfn(&reset_tmr, reset_seq, NULL);
     sdk_os_timer_arm(&reset_tmr, 3000, 0);
     printf("Toggled 3 times. Whait for 3 s.\n");
     break;

     case 14:
     t_count = 0;
     sdk_os_timer_disarm(&toggle_tmr);
     printf("Initiating reset\n");
     reset_configuration();
     break;

     default:
     sdk_os_timer_disarm(&reset_tmr);
     t_count = 0;
     printf("Toggled more than 3 times before the time is up\n");
     break;
   }
 }

   void switch_identify_task(void *_args) {
     // Identify the device by switching relay.
     for (int i=0; i<2; i++) {
             relay_write(true, RELAY_GPIO);
             vTaskDelay(1000 / portTICK_PERIOD_MS);
             relay_write(false, RELAY_GPIO);
             vTaskDelay(1500 / portTICK_PERIOD_MS);
     }
     vTaskDelete(NULL);
 }

 void switch_identify(homekit_value_t _value) {
     printf("Switch identify\n");
     xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 2, NULL);
 }

 homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Shelly1");

 homekit_accessory_t *accessories[] = {
     HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
         HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
             &name,
             HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
             HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "004A1AVBG02P"),
             HOMEKIT_CHARACTERISTIC(MODEL, "Shelly1"),
             HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.1"),
             HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
             NULL
         }),
         HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
             HOMEKIT_CHARACTERISTIC(NAME, "Shelly1"),
             &switch_on,
             NULL
         }),
         NULL
     }),
     NULL
 };

 homekit_server_config_t config = {
     .accessories = accessories,
     .password = "320-10-136"
 };

 void on_wifi_ready() {
     homekit_server_init(&config);
 }

 void create_accessory_name() {
     uint8_t macaddr[6];
     sdk_wifi_get_macaddr(STATION_IF, macaddr);

     int name_len = snprintf(NULL, 0, "Shelly1-%02X%02X%02X",
                             macaddr[3], macaddr[4], macaddr[5]);
     char *name_value = malloc(name_len+1);
     snprintf(name_value, name_len+1, "Shelly1-%02X%02X%02X",
              macaddr[3], macaddr[4], macaddr[5]);

     name.value = HOMEKIT_STRING(name_value);
 }

 void user_init(void) {
     uart_set_baud(0, 115200);
     create_accessory_name();
     wifi_config_init("Shelly1", NULL, on_wifi_ready);
     gpio_init();
     adv_button_create(TOGGLE_GPIO);
     adv_button_register_callback_fn(TOGGLE_GPIO, toggle_switch, 1);
 }
