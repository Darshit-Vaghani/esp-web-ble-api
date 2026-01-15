#ifndef BLE_WEB_H
#define BLE_WEB_H

#include <stdint.h>
#include <stdbool.h>

/* ================= USER CONFIG ================= */
#define BLE_DEVICE_NAME     "ESP32-WebBLE"
#define BLE_MAX_RX_LEN      100
#define BLE_MAX_TX_LEN      100

/* Custom UUIDs (must match Web page) */
#define BLE_SERVICE_UUID    0x00FF
#define BLE_RX_CHAR_UUID    0xFF01   // WRITE
#define BLE_TX_CHAR_UUID    0xFF02   // NOTIFY

/* ================= CALLBACK TYPE ================= */
/* Called automatically when data is received */
typedef void (*ble_rx_callback_t)(const char *data, uint16_t len);

/* ================= API ================= */
void ble_web_init(ble_rx_callback_t rx_cb);
bool ble_web_send(const char *msg);
bool ble_web_is_connected(void);

#endif
