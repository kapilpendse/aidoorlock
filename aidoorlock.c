/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the aws_iot_config.h file and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - TOPIC_LOCKS_CMD
 *
 * If all the certs are correct, you should see the messages received by the application in a loop.
 *
 * The application takes in the certificate path, host name , port and the number of times the publish should happen.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <signal.h>
#include <memory.h>
#include <sys/time.h>
#include <limits.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"

#include "constants.h"

static char passcode[5] = "0000";

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

int onStartup() {
	INFO("Announcing startup");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_READY "\" &");

	//publish own IP address to topic "locks/ip"
	IoT_Error_t rc = NONE_ERROR;
	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
	char cPayload[100];

	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = "locks/ip";

	char wlan0_ip[50] = "";
	getSelfIP("wlan0", wlan0_ip);
	sprintf(cPayload, "wlan0 IP address is %s", wlan0_ip);
	Msg.pPayload = (void *) cPayload;
	Msg.PayloadLen = strlen(cPayload) + 1;

	Params.MessageParams = Msg;
	rc = aws_iot_mqtt_publish(&Params);
	if(rc != NONE_ERROR) {
		printf("Error publishing IP address to 'locks/ip'\n");
	}

	printf("wlan0 IP address is %s", wlan0_ip);

	char eth0_ip[50] = "";
	getSelfIP("eth0", eth0_ip);
	sprintf(cPayload, "eth0 IP address is %s", eth0_ip);
	Msg.pPayload = (void *) cPayload;
	Msg.PayloadLen = strlen(cPayload) + 1;

	Params.MessageParams = Msg;
	rc = aws_iot_mqtt_publish(&Params);
	if(rc != NONE_ERROR) {
		printf("Error publishing IP address to 'locks/ip'\n");
	}

	printf("eth0 IP address is %s", eth0_ip);
}

int cmdHandlerCapturePhoto(MQTTCallbackParams params) {
	INFO("Capture Photo");
	//important to add '&' at end of command so that the MQTT subscription of this program does not get blocked (and timed out)
	system("sh `pwd`/scripts/capture.sh \"" POLLY_PROMPT_LOOK_AT_CAMERA "\" \"" POLLY_PROMPT_WAIT_A_MOMENT "\" &");
}

int cmdHandlerFrFailure(MQTTCallbackParams params) {
	INFO("Facial Recognition Failed");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_FR_FAILURE "\" &");
}

int cmdHandlerUpdatePasscode(MQTTCallbackParams params) {
	INFO("Update Passcode");
	char *pPasscode = ((char*)params.MessageParams.pPayload)+strlen(CMD_UPDATE_PASSCODE)+1;
	strncpy(passcode, pPasscode, 4);
	INFO("New passcode is %s", passcode);
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_SENDING_SMS "\" &");
}

int cmdHandlerAskSecret(MQTTCallbackParams params) {
	INFO("Ask Secret");
	char command[1000];
	bzero(command, sizeof(command));
	sprintf(command,
		"%s %s %s %s %s",
		"sh `pwd`/scripts/passcode.sh ",
		passcode,
		" \"" POLLY_PROMPT_ASK_SECRET "\"",
		" \"" POLLY_PROMPT_ALLOW_ACCESS "\"",
		" \"" POLLY_PROMPT_DENY_ACCESS "\" &");
	system(command);
	//system("sh `pwd`/scripts/passcode.sh \"" POLLY_PROMPT_ASK_SECRET "\" &");
}

int cmdHandlerAllowAccess(MQTTCallbackParams params) {
	INFO("Allow Access");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_ALLOW_ACCESS "\" &");
}

int cmdHandlerDenyAccess(MQTTCallbackParams params) {
	INFO("Deny Access");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_DENY_ACCESS "\" &");
}

int cmdHandlerSMSFailed(MQTTCallbackParams params) {
	INFO("SMS Failed");
	system("python `pwd`/scripts/speak.py \"" POLLY_PROMPT_SMS_FAILED "\" &");
}

