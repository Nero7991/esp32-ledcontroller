
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "Timer.h"
#include "Switch.h"

#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_pm.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include <esp_http_server.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"

//#include <esp_https_server.h>

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

void upButtonPressed(uint8_t SwitchId);
void downButtonPressed(uint8_t SwitchId);
void toggleFadeMode(uint8_t SwitchId);
volatile int FadeLutPointer = 0, SetLutPointer = 1000, CurrentLutPointer = 0;
volatile bool FadeDirection = 0;
volatile bool FadeEnabled = 0;

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "example.com"
#define WEB_PORT "80"
#define WEB_PATH "/"

static const char *TAG = "power_save";

static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

static void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    while(1) {
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.
           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
        // for(int countdown = 10; countdown >= 0; countdown--) {
        //     ESP_LOGI(TAG, "%d... ", countdown);
        //     vTaskDelay(1000 / portTICK_PERIOD_MS);
        // }
        vTaskSuspend( NULL );
        ESP_LOGI(TAG, "Starting again!");
    }
}

void backtofactory()
{
    esp_partition_iterator_t  pi ;                                  // Iterator for find
    const esp_partition_t*    factory ;                             // Factory partition
    esp_err_t                 err ;

    pi = esp_partition_find ( ESP_PARTITION_TYPE_APP,               // Get partition iterator for
                              ESP_PARTITION_SUBTYPE_APP_FACTORY,    // factory partition
                              "factory" ) ;
    if ( pi == NULL )                                               // Check result
    {
        ESP_LOGE ( TAG, "Failed to find factory partition" ) ;
    }
    else
    {
        factory = esp_partition_get ( pi ) ;                        // Get partition struct
        esp_partition_iterator_release ( pi ) ;                     // Release the iterator
        err = esp_ota_set_boot_partition ( factory ) ;              // Set partition for boot
        if ( err != ESP_OK )                                        // Check error
	    {
            ESP_LOGE ( TAG, "Failed to set boot partition" ) ;
	    }
    }
}

static esp_err_t update_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }   
    ESP_LOGI(TAG, "Trying firmware update...");
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    backtofactory();
    esp_restart();   
    return ESP_OK;
}


static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            if(strcmp(buf, "min") == 0)
                SetLutPointer = 0;
            if(strcmp(buf, "max") == 0)
                SetLutPointer = 1000;
            if(strcmp(buf, "inc") == 0)
                upButtonPressed(0);
            if(strcmp(buf, "dec") == 0)
                downButtonPressed(0);
            if(strcmp(buf, "med") == 0)
                SetLutPointer = 300;
            if(strcmp(buf, "fade") == 0)
                toggleFadeMode(0);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t setBrightness = {
    .uri       = "/setledbrightness",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = (void*)"Hello World!"
};


static const httpd_uri_t updateFirmware = {
    .uri       = "/updatefirmware",
    .method    = HTTP_GET,
    .handler   = update_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = (void*)"Resetting to factory OTA program..."
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &setBrightness);
        httpd_register_uri_handler(server, &updateFirmware);
        httpd_register_uri_handler(server, &echo);
        //httpd_register_uri_handler(server, &ctrl);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}



/*set the ssid and password via "idf.py menuconfig"*/
#define DEFAULT_SSID "Collaco WiFi"
#define DEFAULT_PWD "Orencollaco1997"

#define DEFAULT_LISTEN_INTERVAL 3

