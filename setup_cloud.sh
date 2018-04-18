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

# 
# Usage: ./setup_cloud.sh deploy
# 	Sets up the cloud services and provisions the device certificates for AIDoorLock demo.
# Usage: ./setup_cloud.sh teardown
# 	Tears down the cloud services that have been previously set up by setup_cloud.sh start

# Change this to your desired region. Make sure that the following services
# are available in the region of your choice: Polly, Lex, Rekognition, IoT, Lambda, S3, SNS, CloudWatch and DynamoDB
HOST_REGION="us-east-1"

# Give a name to the S3 bucket which will be created.
# S3 bucket name rules apply.
# Your IAM username will be automatically prepended to whatever you set here.
BUCKET_FOR_IMAGES="aidoorlock-images"

# Name of the DynamoDB table that is created for this demo
GUEST_INFO_TABLE_NAME="aidoorlockguests"

# A phone number to receive passcode via SMS (e.g. +1231231231)
GUEST_PHONE_NUMBER=""

# Name of the Thing
THING_NAME="AIDoorLock"
DOORBELL_THING_NAME="AIDoorBell"


# CHECK PREREQUISITES
function check_prerequisites () {
	# Python
	command -v python -V > /dev/null 2>&1 || { echo "Python was not detected. Aborting." >&2; exit 1; }
	echo "python detected"

	# AWS CLI
	command -v aws --version > /dev/null 2>&1 || { echo "AWS CLI was not detected. Aborting." >&2; exit 1; }
	echo "aws cli detected"

	# NODE JS
	command -v node --version > /dev/null 2>&1 || { echo "Node JS was not detected. Aborting." >&2; exit 1; }
	echo "node js detected"

	# SERVERLESS FRAMEWORK
	command -v serverless --version > /dev/null 2>&1 || { echo "Serverless Framework was not detected. Aborting." >&2; exit 1; }
	echo "serverless framework detected"

	if [ -z "$GUEST_PHONE_NUMBER" ]; then
	    echo "Please set GUEST_PHONE_NUMBER at the top of this script."
	    exit 1
	else
	    echo "GUEST_PHONE_NUMBER checked"
	fi

}

