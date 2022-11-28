/*******************************************************************************
 * Copyright (c) 2012, 2022 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *   https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - add full capability
 *******************************************************************************/

#include "MQTTClient.h"
#include "MQTTClientPersistence.h"
#include "pubsub_opts.h"

#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <time.h>

#include <unistd.h>

#if defined(_WIN32)
#define sleep Sleep
#else
#include <sys/time.h>
#endif

volatile int ToStop = 0;

void cfinish(int sig)
{
    signal(SIGINT, NULL);
    ToStop = 1;
}

int64_t getMicroseconds()
{
    struct timespec tms;
    if (!timespec_get(&tms, TIME_UTC)) {
        return -1;
    }
    return tms.tv_sec * 1000000 + llround(tms.tv_nsec / 1000.0);
}

char * getEnvVar(char * var_name, char * default_value)
{
    char * var_value = getenv(var_name);
    return var_value ? var_value : default_value;
}

struct PubSubOpts opts = { 1,
                           0,
                           0,
                           0,
                           "\n",
                           100, /* debug/app options */
                           NULL,
                           NULL,
                           1,
                           0,
                           0, /* message options */
                           MQTTVERSION_DEFAULT,
                           NULL,
                           "paho-cs-pub",
                           0,
                           0,
                           NULL,
                           NULL,
                           "localhost",
                           "1883",
                           NULL,
                           10, /* MQTT options */
                           NULL,
                           NULL,
                           0,
                           0, /* will options */
                           0,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL, /* TLS options */
                           0,
                           { NULL, NULL }, /* MQTT V5 options */
                           NULL,
                           NULL, /* HTTP and HTTPS proxies */
                           3.f, /* send a message every 3 seconds */
                           0 /* send messages until stopped */ };

int mqttConnect(MQTTClient client)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;
    int rc = 0;

    if (opts.verbose) {
        printf("Connecting...\n");
    }

    if (opts.mqtt_version == MQTTVERSION_5) {
        MQTTClient_connectOptions conn_opts5 = MQTTClient_connectOptions_initializer5;
        conn_opts = conn_opts5;
    }

    conn_opts.keepAliveInterval = opts.keepalive;
    conn_opts.username = opts.username;
    conn_opts.password = opts.password;
    conn_opts.MQTTVersion = opts.mqtt_version;
    conn_opts.httpProxy = opts.http_proxy;
    conn_opts.httpsProxy = opts.https_proxy;

    if (opts.will_topic) /* will options */
    {
        will_opts.message = opts.will_payload;
        will_opts.topicName = opts.will_topic;
        will_opts.qos = opts.will_qos;
        will_opts.retained = opts.will_retain;
        conn_opts.will = &will_opts;
    }

    if (opts.connection && (strncmp(opts.connection, "ssl://", 6) == 0 || strncmp(opts.connection, "wss://", 6) == 0)) {
        if (opts.insecure) {
            ssl_opts.verify = 0;
        } else {
            ssl_opts.verify = 1;
        }
        ssl_opts.CApath = opts.capath;
        ssl_opts.keyStore = opts.cert;
        ssl_opts.trustStore = opts.cafile;
        ssl_opts.privateKey = opts.key;
        ssl_opts.privateKeyPassword = opts.keypass;
        ssl_opts.enabledCipherSuites = opts.ciphers;
        conn_opts.ssl = &ssl_opts;
    }

    if (opts.mqtt_version == MQTTVERSION_5) {
        MQTTProperties props = MQTTProperties_initializer;
        MQTTProperties will_props = MQTTProperties_initializer;
        MQTTResponse response = MQTTResponse_initializer;

        conn_opts.cleanstart = 1;
        response = MQTTClient_connect5(client, &conn_opts, &props, &will_props);
        rc = response.reasonCode;
        MQTTResponse_free(response);
    } else {
        conn_opts.cleansession = 1;
        rc = MQTTClient_connect(client, &conn_opts);
    }

    if (opts.verbose && rc == MQTTCLIENT_SUCCESS) {
        printf("Connected!\n");
    } else if (rc != MQTTCLIENT_SUCCESS && !opts.quiet) {
        (void)fprintf(stderr, "Connect failed return code: %s\n", MQTTClient_strerror(rc));
    }

    return rc;
}

void mqttDisconnect(MQTTClient * client)
{
    int rc = 0;
    if (opts.verbose) {
        printf("Disconnecting...\n");
    }
    if (opts.mqtt_version == MQTTVERSION_5) {
        rc = MQTTClient_disconnect5(*client, 0, MQTTREASONCODE_SUCCESS, NULL);
    } else {
        rc = MQTTClient_disconnect(*client, 0);
    }

    if (opts.verbose) {
        printf("Disconnected!\n");
    }
    MQTTClient_destroy(client);
}

// NOLINTNEXTLINE(misc-unused-parameters)
int messageArrived(void * context, char * topic_name, int topic_len, MQTTClient_message * m)
{
    /* not expecting any messages */
    return 1;
}

void traceCallback(enum MQTTCLIENT_TRACE_LEVELS level, char * message)
{
    (void)fprintf(stderr, "Trace : %d, %s\n", level, message);
}

void initializeOptionsFromEnvironmentVars()
{
    opts.username = getEnvVar("IO_USER", NULL);
    opts.password = getEnvVar("IO_KEY", NULL);
    opts.host = getEnvVar("IO_HOST", "localhost");
    opts.port = getEnvVar("IO_PORT", "1883");
    opts.topic = getEnvVar("IO_TOPIC", NULL);
}

