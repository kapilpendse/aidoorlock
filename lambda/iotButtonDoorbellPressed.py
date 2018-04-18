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

import boto3

iotData = boto3.client('iot-data')

def sendCommandToLock(command):
    iotResponse = iotData.publish(
        topic='locks/commands',
        payload=command
    )
    print(iotResponse)
    return 0

def lambda_handler(event, context):
    sendCommandToLock('CAPTURE PHOTO')
    return 'doorbell pressed, face recognition activated'
