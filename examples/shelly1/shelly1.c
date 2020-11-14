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

 #include <sysparam.h>

 #include "adv_button.h"

 #define D_MANIFACTURER "Armo Ltd."
 #define D_MODEL "Switch"
 #define D_FW "0.1.11"
 #define D_AN1 "Switch"
 #define D_PASS "111-11-111"
 #define D_AN2 "Switch-11%02X%02X%02X"

 #define TOGGLE_GPIO     5
 #define RELAY_GPIO      4

 #define SWITCH_SYSPARAM "0"

 int t_count=0;
 bool timer_running = false;
 bool switch_on_status = false;
 static char wifi_mac_address[32];

 ETSTimer reset_tmr, toggle_tmr, memory_timer;

 void toggle_count();
 void switch_on_callback(homekit_value_t value);
 homekit_value_t read_switch_state();

 homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch_state, .setter=switch_on_callback);

 void relay_write(bool on, int gpio) {
     gpio_write(gpio, on ? 1 : 0);
 }

 void remember_state() {
   sysparam_status_t status;
   timer_running = false;

   sdk_os_timer_disarm(&memory_timer);

   status = sysparam_set_bool(SWITCH_SYSPARAM, switch_on_status);
   if (status != SYSPARAM_OK) {
     printf("Saving settings error -> %i\n", status);
   }
   printf("Saving settings to memory %i\n", status);
 }

 void read_memory() {
   sysparam_status_t status;
   bool bool_value;

   status = sysparam_get_bool(SWITCH_SYSPARAM, &bool_value);
   printf("Status %i\n", status);

   if (status == SYSPARAM_OK) {
     switch_on_status = bool_value;
   } else {
     status = sysparam_set_bool(SWITCH_SYSPARAM, false);
   }
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
     //gpio_disable(RELAY_GPIO);
     sysparam_set_bool(SWITCH_SYSPARAM, false);
     printf("Resetting device configuration\n");
     xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
 }

 void gpio_init() {
     gpio_enable(TOGGLE_GPIO, GPIO_INPUT);
     gpio_set_pullup(TOGGLE_GPIO, false, false);
     gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
     read_memory();
     relay_write(switch_on_status, RELAY_GPIO);
     switch_on.value.bool_value = switch_on_status;
     sdk_os_timer_setfn(&memory_timer, remember_state, NULL);
 }

 void switch_on_callback(homekit_value_t value) {
     switch_on.value = value;
     relay_write(switch_on.value.bool_value, RELAY_GPIO);
     read_memory();
     if (switch_on.value.bool_value != switch_on_status) {
       switch_on_status = switch_on.value.bool_value;
       if (!timer_running) {
         timer_running = true;
         sdk_os_timer_arm(&memory_timer, 10000, 0);
       }
     }
 }

 homekit_value_t read_switch_state() {
     return switch_on.value;
 }

 void toggle_switch(const uint8_t gpio) {
     printf("Toggle Switch manual\n");
     switch_on.value.bool_value = !switch_on.value.bool_value;
     relay_write(switch_on.value.bool_value, RELAY_GPIO);
     homekit_characteristic_notify(&switch_on, switch_on.value);
     t_count++;
     toggle_count();
     read_memory();
     if (switch_on.value.bool_value != switch_on_status) {
       switch_on_status = switch_on.value.bool_value;
       if (!timer_running) {
         timer_running = true;
         sdk_os_timer_arm(&memory_timer, 10000, 0);
       }
     }
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
     printf("Position 1\n");
     break;

     case 2:
     sdk_os_timer_setfn(&toggle_tmr, stop_count, NULL);
     sdk_os_timer_arm(&toggle_tmr, 5000, 0);
     printf("Position 2. 5s Timer started.\n");
     break;

     case 3:
     printf("Position 3\n");
     break;

     case 4:
     printf("Position 4\n");
     break;

     case 5:
     printf("Position 5\n");
     break;

     case 6:
     sdk_os_timer_disarm(&toggle_tmr);
     sdk_os_timer_setfn(&reset_tmr, reset_seq, NULL);
     sdk_os_timer_arm(&reset_tmr, 3000, 0);
     printf("Position 6. Switched 3 times. Whait for 3 s.\n");
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
     printf("Switched more than 3 times before the time is up\n");
     break;
   }
 }

   void switch_identify_task(void *_args) {
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

 homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, D_MODEL);

 homekit_accessory_t *accessories[] = {
     HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
         HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
             &name,
             HOMEKIT_CHARACTERISTIC(MANUFACTURER, D_MANIFACTURER),
             HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, wifi_mac_address),
             HOMEKIT_CHARACTERISTIC(MODEL, D_MODEL),
             HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, D_FW),
             HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
             NULL
         }),
         HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
             HOMEKIT_CHARACTERISTIC(NAME, D_MODEL),
             &switch_on,
             NULL
         }),
         NULL
     }),
     NULL
 };

 homekit_server_config_t config = {
     .accessories = accessories,
     .password = D_PASS
 };

 void on_wifi_event(wifi_config_event_t event) {
     switch (event) {
         case WIFI_CONFIG_CONNECTED:
             printf("Connected\n");
             homekit_server_init(&config);
             break;
         case WIFI_CONFIG_DISCONNECTED:
             printf("Disconnected\n");
             break;
     }
 }

 void create_accessory_name() {
     uint8_t macaddr[6];
     sdk_wifi_get_macaddr(STATION_IF, macaddr);

     sprintf(wifi_mac_address, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
     printf("Device WIFI mac_address: %s\n", wifi_mac_address);

     int name_len = snprintf(NULL, 0, D_AN2, macaddr[3], macaddr[4], macaddr[5]);
     char *name_value = malloc(name_len+1);
     snprintf(name_value, name_len+1, D_AN2, macaddr[3], macaddr[4], macaddr[5]);

     name.value = HOMEKIT_STRING(name_value);
 }

 void user_init(void) {
     uart_set_baud(0, 115200);
     create_accessory_name();
     wifi_config_init2(D_AN1, NULL, on_wifi_event);
     gpio_init();
     adv_toggle_create(TOGGLE_GPIO, false);
     adv_toggle_register_callback_fn(TOGGLE_GPIO, toggle_switch, 2);
 }
