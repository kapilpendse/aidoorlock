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
import boto3
import json
import random
import os
# from botocore.errorfactory import InvalidParameterException

# rekognition = boto3.client('rekognition', region_name='us-east-1')
# iotData = boto3.client('iot-data', region_name='ap-southeast-1')
rekognition = boto3.client('rekognition')
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

def generateNewPasscode():
    return str(random.randint(1000, 9999))

def saveNewPasscode(passcode):
    response = guestInfoTable.update_item(
        Key={
            'GuestId': 1
        },
        UpdateExpression="set Passcode = :p",
        ExpressionAttributeValues={
            ":p": passcode
        },
        ReturnValues='UPDATED_NEW'
    )
    # print("DDB operation: " + response)
    return 0

def sendPasscodeToGuest(passcode):
    try:
        sns = boto3.client('sns')
        guestInfoItem = guestInfoTable.get_item(
            Key={
                'GuestId': 1
            }
        )
        phonenumber = guestInfoItem['Item']['PhoneNumber']
        response = sns.publish(PhoneNumber = phonenumber, Message=passcode)
        print("SMS passcode sent to guest, message ID is " + response['MessageId'])
        response = guestInfoTable.update_item(
            Key={
                'GuestId': 1
            },
            UpdateExpression="set MessageId = :m",
            ExpressionAttributeValues={
                ":m": response['MessageId']
            },
            ReturnValues='UPDATED_NEW'
        )
    except AuthorizationErrorException:
        print("AuthorizationErrorException: does this lambda function's IAM role have access to SNS to send SMS?")
    return 0

def updatePasscode():
    newPasscode = generateNewPasscode()
    print("New passcode is " + newPasscode)
    saveNewPasscode(newPasscode)
    sendPasscodeToGuest(newPasscode)
    sendCommandToLock('UPDATE PASSCODE ' + newPasscode)
    return 0

def lambda_handler(event, context):
    # print("Received event: " + json.dumps(event, indent=2))
    bucket = event["Records"][0]["s3"]["bucket"]["name"]
    key = event["Records"][0]["s3"]["object"]["key"]
    print("Key: " + key)

    try:
        rekResponse = rekognition.compare_faces(
            SourceImage={
                'S3Object': {
                    'Bucket': bucket,
                    'Name': key
                }
            },
            TargetImage={
                'S3Object': {
                    'Bucket': bucket,
                    'Name': 'enrolled_guest.jpg'
                }
            },
            SimilarityThreshold=80.0
        )
        print(rekResponse)
    
        print("Bucket: " + bucket)
        print("Key: " + key)

        try:
            similarity = rekResponse["FaceMatches"][0]["Similarity"]
            print("Similarity: " + str(similarity))
    
            if(similarity > 80):
                print("It is a match!")
                updatePasscode()
                # sendCommandToLock('ALLOW ACCESS')
            else:
                print("Face does not match!")
                sendCommandToLock('FACIAL VERIFICATION FAILED')
        except IndexError:
            print("Face does not match")
            sendCommandToLock('FACIAL VERIFICATION FAILED')
    except:
        print("Unexpected exception - likely InvalidParameterException in Rekognition (service bug?), should retry")
        print "Unexpected error:", sys.exc_info()[0]
        sendCommandToLock('FACIAL VERIFICATION FAILED')

    return "done"