#if CONFIG_EXAMPLE_POWER_SAVE_MIN_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MIN_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_MAX_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_NONE
#define DEFAULT_PS_MODE WIFI_PS_NONE
#else
#define DEFAULT_PS_MODE WIFI_PS_NONE
#endif /*CONFIG_POWER_SAVE_MODEM*/


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/*init wifi as sta and set power save mode*/
static void wifi_power_save(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_config_t wifi_config = {
        {
            DEFAULT_SSID,
            DEFAULT_PWD,
            {},
            {},
            {},
            {},
            {},
            DEFAULT_LISTEN_INTERVAL
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "esp_wifi_set_ps().");
    esp_wifi_set_ps(DEFAULT_PS_MODE);
    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
    server = start_webserver();
}

void setupWifi(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_esp32_t pm_config = {
            .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
            .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif // CONFIG_PM_ENABLE

    wifi_power_save();
}

#define GPIO_OUTPUT_IO_0 (gpio_num_t)18
#define GPIO_OUTPUT_IO_1 (gpio_num_t)19
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0 (gpio_num_t)4
#define GPIO_INPUT_IO_1 (gpio_num_t)5
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

#define LEDC_HS_TIMER LEDC_TIMER_0
#define LEDC_HS_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO (18)
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO (19)
#define LEDC_HS_CH1_CHANNEL LEDC_CHANNEL_1

#define LEDC_TEST_CH_NUM (4)
#define LEDC_TEST_DUTY 100

#define INVERSE_TIME 1000
#define APU_CLK 80000000
#define TIMER_DIVIDER 8                              //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // convert counter value to seconds
#define TIMER_ALARM_VALUE APU_CLK / (TIMER_DIVIDER * INVERSE_TIME)

static xQueueHandle gpio_evt_queue = NULL;
TimerClass T1;
SwitchClass S1, S2;
extern TimerClass Timer;
volatile int SetDuty = 0, Duty = 0;

const uint16_t FadeLut[1001] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 19, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 54, 55, 57, 58, 60, 62, 63, 65, 66, 68, 70, 71, 73, 75, 76, 78, 80, 82, 84, 86, 87, 89, 91, 93, 95, 97, 99, 101, 103, 105, 107, 109, 111, 113, 116, 118, 120, 122, 124, 127, 129, 131, 134, 136, 138, 141, 143, 145, 148, 150, 153, 155, 158, 160, 163, 165, 168, 170, 173, 176, 178, 181, 184, 187, 189, 192, 195, 198, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 255, 258, 261, 264, 267, 271, 274, 277, 281, 284, 287, 291, 294, 298, 301, 305, 308, 312, 315, 319, 322, 326, 329, 333, 337, 340, 344, 348, 351, 355, 359, 363, 367, 370, 374, 378, 382, 386, 390, 394, 398, 402, 406, 410, 414, 418, 422, 426, 430, 434, 438, 442, 447, 451, 455, 459, 463, 468, 472, 476, 481, 485, 489, 494, 498, 503, 507, 512, 516, 521, 525, 530, 534, 539, 543, 548, 553, 557, 562, 567, 571, 576, 581, 586, 591, 595, 600, 605, 610, 615, 620, 625, 630, 634, 639, 644, 649, 654, 660, 665, 670, 675, 680, 685, 690, 695, 701, 706, 711, 716, 722, 727, 732, 738, 743, 748, 754, 759, 765, 770, 775, 781, 786, 792, 797, 803, 809, 814, 820, 825, 831, 837, 842, 848, 854, 860, 865, 871, 877, 883, 888, 894, 900, 906, 912, 918, 924, 930, 936, 942, 948, 954, 960, 966, 972, 978, 984, 990, 996, 1003, 1009, 1015, 1021, 1027, 1034, 1040, 1046, 1052, 1059, 1065, 1072, 1078, 1084, 1091, 1097, 1104, 1110, 1117, 1123, 1130, 1136, 1143, 1149, 1156, 1162, 1169, 1176, 1182, 1189, 1196, 1202, 1209, 1216, 1223, 1229, 1236, 1243, 1250, 1257, 1264, 1270, 1277, 1284, 1291, 1298, 1305, 1312, 1319, 1326, 1333, 1340, 1347, 1354, 1361, 1369, 1376, 1383, 1390, 1397, 1404, 1412, 1419, 1426, 1433, 1441, 1448, 1455, 1463, 1470, 1477, 1485, 1492, 1500, 1507, 1514, 1522, 1529, 1537, 1544, 1552, 1559, 1567, 1575, 1582, 1590, 1597, 1605, 1613, 1620, 1628, 1636, 1644, 1651, 1659, 1667, 1675, 1682, 1690, 1698, 1706, 1714, 1722, 1730, 1738, 1746, 1754, 1761, 1769, 1777, 1785, 1794, 1802, 1810, 1818, 1826, 1834, 1842, 1850, 1858, 1867, 1875, 1883, 1891, 1899, 1908, 1916, 1924, 1933, 1941, 1949, 1958, 1966, 1974, 1983, 1991, 1999, 2008, 2016, 2025, 2033, 2042, 2050, 2059, 2067, 2076, 2085, 2093, 2102, 2110, 2119, 2128, 2136, 2145, 2154, 2162, 2171, 2180, 2189, 2197, 2206, 2215, 2224, 2233, 2241, 2250, 2259, 2268, 2277, 2286, 2295, 2304, 2313, 2322, 2331, 2340, 2349, 2358, 2367, 2376, 2385, 2394, 2403, 2412, 2421, 2431, 2440, 2449, 2458, 2467, 2477, 2486, 2495, 2504, 2514, 2523, 2532, 2541, 2551, 2560, 2570, 2579, 2588, 2598, 2607, 2617, 2626, 2635, 2645, 2654, 2664, 2673, 2683, 2692, 2702, 2712, 2721, 2731, 2740, 2750, 2760, 2769, 2779, 2789, 2798, 2808, 2818, 2827, 2837, 2847, 2857, 2866, 2876, 2886, 2896, 2906, 2916, 2925, 2935, 2945, 2955, 2965, 2975, 2985, 2995, 3005, 3015, 3025, 3035, 3045, 3055, 3065, 3075, 3085, 3095, 3105, 3115, 3125, 3135, 3146, 3156, 3166, 3176, 3186, 3196, 3207, 3217, 3227, 3237, 3248, 3258, 3268, 3279, 3289, 3299, 3310, 3320, 3330, 3341, 3351, 3361, 3372, 3382, 3393, 3403, 3414, 3424, 3435, 3445, 3456, 3466, 3477, 3487, 3498, 3508, 3519, 3529, 3540, 3551, 3561, 3572, 3582, 3593, 3604, 3614, 3625, 3636, 3647, 3657, 3668, 3679, 3690, 3700, 3711, 3722, 3733, 3743, 3754, 3765, 3776, 3787, 3798, 3809, 3819, 3830, 3841, 3852, 3863, 3874, 3885, 3896, 3907, 3918, 3929, 3940, 3951, 3962, 3973, 3984, 3995, 4006, 4017, 4028, 4039, 4051, 4062, 4073, 4084, 4095, 4106, 4117, 4129, 4140, 4151, 4162, 4173, 4185, 4196, 4207, 4218, 4230, 4241, 4252, 4264, 4275, 4286, 4297, 4309, 4320, 4332, 4343, 4354, 4366, 4377, 4388, 4400, 4411, 4423, 4434, 4446, 4457, 4469, 4480, 4491, 4503, 4515, 4526, 4538, 4549, 4561, 4572, 4584, 4595, 4607, 4618, 4630, 4642, 4653, 4665, 4677, 4688, 4700, 4711, 4723, 4735, 4747, 4758, 4770, 4782, 4793, 4805, 4817, 4829, 4840, 4852, 4864, 4876, 4887, 4899, 4911, 4923, 4935, 4946, 4958, 4970, 4982, 4994, 5006, 5018, 5029, 5041, 5053, 5065, 5077, 5089, 5101, 5113, 5125, 5137, 5149, 5161, 5173, 5185, 5197, 5209, 5221, 5233, 5245, 5257, 5269, 5281, 5293, 5305, 5317, 5329, 5341, 5353, 5365, 5377, 5389, 5401, 5414, 5426, 5438, 5450, 5462, 5474, 5486, 5499, 5511, 5523, 5535, 5547, 5559, 5572, 5584, 5596, 5608, 5621, 5633, 5645, 5657, 5670, 5682, 5694, 5706, 5719, 5731, 5743, 5755, 5768, 5780, 5792, 5805, 5817, 5829, 5842, 5854, 5866, 5879, 5891, 5904, 5916, 5928, 5941, 5953, 5965, 5978, 5990, 6003, 6015, 6027, 6040, 6052, 6065, 6077, 6090, 6102, 6115, 6127, 6140, 6152, 6164, 6177, 6189, 6202, 6214, 6227, 6239, 6252, 6265, 6277, 6290, 6302, 6315, 6327, 6340, 6352, 6365, 6377, 6390, 6403, 6415, 6428, 6440, 6453, 6465, 6478, 6491, 6503, 6516, 6529, 6541, 6554, 6566, 6579, 6592, 6604, 6617, 6630, 6642, 6655, 6668, 6680, 6693, 6706, 6718, 6731, 6744, 6756, 6769, 6782, 6794, 6807, 6820, 6832, 6845, 6858, 6871, 6883, 6896, 6909, 6921, 6934, 6947, 6960, 6972, 6985, 6998, 7011, 7023, 7036, 7049, 7062, 7074, 7087, 7100, 7113, 7126, 7138, 7151, 7164, 7177, 7189, 7202, 7215, 7228, 7241, 7253, 7266, 7279, 7292, 7305, 7317, 7330, 7343, 7356, 7369, 7382, 7394, 7407, 7420, 7433, 7446, 7459, 7471, 7484, 7497, 7510, 7523, 7536, 7548, 7561, 7574, 7587, 7600, 7613, 7626, 7638, 7651, 7664, 7677, 7690, 7703, 7716, 7728, 7741, 7754, 7767, 7780, 7793, 7806, 7818, 7831, 7844, 7857, 7870, 7883, 7896, 7909, 7922, 7934, 7947, 7960, 7973, 7986, 7999, 8012, 8025, 8037, 8050, 8063, 8076, 8089, 8102, 8115, 8128, 8141, 8153, 8166, 8179, 8191, 8191, 8191, 8191, 8191, 8191};

/*
* Prepare and set configuration of timers
* that will be used by LED Controller
*/
ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
    {
        LEDC_HS_CH0_GPIO,
        LEDC_HS_MODE,
        LEDC_HS_CH0_CHANNEL,
        LEDC_INTR_DISABLE,
        LEDC_HS_TIMER,
        0,
        0,
    },
    {
        LEDC_HS_CH1_GPIO,
        LEDC_HS_MODE,
        LEDC_HS_CH1_CHANNEL,
        LEDC_INTR_DISABLE,
        LEDC_HS_TIMER,
        0,
        0,
    }};

