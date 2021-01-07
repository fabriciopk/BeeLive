from app.home import blueprint
from flask import redirect, url_for, render_template, abort
from app.model.farm import Farm


@blueprint.route("/<template>")
def route_template(template):
    try:
        if template == "ui-tables":
            return render_template(template + ".html", farm=Farm)
        return render_template(template + ".html")
    except Exception as e:
        abort(404)


@blueprint.route("/index")
def index():
    return render_template("index.html")
