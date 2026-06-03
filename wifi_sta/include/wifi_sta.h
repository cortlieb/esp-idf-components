#pragma once

#include "esp_err.h"

esp_err_t ow_wifi_sta_init(void);

esp_err_t ow_wifi_sta_connect(const char *wifi_ssid, const char *wifi_password);

esp_err_t ow_wifi_sta_disconnect(void);

esp_err_t ow_wifi_sta_deinit(void);