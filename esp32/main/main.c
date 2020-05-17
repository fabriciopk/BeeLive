/* 
    MQTT (over TCP) Example
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "hx711.h"
#include <ds18x20.h>
#include "dht11.h"

static const char *TAG = "HIVE_STATUS";
static const char *WIFI_TAG = "WIFI";
static const char *READING_TAG = "SENSORS";
static const char *MQTT_TAG = "MQTT";

#define DOUT_GPIO   18
#define PD_SCK_GPIO 19
static EventGroupHandle_t wifi_event_group;
static int wifi_retry_cnt = 0;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const int MAX_SENSORS = 8;
static const gpio_num_t SENSOR_GPIO = 17;


typedef struct {
	int32_t raw_hx711_data;
    float ds18x20_temp;
    int dht11_status;
    int dht_11temperature;
    int dht11_humidity;
} env_data_t;


void read_sensors(void *params) {
    DHT11_init(GPIO_NUM_5);
    ds18x20_addr_t addrs[MAX_SENSORS];
    float ds18x20_temps[MAX_SENSORS];
    int sensor_count;

    hx711_t hx711_dev = {
        .dout = DOUT_GPIO,
        .pd_sck = PD_SCK_GPIO,
        .gain = HX711_GAIN_A_64
    };
    
    while(1) {
        struct dht11_reading dht = DHT11_read();
        sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
        if (sensor_count == 1) {
            ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, ds18x20_temps);
            float temp_c = ds18x20_temps[0];
            printf("Sensor ds18x20 reports %f deg C\n", temp_c);
        } else {
            printf("No sensors detected!\n");
        }
        while (1)
        {
            esp_err_t r = hx711_init(&hx711_dev);
            if (r == ESP_OK)
                break;
            printf("Could not initialize HX711: %d (%s)\n", r, esp_err_to_name(r));
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        esp_err_t r = hx711_wait(&hx711_dev, 500);
        if (r != ESP_OK)
        {
            printf("Device not found: %d (%s)\n", r, esp_err_to_name(r));
        }

        int32_t hx711_data;
        r = hx711_read_data(&hx711_dev, &hx711_data);
        if (r != ESP_OK)
        {
            printf("Could not read data: %d (%s)\n", r, esp_err_to_name(r));
        }

        printf("DHT11 Temperature is %d \n", dht.temperature);
        printf("DHT11 Humidity is %d\n", dht.humidity);
        printf("DHT11 Status code is %d\n", dht.status);
        printf("HX711 Raw data: %d\n", hx711_data);

        esp_event_post(ESP_EVENT_ANY_ID, MQTT_EVENT_CONNECTED, NULL, 0, 0);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}



static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            printf("Raw data: ------------------------------------\n");
            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}


static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_cnt < 10) {
            esp_wifi_connect();
            wifi_retry_cnt++;
            ESP_LOGI(WIFI_TAG, "Retrt to connect to Access Point...");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"Fail to connect to Access Point.");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_cnt = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    ESP_LOGI(TAG, "Starting esp32 wifi bringup...");
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s",  CONFIG_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s", CONFIG_WIFI_SSID);
    } else {
        ESP_LOGE(WIFI_TAG, "An unexpected event occurred during wifi connection.");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
}


void app_main(void)
{
    ESP_LOGI(TAG, "----- Hive status firmware Startup... -----");
    ESP_LOGI(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "esp-IDF version: %s", esp_get_idf_version());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /*Wifi module configuration and start method*/
    wifi_init_sta();
    mqtt_app_start();

    xTaskCreate(read_sensors, "read_sensors", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL);

    while (true) {
    /* Wake up in 2 seconds, or when button is pressed */
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        // esp_event_post(ESP_EVENT_ANY_ID, MQTT_EVENT_CONNECTED, NULL, 0, 0);
        // esp_mqtt_client_publish(client, "/topic/qos0", "data2", 0, 0, 0);
    }
}
