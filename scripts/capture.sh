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

# this script takes 4 arguments:
# $1 text for voice prompt before taking photo
# $2 text for voice prompt after taking photo
# $3 AWS region to upload photo to
# $4 S3 bucket to upload photo to

VOICE_PROMPT_1=$1
VOICE_PROMPT_2=$2
HOST_REGION=$3
S3_BUCKET_NAME=$4
LOCAL_IMAGE_FILE_PATH="camera_captures/image.jpg"
UPLOADED_FILE_NAME='image.jpg'

# voice prompt before taking photo
python `pwd`/scripts/speak.py "$VOICE_PROMPT_1"

# Capture image using picam module
#raspistill -w 800 -h 600 -q 70 -t 2 -o $LOCAL_IMAGE_FILE_PATH

# Capture image using USB webcam
#fswebcam -r 1280x720 --no-banner --jpeg 100 -S 13 $LOCAL_IMAGE_FILE_PATH

# Capture image using Mac's built-in webcam (FaceTime camera)
imagesnap -w 1.5 $LOCAL_IMAGE_FILE_PATH

# voice prompt after taking photo
python `pwd`/scripts/speak.py "$VOICE_PROMPT_2"

# upload the image to S3 bucket
echo "uploading to $HOST_REGION $S3_BUCKET_NAME $UPLOADED_FILE_NAME from $LOCAL_IMAGE_FILE_PATH"
python `pwd`/scripts/s3uploader.py "$HOST_REGION" "$S3_BUCKET_NAME" "$LOCAL_IMAGE_FILE_PATH" "$UPLOADED_FILE_NAME"

# remove the local file
rm $LOCAL_IMAGE_FILE_PATH
