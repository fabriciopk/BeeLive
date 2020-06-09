import json
import random
import time
from datetime import datetime

from flask import Flask, Response, render_template
from importlib import import_module
from influxdb import InfluxDBClient

application = Flask(__name__, static_folder='home/static')
random.seed()


@application.route('/')
def index():
    return render_template('index.html')


def influx_query(query, host='localhost', port=8086):
    """
    Execute influx db query
    """
    user = 'root'
    password = 'root'
    dbname = 'honeycomb'
    client = InfluxDBClient(host, port, user, password, dbname)
    return client.query(query)


def all_reads():
    """
    Return all mesurements from a given node.
    """
    q = influx_query("select * from pot;")
    return list(q.get_points())


def select_entries(time_delta):
    q = influx_query(f"SELECT * FROM pot WHERE time > now() - {time_delta}h")
    return list(q.get_points())


def last_info():
    """
    Returns last info added to db.
    """
    q = influx_query("SELECT * FROM pot GROUP BY * ORDER BY ASC LIMIT 1")
    return list(q.get_points())


def count_readings():
    q = influx_query("SELECT COUNT(dht_status)  FROM pot;")
    return list(q.get_points())[0]['count']


@application.route('/count')
def count():
    return str(count_readings())


@application.route('/last_info')
def count_route():
    return json.dumps(last_info())



@application.route('/data')
def data_route():
    data = all_reads()
    return json.dumps(data)


@application.route('/data/<time_delta>')
def entry_route(time_delta):
    return json.dumps(select_entries(int(time_delta)))


@application.route('/chart-data')
def chart_data():
    # data = influx_data()
    def generate_random_data():
        while True:
            json_data = json.dumps({})
            yield f"data:{json_data}\n\n"
            time.sleep(1)
    return Response(generate_random_data(), mimetype='text/event-stream')


def create_app():
    module = import_module('app.home.routes')
    application.register_blueprint(module.blueprint)
    return application