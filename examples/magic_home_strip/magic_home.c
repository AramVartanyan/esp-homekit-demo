#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "multipwm.h"

#define STEP_MS 15  // in milliseconds

#define RED_PWM_PIN 5
#define GREEN_PWM_PIN 14
#define BLUE_PWM_PIN 12
#define LED_RGB_SCALE 65535       // this is the scaling factor used for color conversion - was 255

typedef union {
    struct {
        uint16_t red;
        uint16_t green;
        uint16_t blue;
    };
    uint64_t color;
} rgb_color_t;

rgb_color_t target_color = { { 0, 0, 0 } };

float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 0;      // saturation is scaled 0 to 100
float led_brightness1 = 0;     // Current brightness is scaled 0 to 100
float led_brightness2 = 0;     // Target brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off

uint8_t pins[] = {RED_PWM_PIN, GREEN_PWM_PIN, BLUE_PWM_PIN};
pwm_info_t pwm_info;

void lightSET();

void led_strip_init(){
    led_on=false;
    led_brightness2 = 100;
    pwm_info.freq = 300;
    pwm_info.channels = 3;
    pwm_info.reverse = false;
    multipwm_init(&pwm_info);
    multipwm_set_pin(&pwm_info, 0, pins[0]);
    multipwm_set_pin(&pwm_info, 1, pins[1]);
    multipwm_set_pin(&pwm_info, 2, pins[2]);

    //multipwm_set_freq(&pwm_info, 300);
    lightSET();
}

static void hsb2rgb(float hue, float sat, float bright, rgb_color_t* rgb) {
    int r = 0, g = 0, b = 0;
    float sat_f = sat / 100;
    float bright_f = bright / 100;

    // If saturation is 0 then color is gray (achromatic)
    // therefore, R, G and B values will all equal the current brightness
    if (sat <= 0) {
      r = bright_f * LED_RGB_SCALE;
      g = bright_f * LED_RGB_SCALE;
      b = bright_f * LED_RGB_SCALE;
    }

    // if saturation and brightness are greater than 0 then calculate
  	// R, G and B values based on the current hue and brightness
    else {
      if ((hue >= 0 && hue < 120.0) || hue == 360.0) {
        float hue_primary = 1.0 - hue / 120.0;
  			float hue_secondary = hue / 120.0;
  			float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
  			float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
  			float sat_tertiary = 1.0 - sat_f;
  			r = (bright_f * LED_RGB_SCALE) * (hue_primary + sat_primary);
  			g = (bright_f * LED_RGB_SCALE) * (hue_secondary + sat_secondary);
  			b = (bright_f * LED_RGB_SCALE) * sat_tertiary;
      }
      else {
        if (hue >= 120.0 && hue < 240.0) {
          float hue_primary = 1.0 - (hue-120.0) / 120.0;
  		  	float hue_secondary = (hue-120.0) / 120.0;
  		  	float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
  		  	float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
  		  	float sat_tertiary = 1.0 - sat_f;
  	  		r = (bright_f * LED_RGB_SCALE) * sat_tertiary;
  		  	g = (bright_f * LED_RGB_SCALE) * (hue_primary + sat_primary);
  		  	b = (bright_f * LED_RGB_SCALE) * (hue_secondary + sat_secondary);
        }
        else {
          if (hue >= 240.0 && hue <= 360.0) {
            float hue_primary = 1.0 - (hue-240.0) / 120.0;
  		    	float hue_secondary = (hue-240.0) / 120.0;
  			    float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
  			    float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
  			    float sat_tertiary = 1.0 - sat_f;
  			    r = (bright_f * LED_RGB_SCALE) * (hue_secondary + sat_secondary);
  			    g = (bright_f * LED_RGB_SCALE) * sat_tertiary;
  			    b = (bright_f * LED_RGB_SCALE) * (hue_primary + sat_primary);
          }
        }
      }
    }
    rgb->red = (uint16_t) r;
    rgb->green = (uint16_t) g;
    rgb->blue = (uint16_t) b;
  }