static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    S1.pinStateChanged();
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

volatile uint64_t cnt = 0;
static void timer_group0_isr(void *para)
{
    timer_spinlock_take(TIMER_GROUP_0);
    int timer_idx = (int)para;
    cnt += 1;
    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);

    /* Clear the interrupt
       and update the alarm time for the timer with with reload */
    if (timer_intr & TIMER_INTR_T0)
    {
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
        timer_group_set_alarm_value_in_isr(TIMER_GROUP_0, (timer_idx_t)timer_idx, TIMER_ALARM_VALUE);
    }
    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, (timer_idx_t)timer_idx);
    Timer.milliHappened((uint8_t)timer_idx);
    timer_spinlock_give(TIMER_GROUP_0);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        printf("Running");
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
        }
    }
}

void timerEnded(uint8_t TimerId)
{
    if (FadeEnabled)
    {
        if (!FadeDirection)
        {
            Duty = FadeLut[FadeLutPointer];
            FadeLutPointer += 1;
            if (FadeLutPointer == 1000)
                FadeDirection = 1;
        }
        else
        {
            Duty = FadeLut[FadeLutPointer];
            FadeLutPointer -= 1;
            if (FadeLutPointer == 0)
                FadeDirection = 0;
        }
    }
    else
    {
        if (CurrentLutPointer != SetLutPointer)
        {
           
            if (CurrentLutPointer < SetLutPointer)
            {
                CurrentLutPointer += 1;
            }
            else
            {
                CurrentLutPointer -= 1;
            }
            Duty = FadeLut[CurrentLutPointer];
        }
    }
    uint16_t ch;
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, Duty);
        ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
    }
}

