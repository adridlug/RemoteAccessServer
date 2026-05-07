import configparser

import psycopg2


def get_db_connection(config_path="/var/www/remote-admin/db.conf", host="localhost"):
    db_config = configparser.ConfigParser()
    if not db_config.read(config_path):
        raise ValueError("Database config file not found")

    db_name = db_config["database"]["name"]
    db_user = db_config["database"]["user"]
    db_password = db_config["database"]["password"]

    return psycopg2.connect(dbname=db_name, user=db_user, password=db_password, host=host)