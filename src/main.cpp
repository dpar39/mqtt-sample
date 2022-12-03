/*
 Sample application that publish MQTT messages on a timer loop and subscribes for messages on a separate topic
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mosquitto.h>

#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

std::atomic_bool StopPublisherLoop = false;
std::condition_variable SyncConditionVar;

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
    /* Print out the connection result. mosquitto_connack_string() produces an
     * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
     * clients is mosquitto_reason_string(). */
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

    int rc = mosquitto_subscribe(mosq, nullptr, consume_topic, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
        printMosquittoError(rc, "Error subscribing");
        /* We might as well disconnect if we were unable to subscribe */
        mosquitto_disconnect(mosq);
    }
}

/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void onSubscribe(struct mosquitto * mosq, void * /*user_data*/, int /*mid*/, int qos_count, const int * granted_qos)
{
    /* In this example we only subscribe to a single topic at once, but a
     * SUBSCRIBE can contain many topics at once, so this is one way to check
     * them all. */
    bool have_subscription = false;
    for (int i = 0; i < qos_count; i++) {
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

const char * getEnvVarOrDefault(const char * var_name, const char * default_val = nullptr)
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
    /* NOLINTNEXTLINE(hicpp-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays) */
    char payload[20];

    /* Get our pretend data */
    int temp = getTemperature();
    snprintf(payload, sizeof(payload), "%d", temp); // NOLINT(cert-err33-c)

    std::cout << "Publishing message: " << payload << std::endl;
    int rc = mosquitto_publish(mosq, nullptr, topic_name, strlen(payload), payload, 0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        printMosquittoError(rc, "Error publishing");
    }
}

void setupSigintHandler(struct sigaction * sa)
{
    /* NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) */
    sa->sa_handler = [](int /*sig*/) {
        StopPublisherLoop = true;
        SyncConditionVar.notify_all();
    };
    sa->sa_flags = 0;
    sigaction(SIGINT, sa, nullptr);
    sigaction(SIGTERM, sa, nullptr);
}

int main(int argc, char * argv[])
{
    /* Input parameters */
    const auto * host = getEnvVarOrDefault("IO_HOST", "localhost");
    const int port = std::stoi(getEnvVarOrDefault("IO_PORT", "1883"));
    const auto * device_id = getEnvVarOrDefault("IO_DEVICE_ID", "darien-pubsub-client");
    const auto * username = getEnvVarOrDefault("IO_USER");
    const auto * password = getEnvVarOrDefault("IO_KEY");
    const auto * publish_topic = getEnvVarOrDefault("IO_PUBLISH_TOPIC", "publish_feed");
    const auto * consume_topic = getEnvVarOrDefault("IO_CONSUME_TOPIC", "consume_feed");
    const int num_messages_to_send = std::stoi(getEnvVarOrDefault("IO_MESSAGE_COUNT", "1"));
    const float message_period_sec = std::stof(getEnvVarOrDefault("IO_MESSAGE_PERIOD_SECONDS", "3.0"));

    /* TLS */
    const auto * ca_file = getEnvVarOrDefault("IO_CAFILE");
    const auto * ca_path = getEnvVarOrDefault("IO_CAPATH");
    const auto * cert_file = getEnvVarOrDefault("IO_CERTFILE");
    const auto * key_file = getEnvVarOrDefault("IO_KEYFILE");

    struct sigaction sa = {};
    setupSigintHandler(&sa);

    /* Required before calling other mosquitto functions */
    mosquitto_lib_init();

    /* Create a new client instance.
     * id = device id that is registered with the broker
     * clean session = true -> the broker should remove old sessions when we connect
     * obj = consume_topic -> pass the consume topic name as user data  */
    auto * mosq = mosquitto_new(device_id, true, (void *)consume_topic);
    if (mosq == nullptr) {
        (void)fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    /* Configure callbacks. This should be done before connecting ideally. */
    mosquitto_connect_callback_set(mosq, onConnect);
    mosquitto_subscribe_callback_set(mosq, onSubscribe);
    mosquitto_message_callback_set(mosq, onMessage);

    /* Set username and password before connecting */
    if (username && password && strlen(username) && strlen(password)) {
        mosquitto_username_pw_set(mosq, username, password);
    }
    if (ca_file || ca_path || cert_file || key_file) {
        auto lambda = [](char * /*buf*/, int /*size*/, int /*rwflag*/, void * /*userdata*/) -> int { return 0; };
        mosquitto_tls_set(mosq, ca_file, ca_path, cert_file, key_file, nullptr);
    }

    /* Connect to the MQTT broker */
    int rc = mosquitto_connect(mosq, host, port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        printMosquittoError(rc, "Failed to connect");
        return 1;
    }

    rc = mosquitto_loop_start(mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        printMosquittoError(rc, "Failed to start loop");
        return 1;
    }

    std::thread publisher([&] {
        int num_messages = 0;
        std::mutex m;
        auto period_us = std::chrono::microseconds(static_cast<int64_t>(message_period_sec * 1000000));
        {
            std::unique_lock lk(m);
            SyncConditionVar.wait_for(lk, period_us, []() -> bool { return StopPublisherLoop; });
        }
        auto start_tp = std::chrono::steady_clock::now();
        while ((num_messages_to_send <= 0 || num_messages < num_messages_to_send) && !StopPublisherLoop) {
            publishSensorData(mosq, publish_topic);
            ++num_messages;
            auto sleep_until = start_tp + num_messages * period_us;
            std::unique_lock lk(m);
            SyncConditionVar.wait_until(lk, sleep_until, []() -> bool { return StopPublisherLoop; });
        }
    });
    publisher.join();

    mosquitto_loop_stop(mosq, true);
    mosquitto_lib_cleanup();
    return 0;
}