void addUserProperties(MQTTProperties * pub_props)
{
    MQTTProperty property;
    if (opts.message_expiry > 0) {
        property.identifier = MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL;
        property.value.integer4 = opts.message_expiry;
        MQTTProperties_add(pub_props, &property);
    }
    if (opts.user_property.name) {
        property.identifier = MQTTPROPERTY_CODE_USER_PROPERTY;
        property.value.data.data = opts.user_property.name;
        property.value.data.len = (int)strlen(opts.user_property.name);
        property.value.value.data = opts.user_property.value;
        property.value.value.len = (int)strlen(opts.user_property.value);
        MQTTProperties_add(pub_props, &property);
    }
}

int main(int argc, char ** argv)
{
    MQTTClient client;
    MQTTProperties pub_props = MQTTProperties_initializer;
    MQTTClient_createOptions create_opts = MQTTClient_createOptions_initializer;
    MQTTClient_deliveryToken last_token;
    char * buffer = NULL;
    int rc = 0;
    char * url;
    const char * version = NULL;
    int num_messages_sent = 0;
#if !defined(_WIN32)
    struct sigaction sa;
#endif
    const char * program_name = "paho_cs_pub";
    MQTTClient_nameValue * infos = MQTTClient_getVersionInfo();

    initializeOptionsFromEnvironmentVars();

    if (argc < 2) {
        usage(&opts, (PubSubOptsNameValue *)infos, program_name);
    }

    if (getopts(argc, argv, &opts) != 0) {
        usage(&opts, (PubSubOptsNameValue *)infos, program_name);
    }

    if (opts.connection) {
        url = opts.connection;
    } else {
        url = malloc(100);
        (void)sprintf(url, "%s:%s", opts.host, opts.port);
    }
    if (opts.verbose) {
        (void)fprintf(stdout, "URL is %s\n", url);
    }

    if (opts.tracelevel > 0) {
        MQTTClient_setTraceCallback(traceCallback);
        MQTTClient_setTraceLevel(opts.tracelevel);
    }

    if (opts.mqtt_version >= MQTTVERSION_5) {
        create_opts.MQTTVersion = MQTTVERSION_5;
    }
    rc = MQTTClient_createWithOptions(&client, url, opts.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        if (!opts.quiet) {
            (void)fprintf(stderr, "Failed to create client, return code: %s\n", MQTTClient_strerror(rc));
        }
        exit(EXIT_FAILURE);
    }

#if defined(_WIN32)
    signal(SIGINT, cfinish);
    signal(SIGTERM, cfinish);
#else
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = cfinish;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif

    rc = MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        if (!opts.quiet) {
            (void)fprintf(stderr, "Failed to set callbacks, return code: %s\n", MQTTClient_strerror(rc));
        }
        exit(EXIT_FAILURE);
    }

    if (mqttConnect(client) != MQTTCLIENT_SUCCESS) {
        goto exit;
    }

    if (opts.mqtt_version >= MQTTVERSION_5) {
        addUserProperties(&pub_props);
    }

    double start_us = getMicroseconds();

    while (!ToStop) {
        int data_len = 0;
        int delim_len = 0;

        if (opts.stdin_lines) {
            buffer = malloc(opts.maxdatalen);
            delim_len = (int)strlen(opts.delimiter);
            do {
                int c = getchar();
                if (c < 0) {
                    goto exit;
                }
                buffer[data_len++] = c;
                if (data_len > delim_len) {
                    if (strncmp(opts.delimiter, &buffer[data_len - delim_len], delim_len) == 0) {
                        break;
                    }
                }
            } while (data_len < opts.maxdatalen);
        } else if (opts.message) {
            if (buffer == NULL) {
                buffer = malloc((int)strlen(opts.message) + 100);
            }
            data_len = sprintf(buffer, "%s #%d", opts.message, num_messages_sent);
        } else if (opts.filename) {
            buffer = readfile(&data_len, &opts);
            if (buffer == NULL) {
                goto exit;
            }
        }
        if (opts.verbose) {
            (void)fprintf(stderr, "Publishing data of length %d\n", data_len);
        }

        if (opts.mqtt_version == MQTTVERSION_5) {
            MQTTResponse response = MQTTResponse_initializer;
            response = MQTTClient_publish5(client,
                                           opts.topic,
                                           data_len,
                                           buffer,
                                           opts.qos,
                                           opts.retained,
                                           &pub_props,
                                           &last_token);
            rc = response.reasonCode;
        } else {
            rc = MQTTClient_publish(client, opts.topic, data_len, buffer, opts.qos, opts.retained, &last_token);
        }

        ++num_messages_sent;
        if (opts.message_count > 0 && num_messages_sent >= opts.message_count) {
            ToStop = 1;
            break;
        }

        int64_t elapsed_us = getMicroseconds() - start_us;
        int64_t to_sleep_us = opts.message_interval_sec * 1000000.0 * num_messages_sent - elapsed_us;
        if (to_sleep_us >= 0) {
            (void)fprintf(stdout, "Sleeping for %" PRId64 " us ...\n", to_sleep_us);
            usleep(to_sleep_us);
        }

        if (rc != 0) {
            mqttConnect(client);
            if (opts.mqtt_version == MQTTVERSION_5) {
                MQTTResponse response = MQTTResponse_initializer;

                response = MQTTClient_publish5(client,
                                               opts.topic,
                                               data_len,
                                               buffer,
                                               opts.qos,
                                               opts.retained,
                                               &pub_props,
                                               &last_token);
                rc = response.reasonCode;
            } else {
                rc = MQTTClient_publish(client, opts.topic, data_len, buffer, opts.qos, opts.retained, &last_token);
            }
        }
        if (opts.qos > 0) {
            MQTTClient_yield();
        }
    }

    rc = MQTTClient_waitForCompletion(client, last_token, 5000);

exit:
    free(buffer);

    mqttDisconnect(&client);

    return EXIT_SUCCESS;
}
