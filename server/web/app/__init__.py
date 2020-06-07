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


def do_querey():
    pass


def all_reads(host='influxdb', port=8086):
    """
    Return all mesurements from a given node.
    """
    query = 'select * from pot;'
    user = 'root'
    password = 'root'
    dbname = 'honeycomb'
    client = InfluxDBClient(host, port, user, password, dbname)
    return list(client.query(query).get_points())


def select_entries(n):
    pass


def last_info():
    """
    Returns last info added to db.
    """
    pass


@application.route('/data')
def data():
    data = all_reads()
    return json.dumps(data)


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