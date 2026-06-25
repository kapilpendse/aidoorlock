#!/bin/sh

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

# Sets up the AIDoorLock 'thing'. For the thing to work, 'cloud' side functionality must also be set up. Run 'setup_cloud.sh' to do that.
# Usage: ./setup_thing.sh

HOST_REGION=$(cat .build/host_region.txt)
S3_BUCKET_NAME=$(cat .build/bucket_name.txt)
AWS_IOT_MQTT_HOST=$(aws --region $HOST_REGION iot describe-endpoint --output text)
AWS_IOT_MQTT_PORT=8883
AWS_IOT_MQTT_CLIENT_ID="ai-doorlock-$RANDOM"
AWS_IOT_THING_NAME=$(cat .build/thing_name.txt)
AWS_IOT_THING_CERTIFICATE="certificate.pem.crt"
AWS_IOT_THING_PRIVATE_KEY="private.pem.key"

AWS_IOT_MQTT_CLIENT_ID_DOORBELL="ai-doorbell-$RANDOM"
AWS_IOT_THING_NAME_DOORBELL=$(cat .build/doorbell_thing_name.txt)
AWS_IOT_THING_CERTIFICATE_DOORBELL="doorbell-certificate.pem.crt"
AWS_IOT_THING_PRIVATE_KEY_DOORBELL="doorbell-private.pem.key"

### CHECK PREREQUISITES ###

# Python 3
command -v python3 > /dev/null 2>&1 || { echo "python3 was not detected. Aborting." >&2; exit 1; }
echo "python3 detected"

# pip3
command -v pip3 > /dev/null 2>&1 || { echo "pip3 was not detected. Aborting." >&2; exit 1; }
echo "pip3 detected"

# AWS CLI
command -v aws > /dev/null 2>&1 || { echo "AWS CLI was not detected. Aborting." >&2; exit 1; }
echo "aws cli detected"

# Check if device certificate and private key are present in 'certs' folder
if [ ! -f certs/$AWS_IOT_THING_CERTIFICATE ]; then
    echo "$AWS_IOT_THING_CERTIFICATE is not present in the 'certs' folder. Aborting."
    exit 1
else
    echo "$AWS_IOT_THING_CERTIFICATE found"
fi
if [ ! -f certs/$AWS_IOT_THING_PRIVATE_KEY ]; then
    echo "$AWS_IOT_THING_PRIVATE_KEY is not present in the 'certs' folder. Aborting."
    exit 1
else
    echo "$AWS_IOT_THING_PRIVATE_KEY found"
fi

# If OS is Mac, check for brew
if [ "$(uname)" = "Darwin" ]; then
    command -v brew > /dev/null 2>&1 || { echo "brew was not detected. Aborting." >&2; exit 1; }
    echo "OS is mac, brew detected"
fi

### SETUP THING ###

# Install Python dependencies
echo "installing Python dependencies"
pip3 install -r requirements.txt || { echo "Failed to install Python dependencies. Aborting." >&2; exit 1; }
echo "Python dependencies installed"

# Install mpg123
echo "checking for mpg123"
if [ "$(uname)" = "Linux" ]; then
    command -v mpg123 > /dev/null 2>&1 || { echo "mpg123 not detected, installing." >&2; sudo apt-get install -y mpg123; }
elif [ "$(uname)" = "Darwin" ]; then
    command -v mpg123 > /dev/null 2>&1 || { echo "mpg123 not detected, installing." >&2; brew install mpg123; }
fi

echo "checking for SoX"
# Install SoX for the command 'rec'
if [ "$(uname)" = "Linux" ]; then
    command -v rec > /dev/null 2>&1 || { echo "SoX not detected, installing." >&2; sudo apt-get install -y sox; }
elif [ "$(uname)" = "Darwin" ]; then
    command -v rec > /dev/null 2>&1 || { echo "SoX not detected, installing." >&2; brew install sox; }
fi

# Install libcamera-utils (for Raspberry Pi) and ffmpeg (cross-platform fallback)
if [ "$(uname)" = "Linux" ]; then
    echo "checking for libcamera-utils"
    command -v libcamera-still > /dev/null 2>&1 || { echo "libcamera-still not detected, installing libcamera-utils." >&2; sudo apt-get install -y libcamera-utils 2>/dev/null || true; }
    echo "checking for ffmpeg"
    command -v ffmpeg > /dev/null 2>&1 || { echo "ffmpeg not detected, installing." >&2; sudo apt-get install -y ffmpeg; }
elif [ "$(uname)" = "Darwin" ]; then
    echo "checking for ffmpeg"
    command -v ffmpeg > /dev/null 2>&1 || { echo "ffmpeg not detected, installing." >&2; brew install ffmpeg; }
fi

# Generate config.json with IoT settings
echo "generating config.json with IoT settings"
cat > config.json << EOF
{
    "mqtt_host": "${AWS_IOT_MQTT_HOST}",
    "mqtt_port": ${AWS_IOT_MQTT_PORT},
    "client_id": "${AWS_IOT_MQTT_CLIENT_ID}",
    "thing_name": "${AWS_IOT_THING_NAME}",
    "cert_path": "certs/${AWS_IOT_THING_CERTIFICATE}",
    "key_path": "certs/${AWS_IOT_THING_PRIVATE_KEY}",
    "root_ca_path": "certs/root-ca.pem",
    "host_region": "${HOST_REGION}",
    "s3_bucket_name": "${S3_BUCKET_NAME}",
    "doorbell_client_id": "${AWS_IOT_MQTT_CLIENT_ID_DOORBELL}",
    "doorbell_thing_name": "${AWS_IOT_THING_NAME_DOORBELL}",
    "doorbell_cert_path": "certs/${AWS_IOT_THING_CERTIFICATE_DOORBELL}",
    "doorbell_key_path": "certs/${AWS_IOT_THING_PRIVATE_KEY_DOORBELL}"
}
EOF

echo ""
echo "Thing setup succeeded!"
echo "Upload a picture of the expected guest into the S3 bucket named '$S3_BUCKET_NAME',"
echo "and then run: python3 aidoorlock.py"
echo ""
echo "Wait for it to announce that it is ready (unmute your speakers!),"
echo "and then in another terminal window, in the same folder, run: python3 doorbell.py"
echo ""
echo "You should hear ding dong of a door bell, and then follow the instructions"
echo "that the aidoorlock program speaks out."
echo "Happy demoing!"
