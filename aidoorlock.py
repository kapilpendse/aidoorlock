#!/usr/bin/env python3
#
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

"""
aidoorlock.py - AWS IoT connected door lock device program.

Connects to AWS IoT Core via MQTT, subscribes to the 'locks/commands' topic,
and dispatches commands to helper scripts for camera capture, speech synthesis,
and passcode verification.
"""

import json
import logging
import os
import signal
import socket
import subprocess
import sys
import threading
import time

from awscrt import io, mqtt
from awsiot import mqtt_connection_builder

# ---------------------------------------------------------------------------
# Constants - MQTT Topics
# ---------------------------------------------------------------------------
TOPIC_LOCKS_CMD = "locks/commands"
TOPIC_LOCKS_IP = "locks/ip"

# ---------------------------------------------------------------------------
# Constants - Command Strings
# ---------------------------------------------------------------------------
CMD_CAPTURE_PHOTO = "CAPTURE PHOTO"
CMD_FR_FAILURE = "FACIAL VERIFICATION FAILED"
CMD_UPDATE_PASSCODE = "UPDATE PASSCODE"
CMD_ASK_SECRET = "ASK SECRET"
CMD_ALLOW_ACCESS = "ALLOW ACCESS"
CMD_DENY_ACCESS = "DENY ACCESS"
CMD_SMS_FAILED = "SMS FAILED"

# ---------------------------------------------------------------------------
# Constants - Polly SSML Prompts
# ---------------------------------------------------------------------------
POLLY_PROMPT_READY = "<speak>Doorlock is ready</speak>"
POLLY_PROMPT_LOOK_AT_CAMERA = (
    "<speak>Hello! Please look at the camera. Remember to remove your glasses "
    "if you are wearing any.<break time='1s'/> Three. Two. One. And click!</speak>"
)
POLLY_PROMPT_WAIT_A_MOMENT = "<speak>Please wait a moment.</speak>"
POLLY_PROMPT_FR_FAILURE = (
    "<speak>Nope. I could not find you on the expected guest list. Go away!</speak>"
)
POLLY_PROMPT_ASK_SECRET = (
    "<speak>A passcode has been sent to your phone, please read it out aloud.</speak>"
)
POLLY_PROMPT_ALLOW_ACCESS = "<speak>Welcome, you may enter now!</speak>"
POLLY_PROMPT_DENY_ACCESS = (
    "<speak><prosody volume='+20dB'>Nope, that passcode is incorrect. "
    "I have alerted the police and the landlord. Go away, run for your life!</prosody></speak>"
)
POLLY_PROMPT_SENDING_SMS = (
    "<speak>You will receive an SMS with passcode shortly, please standby.</speak>"
)
POLLY_PROMPT_SMS_FAILED = (
    "<speak>I am sorry, I was unable to send SMS with passcode to your "
    "registered phone number.</speak>"
)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("aidoorlock")

# ---------------------------------------------------------------------------
# Global state
# ---------------------------------------------------------------------------
passcode = "0000"
passcode_lock = threading.Lock()
base_dir = os.path.dirname(os.path.abspath(__file__))
shutdown_event = threading.Event()


def load_config(config_path=None):
    """Load configuration from config.json."""
    if config_path is None:
        config_path = os.path.join(base_dir, "config.json")
    with open(config_path, "r") as f:
        return json.load(f)


def get_self_ip(interface):
    """Get the IP address of a network interface (cross-platform)."""
    if sys.platform == "linux":
        try:
            import fcntl
            import struct
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            ip = socket.inet_ntoa(
                fcntl.ioctl(
                    s.fileno(),
                    0x8915,  # SIOCGIFADDR
                    struct.pack("256s", interface.encode("utf-8")[:15]),
                )[20:24]
            )
            s.close()
            return ip
        except Exception:
            return None
    elif sys.platform == "darwin":
        # macOS: use socket.getaddrinfo as a fallback
        try:
            # Map common Linux interface names to a hostname lookup
            # On macOS, we resolve the hostname to get a routable IP
            hostname = socket.gethostname()
            addrs = socket.getaddrinfo(hostname, None, socket.AF_INET)
            for addr in addrs:
                ip = addr[4][0]
                if ip and not ip.startswith("127."):
                    return ip
            return None
        except Exception:
            return None
    else:
        # Unsupported platform
        return None


