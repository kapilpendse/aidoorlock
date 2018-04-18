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

# This script converts text to audio using Amazon Polly, and plays out the audio
# using mpg123 system command.
#
# Prereq: mpg123 utility must be installed. 'sudo apt-get install mpg123'

import os
import sys
import time
from contextlib import closing
from boto3 import Session
from botocore.exceptions import BotoCoreError, ClientError

CHUNK_SIZE = 1024
AUDIO_FILE = "/tmp/audio.mp3"

# Create a client using the credentials and region defined in the adminuser
# section of the AWS credentials and configuration files
session = Session(region_name="us-west-2")
polly = session.client("polly")

try:
    # Request speech synthesis
    if len(sys.argv) == 1:
        exit()	#nothing to synthesize

    response = polly.synthesize_speech(Text=sys.argv[1],
                                        VoiceId="Joanna",
                                        TextType="ssml",
                                        OutputFormat="mp3",
					SampleRate="22050")
    audioStream = response.get("AudioStream")
    if audioStream:
        mp3file = open(AUDIO_FILE, 'w')
        # Note: Closing the stream is important as the service throttles on
        # the number of parallel connections. Here we are using
        # contextlib.closing to ensure the close method of the stream object
        # will be called automatically at the end of the with statement's
        # scope.
        with closing(audioStream) as managed_stream:
            # Write the stream's content in chunks to a file
            while True:
                data = managed_stream.read(CHUNK_SIZE)
                mp3file.write(data)

                # If there's no more data to read, stop streaming
                if not data:
                    break

            # Ensure any buffered output has been transmitted and close the
            # stream
            mp3file.flush()
            mp3file.close()

        print("Streaming completed, starting player...")
        command_to_run = 'mpg123 ' + AUDIO_FILE
        os.system(command_to_run)
        print("Player finished.")
    else:
        # The stream passed in is empty
        print("Nothing to stream.")

except (BotoCoreError, ClientError) as err:
    # The service returned an error
    print("ERROR: %s" % err)

