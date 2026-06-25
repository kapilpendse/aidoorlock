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
python3 `pwd`/scripts/speak.py "$VOICE_PROMPT_1" "$HOST_REGION"

# Capture image - platform detection
if command -v libcamera-still > /dev/null 2>&1; then
    # Raspberry Pi with libcamera (preferred on RPi OS Bullseye+)
    libcamera-still -o "$LOCAL_IMAGE_FILE_PATH" --width 800 --height 600 -t 2000 --nopreview
    if [ $? -ne 0 ]; then
        echo "ERROR: libcamera-still failed to capture image." >&2
        exit 1
    fi
elif command -v ffmpeg > /dev/null 2>&1; then
    # Cross-platform fallback using ffmpeg
    if [ "$(uname)" = "Darwin" ]; then
        # macOS - use avfoundation input
        ffmpeg -y -f avfoundation -framerate 30 -i "0" -frames:v 1 "$LOCAL_IMAGE_FILE_PATH" 2>/tmp/capture_err.log
    else
        # Linux - use video4linux2 input
        ffmpeg -y -f v4l2 -framerate 30 -i /dev/video0 -frames:v 1 "$LOCAL_IMAGE_FILE_PATH" 2>/tmp/capture_err.log
    fi
    if [ $? -ne 0 ]; then
        echo "ERROR: ffmpeg failed to capture image. See /tmp/capture_err.log for details." >&2
        exit 1
    fi
else
    echo "ERROR: No supported camera capture tool found. Install libcamera-still or ffmpeg."
    exit 1
fi

# Legacy capture methods (kept for reference):
# raspistill -w 800 -h 600 -q 70 -t 2 -o $LOCAL_IMAGE_FILE_PATH
# fswebcam -r 1280x720 --no-banner --jpeg 100 -S 13 $LOCAL_IMAGE_FILE_PATH
# imagesnap -w 1.5 $LOCAL_IMAGE_FILE_PATH

# voice prompt after taking photo
python3 `pwd`/scripts/speak.py "$VOICE_PROMPT_2" "$HOST_REGION"

# upload the image to S3 bucket
echo "uploading to $HOST_REGION $S3_BUCKET_NAME $UPLOADED_FILE_NAME from $LOCAL_IMAGE_FILE_PATH"
python3 `pwd`/scripts/s3uploader.py "$HOST_REGION" "$S3_BUCKET_NAME" "$LOCAL_IMAGE_FILE_PATH" "$UPLOADED_FILE_NAME"

# remove the local file
rm $LOCAL_IMAGE_FILE_PATH
