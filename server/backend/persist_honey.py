import os
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import datetime
import logging
from json import loads, JSONDecodeError


def persist(msg):
    current_time = datetime.datetime.utcnow().isoformat()
    try:
        sensor_data = loads(msg.payload)
        json_body = [
            {
                "measurement": "environment_data",
                "tags": {
                    "esp-idf-ver": sensor_data["version"],
                    "sensor-id": sensor_data["sensor_id"],
                },
                "time": current_time,
                "fields": {
                    "hx_value": int(sensor_data["hx_value"]),
                    "ds_temp": float(sensor_data["ds_temp"]),
                    "dht_status": int(sensor_data["dht_status"]),
                    "dht_tmp": int(sensor_data["dht_tmp"]),
                    "dht_hum": int(sensor_data["dht_hum"]),
                },
            }
        ]
    except (ValueError, KeyError, JSONDecodeError) as err:
        logging.error(f"Not possible to serialize sensor data: {err}.")
    except AssertionError:
        logging.error(f"Wrong system protection key.")
    else:
        try:
            influx_client.write_points(json_body)
            logging.info(f"Writing to database: {json_body}")
        except Exception as err:
            logging.error(f"Not possible to write data to db: {err}.")


def on_connect(client, userdata, flags, rc):
    print("Connected with result code {0}".format(str(rc)))
    client.subscribe("/honey_topic")


if __name__ == "__main__":
    assert any(
        [os.environ[i] for i in ["DB_URL", "DB_USER", "DB_PASSW", "DB", "MQTT_URL"]]
    )

    logging.basicConfig(level=logging.DEBUG)
    influx_client = InfluxDBClient(
        os.environ["DB_URL"],
        8086,
        os.environ["DB_USER"],
        os.environ["DB_PASSW"],
        database=os.environ["DB"],
    )
    try:
        influx_client.create_database("honey-comb")
    except Exception as err:
        logging.error(f"Not possible to connect to db: {err}.")
    else:
        client = mqtt.Client()
        client.on_connect = on_connect
        client.on_message = lambda client, userdata, msg: persist(msg)
        client.connect(os.environ["MQTT_URL"], 1883, 60)
        client.loop_forever()
