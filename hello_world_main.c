#include "ble_web.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "APP";

/* Async RX callback */
static void on_ble_rx(const char *data, uint16_t len)
{
    ESP_LOGI(TAG, "RX: %s", data);
    
    ble_web_send("ok rec");
    /* JSON handling */
    cJSON *json = cJSON_Parse(data);
    if (!json) return;

    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (cmd && strcmp(cmd->valuestring, "ping") == 0) {
        ble_web_send("{\"rsp\":\"pong\"}");
    }

    cJSON_Delete(json);
}

void app_main(void)
{
    ble_web_init(on_ble_rx);
}