# CREATE LEX BOT
function create_lex_bot() {
	echo "checking if service linked role for Lex already exists"
	LEX_SERVICE_ROLE=$(aws iam get-role --role-name AWSServiceRoleForLexBots --output text --query 'Role.RoleId')
	if [ -z "$LEX_SERVICE_ROLE" ]; then
		echo "no, creating one"
	    aws iam create-service-linked-role --aws-service-name lex.amazonaws.com
	else
		echo "yes, 'AWSServiceRoleForLexBots' exists"
	fi
	echo "creating intent"
	aws --region $HOST_REGION lex-models put-intent --name RequestForEchoIntent --cli-input-json file://lex/RequestForEchoIntent.json
	echo "getting intent's checksum"
	LEX_INTENT_CHECKSUM=$(aws --region $HOST_REGION lex-models get-intent --name RequestForEchoIntent --intent-version "\$LATEST" --output text --query 'checksum')
	echo "publishing intent version for checksum $LEX_INTENT_CHECKSUM"
	LEX_INTENT_VERSION=$(aws --region $HOST_REGION lex-models create-intent-version --name RequestForEchoIntent --checksum "$LEX_INTENT_CHECKSUM" --output text --query 'version')
	echo "generating lex bot json file"
	cp lex/AIDoorLockEchoBot_template.json lex/AIDoorLockEchoBot.json
	sed -i -e "s/LEX_INTENT_VERSION/$LEX_INTENT_VERSION/g" lex/AIDoorLockEchoBot.json
	echo "creating lex bot"
	LEX_BOT_STATUS=$(aws --region $HOST_REGION lex-models put-bot --name AIDoorLockEchoBot --cli-input-json file://lex/AIDoorLockEchoBot.json --output text --query 'status')
	while [ "$LEX_BOT_STATUS" != "READY" ]; do
		echo "checking lex bot status: $LEX_BOT_STATUS, please wait"
		sleep 2
		LEX_BOT_STATUS=$(aws --region $HOST_REGION lex-models get-bot --name AIDoorLockEchoBot --version-or-alias "\$LATEST" --output text --query 'status')
	done
	echo "checking lex bot status: $LEX_BOT_STATUS"
	echo "lex bot created, getting checksum"
	LEX_BOT_CHECKSUM=$(aws --region $HOST_REGION lex-models get-bot --name AIDoorLockEchoBot --version-or-alias "\$LATEST" --output text --query 'checksum')
	echo "publishing lex bot for checksum $LEX_BOT_CHECKSUM"
	LEX_BOT_VERSION=$(aws --region $HOST_REGION lex-models create-bot-version --name AIDoorLockEchoBot --checksum "$LEX_BOT_CHECKSUM" --output text --query 'version')
	echo "checking lex bot status"
	LEX_BOT_STATUS=$(aws --region $HOST_REGION lex-models get-bot --name AIDoorLockEchoBot --version-or-alias "$LEX_BOT_VERSION" --output text --query 'status')
	while [ "$LEX_BOT_STATUS" != "READY" ]; do
		echo "checking lex bot status: $LEX_BOT_STATUS, please wait"
		sleep 2
		LEX_BOT_STATUS=$(aws --region $HOST_REGION lex-models get-bot --name AIDoorLockEchoBot --version-or-alias "$LEX_BOT_VERSION" --output text --query 'status')
	done
	echo "checking lex bot status: $LEX_BOT_STATUS"
	aws --region $HOST_REGION lex-models put-bot-alias --name Dev --bot-name AIDoorLockEchoBot --bot-version $LEX_BOT_VERSION
	echo "Lex bot 'AIDoorLockEchoBot' published with alias 'Dev'"
}

# DELETE LEX BOT
function delete_lex_bot() {
	echo "deleting bot alias"
	aws --region $HOST_REGION lex-models delete-bot-alias --name Dev --bot-name AIDoorLockEchoBot
	echo "deleting bot"
	aws --region $HOST_REGION lex-models delete-bot --name AIDoorLockEchoBot
	sleep 5
	echo "deleting intent"
	aws --region $HOST_REGION lex-models delete-intent --name RequestForEchoIntent
}

# prepend IAM username to the S3 bucket name
BUCKET_FOR_IMAGES=$(aws --output text iam get-user --query 'User.UserName')"-"$BUCKET_FOR_IMAGES

case "$1" in
	deploy)
		# Check prerequisites
		check_prerequisites

		echo "A bucket with the name '$BUCKET_FOR_IMAGES' will be created. Please upload 'enrolled_guest.jpg' to this bucket."

		# Generate configuration file for serverless
		if [ ! -d ".build" ]; then
		    mkdir .build
		fi
		echo "generating cloud configuration file (.build/cloud_config.yml)"
		cp cloud_config.yml .build/cloud_config.yml
		ACCOUNT_ID=`aws sts get-caller-identity --output text --query 'Account'`
		sed -i -e "s/ACCOUNT_ID/$ACCOUNT_ID/g" .build/cloud_config.yml
		sed -i -e "s/HOST_REGION/$HOST_REGION/g" .build/cloud_config.yml
		sed -i -e "s/BUCKET_FOR_IMAGES/$BUCKET_FOR_IMAGES/g" .build/cloud_config.yml
		sed -i -e "s/GUEST_INFO_TABLE_NAME/$GUEST_INFO_TABLE_NAME/g" .build/cloud_config.yml
		sed -i -e "s/THING_NAME/$THING_NAME/g" .build/cloud_config.yml
		sed -i -e "s/DOORBELL_NAME/$DOORBELL_THING_NAME/g" .build/cloud_config.yml
		echo "generating seed data file (.build/seed_data.json)"
		cp seed_data.json .build/seed_data.json
		sed -i -e "s/GUEST_PHONE_NUMBER/$GUEST_PHONE_NUMBER/g" .build/seed_data.json
		echo $THING_NAME > .build/thing_name.txt
		echo $DOORBELL_THING_NAME > .build/doorbell_thing_name.txt
		echo $HOST_REGION > .build/host_region.txt
		echo $BUCKET_FOR_IMAGES > .build/bucket_name.txt

		# Deploy serverless package
		echo "deploying serverless package to cloud"
		serverless deploy -v || { echo "Deployment failed." >&2; exit 1; }

		# Create item in $GUEST_INFO_TABLE_NAME with default seed data
		echo "seeding dynamodb with initial data"
		aws --region $HOST_REGION dynamodb put-item --table-name $GUEST_INFO_TABLE_NAME --item file://.build/seed_data.json  || { echo "Data initialisation failed." >&2; exit 1; }

		echo "provisioning IoT device identities"

		# provision doorlock identity
		aws --output text --region $HOST_REGION iot create-keys-and-certificate --set-as-active --certificate-pem-outfile certs/certificate.pem.crt --public-key-outfile certs/public.pem.key --private-key-outfile certs/private.pem.key --query 'certificateArn' > .build/cert_arn.txt  || { echo "Failed to provision doorlock certificate." >&2; exit 1; }
		aws --region $HOST_REGION iot attach-principal-policy --policy-name $THING_NAME"_Policy" --principal `cat .build/cert_arn.txt` || { echo "Failed to attach policy to doorlock certificate." >&2; exit 1; }
		aws --region $HOST_REGION iot attach-thing-principal --thing-name $THING_NAME --principal `cat .build/cert_arn.txt` || { echo "Failed to attach doorlock certificate to thing." >&2; exit 1; }

		# provision doorbell identity (which simulates an AWS IoT button)
		aws --output text --region $HOST_REGION iot create-keys-and-certificate --set-as-active --certificate-pem-outfile certs/doorbell-certificate.pem.crt --public-key-outfile certs/doorbell-public.pem.key --private-key-outfile certs/doorbell-private.pem.key --query 'certificateArn' > .build/doorbell_cert_arn.txt  || { echo "Failed to provision doorbell certificate." >&2; exit 1; }
		aws --region $HOST_REGION iot attach-principal-policy --policy-name $DOORBELL_THING_NAME"_Policy" --principal `cat .build/doorbell_cert_arn.txt` || { echo "Failed to attach policy to doorbell certificate." >&2; exit 1; }
		aws --region $HOST_REGION iot attach-thing-principal --thing-name $DOORBELL_THING_NAME --principal `cat .build/doorbell_cert_arn.txt` || { echo "Failed to attach doorbell certificate to thing." >&2; exit 1; }

		# create lex echo bot
		echo "setting up Lex Bot"
		create_lex_bot

		# configure SNS for SMS
		echo "configuring SNS for sending transactional SMS"
		cp sns/sms-attributes-template.json .build/sms-attributes.json
		sed -i -e "s/ACCOUNT_ID/$ACCOUNT_ID/g" .build/sms-attributes.json
		aws --region $HOST_REGION sns set-sms-attributes --cli-input-json file://.build/sms-attributes.json

		echo "cloud deployment completed, now you can run ./setup_thing.sh"
		;;
	teardown)
		# Check prerequisites
		check_prerequisites

		# Verify existence of .build directory and its contents
		if [ ! -f ".build/cloud_config.yml" ]; then
			echo "Config not found. Aborting."
		fi

		# delete lex echo bot
		delete_lex_bot

		# delete device identities
		echo "deleting device identities"
		aws --region $HOST_REGION iot detach-thing-principal --thing-name $THING_NAME --principal `cat .build/cert_arn.txt` || { echo "Failed to detach certificate from thing." >&2; exit 1; }
		aws --region $HOST_REGION iot detach-principal-policy --policy-name $THING_NAME"_Policy" --principal `cat .build/cert_arn.txt` || { echo "Failed to detach policy from certificate." >&2; exit 1; }
		CERT_ID=$(cat .build/cert_arn.txt | sed 's/.*cert\///')
		aws --output text --region $HOST_REGION iot update-certificate --certificate-id $CERT_ID --new-status "INACTIVE" || { echo "Failed to make certificate INACTIVE." >&2; exit 1; }
		aws --output text --region $HOST_REGION iot delete-certificate --certificate-id $CERT_ID || { echo "Failed to delete certificate." >&2; exit 1; }
		rm certs/certificate.pem.crt
		rm certs/public.pem.key
		rm certs/private.pem.key

		aws --region $HOST_REGION iot detach-thing-principal --thing-name $DOORBELL_THING_NAME --principal `cat .build/doorbell_cert_arn.txt` || { echo "Failed to detach doorbell certificate from thing." >&2; exit 1; }
		aws --region $HOST_REGION iot detach-principal-policy --policy-name $DOORBELL_THING_NAME"_Policy" --principal `cat .build/doorbell_cert_arn.txt` || { echo "Failed to detach policy from doorbell certificate." >&2; exit 1; }
		DOORBELL_CERT_ID=$(cat .build/doorbell_cert_arn.txt | sed 's/.*cert\///')
		aws --output text --region $HOST_REGION iot update-certificate --certificate-id $DOORBELL_CERT_ID --new-status "INACTIVE" || { echo "Failed to make doorbell certificate INACTIVE." >&2; exit 1; }
		aws --output text --region $HOST_REGION iot delete-certificate --certificate-id $DOORBELL_CERT_ID || { echo "Failed to delete doorbell certificate." >&2; exit 1; }
		rm certs/doorbell-certificate.pem.crt
		rm certs/doorbell-public.pem.key
		rm certs/doorbell-private.pem.key

		# Empty the S3 bucket
		echo "emptying the S3 bucket: $BUCKET_FOR_IMAGES"
		aws --region $HOST_REGION s3 rm --recursive s3://$BUCKET_FOR_IMAGES || { echo "Failed to remove files from S3 bucket $BUCKET_FOR_IMAGES." >&2; exit 1; }

		# Remove cloud stack
		echo "removing serverless stack from cloud"
		serverless remove || { echo "Failed." >&2; exit 1; }

		echo "deleting cloud configuration files (.build/*)"
		rm -rf .build/

		echo "cloud teardown completed, end of script"
		;;
	*)
		echo "Usage: $0 {deploy|teardown}"
		exit 1
esac

