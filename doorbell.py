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
doorbell.py - AWS IoT connected doorbell device program.

Connects to AWS IoT Core via MQTT, publishes 'CAPTURE PHOTO' to the
'locks/commands' topic, plays the doorbell sound, then disconnects.
"""

import json
import logging
import os
import subprocess
import sys

from awscrt import io, mqtt
from awsiot import mqtt_connection_builder

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
TOPIC_LOCKS_CMD = "locks/commands"
CMD_CAPTURE_PHOTO = "CAPTURE PHOTO"

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("doorbell")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
base_dir = os.path.dirname(os.path.abspath(__file__))


def load_config(config_path=None):
    """Load configuration from config.json.

    Uses doorbell-specific fields if present, otherwise falls back to
    the main device fields.
    """
    if config_path is None:
        config_path = os.path.join(base_dir, "config.json")
    with open(config_path, "r") as f:
        cfg = json.load(f)

    # Build doorbell-specific config with fallbacks
    return {
        "mqtt_host": cfg["mqtt_host"],
        "mqtt_port": cfg.get("mqtt_port", 8883),
        "client_id": cfg.get("doorbell_client_id", cfg["client_id"]),
        "thing_name": cfg.get("doorbell_thing_name", cfg["thing_name"]),
        "cert_path": cfg.get("doorbell_cert_path", cfg["cert_path"]),
        "key_path": cfg.get("doorbell_key_path", cfg["key_path"]),
        "root_ca_path": cfg.get("root_ca_path", "certs/root-ca.pem"),
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    """Main entry point for doorbell."""
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
        port=config["mqtt_port"],
        cert_filepath=config["cert_path"],
        pri_key_filepath=config["key_path"],
        ca_filepath=config["root_ca_path"],
        client_bootstrap=client_bootstrap,
        client_id=config["client_id"],
        clean_session=True,
        keep_alive_secs=10,
    )

    # Connect
    logger.info("Connecting to %s:%d...", config["mqtt_host"], config["mqtt_port"])
    connect_future = mqtt_connection.connect()
    connect_future.result()
    logger.info("Connected!")

    # Publish CAPTURE PHOTO command
    logger.info("Publishing '%s' to '%s'", CMD_CAPTURE_PHOTO, TOPIC_LOCKS_CMD)
    publish_future, packet_id = mqtt_connection.publish(
        topic=TOPIC_LOCKS_CMD,
        payload=CMD_CAPTURE_PHOTO,
        qos=mqtt.QoS.AT_MOST_ONCE,
    )
    publish_future.result()
    logger.info("Publish done")

    # Play doorbell sound
    logger.info("Ding Dong")
    ding_dong_path = os.path.join(base_dir, "ding_dong.mp3")
    subprocess.Popen(
        ["mpg123", ding_dong_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    # Disconnect
    logger.info("Disconnecting...")
    disconnect_future = mqtt_connection.disconnect()
    disconnect_future.result()
    logger.info("Disconnected. Goodbye!")


if __name__ == "__main__":
    main()
