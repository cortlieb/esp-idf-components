/**
 * MQTT Client Component for ESP32
 *
 * Provides MQTT client functionality with automatic reconnection.
 * Based on ESP-IDF MQTT client library.
 */

#include "ow_mqtt_client.h"
#include <inttypes.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

// Tag for debug messages
static const char *TAG = "ow_mqtt_client";

// Static global variables
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;

/*******************************************************************************
 * Private function prototypes
 */

static void mqtt_event_handler(void *handler_args,
							   esp_event_base_t base,
							   int32_t event_id,
							   void *event_data);

/*******************************************************************************
 * Private function definitions
 */

// Event handler: MQTT events
static void mqtt_event_handler(void *handler_args,
							   esp_event_base_t base,
							   int32_t event_id,
							   void *event_data)
{
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT connected to broker");
		xEventGroupClearBits(s_mqtt_event_group, MQTT_CLIENT_DISCONNECTED_BIT);
		xEventGroupSetBits(s_mqtt_event_group, MQTT_CLIENT_CONNECTED_BIT);

		// Publish "online" status (retained)
		if (esp_mqtt_client_publish(event->client, CONFIG_OW_MQTT_STATUS_TOPIC, "online", 0, 1, 1) < 0)
		{
			ESP_LOGE(TAG, "Failed to publish online status to %s", CONFIG_OW_MQTT_STATUS_TOPIC);
		}
		break;

	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT disconnected from broker");
		xEventGroupClearBits(s_mqtt_event_group, MQTT_CLIENT_CONNECTED_BIT);
		xEventGroupSetBits(s_mqtt_event_group, MQTT_CLIENT_DISCONNECTED_BIT);
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT published, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT data received");
		if (event->topic != NULL && event->topic_len > 0)
		{
			ESP_LOGI(TAG, "  Topic: %.*s", event->topic_len, event->topic);
		}
		if (event->data != NULL && event->data_len > 0)
		{
			ESP_LOGI(TAG, "  Data: %.*s", event->data_len, event->data);
		}
		break;

	case MQTT_EVENT_ERROR:
		ESP_LOGE(TAG, "MQTT error occurred");
		if (event->error_handle == NULL)
		{
			break;
		}
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			ESP_LOGE(TAG, "  Last error code reported from esp-tls: 0x%x",
					 event->error_handle->esp_tls_last_esp_err);
			ESP_LOGE(TAG, "  Last tls stack error number: 0x%x",
					 event->error_handle->esp_tls_stack_err);
			ESP_LOGE(TAG, "  Last captured errno : %d (%s)",
					 event->error_handle->esp_transport_sock_errno,
					 strerror(event->error_handle->esp_transport_sock_errno));
		}
		else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
		{
			ESP_LOGE(TAG, "  Connection refused error: 0x%x",
					 event->error_handle->connect_return_code);
		}
		break;

	default:
		ESP_LOGD(TAG, "Unhandled MQTT event: %" PRId32, event_id);
		break;
	}
}

/*******************************************************************************
 * Public function definitions
 */

