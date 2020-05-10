#ifndef DHT11_H_ 
#define DHT11_H_
#include "driver/gpio.h"

#define DHT_TIMEOUT_ERROR -2
#define DHT_CHECKSUM_ERROR -1
#define DHT_OKAY  0

typedef struct dht11_reading
{
    int temperature;
    int humidity;
    int status;
} dht11_reading_t;

void dht11_init(gpio_num_t dht_gpio);
dht11_reading_t dht11_read();

#endif