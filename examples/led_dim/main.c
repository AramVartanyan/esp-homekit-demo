/*
 * Example of using esp-homekit library to control
 * a white LED pwm dimmer with chinees mInI D1 (Wemos) USB using HomeKit.
 * The dimmer has the ability to smoothly reach the set Brightness.
 * The esp-wifi-config library is also used in this
 * example. This means you don't have to specify
 * your network's SSID and password before building.
 *
 * WARNING: Never connect the device to another power source while
 * it's connected to the computer port. This may fry them all.
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
 #include "wifi.h"

 #include "button.h"

 // The GPIO pin that is connected to D4 pin on the mInI D1 - the Built-in LED.
 const int led_gpio = 2;

 // The GPIO pin that is connected to D3 pin on the mInI D1 - an external button.
 const int button_gpio = 0;

 #include <pwm.h>
 // The PWM pin that is connected to D2 pin on the mInI D1 - external PWM power board.
 const int pwm_gpio = 4;

 const bool dev = true;

 float bri1=0;
 float bri2=0;
 bool on;
 uint8_t pins[1];

 void led_write(bool on) {
     gpio_write(led_gpio, on ? 0 : 1);
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
     printf("Resetting LED Dimmer configuration\n");
     xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
 }

 void gpio_init() {
     gpio_enable(led_gpio, GPIO_OUTPUT);
     led_write(on);
     pins[0] = pwm_gpio;
     pwm_init(1, pins, true); //Check your dimming hardware if you have to change this to "false"
 }


 void lightSET_task(void *pvParameters) {
     int w;
     if (on) {
         if (bri2 > bri1) {
         while (bri2 > bri1) {
         bri1=bri1+1;
         w = (UINT16_MAX - UINT16_MAX*bri1*0.01);
         pwm_set_duty(w);
         printf("ON  %3d [%5d]\n", (int)bri1 , w);
         vTaskDelay(7);
         }
       } else {
          if (bri2 < bri1) {
          while (bri2 < bri1) {
          bri1=bri1-1;
          w = (UINT16_MAX - UINT16_MAX*bri1*0.01);
          pwm_set_duty(w);
          printf("ON  %3d [%5d]\n", (int)bri1 , w);
          vTaskDelay(7);
         }
     } else {
          if (bri2==bri1) {
          w = (UINT16_MAX - UINT16_MAX*bri1*0.01);
          pwm_set_duty(w);
          printf("ON  %3d [%5d]\n", (int)bri1 , w);
          }
          }
         }
     } else {
          while (bri1 > 0) {
          bri1=bri1-1;
          w = (UINT16_MAX - UINT16_MAX*bri1*0.01);
          pwm_set_duty(w);
          printf("OFF  %3d [%5d]\n", (int)bri1 , w);
          vTaskDelay(2);
         }
     }
     vTaskDelete(NULL);
 }


 void lightSET() {
     xTaskCreate(lightSET_task, "Light Set", 256, NULL, 2, NULL);
 }


 void light_init() {
     printf("light_init:\n");
     on=false;
     bri2=100;
     printf("on = false  bri2 = 100 %%\n");
     pwm_set_freq(1000);
     printf("PWMpwm_set_freq = 1000 Hz  pwm_set_duty = 0 = 0%%\n");
     pwm_set_duty(UINT16_MAX);
     pwm_start();
     lightSET();
 }


 homekit_value_t light_on_get() { return HOMEKIT_BOOL(on); }

 void light_on_set(homekit_value_t value) {
     if (value.format != homekit_format_bool) {
         printf("Invalid on-value format: %d\n", value.format);
         return;
     }
     on = value.bool_value;
     led_write(on);
     lightSET();
 }

 homekit_value_t light_bri_get() { return HOMEKIT_INT(bri2); }

 void light_bri_set(homekit_value_t value) {
     if (value.format != homekit_format_int) {
         printf("Invalid bri-value format: %d\n", value.format);
         return;
     }
     bri2 = value.int_value;
     lightSET();
 }


 void light_identify_task(void *_args) {
     //Identify Dimmer by Pulsing LED.
     for (int j=0; j<3; j++) {
         for (int j=0; j<2; j++) {
             for (int i=0; i<=40; i++) {
                 int w;
                 float b;
                 w = (UINT16_MAX - UINT16_MAX*i/20);
                 if(i>20) {
                     w = (UINT16_MAX - UINT16_MAX*abs(i-40)/20);
                 }
                 b = 100.0*(UINT16_MAX-w)/UINT16_MAX;
                 pwm_set_duty(w);
                 printf("Light_Identify: i = %2d b = %3.0f w = %5d\n",i, b, UINT16_MAX);
                 vTaskDelay(20 / portTICK_PERIOD_MS);
             }
         }
     vTaskDelay(500 / portTICK_PERIOD_MS);
     }
     pwm_set_duty(0);
     led_write(on);
     lightSET();
     vTaskDelete(NULL);
 }


 void light_identify(homekit_value_t _value) {
     printf("Light Identify\n");
     xTaskCreate(light_identify_task, "Light identify", 256, NULL, 2, NULL);
 }


 homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "LED Dimmer");

 homekit_characteristic_t lightbulb_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=light_on_get, .setter=light_on_set);


 void button_callback(uint8_t gpio, button_event_t event) {
     switch (event) {
         case button_event_single_press:
             printf("Toggling lightbulb due to button at GPIO %2d\n", gpio);
             lightbulb_on.value.bool_value = !lightbulb_on.value.bool_value;
             on = lightbulb_on.value.bool_value;
             lightSET();
             homekit_characteristic_notify(&lightbulb_on, lightbulb_on.value);
             break;
         case button_event_long_press:
             printf("Reseting WiFi configuration!\n");
             reset_configuration();
             break;
         default:
             printf("Unknown button event: %d\n", event);
     }
 }

 homekit_accessory_t *accessories[] = {
     HOMEKIT_ACCESSORY(
         .id=1,
         .category=homekit_accessory_category_switch,
         .services=(homekit_service_t*[]){
         HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
             .characteristics=(homekit_characteristic_t*[]){
                 &name,
                 HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
                 HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "002A1AVBG03P"),
                 HOMEKIT_CHARACTERISTIC(MODEL, "PWM Dimmer"),
                 HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.8"),
                 HOMEKIT_CHARACTERISTIC(IDENTIFY, light_identify),
                 NULL
             }),
         HOMEKIT_SERVICE(LIGHTBULB, .primary=true,
             .characteristics=(homekit_characteristic_t*[]){
                 HOMEKIT_CHARACTERISTIC(NAME, "LED Dimmer"),
                 &lightbulb_on,
                 HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter=light_bri_get, .setter=light_bri_set),
             NULL
         }),
         NULL
     }),
     NULL
 };


 homekit_server_config_t config = {
     .accessories = accessories,
     .password = "111-11-111" //It must to be changed to be valid
 };

 void on_wifi_ready() {
     homekit_server_init(&config);
 }

 void create_accessory_name() {
     uint8_t macaddr[6];
     sdk_wifi_get_macaddr(STATION_IF, macaddr);

     int name_len = snprintf(NULL, 0, "LED Dimmer %02X:%02X:%02X",
             macaddr[3], macaddr[4], macaddr[5]);
     char *name_value = malloc(name_len+1);
     snprintf(name_value, name_len+1, "LED Dimmer %02X:%02X:%02X",
             macaddr[3], macaddr[4], macaddr[5]);

     name.value = HOMEKIT_STRING(name_value);
 }


 void user_init(void) {
     uart_set_baud(0, 115200);
     create_accessory_name();

     wifi_config_init("LED Dimmer", NULL, on_wifi_ready);

     gpio_init();
     light_init();

     if (button_create(button_gpio, 0, 4000, button_callback)) {
         printf("Failed to initialize button\n");
     }
 }
 