// Initialize and start MQTT client
esp_err_t mqtt_client_init(EventGroupHandle_t event_group)
{
	esp_err_t esp_ret;

	// Print message
	ESP_LOGI(TAG, "Starting MQTT client...");

	if (s_mqtt_client != NULL)
	{
		ESP_LOGE(TAG, "MQTT client already initialized");
		return ESP_ERR_INVALID_STATE;
	}
	if (event_group == NULL)
	{
		ESP_LOGE(TAG, "Event group handle is NULL");
		return ESP_ERR_INVALID_ARG;
	}

	// Only unencrypted MQTT is supported by this component.
	const char *broker_uri = CONFIG_OW_MQTT_BROKER_URI;
	if (strncmp(broker_uri, "mqtt://", 7) != 0 || broker_uri[7] == '\0')
	{
		ESP_LOGE(TAG, "Broker URI must use mqtt:// and contain a broker address");
		return ESP_ERR_INVALID_ARG;
	}

	s_mqtt_event_group = event_group;
	xEventGroupClearBits(s_mqtt_event_group,
						 MQTT_CLIENT_CONNECTED_BIT | MQTT_CLIENT_DISCONNECTED_BIT);

	// Configure MQTT client
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = broker_uri,

		// Last Will Testament (LWT) - broker sends "offline" if connection lost
		.session.last_will = {
			.topic = CONFIG_OW_MQTT_STATUS_TOPIC,
			.msg = "offline",
			.msg_len = 7,
			.qos = 1,
			.retain = true,
		},
	};

	// Set username and password if configured
	if (strlen(CONFIG_OW_MQTT_USERNAME) > 0)
	{
		mqtt_cfg.credentials.username = CONFIG_OW_MQTT_USERNAME;
	}
	if (strlen(CONFIG_OW_MQTT_PASSWORD) > 0)
	{
		mqtt_cfg.credentials.authentication.password = CONFIG_OW_MQTT_PASSWORD;
	}

	// Set client ID
	mqtt_cfg.credentials.client_id = CONFIG_OW_MQTT_CLIENT_ID;

	// Create MQTT client
	s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	if (s_mqtt_client == NULL)
	{
		ESP_LOGE(TAG, "Failed to initialize MQTT client");
		s_mqtt_event_group = NULL;
		return ESP_FAIL;
	}

	// Register event handler
	esp_ret = esp_mqtt_client_register_event(s_mqtt_client,
											 ESP_EVENT_ANY_ID,
											 mqtt_event_handler,
											 NULL);
	if (esp_ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to register MQTT event handler");
		esp_mqtt_client_destroy(s_mqtt_client);
		s_mqtt_client = NULL;
		s_mqtt_event_group = NULL;
		return esp_ret;
	}

	// Start MQTT client
	esp_ret = esp_mqtt_client_start(s_mqtt_client);
	if (esp_ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to start MQTT client");
		esp_mqtt_client_destroy(s_mqtt_client);
		s_mqtt_client = NULL;
		s_mqtt_event_group = NULL;
		return esp_ret;
	}

	ESP_LOGI(TAG, "MQTT client started, connecting to %s", broker_uri);

	return ESP_OK;
}

// Stop MQTT client
esp_err_t mqtt_client_stop(void)
{
	esp_err_t esp_ret;

	// Print message
	ESP_LOGI(TAG, "Stopping MQTT client...");

	// Check if client exists
	if (s_mqtt_client == NULL)
	{
		ESP_LOGI(TAG, "MQTT client already stopped");
		return ESP_OK;
	}

	// Stop MQTT client
	esp_ret = esp_mqtt_client_stop(s_mqtt_client);
	if (esp_ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to stop MQTT client");
		return esp_ret;
	}

	// Destroy MQTT client
	esp_ret = esp_mqtt_client_destroy(s_mqtt_client);
	if (esp_ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to destroy MQTT client");
		return esp_ret;
	}

	s_mqtt_client = NULL;

	// Clear event group bits
	if (s_mqtt_event_group != NULL)
	{
		xEventGroupClearBits(s_mqtt_event_group,
							 MQTT_CLIENT_CONNECTED_BIT | MQTT_CLIENT_DISCONNECTED_BIT);
	}
	s_mqtt_event_group = NULL;

	ESP_LOGI(TAG, "MQTT client stopped");

	return ESP_OK;
}

// Publish message to MQTT topic
int mqtt_client_publish(const char *topic, const char *data, int len, int qos, int retain)
{
	if (s_mqtt_client == NULL)
	{
		ESP_LOGE(TAG, "MQTT client not initialized");
		return -1;
	}

	int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain);
	if (msg_id < 0)
	{
		ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
	}

	return msg_id;
}

// Subscribe to MQTT topic
int mqtt_client_subscribe(const char *topic, int qos)
{
	if (s_mqtt_client == NULL)
	{
		ESP_LOGE(TAG, "MQTT client not initialized");
		return -1;
	}

	int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
	if (msg_id < 0)
	{
		ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
	}

	return msg_id;
}

// Unsubscribe from MQTT topic
int mqtt_client_unsubscribe(const char *topic)
{
	if (s_mqtt_client == NULL)
	{
		ESP_LOGE(TAG, "MQTT client not initialized");
		return -1;
	}

	int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
	if (msg_id < 0)
	{
		ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic);
	}

	return msg_id;
}
