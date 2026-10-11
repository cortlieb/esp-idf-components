#ifndef OW_MQTT_CLIENT_H
#define OW_MQTT_CLIENT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * @brief Event group bits for MQTT events
 */
#define MQTT_CLIENT_CONNECTED_BIT BIT0
#define MQTT_CLIENT_DISCONNECTED_BIT BIT1

/**
 * @brief Initialize and start the MQTT client.
 *
 * This function initializes the MQTT client and attempts to connect to the
 * configured MQTT broker. The client will automatically reconnect if the
 * connection is lost.
 *
 * Important! You must have a working network connection before calling this
 * function.
 *
 * The caller owns the event group and must keep it valid until stop succeeds.
 * BIT0 and BIT1 are reserved for this component and cleared during init/stop.
 * Lifecycle calls must be serialized and must not overlap publish/subscribe calls.
 * Only mqtt:// broker URIs are supported; TLS is not supported.
 *
 * @param[in] event_group Valid, non-NULL event group handle for MQTT events.
 *
 * @return
 *  - ESP_OK when the client has started; connection is asynchronous and is
 *    indicated by MQTT_CLIENT_CONNECTED_BIT.
 *  - ESP_ERR_INVALID_ARG for a NULL event group or unsupported broker URI.
 *  - ESP_ERR_INVALID_STATE if the client is already initialized.
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t mqtt_client_init(EventGroupHandle_t event_group);

/**
 * @brief Stop the MQTT client and clean up resources.
 *
 * Must not be called from the MQTT event handler. The caller's event group is
 * not deleted. After successful stop it may be deleted or reused by the caller.
 * @return
 *  - ESP_OK on success
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t mqtt_client_stop(void);

/**
 * @brief Publish a message to an MQTT topic.
 *
 * @param[in] topic The topic to publish to
 * @param[in] data The message data to publish
 * @param[in] len Length of the message data (0 for null-terminated string)
 * @param[in] qos Quality of Service level (0, 1, or 2)
 * @param[in] retain Retain flag for the message
 *
 * @return
 *  - Message ID on success (>= 0)
 *  - Negative value on failure (forwarded from ESP-MQTT)
 */
int mqtt_client_publish(const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief Subscribe to an MQTT topic.
 *
 * @param[in] topic The topic to subscribe to
 * @param[in] qos Quality of Service level (0, 1, or 2)
 *
 * @return
 *  - Message ID on success (>= 0)
 *  - Negative value on failure (forwarded from ESP-MQTT)
 */
int mqtt_client_subscribe(const char *topic, int qos);

/**
 * @brief Unsubscribe from an MQTT topic.
 *
 * @param[in] topic The topic to unsubscribe from
 *
 * @return
 *  - Message ID on success (>= 0)
 *  - Negative value on failure (forwarded from ESP-MQTT)
 */
int mqtt_client_unsubscribe(const char *topic);

#endif // OW_MQTT_CLIENT_H
