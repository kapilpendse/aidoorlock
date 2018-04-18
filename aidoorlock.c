/*
 * Copyright 2017 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * Licensed under the Amazon Software License (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *   http://aws.amazon.com/asl/

 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file aidoorlock.c
 * @brief Demo program to showcase AWS IoT Device SDK, Polly, Lex & Rekognition
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <signal.h>
#include <memory.h>
#include <sys/time.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "constants.h"

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "certs";

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

static char passcode[5] = "0000";
AWS_IoT_Client client;

void getSelfIP(char *iface, char* ip) {
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	//printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	sprintf(ip, "%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	close(fd);
}

void onStartup() {
	IOT_INFO("Announcing startup");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_READY "\" &");

	//publish own IP address to topic "locks/ip"
	IoT_Error_t rc = SUCCESS;
	char cPayload[100];

	char wlan0_ip[50] = "";
	getSelfIP("wlan0", wlan0_ip);
	printf("wlan0 IP address is %s", wlan0_ip);
	IoT_Publish_Message_Params paramsQOS1;
	paramsQOS1.qos = QOS1;
	paramsQOS1.payload = (void *) cPayload;
	paramsQOS1.isRetained = 0;
	sprintf(cPayload, "wlan0 IP address is %s", wlan0_ip);
	paramsQOS1.payloadLen = strlen(cPayload);
	rc = aws_iot_mqtt_publish(&client, TOPIC_LOCKS_IP, strlen(TOPIC_LOCKS_IP), &paramsQOS1);
	if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
		IOT_WARN("QOS1 publish ack not received.\n");
		rc = SUCCESS;
	}

	char eth0_ip[50] = "";
	getSelfIP("eth0", eth0_ip);
	printf("eth0 IP address is %s", eth0_ip);
	paramsQOS1.qos = QOS1;
	paramsQOS1.payload = (void *) cPayload;
	paramsQOS1.isRetained = 0;
	sprintf(cPayload, "eth0 IP address is %s", eth0_ip);
	paramsQOS1.payloadLen = strlen(cPayload);
	rc = aws_iot_mqtt_publish(&client, TOPIC_LOCKS_IP, strlen(TOPIC_LOCKS_IP), &paramsQOS1);
	if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
		IOT_WARN("QOS1 publish ack not received.\n");
		rc = SUCCESS;
	}

}

void cmdHandlerCapturePhoto(IoT_Publish_Message_Params *params) {
	IOT_INFO("Capture Photo");
	//important to add '&' at end of command so that the MQTT subscription of this program does not get blocked (and timed out)
	system("sh `pwd`/scripts/capture.sh \"" POLLY_PROMPT_LOOK_AT_CAMERA "\" \"" POLLY_PROMPT_WAIT_A_MOMENT "\" \"" AWS_HOST_REGION "\" \"" AWS_S3_BUCKET_NAME "\" &");
}

void cmdHandlerFrFailure(IoT_Publish_Message_Params *params) {
	IOT_INFO("Facial Recognition Failed");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_FR_FAILURE "\" &");
}

void cmdHandlerUpdatePasscode(IoT_Publish_Message_Params *params) {
	IOT_INFO("Update Passcode");
	char *pPasscode = ((char*)params->payload)+strlen(CMD_UPDATE_PASSCODE)+1;
	strncpy(passcode, pPasscode, 4);
	IOT_INFO("New passcode is %s", passcode);
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_SENDING_SMS "\" &");
}

void cmdHandlerAskSecret(IoT_Publish_Message_Params *params) {
	IOT_INFO("Ask Secret");
	char command[1000];
	bzero(command, sizeof(command));
	sprintf(command,
		"%s %s %s %s %s %s",
		"sh `pwd`/scripts/passcode.sh ",
		passcode,
		" \"" POLLY_PROMPT_ASK_SECRET "\"",
		" \"" POLLY_PROMPT_ALLOW_ACCESS "\"",
		" \"" POLLY_PROMPT_DENY_ACCESS "\"",
		" \"" AWS_HOST_REGION "\" &");
	system(command);
}

void cmdHandlerAllowAccess(IoT_Publish_Message_Params *params) {
	IOT_INFO("Allow Access");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_ALLOW_ACCESS "\" &");
}

void cmdHandlerDenyAccess(IoT_Publish_Message_Params *params) {
	IOT_INFO("Deny Access");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_DENY_ACCESS "\" &");
}

void cmdHandlerSMSFailed(IoT_Publish_Message_Params *params) {
	IOT_INFO("SMS Failed");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_SMS_FAILED "\" &");
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("Subscribe callback");
	IOT_INFO("%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, params->payload);
	if(strncmp((char*)params->payload, CMD_CAPTURE_PHOTO, (int)params->payloadLen) == 0) {
		cmdHandlerCapturePhoto(params);
	}
	else if(strncmp((char*)params->payload, CMD_FR_FAILURE, (int)params->payloadLen) == 0) {
		cmdHandlerFrFailure(params);
	}
	else if(strncmp((char*)params->payload, CMD_UPDATE_PASSCODE, strlen(CMD_UPDATE_PASSCODE)) == 0) {
		cmdHandlerUpdatePasscode(params);
	}
	else if(strncmp((char*)params->payload, CMD_ASK_SECRET, (int)params->payloadLen) == 0) {
		cmdHandlerAskSecret(params);
	}
	else if(strncmp((char*)params->payload, CMD_ALLOW_ACCESS, (int)params->payloadLen) == 0) {
		cmdHandlerAllowAccess(params);
	}
	else if(strncmp((char*)params->payload, CMD_DENY_ACCESS, (int)params->payloadLen) == 0) {
		cmdHandlerDenyAccess(params);
	}
	else if(strncmp((char*)params->payload, CMD_SMS_FAILED, (int)params->payloadLen) == 0) {
		cmdHandlerSMSFailed(params);
	}
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

void parseInputArgsForConnectParams(int argc, char **argv) {
	int opt;

	while(-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) {
		switch(opt) {
			case 'h':
				strcpy(HostAddress, optarg);
				IOT_DEBUG("Host %s", optarg);
				break;
			case 'p':
				port = atoi(optarg);
				IOT_DEBUG("arg %s", optarg);
				break;
			case 'c':
				strcpy(certDirectory, optarg);
				IOT_DEBUG("cert root directory %s", optarg);
				break;
			case 'x':
				publishCount = atoi(optarg);
				IOT_DEBUG("publish %s times\n", optarg);
				break;
			case '?':
				if(optopt == 'c') {
					IOT_ERROR("Option -%c requires an argument.", optopt);
				} else if(isprint(optopt)) {
					IOT_WARN("Unknown option `-%c'.", optopt);
				} else {
					IOT_WARN("Unknown option character `\\x%x'.", optopt);
				}
				break;
			default:
				IOT_ERROR("Error in command line argument parsing");
				break;
		}
	}

}

int main(int argc, char **argv) {
	bool infinitePublishFlag = true;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cPayload[100];

	int32_t i = 0;

	IoT_Error_t rc = FAILURE;

	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IoT_Publish_Message_Params paramsQOS0;
	IoT_Publish_Message_Params paramsQOS1;

	parseInputArgsForConnectParams(argc, argv);

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	getcwd(CurrentWD, sizeof(CurrentWD));
	snprintf(rootCA, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 10;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&client, &connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
		return rc;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, TOPIC_LOCKS_CMD, strlen(TOPIC_LOCKS_CMD), QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}

	sprintf(cPayload, "%s : %d ", "hello from SDK", i);

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;

	paramsQOS1.qos = QOS1;
	paramsQOS1.payload = (void *) cPayload;
	paramsQOS1.isRetained = 0;

	if(publishCount != 0) {
		infinitePublishFlag = false;
	}

	//Announce readiness
	onStartup();

	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
		  && (publishCount > 0 || infinitePublishFlag)) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		// IOT_INFO("-->sleep");
		sleep(1);
		// sprintf(cPayload, "%s : %d ", "hello from SDK QOS0", i++);
		// paramsQOS0.payloadLen = strlen(cPayload);
		// rc = aws_iot_mqtt_publish(&client, "sdkTest/sub", 11, &paramsQOS0);
		// if(publishCount > 0) {
		// 	publishCount--;
		// }

		// sprintf(cPayload, "%s : %d ", "hello from SDK QOS1", i++);
		// paramsQOS1.payloadLen = strlen(cPayload);
		// rc = aws_iot_mqtt_publish(&client, "sdkTest/sub", 11, &paramsQOS1);
		// if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
		// 	IOT_WARN("QOS1 publish ack not received.\n");
		// 	rc = SUCCESS;
		// }
		// if(publishCount > 0) {
		// 	publishCount--;
		// }
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop.\n");
	} else {
		IOT_INFO("Publish done\n");
	}

	return rc;
}