void toggleLight(uint8_t SwitchId){
    if(SetLutPointer > 500)
    SetLutPointer = 0;
    else
    {
        SetLutPointer = 1000;
    }
}

void toggleFadeMode(uint8_t SwitchId){
    FadeEnabled ^= 1;
}

void upButtonPressed(uint8_t SwitchId)
{
    if(SetLutPointer < 1000)
    SetLutPointer += 100;
    if(SetLutPointer > 1000)
    SetLutPointer = 1000;
}

void downButtonPressed(uint8_t SwitchId)
{
    if(SetLutPointer > 0)
    SetLutPointer -= 100;
}

void setupLedPwm();
void setupGpio();
void setupGroup0Timer0();

extern "C" void app_main(void)
{
   
    backtofactory();


    const esp_partition_t *running = esp_ota_get_running_partition();
    // Display the running partition
    ESP_LOGI(TAG, "Running partition: %s", running->label);
    ESP_LOGI(TAG, "Partition Type: %d", (uint8_t)running->type);
    ESP_LOGI(TAG, "Partition Subtype: %d", (uint8_t)running->subtype);


    setupWifi();
    //vTaskDelay(2000 / portTICK_RATE_MS);  
    setupLedPwm();
    setupGpio();
    setupGroup0Timer0();

    T1.initializeTimer();
    T1.setCallBackTime(1, true, timerEnded);

    S1.initializeSwitch(4);
    S2.initializeSwitch(5);

    S1.shortPress(upButtonPressed);
    S2.shortPress(downButtonPressed);
    S1.longPress(toggleLight);
    S2.longPress(toggleFadeMode);
    while (1)
    {

        printf("Time: %" PRIu64 "\n", Timer.millis());
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
        gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);
        printf("3. LEDC set duty = %d\n", Duty);
        // for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
        //     ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, Duty);
        //     ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        // }
    }
}

void setupLedPwm()
{
    uint16_t ch;
    ledc_timer_config_t ledc_timer = {
        LEDC_HS_MODE,      // resolution of PWM duty
        LEDC_TIMER_13_BIT, // frequency of PWM signal
        LEDC_HS_TIMER,     // timer mode
        5000,              // timer index
        LEDC_AUTO_CLK,     // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_channel_config(&ledc_channel[ch]);
    }
}

void setupGpio()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)0;
    //configure GPIO with the given settings
    //gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    //gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *)GPIO_INPUT_IO_1);

    //remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
}

void setupGroup0Timer0()
{
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = (timer_autoreload_t) true;
#ifdef TIMER_GROUP_SUPPORTS_XTAL_CLOCK
    config.clk_src = TIMER_SRC_CLK_APB;
#endif
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 10000);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_group0_isr,
                       (void *)TIMER_0, ESP_INTR_FLAG_DEFAULT, NULL);
    timer_start(TIMER_GROUP_0, TIMER_0);
}