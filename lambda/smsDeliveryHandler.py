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

import sys
import gzip
from StringIO import StringIO
import json
import boto3
import os

# iotData = boto3.client('iot-data', region_name='ap-southeast-1')
iotData = boto3.client('iot-data')
guestInfoTableName = os.environ['GUEST_INFO_TABLE_NAME']
guestInfoTable = boto3.resource('dynamodb').Table(guestInfoTableName)

def sendCommandToLock(command):
    iotResponse = iotData.publish(
        topic='locks/commands',
        payload=command
    )
    print(iotResponse)
    return 0

def lambda_handler(event, context):
    try:
        #capture the CloudWatch log data
        logData = str(event['awslogs']['data'])
        # print(logData)
        
        #decode and unzip the log data
        decodedData = gzip.GzipFile(fileobj=StringIO(logData.decode('base64','strict'))).read()
        print(decodedData)
        
        #convert the log data from JSON into a dictionary
        jsonData = json.loads(decodedData)
        jsonMessageDetails = json.loads(jsonData['logEvents'][0]['message'])
        print(jsonMessageDetails)
        messageId = jsonMessageDetails['notification']['messageId']
        deliveryStatus = jsonMessageDetails['status']
        print("Message ID: " + messageId)
        print("Delivery Status: " + deliveryStatus)
        guestInfoItem = guestInfoTable.get_item(
            Key={
                'GuestId': 1
            }
        )
        print("MessageId: " + str(guestInfoItem['Item']['MessageId']))
        if(str(guestInfoItem['Item']['MessageId']) == messageId and deliveryStatus == "SUCCESS"):
            sendCommandToLock('ASK SECRET')
        else:
            sendCommandToLock('ASK SECRET')
            # sendCommandToLock('SMS FAILED')
    except:
        print "Unexpected error:", sys.exc_info()[0]
    return "done"
