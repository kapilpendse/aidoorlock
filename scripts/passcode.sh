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

# this script takes 2 arguments: text for voice prompt asking for the passcode and text for voice prompt
# after listening the passcode

passcode=$1
askPrompt=$2
allowPrompt=$3
denyPrompt=$4
HOST_REGION=$5

# voice prompt asking for passcode
python `pwd`/scripts/speak.py "$askPrompt"

# record the spoken passcode
rec -c 1 -r 16000 -e signed -b 16 /tmp/audiorec.wav trim 0 0:00:05

# send the recorded audio clip to Lex for verification, and take appropriate action after verification
python `pwd`/scripts/verify_passcode.py /tmp/audiorec.wav "$passcode" "$allowPrompt" "$denyPrompt" "$HOST_REGION"

