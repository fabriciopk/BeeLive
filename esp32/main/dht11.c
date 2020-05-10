#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp32/rom/ets_sys.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "dht11.h"


int DHT_PIN = 4;
int64_t last_read_time = -2000000;
dht11_reading_t dht_data;

static dht11_reading_t _timeoutError() {
    dht11_reading_t timeoutError = {DHT_TIMEOUT_ERROR, -1, -1};
    return timeoutError;
}

static dht11_reading_t _crcError() {
    dht11_reading_t crcError = {DHT_CHECKSUM_ERROR, -1, -1};
    return crcError;
}

int us_signal_level(int usTimeOut, bool state)
{
    int uSec = 0;
    while (gpio_get_level(DHT_PIN) == state)
    {
        if (uSec > usTimeOut)
            return -1;
        ++uSec;
        ets_delay_us(1); // uSec delay
    }
    return uSec;
}


void dht11_init(gpio_num_t dht_gpio) {
    DHT_PIN = dht_gpio;
}

static void dht_start_signal() {
    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_PIN, 0);
    ets_delay_us(20 * 1000);
    gpio_set_level(DHT_PIN, 1);
    ets_delay_us(40);
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
}

dht11_reading_t dht11_read() {

    if(esp_timer_get_time() - 2000000 < last_read_time) {
        return dht_data;
    }

    last_read_time = esp_timer_get_time();

    uint8_t data[5] = {0,0,0,0,0};

    dht_start_signal();

    if(us_signal_level(80, 0) == DHT_TIMEOUT_ERROR)
        return dht_data = _timeoutError();
    
    /* Read response */
    for(int i = 0; i < 40; i++) {
         //Wait for a response from the DHT11 device
        //This requires waiting for 20-40 us 
        if(us_signal_level(50, 0) == DHT_TIMEOUT_ERROR)
            return dht_data = _timeoutError();
        //Now that the DHT has pulled the line low, 
        //it will keep the line low for 80 us and then high for 80us
        //check to see if it keeps low
        if(us_signal_level(70, 1) > 28) {
            /* Bit received was a 1 */
            data[i/8] |= (1 << (7-(i%8)));
        }
    }
    /* Check data integrity */
    if(data[4] == (data[0] + data[1] + data[2] + data[3])) {
        dht_data.status = DHT_OKAY;
        dht_data.temperature = data[2];
        dht_data.humidity = data[0];
        return dht_data;
    } else {
        return dht_data = _crcError();
    }
}