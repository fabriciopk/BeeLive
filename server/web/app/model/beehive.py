import dateutil.parser

from app.db.influxdb import influx_query


class BeeHive:
    """
    This class stands for the model of one beehive sensor.
    """

    def __init__(self, sensor_id, measurement="environment_data"):
        self.sensor_id = sensor_id
        self.measurement = measurement
        self._last_mesurement = self.select_last()

    @property
    def get_id(self):
        return self.sensor_id

    @property
    def last_weight(self):
        return self._last_mesurement[-1]["hx_value"]

    @property
    def last_internal_tmp(self):
        return self._last_mesurement[-1]["ds_temp"]

    @property
    def last_internal_hum(self):
        return self._last_mesurement[-1]["dht_hum"]

    @property
    def coordinates(self):
        return self.sensor_id

    @property
    def last_update(self):
        date = dateutil.parser.parse(self._last_mesurement[0]["time"])
        return date.strftime("%d/%m/%Y %H:%m")

    def select_all(self):
        """
        Return all mesurements from a given beehive.
        """
        q = influx_query(
            f"select * from {self.measurement} where \"sensor-id\"='{self.sensor_id}'"
        )
        return list(q.get_points())

    def select_series(self, hours):
        """
        Return series of mesurements for a given period of time in hours.
        @pram int hours: Timedelta to retrive information from db.
        """
        q = influx_query(
            f"SELECT * FROM {self.measurement} where \"sensor-id\"='{self.sensor_id}' AND time > now() - {hours}h"
        )
        return list(q.get_points())

    def select_last(self):
        """
        Returns last mesurement done by sensor from the given behive.
        """
        q = influx_query(
            f"SELECT * FROM {self.measurement} where \"sensor-id\"='{self.sensor_id}' GROUP BY * ORDER BY DESC LIMIT 1"
        )
        return list(q.get_points())

    def mesurement_cnt(self):
        """
        Return the number of mesurements persisted into db of a given beehive.
        """

        q = influx_query(
            f"SELECT COUNT(dht_status) FROM {self.measurement} where \"sensor-id\"='{self.sensor_id}'"
        )
        return list(q.get_points())[0]["count"]