int MQTTcallbackHandler(MQTTCallbackParams params) {

	INFO("Subscribe callback");
	INFO("%.*s\t%.*s",
			(int)params.TopicNameLen, params.pTopicName,
			(int)params.MessageParams.PayloadLen, (char*)params.MessageParams.pPayload);
	if(strncmp((char*)params.MessageParams.pPayload, CMD_CAPTURE_PHOTO, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerCapturePhoto(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_FR_FAILURE, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerFrFailure(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_UPDATE_PASSCODE, strlen(CMD_UPDATE_PASSCODE)) == 0) {
		cmdHandlerUpdatePasscode(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_ASK_SECRET, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerAskSecret(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_ALLOW_ACCESS, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerAllowAccess(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_DENY_ACCESS, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerDenyAccess(params);
	}
	else if(strncmp((char*)params.MessageParams.pPayload, CMD_SMS_FAILED, (int)params.MessageParams.PayloadLen) == 0) {
		cmdHandlerSMSFailed(params);
	}

	return 0;
}

void disconnectCallbackHandler(void) {
	WARN("MQTT Disconnect");
	IoT_Error_t rc = NONE_ERROR;
	if(aws_iot_is_autoreconnect_enabled()){
		INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	}else{
		WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect();
		if(RECONNECT_SUCCESSFUL == rc){
			WARN("Manual Reconnect Successful");
		}else{
			WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "../../certs";

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

void parseInputArgsForConnectParams(int argc, char** argv) {
	int opt;

	while (-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) {
		switch (opt) {
		case 'h':
			strcpy(HostAddress, optarg);
			DEBUG("Host %s", optarg);
			break;
		case 'p':
			port = atoi(optarg);
			DEBUG("arg %s", optarg);
			break;
		case 'c':
			strcpy(certDirectory, optarg);
			DEBUG("cert root directory %s", optarg);
			break;
		case 'x':
			publishCount = atoi(optarg);
			DEBUG("publish %s times\n", optarg);
			break;
		case '?':
			if (optopt == 'c') {
				ERROR("Option -%c requires an argument.", optopt);
			} else if (isprint(optopt)) {
				WARN("Unknown option `-%c'.", optopt);
			} else {
				WARN("Unknown option character `\\x%x'.", optopt);
			}
			break;
		default:
			ERROR("Error in command line argument parsing");
			break;
		}
	}

}

int main(int argc, char** argv) {
	IoT_Error_t rc = NONE_ERROR;
	int32_t i = 0;
	bool infinitePublishFlag = true;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
	char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
	char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

	parseInputArgsForConnectParams(argc, argv);

	INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	getcwd(CurrentWD, sizeof(CurrentWD));
	sprintf(rootCA, "%s/%s/%s", CurrentWD, certDirectory, cafileName);
	sprintf(clientCRT, "%s/%s/%s", CurrentWD, certDirectory, clientCRTName);
	sprintf(clientKey, "%s/%s/%s", CurrentWD, certDirectory, clientKeyName);

	DEBUG("rootCA %s", rootCA);
	DEBUG("clientCRT %s", clientCRT);
	DEBUG("clientKey %s", clientKey);

	MQTTConnectParams connectParams = MQTTConnectParamsDefault;

	connectParams.KeepAliveInterval_sec = 10;
	connectParams.isCleansession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = "CSDK-test-device";
	connectParams.pHostURL = HostAddress;
	connectParams.port = port;
	connectParams.isWillMsgPresent = false;
	connectParams.pRootCALocation = rootCA;
	connectParams.pDeviceCertLocation = clientCRT;
	connectParams.pDevicePrivateKeyLocation = clientKey;
	connectParams.mqttCommandTimeout_ms = 2000;
	connectParams.tlsHandshakeTimeout_ms = 5000;
	connectParams.isSSLHostnameVerify = true; // ensure this is set to true for production
	connectParams.disconnectHandler = disconnectCallbackHandler;

	INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&connectParams);
	if (NONE_ERROR != rc) {
		ERROR("Error(%d) connecting to %s:%d", rc, connectParams.pHostURL, connectParams.port);
		goto EXIT_CLEANUP;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(true);
	if (NONE_ERROR != rc) {
		ERROR("Unable to set Auto Reconnect to true - %d", rc);
		goto EXIT_CLEANUP;
	}

	MQTTSubscribeParams subParams = MQTTSubscribeParamsDefault;
	subParams.mHandler = MQTTcallbackHandler;
	subParams.pTopic = TOPIC_LOCKS_CMD;
	subParams.qos = QOS_0;

	if (NONE_ERROR == rc) {
		INFO("Subscribing...");
		rc = aws_iot_mqtt_subscribe(&subParams);
		if (NONE_ERROR != rc) {
			ERROR("Error subscribing");
			goto EXIT_CLEANUP;
		}
	}

	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
	char cPayload[100];
	sprintf(cPayload, "%s : %d ", "hello from SDK", i);
	Msg.pPayload = (void *) cPayload;

	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = TOPIC_LOCKS_CMD;

	if (publishCount != 0) {
		infinitePublishFlag = false;
	}

	//Announce readiness
	onStartup();

	while ((NETWORK_ATTEMPTING_RECONNECT == rc || RECONNECT_SUCCESSFUL == rc || NONE_ERROR == rc)
			&& (publishCount > 0 || infinitePublishFlag)) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc){
			//INFO("-->sleep");
			sleep(1);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}
		//INFO("-->sleep");
		sleep(1);
/*
		sprintf(cPayload, "%s : %d ", "hello from SDK", i++);
		Msg.PayloadLen = strlen(cPayload) + 1;
		Params.MessageParams = Msg;
		rc = aws_iot_mqtt_publish(&Params);
		if (publishCount > 0) {
			publishCount--;
		}
*/
	}

EXIT_CLEANUP:
	if (NONE_ERROR != rc) {
		ERROR("An error occurred, exiting.\n");
	} else {
		INFO("Exiting without errors.\n");
	}

	return rc;
}