def run_speak(text, region):
    """Run speak.py in a background subprocess (non-blocking)."""
    script = os.path.join(base_dir, "scripts", "speak.py")
    subprocess.Popen(
        ["python3", script, text, region],
        cwd=base_dir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def run_capture(prompt1, prompt2, region, bucket):
    """Run capture.sh in a background subprocess (non-blocking)."""
    script = os.path.join(base_dir, "scripts", "capture.sh")
    subprocess.Popen(
        ["sh", script, prompt1, prompt2, region, bucket],
        cwd=base_dir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def run_passcode(code, ask_prompt, allow_prompt, deny_prompt, region):
    """Run passcode.sh in a background subprocess (non-blocking)."""
    script = os.path.join(base_dir, "scripts", "passcode.sh")
    subprocess.Popen(
        ["sh", script, code, ask_prompt, allow_prompt, deny_prompt, region],
        cwd=base_dir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


# ---------------------------------------------------------------------------
# Command Handlers
# ---------------------------------------------------------------------------

def cmd_handler_capture_photo(config):
    """Handle CAPTURE PHOTO command."""
    logger.info("Capture Photo")
    run_capture(
        POLLY_PROMPT_LOOK_AT_CAMERA,
        POLLY_PROMPT_WAIT_A_MOMENT,
        config["host_region"],
        config["s3_bucket_name"],
    )


def cmd_handler_fr_failure(config):
    """Handle FACIAL VERIFICATION FAILED command."""
    logger.info("Facial Recognition Failed")
    run_speak(POLLY_PROMPT_FR_FAILURE, config["host_region"])


def cmd_handler_update_passcode(payload, config):
    """Handle UPDATE PASSCODE command - extract 4-digit passcode."""
    global passcode
    logger.info("Update Passcode")
    # Payload format: "UPDATE PASSCODE XXXX"
    new_passcode = payload[len(CMD_UPDATE_PASSCODE) + 1:][:4]
    with passcode_lock:
        passcode = new_passcode
    logger.debug("New passcode is %s", new_passcode)
    run_speak(POLLY_PROMPT_SENDING_SMS, config["host_region"])


def cmd_handler_ask_secret(config):
    """Handle ASK SECRET command."""
    logger.info("Ask Secret")
    with passcode_lock:
        current_passcode = passcode
    run_passcode(
        current_passcode,
        POLLY_PROMPT_ASK_SECRET,
        POLLY_PROMPT_ALLOW_ACCESS,
        POLLY_PROMPT_DENY_ACCESS,
        config["host_region"],
    )


def cmd_handler_allow_access(config):
    """Handle ALLOW ACCESS command."""
    logger.info("Allow Access")
    run_speak(POLLY_PROMPT_ALLOW_ACCESS, config["host_region"])


def cmd_handler_deny_access(config):
    """Handle DENY ACCESS command."""
    logger.info("Deny Access")
    run_speak(POLLY_PROMPT_DENY_ACCESS, config["host_region"])


def cmd_handler_sms_failed(config):
    """Handle SMS FAILED command."""
    logger.info("SMS Failed")
    run_speak(POLLY_PROMPT_SMS_FAILED, config["host_region"])


# ---------------------------------------------------------------------------
# MQTT Callbacks
# ---------------------------------------------------------------------------

def on_message_received(topic, payload, dup, qos, retain, config, **kwargs):
    """Callback for messages received on subscribed topics."""
    message = payload.decode("utf-8")
    logger.info("Received message on %s: %s", topic, message)

    if message == CMD_CAPTURE_PHOTO:
        cmd_handler_capture_photo(config)
    elif message == CMD_FR_FAILURE:
        cmd_handler_fr_failure(config)
    elif message.startswith(CMD_UPDATE_PASSCODE):
        cmd_handler_update_passcode(message, config)
    elif message == CMD_ASK_SECRET:
        cmd_handler_ask_secret(config)
    elif message == CMD_ALLOW_ACCESS:
        cmd_handler_allow_access(config)
    elif message == CMD_DENY_ACCESS:
        cmd_handler_deny_access(config)
    elif message == CMD_SMS_FAILED:
        cmd_handler_sms_failed(config)
    else:
        logger.warning("Unknown command: %s", message)


def on_connection_interrupted(connection, error, **kwargs):
    """Callback when connection is interrupted."""
    logger.warning("Connection interrupted: %s", error)


def on_connection_resumed(connection, return_code, session_present, **kwargs):
    """Callback when connection is resumed (auto-reconnect)."""
    logger.info("Connection resumed. Return code: %s, Session present: %s",
                return_code, session_present)


# ---------------------------------------------------------------------------
# Startup - publish IP addresses
# ---------------------------------------------------------------------------

def on_startup(mqtt_connection, config):
    """Announce startup: publish IP addresses and speak ready prompt."""
    logger.info("Announcing startup")
    run_speak(POLLY_PROMPT_READY, config["host_region"])

    # Publish wlan0 IP
    wlan0_ip = get_self_ip("wlan0")
    if wlan0_ip:
        msg = f"wlan0 IP address is {wlan0_ip}"
        logger.info(msg)
        mqtt_connection.publish(
            topic=TOPIC_LOCKS_IP,
            payload=msg,
            qos=mqtt.QoS.AT_LEAST_ONCE,
        )

    # Publish eth0 IP
    eth0_ip = get_self_ip("eth0")
    if eth0_ip:
        msg = f"eth0 IP address is {eth0_ip}"
        logger.info(msg)
        mqtt_connection.publish(
            topic=TOPIC_LOCKS_IP,
            payload=msg,
            qos=mqtt.QoS.AT_LEAST_ONCE,
        )


# ---------------------------------------------------------------------------
# Signal Handling
# ---------------------------------------------------------------------------

def signal_handler(signum, frame):
    """Handle SIGINT/SIGTERM for graceful shutdown."""
    logger.info("Received signal %d, shutting down...", signum)
    shutdown_event.set()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    """Main entry point for aidoorlock."""
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Load configuration
    config = load_config()
    logger.info("Configuration loaded successfully")

    # Set up AWS CRT event loop
    event_loop_group = io.EventLoopGroup(1)
    host_resolver = io.DefaultHostResolver(event_loop_group)
    client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)

    # Build MQTT connection with mutual TLS
    mqtt_connection = mqtt_connection_builder.mtls_from_path(
        endpoint=config["mqtt_host"],
        port=config.get("mqtt_port", 8883),
        cert_filepath=config["cert_path"],
        pri_key_filepath=config["key_path"],
        ca_filepath=config["root_ca_path"],
        client_bootstrap=client_bootstrap,
        client_id=config["client_id"],
        clean_session=True,
        keep_alive_secs=30,
        on_connection_interrupted=on_connection_interrupted,
        on_connection_resumed=on_connection_resumed,
    )

    logger.info("Connecting to %s:%d...", config["mqtt_host"],
                config.get("mqtt_port", 8883))
    connect_future = mqtt_connection.connect()
    connect_future.result()
    logger.info("Connected!")

    # Subscribe to commands topic
    logger.info("Subscribing to topic: %s", TOPIC_LOCKS_CMD)
    subscribe_future, packet_id = mqtt_connection.subscribe(
        topic=TOPIC_LOCKS_CMD,
        qos=mqtt.QoS.AT_LEAST_ONCE,
        callback=lambda topic, payload, dup, qos, retain, **kwargs: on_message_received(
            topic, payload, dup, qos, retain, config, **kwargs
        ),
    )
    subscribe_result = subscribe_future.result()
    logger.info("Subscribed with QoS: %s", str(subscribe_result["qos"]))

    # Announce startup
    on_startup(mqtt_connection, config)

    # Main loop - wait for shutdown signal
    logger.info("Door lock is running. Press Ctrl+C to exit.")
    while not shutdown_event.is_set():
        time.sleep(1)

    # Graceful disconnect
    logger.info("Disconnecting...")
    disconnect_future = mqtt_connection.disconnect()
    disconnect_future.result()
    logger.info("Disconnected. Goodbye!")


if __name__ == "__main__":
    main()
