from app.db.influxdb import influx_query
from app.model.beehive import BeeHive

class Farm:
    def __init__(self, measurement="environment_data"):
        self.measurement = measurement
        self._behive_lst = []

    def __get_sensor_tag(self):
        q = influx_query(f"show tag values with key=\"sensor-id\"")
        for item, key in q.items():
            if self.measurement in item[0]:
                for value in key:
                    yield value

    def list_beehives(self):
        """
        Return a list of behives instances.
        """
        sensor_id_list = []
        for sensor in self.__get_sensor_tag():
            sensor_id_list.append(BeeHive(sensor['value']))
        return sensor_id_list

    def __iter__(self):
        self._behive_lst = iter(self.list_beehives())
        return self

    def __next__(self):
        return next(self._behive_lst)