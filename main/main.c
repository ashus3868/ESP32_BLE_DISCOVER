#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

char *TAG = "BLE-Server";
uint8_t ble_addr_type;

struct ble_gap_adv_params adv_params;
bool status = false;

void ble_app_advertise(void);

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);

    char *data = (char *)ctxt->om->om_data;
    printf("%d %s\n", strcmp(data, (char *)"LIGHT ON") == 0, data);
    if (strcmp(data, (char *)"LIGHT ON\0") == 0)
    {
        printf("LIGHT ON\n");
        gpio_set_level(GPIO_NUM_2, 1);
    }
    else if (strcmp(data, (char *)"LIGHT OFF\0") == 0)
    {
        printf("LIGHT OFF\n");
        gpio_set_level(GPIO_NUM_2, 0);
    }
    else if (strcmp(data, (char *)"LED ON\0") == 0)
    {
        printf("LED ON\n");
        gpio_set_level(GPIO_NUM_15, 1);
    }
    else if (strcmp(data, (char *)"LED OFF\0") == 0)
    {
        printf("LED OFF\n");
        gpio_set_level(GPIO_NUM_15, 0);
    }
    // else
    // {
    //     printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
    // }
    memset(data, 0, strlen(data));

    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
    return 0;
}

static int device_read2(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    os_mbuf_append(ctxt->om, "Message from Innovate Yourself", strlen("Message from Innovate Yourself"));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180), // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4), // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD), // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD), // Define UUID for writing
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read2},
         {0}}},
    {0}};

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    // struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    // adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    // adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    // ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void connect_ble(void)
{
    nvs_flash_init(); // 1 - Initialize NVS flash using
    // esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("BLE-Server"); // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread
}

void boot_creds_clear(void *param)
{

    // printf("%lld\n", n - m);
    int64_t m = esp_timer_get_time();
    while (1)
    {

        if (!gpio_get_level(GPIO_NUM_0))
        {
            int64_t n = esp_timer_get_time();

            if ((n - m) / 1000 >= 2000)
            {
                ESP_LOGI("BOOT BUTTON:", "Button Pressed FOR 3 SECOND\n");

                adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
                adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
                ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
                status = true;
                vTaskDelay(100);
                m = esp_timer_get_time();
            }
        }
        else
        {
            m = esp_timer_get_time();
        }
        vTaskDelay(10);

        if (status)
        {
            // ESP_LOGI("GAP", "BLE GAP status");
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
            ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
        }
    }
}

void app_main()
{
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT); // boot button
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    connect_ble();
    xTaskCreate(boot_creds_clear, "boot_creds_clear", 2048, NULL, 5, NULL);
}
