import os
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import datetime
import logging
from json import loads, JSONDecodeError

def persists(msg):
    current_time = datetime.datetime.utcnow().isoformat()
    try:
        sensor_data = loads(msg.payload)
        json_body = [
            {
                "measurement": "pot",
                "tags": {
                    "esp-idf-ver": sensor_data['version']
                },
                "time": current_time,
                "fields": {
                    "hx_value": int(sensor_data['hx_value']),
                    "ds_temp": float(sensor_data['ds_temp']),
                    "dht_status": int(sensor_data['dht_status']),
                    "dht_tmp": int(sensor_data['dht_tmp']),
                    "dht_hum": int(sensor_data['dht_hum']),

                }
            }
        ]
    except (ValueError, JSONDecodeError) as err:
        logging.error(f"Not possible to serialize sensor data {err}")

    logging.info(json_body)
    try:
        influx_client.write_points(json_body)
    except Exception as err:
        logging.error(f"Not possible to serialize sensor data {err}")

def on_connect(client, userdata, flags, rc):
    print("Connected with result code {0}".format(str(rc)))
    client.subscribe("/honey_topic")


# TODO: get from os.environ['compose-var']
# db_password
# db_username
# log_level

logging.basicConfig(level=logging.DEBUG)
influx_client = InfluxDBClient('localhost', 8086, 'admin', 'teste123', database='honeycomb')
try:
    influx_client.create_database('honeycomb')
except:
    pass
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = lambda client, userdata, msg: persists(msg)
client.connect("localhost", 1883, 60)
client.loop_forever()