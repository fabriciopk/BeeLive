from flask import Blueprint

blueprint = Blueprint(
    'home_bp',
    __name__,
    url_prefix='',
    template_folder='templates',
    static_folder='static'
)