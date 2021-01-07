from influxdb import InfluxDBClient


def influx_query(query, host="influxdb", port=8086):
    """
    Execute influx db query
    """
    user = "root"
    password = "root"
    dbname = "honey-comb"
    client = InfluxDBClient(host, port, user, password, dbname)
    try:
        return client.query(query)
    except Exception as e:
        print(f"Not possible to fetch data from db {e}.")