void led_strip_send (rgb_color_t *color){
    multipwm_stop(&pwm_info);
    multipwm_set_duty(&pwm_info, 0, color->red);
    multipwm_set_duty(&pwm_info, 1, color->green);
    multipwm_set_duty(&pwm_info, 2, color->blue);
    multipwm_start(&pwm_info);
    printf ("sending r=%d, g=%d, b=%d,\n", color->red,color->green, color->blue);
}

void lightSET_task(void *pvParameters) {
  rgb_color_t black_color = { { 0, 0, 0 } };

  if (led_on) {
    if (led_brightness2 > led_brightness1) {
      while (led_brightness2 > led_brightness1) {
        led_brightness1=led_brightness1+1;
        hsb2rgb(led_hue, led_saturation, led_brightness1, &target_color);
        led_strip_send(&target_color);
        vTaskDelay(STEP_MS);
      }
    } else {
      if (led_brightness2 < led_brightness1) {
        while (led_brightness2 < led_brightness1) {
          led_brightness1=led_brightness1-1;
          hsb2rgb(led_hue, led_saturation, led_brightness1, &target_color);
          led_strip_send(&target_color);
          vTaskDelay(STEP_MS);
        }
      } else {
        if (led_brightness2==led_brightness1) {
          hsb2rgb(led_hue, led_saturation, led_brightness1, &target_color);
          led_strip_send(&target_color);
          vTaskDelay(STEP_MS);
        }
      }
    }
  } else {
//       while (led_brightness1 > 0) {
//         led_brightness1=led_brightness1-1;
//         hsb2rgb(led_hue, led_saturation, led_brightness1, &target_color);
         led_strip_send(&black_color);
//         vTaskDelay(STEP_MS);
//       }
     }
     vTaskDelete(NULL);
}

void lightSET() {
    xTaskCreate(lightSET_task, "Light Set", 256, NULL, 2, NULL);
}

void led_identify_task(void *_args) {

    printf("LED identify\n");

    rgb_color_t black_color = { { 0, 0, 0 } };
    rgb_color_t white_color = { { 32767, 32767, 32767 } };

    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_strip_send (&black_color);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_strip_send (&white_color);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    led_strip_send (&target_color);
    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        // printf("Invalid on-value format: %d\n", value.format);
        return;
    }

    led_on = value.bool_value;
    lightSET();
}

homekit_value_t led_brightness_get() {
    return HOMEKIT_INT(led_brightness2);
}

void led_brightness_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        // printf("Invalid brightness-value format: %d\n", value.format);
        return;
    }
    led_brightness2 = value.int_value;
    lightSET();
}

homekit_value_t led_hue_get() {
    return HOMEKIT_FLOAT(led_hue);
}

void led_hue_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    led_hue = value.float_value;
    lightSET();
}

homekit_value_t led_saturation_get() {
    return HOMEKIT_FLOAT(led_saturation);
}

void led_saturation_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid sat-value format: %d\n", value.format);
        return;
    }
    led_saturation = value.float_value;
    lightSET();
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "RGB LED");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Armo Ltd."),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001A1AVBG06P"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MagicHome"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "RGB LED"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
                .getter = led_on_get,
                .setter = led_on_set
            ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
                .getter = led_brightness_get,
                .setter = led_brightness_set
            ),
            HOMEKIT_CHARACTERISTIC(
                HUE, 0,
                .getter = led_hue_get,
                .setter = led_hue_set
            ),
            HOMEKIT_CHARACTERISTIC(
                SATURATION, 0,
                .getter = led_saturation_get,
                .setter = led_saturation_set
            ),
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "320-10-120"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = snprintf(NULL, 0, "RGB LED-%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
    char *name_value = malloc(name_len + 1);
    snprintf(name_value, name_len + 1, "RGB LED-%02X%02X%02X", macaddr[1], macaddr[2], macaddr[3]);
    name.value = HOMEKIT_STRING(name_value);

    wifi_config_init("RGB LED", NULL, on_wifi_ready);

    led_strip_init();
}
