/*
 * Copyright 2017 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * Licensed under the Amazon Software License (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *   http://aws.amazon.com/asl/

 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#define TOPIC_LOCKS_CMD		"locks/commands"
#define TOPIC_LOCKS_IP		"locks/ip"

#define CMD_CAPTURE_PHOTO	"CAPTURE PHOTO"
#define CMD_FR_FAILURE		"FACIAL VERIFICATION FAILED"
#define CMD_UPDATE_PASSCODE	"UPDATE PASSCODE"
#define CMD_ASK_SECRET		"ASK SECRET"
#define CMD_ALLOW_ACCESS	"ALLOW ACCESS"
#define CMD_DENY_ACCESS		"DENY ACCESS"
#define CMD_SMS_FAILED		"SMS FAILED"

#define POLLY_PROMPT_READY		"<speak>Doorlock is ready</speak>"
#define POLLY_PROMPT_LOOK_AT_CAMERA	"<speak>Hello! Please look at the camera. Remember to remove your glasses if you are wearing any.<break time='1s'/> Three. Two. One. And click!</speak>"
#define POLLY_PROMPT_WAIT_A_MOMENT	"<speak>Please wait a moment.</speak>"
#define POLLY_PROMPT_FR_FAILURE		"<speak>Nope. I could not find you on the expected guest list. Go away!</speak>"
#define POLLY_PROMPT_ASK_SECRET		"<speak>A passcode has been sent to your phone, please read it out aloud.</speak>"
#define POLLY_PROMPT_ALLOW_ACCESS	"<speak>Welcome, you may enter now!</speak>"
#define POLLY_PROMPT_DENY_ACCESS	"<speak><prosody volume='+20dB'>Nope, that passcode is incorrect. I have alerted the police and the landlord. Go away, run for your life!</prosody></speak>"
#define POLLY_PROMPT_SENDING_SMS	"<speak>You will receive an SMS with passcode shortly, please standby.</speak>"
#define POLLY_PROMPT_SMS_FAILED		"<speak>I am sorry, I was unable to send SMS with passcode to your registered phone number.</speak>"

