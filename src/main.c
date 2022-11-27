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

int64_t get_microseconds()
{
    struct timespec tms;
    if (!timespec_get(&tms, TIME_UTC)) {
        return -1;
    }
    return tms.tv_sec * 1000000 + llround(tms.tv_nsec / 1000.0);
}

char * getEnvVar(char * varName, char * defaultValue)
{
    char * varValue = getenv(varName);
    return varValue ? varValue : defaultValue;
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

int myconnect(MQTTClient client)
{
    MQTTClient_connectOptions connOpts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions sslOpts = MQTTClient_SSLOptions_initializer;
    MQTTClient_willOptions willOpts = MQTTClient_willOptions_initializer;
    int rc = 0;

    if (opts.verbose) {
        printf("Connecting\n");
    }

    if (opts.MQTTVersion == MQTTVERSION_5) {
        MQTTClient_connectOptions connOpts5 = MQTTClient_connectOptions_initializer5;
        connOpts = connOpts5;
    }

    connOpts.keepAliveInterval = opts.keepalive;
    connOpts.username = opts.username;
    connOpts.password = opts.password;
    connOpts.MQTTVersion = opts.MQTTVersion;
    connOpts.httpProxy = opts.http_proxy;
    connOpts.httpsProxy = opts.https_proxy;

    if (opts.will_topic) /* will options */
    {
        willOpts.message = opts.will_payload;
        willOpts.topicName = opts.will_topic;
        willOpts.qos = opts.will_qos;
        willOpts.retained = opts.will_retain;
        connOpts.will = &willOpts;
    }

    if (opts.connection && (strncmp(opts.connection, "ssl://", 6) == 0 || strncmp(opts.connection, "wss://", 6) == 0)) {
        if (opts.insecure) {
            sslOpts.verify = 0;
        } else {
            sslOpts.verify = 1;
        }
        sslOpts.CApath = opts.capath;
        sslOpts.keyStore = opts.cert;
        sslOpts.trustStore = opts.cafile;
        sslOpts.privateKey = opts.key;
        sslOpts.privateKeyPassword = opts.keypass;
        sslOpts.enabledCipherSuites = opts.ciphers;
        connOpts.ssl = &sslOpts;
    }

    if (opts.MQTTVersion == MQTTVERSION_5) {
        MQTTProperties props = MQTTProperties_initializer;
        MQTTProperties willProps = MQTTProperties_initializer;
        MQTTResponse response = MQTTResponse_initializer;

        connOpts.cleanstart = 1;
        response = MQTTClient_connect5(client, &connOpts, &props, &willProps);
        rc = response.reasonCode;
        MQTTResponse_free(response);
    } else {
        connOpts.cleansession = 1;
        rc = MQTTClient_connect(client, &connOpts);
    }

    if (opts.verbose && rc == MQTTCLIENT_SUCCESS) {
        printf("Connected\n");
    } else if (rc != MQTTCLIENT_SUCCESS && !opts.quiet) {
        fprintf(stderr, "Connect failed return code: %s\n", MQTTClient_strerror(rc));
    }

    return rc;
}

int messageArrived(void * context, char * topicName, int topicLen, MQTTClient_message * m)
{
    /* not expecting any messages */
    return 1;
}

void traceCallback(enum MQTTCLIENT_TRACE_LEVELS level, char * message)
{
    fprintf(stderr, "Trace : %d, %s\n", level, message);
}

void initializeOptionsFromEnvironmentVars()
{
    opts.username = getEnvVar("IO_USER", NULL);
    opts.password = getEnvVar("IO_KEY", NULL);
    opts.host = getEnvVar("IO_HOST", "localhost");
    opts.port = getEnvVar("IO_PORT", "1883");
    opts.topic = getEnvVar("IO_TOPIC", NULL);
}

void addUserProperties(MQTTProperties * pubProps)
{
    MQTTProperty property;
    if (opts.message_expiry > 0) {
        property.identifier = MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL;
        property.value.integer4 = opts.message_expiry;
        MQTTProperties_add(pubProps, &property);
    }
    if (opts.user_property.name) {
        property.identifier = MQTTPROPERTY_CODE_USER_PROPERTY;
        property.value.data.data = opts.user_property.name;
        property.value.data.len = (int)strlen(opts.user_property.name);
        property.value.value.data = opts.user_property.value;
        property.value.value.len = (int)strlen(opts.user_property.value);
        MQTTProperties_add(pubProps, &property);
    }
}

int main(int argc, char ** argv)
{
    MQTTClient client;
    MQTTProperties pubProps = MQTTProperties_initializer;
    MQTTClient_createOptions createOpts = MQTTClient_createOptions_initializer;
    MQTTClient_deliveryToken lastToken;
    char * buffer = NULL;
    int rc = 0;
    char * url;
    const char * version = NULL;
    int numMessagesSent = 0;
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
    if (opts.verbose)
        fprintf("URL is %s\n", url);

    if (opts.tracelevel > 0) {
        MQTTClient_setTraceCallback(traceCallback);
        MQTTClient_setTraceLevel(opts.tracelevel);
    }

    if (opts.MQTTVersion >= MQTTVERSION_5) {
        createOpts.MQTTVersion = MQTTVERSION_5;
    }
    rc = MQTTClient_createWithOptions(&client, url, opts.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL, &createOpts);
    if (rc != MQTTCLIENT_SUCCESS) {
        if (!opts.quiet) {
            fprintf(stderr, "Failed to create client, return code: %s\n", MQTTClient_strerror(rc));
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
            fprintf(stderr, "Failed to set callbacks, return code: %s\n", MQTTClient_strerror(rc));
        }
        exit(EXIT_FAILURE);
    }

    if (myconnect(client) != MQTTCLIENT_SUCCESS) {
        goto exit;
    }

    if (opts.MQTTVersion >= MQTTVERSION_5) {
        addUserProperties(&pubProps);
    }

    double start_us = get_microseconds();

    while (!ToStop) {
        int dataLen = 0;
        int delimLen = 0;

        if (opts.stdin_lines) {
            buffer = malloc(opts.maxdatalen);
            delimLen = (int)strlen(opts.delimiter);
            do {
                int c = getchar();
                if (c < 0) {
                    goto exit;
                }
                buffer[dataLen++] = c;
                if (dataLen > delimLen) {
                    if (strncmp(opts.delimiter, &buffer[dataLen - delimLen], delimLen) == 0) {
                        break;
                    }
                }
            } while (dataLen < opts.maxdatalen);
        } else if (opts.message) {
            if (buffer == NULL) {
                buffer = malloc((int)strlen(opts.message) + 100);
            }
            dataLen = sprintf(buffer, "%s #%d", opts.message, numMessagesSent);
        } else if (opts.filename) {
            buffer = readfile(&dataLen, &opts);
            if (buffer == NULL) {
                goto exit;
            }
        }
        if (opts.verbose) {
            (void)fprintf(stderr, "Publishing data of length %d\n", dataLen);
        }

        if (opts.MQTTVersion == MQTTVERSION_5) {
            MQTTResponse response = MQTTResponse_initializer;
            response = MQTTClient_publish5(client,
                                           opts.topic,
                                           dataLen,
                                           buffer,
                                           opts.qos,
                                           opts.retained,
                                           &pubProps,
                                           &lastToken);
            rc = response.reasonCode;
        } else {
            rc = MQTTClient_publish(client, opts.topic, dataLen, buffer, opts.qos, opts.retained, &lastToken);
        }

        ++numMessagesSent;
        if (opts.num_messages > 0 && numMessagesSent >= opts.num_messages) {
            ToStop = 1;
            break;
        }

        int64_t elapsed_us = get_microseconds() - start_us;
        int64_t to_sleep_us = opts.message_interval_sec * 1000000.0 * numMessagesSent - elapsed_us;
        if (to_sleep_us >= 0) {
            (void)fprintf(stdout, "Sleeping for %" PRId64 " us ...\n", to_sleep_us);
            usleep(to_sleep_us);
        }

        if (rc != 0) {
            myconnect(client);
            if (opts.MQTTVersion == MQTTVERSION_5) {
                MQTTResponse response = MQTTResponse_initializer;

                response = MQTTClient_publish5(client,
                                               opts.topic,
                                               dataLen,
                                               buffer,
                                               opts.qos,
                                               opts.retained,
                                               &pubProps,
                                               &lastToken);
                rc = response.reasonCode;
            } else {
                rc = MQTTClient_publish(client, opts.topic, dataLen, buffer, opts.qos, opts.retained, &lastToken);
            }
        }
        if (opts.qos > 0) {
            MQTTClient_yield();
        }
    }

    rc = MQTTClient_waitForCompletion(client, lastToken, 5000);

exit:
    free(buffer);

    if (opts.MQTTVersion == MQTTVERSION_5) {
        rc = MQTTClient_disconnect5(client, 0, MQTTREASONCODE_SUCCESS, NULL);
    } else {
        rc = MQTTClient_disconnect(client, 0);
    }

    MQTTClient_destroy(&client);

    return EXIT_SUCCESS;
}
