#include "ble_web.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#define TAG "BLE_WEB"

/* ================= UUID DEFINITIONS ================= */
static uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static uint16_t char_decl_uuid       = ESP_GATT_UUID_CHAR_DECLARE;
static uint16_t cccd_uuid            = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static uint8_t  cccd_value[2]         = {0x00, 0x00};

static uint16_t service_uuid = BLE_SERVICE_UUID;
static uint16_t rx_char_uuid = BLE_RX_CHAR_UUID;
static uint16_t tx_char_uuid = BLE_TX_CHAR_UUID;

/* Properties */
static uint8_t char_prop_write  = ESP_GATT_CHAR_PROP_BIT_WRITE;
static uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

/* ================= STATE ================= */
static uint16_t service_handle;
static uint16_t rx_handle;
static uint16_t tx_handle;

static esp_gatt_if_t gatts_if_global;
static uint16_t conn_id;
static bool connected = false;

static ble_rx_callback_t user_rx_cb = NULL;

/* ================= ADVERTISING ================= */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC |
            ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ================= GATT DB ================= */
static esp_gatts_attr_db_t gatt_db[] = {

    [0] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
         ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t),
         (uint8_t *)&service_uuid}
    },

    [1] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid,
         ESP_GATT_PERM_READ,
         1, 1,
         &char_prop_write}
    },

    [2] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&rx_char_uuid,
         ESP_GATT_PERM_WRITE,
         BLE_MAX_RX_LEN, 0,
         NULL}
    },

    [3] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid,
         ESP_GATT_PERM_READ,
         1, 1,
         &char_prop_notify}
    },

    [4] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&tx_char_uuid,
         ESP_GATT_PERM_READ,
         BLE_MAX_TX_LEN, 0,
         NULL}
    },

    [5] = {{ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid,
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_value),
         cccd_value}
    },
};

/* ================= GAP HANDLER ================= */
static void gap_handler(esp_gap_ble_cb_event_t event,
                        esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        esp_ble_gap_start_advertising(&adv_params);
    }
}

/* ================= GATTS HANDLER ================= */
static void gatts_handler(esp_gatts_cb_event_t event,
                          esp_gatt_if_t gatts_if,
                          esp_ble_gatts_cb_param_t *param)
{
    switch (event) {

    case ESP_GATTS_REG_EVT:
        gatts_if_global = gatts_if;
        esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 6, 0);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        service_handle = param->add_attr_tab.handles[0];
        rx_handle      = param->add_attr_tab.handles[2];
        tx_handle      = param->add_attr_tab.handles[4];
        esp_ble_gatts_start_service(service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        connected = true;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        connected = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == rx_handle && user_rx_cb) {
            char buf[BLE_MAX_RX_LEN + 1] = {0};
            memcpy(buf, param->write.value, param->write.len);
            user_rx_cb(buf, param->write.len);
        }
        break;

    default:
        break;
    }
}

/* ================= PUBLIC API ================= */

void ble_web_init(ble_rx_callback_t rx_cb)
{
    user_rx_cb = rx_cb;

    nvs_flash_init();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_config_t bt_cfg =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_handler);
    esp_ble_gatts_register_callback(gatts_handler);
    esp_ble_gatts_app_register(0);

    ESP_LOGI(TAG, "BLE Web module initialized");
}

bool ble_web_send(const char *msg)
{
    if (!connected) return false;

    esp_ble_gatts_send_indicate(
        gatts_if_global,
        conn_id,
        tx_handle,
        strlen(msg),
        (uint8_t *)msg,
        false
    );
    return true;
}

bool ble_web_is_connected(void)
{
    return connected;
}
