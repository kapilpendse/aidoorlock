#!/bin/bash

# Copyright 2017 Amazon.com, Inc. and its affiliates. All Rights Reserved.
#
# Licensed under the Amazon Software License (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#   http://aws.amazon.com/asl/
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.

cd /home/pi/deviceSDK/linux_mqtt_openssl/sample_apps/aidoorlock
echo "Starting AI Door Lock Daemon in 30 seconds" > /tmp/aidoorlock.log
sleep 30
su -c "./aidoorlock > /tmp/aidoorlock2.log &" -s /bin/sh pi
echo "Done" >> /tmp/aidoorlock.log

