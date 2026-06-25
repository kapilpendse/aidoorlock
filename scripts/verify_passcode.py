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

import io
import sys
import json
import base64
import boto3
import os

audioFileName = sys.argv[1]
passcode = sys.argv[2]
allowPrompt = sys.argv[3]
denyPrompt = sys.argv[4]
HOST_REGION = sys.argv[5]

# Read bot configuration from config.json
config_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'config.json')
with open(config_path, 'r') as f:
    config = json.load(f)

bot_id = config['lex_bot_id']
bot_alias_id = config['lex_bot_alias_id']

lex = boto3.client('lexv2-runtime', region_name=HOST_REGION)
print("got lex v2 runtime")

try:
    # Initiate the lex bot conversation
    response = lex.recognize_text(
        botId=bot_id,
        botAliasId=bot_alias_id,
        localeId='en_US',
        sessionId='1234',
        text='Echo my passcode'
    )
    print(response)

    # Send the spoken passcode for interpretation
    audioFile = io.open(audioFileName, "rb")
    response = lex.recognize_utterance(
        botId=bot_id,
        botAliasId=bot_alias_id,
        localeId='en_US',
        sessionId='1234',
        requestContentType='audio/l16; rate=16000; channels=1',
        responseContentType='text/plain; charset=utf-8',
        inputStream=audioFile
    )
    print(response)

    # In V2, sessionState in recognize_utterance response is base64-encoded JSON
    session_state_encoded = response['sessionState']
    session_state = json.loads(base64.b64decode(session_state_encoded).decode('utf-8'))

    # Safely extract the passcode slot value with null checks at each level
    intent = session_state.get('intent')
    slots = intent.get('slots') if intent else None
    passcode_slot = slots.get('Passcode') if slots else None
    passcode_value = passcode_slot.get('value') if passcode_slot else None
    interpreted_value = passcode_value.get('interpretedValue') if passcode_value else None

    if interpreted_value is None:
        print("Passcode slot was not filled by Lex")
        os.system('python3 scripts/speak.py "' + denyPrompt + '" "' + HOST_REGION + '"')
    else:
        userSpokenPasscode = str(interpreted_value)
        print(userSpokenPasscode)

        if userSpokenPasscode == passcode:
            os.system('python3 scripts/speak.py "' + allowPrompt + '" "' + HOST_REGION + '"')
        else:
            os.system('python3 scripts/speak.py "' + denyPrompt + '" "' + HOST_REGION + '"')

    # End the conversation with lex bot
    response = lex.recognize_text(
        botId=bot_id,
        botAliasId=bot_alias_id,
        localeId='en_US',
        sessionId='1234',
        text='yes'
    )
    print(response)

except Exception:
    print("There was an exception")
    print(sys.exc_info())
