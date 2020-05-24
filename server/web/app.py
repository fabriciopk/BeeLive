import json
import random
import time
from datetime import datetime

from flask import Flask, Response, render_template
from influxdb import InfluxDBClient

application = Flask(__name__)
random.seed()


@application.route('/')
def index():
    return render_template('index.html')


def influx_data(host='localhost', port=8086):
    query = 'select * from pot;'
    user = 'root'
    password = 'root'
    dbname = 'honeycomb'
    client = InfluxDBClient(host, port, user, password, dbname)
    return list(client.query(query).get_points())

@application.route('/data')
def data():
    data = influx_data()
    return json.dumps(data)

@application.route('/chart-data')
def chart_data():
    data = influx_data()
    def generate_random_data():
        while True:
            json_data = json.dumps({})
            yield f"data:{json_data}\n\n"
            time.sleep(1)
    return Response(generate_random_data(), mimetype='text/event-stream')


if __name__ == '__main__':
    application.run(debug=True, threaded=True)