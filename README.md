# README #

This is a concept demo of a door lock equipped with AI capabilities such as voice interaction and facial verification. It uses AWS services such as IoT, Rekognition, Polly, Lex, Lambda, S3 and SNS.

There are 2 components to this demo - the 'aidoorlock' program that runs on a thing and a bunch of cloud services that runs on AWS cloud. This demo supports the following platforms as 'thing' - Raspberry Pi 3 with:

* Raspbian (setup scripts don't work, WIP)
* Debian/Ubuntu Linux (setup scripts don't work, WIP)
* Mac with macOS Sierra

Most of the setup process is automated, however some manual setup is still required. Refer to the "Setup Instructions" section below for step by step instructions.

## Prerequisites ##

### Common prerequisites for all platforms
* [Python 2.7](https://www.python.org/download/releases/2.7/)
* [AWS CLI version >=1.11.102](https://aws.amazon.com/cli/) *This is important because earlier versions of AWS CLI do not support Lex bot deployment.*
* [AWS IoT Device SDK 2.1.1](https://github.com/aws/aws-iot-device-sdk-embedded-C/archive/v2.1.1.tar.gz)
* [mbedTLS 2.1.1](https://github.com/ARMmbed/mbedtls/archive/mbedtls-2.1.1.tar.gz) library which is a dependency of the AWS IoT Device SDK 2.1.1
* **make** and **gcc** for compiling the IoT programs.
* [Node.js](https://nodejs.org/) and the [Serverless Framework](https://serverless.com/)
* AWS SNS configured in your AWS account for sending out SMS, with default spending limit increased to suitable value

### Prerequisites for Raspberry Pi 3
* Raspberry Pi 3 board with PiCam 2 or USB webcam
* Raspbian
* raspistill (comes with Raspbian, used only if using PiCam)
* A USB audio sound card adaptor/dongle for connecting a speaker and a microphone. [Here's an example.](http://www.lazada.sg/easybuy-new-pc-laptop-usb-2-3d-virtual-kx3p-71-channel-audio-soundcard-adapter-9019448.html)

### Prerequisites for Mac OSX with built-in webcam
* [brew](https://brew.sh/)

## Setup Instructions ##

1. Download the AWS IoT Device SDK and mbedTLS library from the links above (common prerequisites section)
2. Open a bash terminal and type in the following commands:

~~~~
mkdir ~/ai-iot-demo
cd ~/ai-iot-demo
tar xzf ~/Downloads/aws-iot-device-sdk-embedded-C-2.1.1.tar.gz # Change ~/Downloads to the path where you have downloaded the AWS Device SDK to
cd aws-iot-device-sdk-embedded-C-2.1.1/external_libs/mbedTLS
tar xzf ~/Downloads/mbedtls-mbedtls-2.1.1.tar.gz --strip-components 1  # Change ~/Downloads to the path where you have downloaded the mbedTLS library to
cd ../../samples/linux
git clone ssh://git.amazon.com/pkg/AIDoorLock # Code is hosted on Amazon's internal code.amazon.com; you will be asked for your Amazon account password.
cd AIDoorLock
vi setup_cloud.sh # Set HOST_REGION, BUCKET_FOR_IMAGES and GUEST_PHONE_NUMBER to your desired values. HOST_REGION must be set to the one that has Rekognition, Polly and Lex. Leave default value us-east-1 if you don't care where the demo runs. You can use any text editor.
./setup_cloud.sh deploy
./setup_thing.sh
~~~~

If there are no errors, run `./aidoorlock`, the program should output the IP addresses of your computer (eth0 & wlan0) to STDOUT (along with a bunch of debug messages) and also publish the IP addresses to the AWS IoT topic 'locks/ip'; Additionally, you should hear the words 'Doorlock is ready' from your computer's speakers.

### Setup of USB audio dongle on Raspberry Pi
Edit ~/.asoundrc and replace its contents with following 2 lines:
~~~~
pcm.!default plughw:Device
ctl.!default plughw:Device
~~~~

### Autostart on bootup of Raspberry Pi
* To auto run the application on Raspberry Pi bootup, create links under /etc/network/if-up.d/ and /etc/network/if-down.d/ as below:
* sudo ln -s /home/pi/deviceSDK/linux_mqtt_openssl/sample_apps/aidoorlock/ifup.sh /etc/network/if-up.d/aidoorlock
* sudo ln -s /home/pi/deviceSDK/linux_mqtt_openssl/sample_apps/aidoorlock/ifdown.sh /etc/network/if-down.d/aidoorlock

## Demo Suggestions ##
The easiest way to run this demo is on a Mac with macOS Sierra. This is easy, portable and you don't need to setup a Raspberry Pi with all its accessories. Simply follow the setup instructions above, run `./aidoorlock` and then run `./doorbell` in another terminal window. The `doorbell` program publishes a message to an AWS IoT topic to simulate the pressing of an AWS IoT Button (see below). This message is picked up by the `aidoorlock` program and the magic begins.

### Using AWS IoT Button ###
If you are feeling particularly adventurous, you can use an AWS IoT Button instead of the virtual doorbell. You will have to manually configure an AWS IoT Button in your account, setup the Lambda function 'lambda/iotButtonDoorbellPressed.py', and add a rule for the button so that it invokes the 'iotButtonDoorbellPressed.py' Lambda function.

### Telling the story ###
Here's an outline of the story that I tell the audience: If you have ever used Airbnb, you will know that the first thing you do when you book an apartment is to coordinate with the host about your arrival time, so that the host can be there to give you the apartment keys. It is quite common that travel plans get disrupted due to flight delays and what not. When this happens, the host has only 2 miserable options: wait endlessly for the guest to arrive (because often the guest would not be reachable by phone), or leave the apartment locked and wait for the guest to somehow call you when they arrive (a great way to earn bad rating from the guest).

We've solved this problem by using AWS services to create a smart door lock with a camera. The door lock takes a picture of the guest when they press the doorbell, and uses Amazon Rekognition to verify that the person at the door is the expected guest. It then sends a dynamically generated 4 digit passcode via SMS to the expected guest's registered phone number. This is 'multi factor authentication' (MFA). The doorbell does not have any keypad. Instead, it implements a VUI (voice user interface). A speaker speaks out instructions using the Amazon Polly service, and a microphone listens to what the guest speaks, and uses the Amazon Lex service to understand the spoken words. If the spoken words match the dynamically generated passcode, the speaker announces that the guest is welcome to enter, otherwise it scares the unexpected guest by saying that the police has been alerted ;-)

Behind the scenes, all of this interaction is stitched together by AWS IoT and AWS Lambda services, with a delicious dressing of S3, DynamoDB and SNS.

This demo focuses on highlighting the ease with which it is possible to solve real world problems using AWS services. The actual door lock hardware control is not implemented because that is a fairly straight-forward and well understood technology - just hook up the Raspberry Pi with a servo controlled lock. Another reason to leave that component out of scope for this demo is that we want this demo to be portable and easy to setup. Setting up specialised hardware like servos and locks is straight-forward, but not easy & portable.

## Demo Runbook ##
1. Login to AWS console and navigate to the S3 Management Console.
2. The `setup_cloud.sh` script creates a bucket where the `aidoorlock` program uploads captured photos for face verification. Name of this bucket can be found at the top of the `setup_cloud.sh` script (*BUCKET_FOR_IMAGES*). Open this bucket in the S3 Management Console.
3. Upload a photo of yourself (or the expected guest) with the file name `enrolled_guest.jpg`. It must be a full frontal mugshot with the face clearly visible. Don't use photos that have multiple faces.
4. Open a terminal window, navigate to the *aidoorlock* project directory, and run the door lock program like so: `./aidoorlock`. Confirm that you see the program print out the local IP addresses (eth0 and wlan0). You should also hear the words 'Doorlock is ready' from the speakers.
5. Open another terminal window, navigate to the *aidoorlock* project directory, and run the door bell program like so: `./doorbell`.
6. If everything is setup correctly, the *aidoorlock* program should receive the message that is published by *doorbell* (via AWS IoT), and you should hear voice instructions from the speakers.
7. Follow the voice instructions.

## Teardown ##
To teardown the demo from your AWS account, open a terminal, navigate to the *aidoorlock* project directory and run `./setup_cloud.sh teardown`. This will tear down all the cloud resources that were setup by this script, including IoT device certificates.

## Troubleshooting ##
* If the `setup_cloud.sh` script fails during deployment with the error message `Unable to validate the following destination configurations`, do the following steps:
	* Login to your AWS web console, go to Cloud Formation page and delete the stack named 'aidoorlock-dev'. If this fails, delete again but this time choose to retain the S3 bucket 'aidoorlock-dev-serverlessdeployment-*'. Once the stack is deleted, delete the S3 bucket manually if it still exists.
	* On your Raspberry Pi or computer, run the `setup_cloud.sh` script again.
	* [This problem has been reported](https://github.com/serverless/serverless/issues/3038) by many users of the 'serverless' framework.
* If the `setup_cloud.sh` script fails with an error like `S3BucketXYZ - A conflicting conditional operation is currently in progress against this resource.`, try changing the S3 bucket name: open the script in a text editor and change the bucket name assigned to the variable *BUCKET_FOR_IMAGES*.
* If the `setup_cloud.sh` scripts fails with an error message *'The specified bucket does not exist.'*, go to your AWS Cloud Formation web console and delete the stack named **aidoorlock-dev**.
* Amazon Rekognition is great, but face verification is sensitive and might not produce a match if the same person's face in *enrolled_guest.jpg* looks very different from the *image.jpg* captured by the *aidoorlock* program. Open the *image.jpg* file in the S3 bucket to see what the doorlock sees. Some aspects that affect the face verification:
	* Camera orientation - make sure you capture your photo from the same angle as in *enrolled_guest.jpg*. Try to keep the camera as the same height as your face. If the camera is on a speaker's desk, chances are that it sees more of your chin and less of your forehead. Bend your knees when the camera takes your photo ;-)
	* Background lights - if you are presenting from a stage that has strong lights on your face, these lights can throw the Rekognition off. It is best to capture a photo on the stage, then rename the file from *image.jpg* to *enrolled_guest.jpg*.
* When the lock prompts you "A passcode has been sent to your phone, please read it out aloud.", you have 5 seconds to read the passcode. Speak loudly and clearly, because background noise is not your friend.
* If `setup_cloud.sh deploy` fails with a message saying SNS log groups already exist, please go to CloudWatch console and delete these log groups:
	* sns/<region>/<account-id>/DirectPublishToPhoneNumber
	* sns/<region>/<account-id>/DirectPublishToPhoneNumber/Failure
* If you don't receive the SMS, fret not, the *aidoorlock* program prints out the passcode in the terminal, and you can also see the passcode in DynamoDB in the AWS console.
* Contact me ;-)

### Who do I talk to? ###
* Kapil Pendse (kapilpen@amazon.com)
