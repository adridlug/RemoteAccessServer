#!/usr/bin/python
import cgi, cgitb
import sys
import xml.etree.ElementTree as ET
import psycopg2
from core.db_connection import get_db_connection
from core.session_helper import get_session_cookie, get_session_username

def cmd_exists(db_cursor, client_id, cmd, local_port = 0, remote_port = 0):
    db_cursor.execute("select * from commands where clients_id = %s and cmd = %s and local_port = %s and remote_port = %s and executed = false and marked_as_deleted = false", 
                      (client_id, cmd, local_port, remote_port))
    rows = db_cursor.fetchall()
    return len(rows) > 0

def create_cmd(db_connection, db_cursor, client_id, cmd, local_port = 0, remote_port = 0):
    db_cursor.execute("insert into commands (clients_id, cmd, local_port, remote_port) values (%s, %s, %s, %s)",
                      (client_id, cmd, local_port, remote_port))
    db_connection.commit()

try:
    username = get_session_username(get_session_cookie())
    
    if not username:
        print("Status: 401 Unauthorized")
        print()
        exit(0)
    
    db_connection = get_db_connection()
    db_cursor = db_connection.cursor()
    
    form = cgi.FieldStorage()
    client_id = int(form["clientid"].value)
    cmd = form["cmd"].value
    local_port = 0
    remote_port = 0

    if cmd != "opensshtunnel" and cmd != "opentcptunnel" and cmd != "opendynamictunnel" and cmd != "closesshtunnel" and cmd != "closetcptunnel" and cmd != "closedynamictunnel":
        raise ValueError("Invalid command")

    if cmd == "opensshtunnel" or cmd == "opentcptunnel" or cmd == "opendynamictunnel":
        local_port = int(form["L"].value)
        remote_port = int(form["R"].value)
        if local_port < 1 or local_port > 65535 or remote_port < 1 or remote_port > 65535:
            raise ValueError("Port out of range")
        
    if not cmd_exists(db_cursor, client_id, cmd, local_port, remote_port):
        create_cmd(db_connection, db_cursor, client_id, cmd, local_port, remote_port)

    print("Status: 204 No Content")
    print("Content-Type: text/plain\r\n")
    print()

except Exception:
    print("Status: 500 Internal Server Error")
    print()
finally:
    db_cursor.close()
    db_connection.close()



