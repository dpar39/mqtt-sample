/*
 * This example shows how to write a client that subscribes to a topic and does
 * not do anything other than handle the messages that are received.
 */

#include <mosquitto.h>

#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

void printMosquittoError(int rc, const char * error_prefix = nullptr)
{
    if (!error_prefix) {
        error_prefix = "Error";
    }
    std::cerr << error_prefix << ": " << (rc != 0 ? mosquitto_strerror(rc) : "") << std::endl;
}

/* Callback called when the client receives a CONNACK message from the broker. */
void onConnect(struct mosquitto * mosq, void * user_data, int reason_code)
{
    int rc;
    /* Print out the connection result. mosquitto_connack_string() produces an
     * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
     * clients is mosquitto_reason_string().
     */
    printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code != 0) {
        /* If the connection fails for any reason, we don't want to keep on
         * retrying in this example, so disconnect. Without this, the client
         * will attempt to reconnect. */
        mosquitto_disconnect(mosq);
    }

    /* Making subscriptions in the on_connect() callback means that if the
     * connection drops and is automatically resumed by the client, then the
     * subscriptions will be recreated when the client reconnects. */
    const char * consume_topic = static_cast<const char *>(user_data);

    rc = mosquitto_subscribe(mosq, nullptr, consume_topic, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
        printMosquittoError(rc, "Error subscribing");
        /* We might as well disconnect if we were unable to subscribe */
        mosquitto_disconnect(mosq);
    }
}

/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void onSubscribe(struct mosquitto * mosq, void * /*user_data*/, int /*mid*/, int qos_count, const int * granted_qos)
{
    int i;
    bool have_subscription = false;

    /* In this example we only subscribe to a single topic at once, but a
     * SUBSCRIBE can contain many topics at once, so this is one way to check
     * them all. */
    for (i = 0; i < qos_count; i++) {
        printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
        if (granted_qos[i] <= 2) {
            have_subscription = true;
        }
    }
    if (!have_subscription) {
        /* The broker rejected all of our subscriptions, we know we only sent
         * the one SUBSCRIBE, so there is no point remaining connected. */
        printMosquittoError(0, "Error: All subscriptions rejected.");
        mosquitto_disconnect(mosq);
    }
}

/* Callback called when the client receives a message. */
void onMessage(struct mosquitto * /*mosq*/, void * /*user_data*/, const struct mosquitto_message * msg)
{
    /* This blindly prints the payload, but the payload can be anything so take care. */
    printf("%s %d %s\n", msg->topic, msg->qos, static_cast<char *>(msg->payload));
}

const char * getEnvVarOrDefault(const char * var_name, const char * default_val)
{
    if (auto * value = std::getenv(var_name); value != nullptr) {
        return value;
    }
    return default_val;
}

int getTemperature()
{
    return random() % 100;
}

/* This function pretends to read some data from a sensor and publish it.*/
void publishSensorData(struct mosquitto * mosq, const char * topic_name)
{
    char payload[20]; // NOLINT(hicpp-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
    int temp;
    int rc;
    /* Get our pretend data */
    temp = getTemperature();
    snprintf(payload, sizeof(payload), "%d", temp); // NOLINT(cert-err33-c)

    std::cout << "Publishing message " << payload << std::endl;
    rc = mosquitto_publish(mosq, nullptr, topic_name, strlen(payload), payload, 0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        printMosquittoError(rc, "Error publishing");
    }
}

int main(int argc, char * argv[])
{
    // Input parameters
    const auto * host = getEnvVarOrDefault("IO_HOST", "io.adafruit.com");
    const int port = std::stoi(getEnvVarOrDefault("IO_PORT", "1883"));
    const auto * username = getEnvVarOrDefault("IO_USER", "dpar39");
    const auto * password = getEnvVarOrDefault("IO_KEY", nullptr);
    const auto * publish_topic = getEnvVarOrDefault("IO_PUBLISH_TOPIC", "dpar39/feeds/feed-two");
    const auto * consume_topic = getEnvVarOrDefault("IO_CONSUME_TOPIC", "dpar39/feeds/feed-one");

    struct mosquitto * mosq;
    int rc;

    /* Required before calling other mosquitto functions */
    mosquitto_lib_init();

    /* Create a new client instance.
     * id = NULL -> ask the broker to generate a client id for us
     * clean session = true -> the broker should remove old sessions when we connect
     * obj = consume_topic -> pass the consume topic name as user data
     */
    mosq = mosquitto_new(nullptr, true, (void *)consume_topic);
    if (mosq == nullptr) {
        (void)fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    /* Configure callbacks. This should be done before connecting ideally. */
    mosquitto_connect_callback_set(mosq, onConnect);
    mosquitto_subscribe_callback_set(mosq, onSubscribe);
    mosquitto_message_callback_set(mosq, onMessage);

    /* Set username and password before connecting */
    rc = mosquitto_username_pw_set(mosq, username, password);

    /* Connect to the MQTT broker */
    rc = mosquitto_connect(mosq, host, port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        printMosquittoError(rc);
        return 1;
    }

    rc = mosquitto_loop_start(mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        printMosquittoError(rc);
        return 1;
    }

    {
        std::jthread publisher([&] {
            int num_messages = 0;
            while (num_messages++ < 10) {
                publishSensorData(mosq, publish_topic);
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(3s);
            }
        });
    }

    mosquitto_loop_stop(mosq, true);
    mosquitto_lib_cleanup();
    return 0;
}