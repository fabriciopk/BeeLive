import os
import json
import random
import time
from datetime import datetime

from flask import Flask, Response, render_template, send_from_directory
from importlib import import_module

from app.model.beehive import BeeHive
from app.model.farm import Farm

application = Flask(__name__, static_folder="home/static")
random.seed()


@application.route("/")
def index():
    return render_template("index.html")


@application.route("/beehive/<id>")
def beehive(id):
    return render_template("beehive.html", bee=BeeHive(id))


@application.route("/data/<id>")
def data_route(id):
    bh = BeeHive(id)
    return json.dumps(bh.select_all())


@application.route("/favicon.ico")
def favicon():
    return send_from_directory(
        os.path.join(application.root_path, "home/static"),
        "bee.ico",
        mimetype="image/vnd.microsoft.ico",
    )


def create_app():
    module = import_module("app.home.routes")
    application.register_blueprint(module.blueprint)
    return application
