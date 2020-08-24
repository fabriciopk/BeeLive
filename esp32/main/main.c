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
#include "esp_sleep.h"

#include "cJSON.h"

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

QueueHandle_t  eventQueue = NULL;

static char *message(env_data_t data) {
    uint8_t mac_address[6];
    char mac_address_string[13];
	ESP_ERROR_CHECK(esp_read_mac(mac_address, ESP_MAC_WIFI_STA));

    for (int i = 0; i < 6; i++) {
        char part[3];
		sprintf(part, "%02x", mac_address[i]);
		strncpy(&mac_address_string[i * 2], part, 2);
    }
    mac_address_string[12] = '\0';

    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddStringToObject(root, "sensor_id", mac_address_string);
    cJSON_AddNumberToObject(root, "hx_value", data.raw_hx711_data);
    cJSON_AddNumberToObject(root, "ds_temp", data.ds18x20_temp);
    cJSON_AddNumberToObject(root, "dht_status", data.dht11_status);
    cJSON_AddNumberToObject(root, "dht_tmp", data.dht_11temperature);
    cJSON_AddNumberToObject(root, "dht_hum", data.dht11_humidity);
    return cJSON_Print(root);
}

void read_sensors(void *params) {
    uint32_t err_counter = 1;
    xQueueSend(eventQueue, (void *)&err_counter, (TickType_t )0);

    esp_mqtt_client_handle_t client = *((esp_mqtt_client_handle_t*)params);
    DHT11_init(GPIO_NUM_5);
    ds18x20_addr_t addrs[MAX_SENSORS];
    float ds18x20_temps[MAX_SENSORS];
    int sensor_count;
    env_data_t data;
    hx711_t hx711_dev = {
        .dout = DOUT_GPIO,
        .pd_sck = PD_SCK_GPIO,
        .gain = HX711_GAIN_A_64
    };
    while(1) {
        struct dht11_reading dht = DHT11_read();
        if (dht.status != 0) {
            // TODO fix: This can couse the node hang in this task for ever.
            err_counter++;
            if(err_counter > 10) {
                xQueueSend(eventQueue, (void *)&err_counter, (TickType_t )0);
            }
            continue;
        } else {
            err_counter = 0;
        }

        sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
        if (sensor_count == 1) {
            ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, ds18x20_temps);
            float temp_c = ds18x20_temps[0];
            data.ds18x20_temp = temp_c;
        } else {
            ESP_LOGE(READING_TAG, "No sensors detected!");
        }
        while (1) {
            esp_err_t r = hx711_init(&hx711_dev);
            if (r == ESP_OK)
                break;
            ESP_LOGE(READING_TAG,"Could not initialize HX711: %d (%s)\n", r, esp_err_to_name(r));
            err_counter++;
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        esp_err_t r = hx711_wait(&hx711_dev, 500);
        if (r != ESP_OK) {
            ESP_LOGE(READING_TAG,"Device not found: %d (%s)\n", r, esp_err_to_name(r));
        }

        int32_t hx711_data;
        r = hx711_read_data(&hx711_dev, &hx711_data);
        if (r != ESP_OK) {
            continue;
        }

        data.raw_hx711_data = hx711_data;
        data.dht11_status = dht.status;
        data.dht_11temperature = dht.temperature;
        data.dht11_humidity = dht.humidity;
        esp_mqtt_client_publish(client, "/honey_topic", message(data), 0, 0, 0);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        err_counter = 2;
        // Notify the main task that the package was sent.
        xQueueSend(eventQueue, (void *)&err_counter, (TickType_t )0);
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
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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
    ESP_LOGI(TAG, "Hive status firmware Startup.");
    ESP_LOGI(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "esp-IDF version: %s", esp_get_idf_version());

    // Event queue used to communicate between sensor and main tasks.
    eventQueue = xQueueCreate(20, sizeof(unsigned long));

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Wifi module configuration and start method
    wifi_init_sta();

    //MQTT modue configurantion and start
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    //Create task responsible of reading and sending environment data.
    xTaskCreate(read_sensors, "read_sensors", configMINIMAL_STACK_SIZE * 4, (void*)&client, 5, NULL);

    const int deep_sleep_sec= 600;
    uint32_t event;
    while(true) {
        xQueueReceive(eventQueue, &event, (TickType_t )(1000/portTICK_PERIOD_MS));
        vTaskDelay(500/portTICK_PERIOD_MS); //wait for 500 ms
        if (event == 2){
            ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
            ESP_ERROR_CHECK(esp_mqtt_client_stop(client));
            ESP_LOGI(TAG, "MQTT client has stopped...");
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_LOGI(TAG, "Wifi has stopped...");
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            esp_deep_sleep(1000000LL * deep_sleep_sec);
        }
    }
}
