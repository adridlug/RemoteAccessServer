#!/usr/bin/python
import cgi, cgitb
import sys
import xml.etree.ElementTree as ET
import psycopg2
import json
from db_connection import get_db_connection
from session_helper import get_session_cookie, get_session_username

try:
    username = get_session_username(get_session_cookie())
    
    if not username:
        print("Status: 401 Unauthorized")
        print()
        sys.exit(0)

    db_connection = get_db_connection()
    db_cursor = db_connection.cursor()

    form = cgi.FieldStorage()

    client_id = form["clientid"].value

    db_cursor.execute("select * from commands where clients_id = %s and marked_as_deleted = false", (client_id,))
    commands = db_cursor.fetchall()

    result = []

    for command in commands:
        is_queued = command[5] == False and command[6] == False
        is_released = command[5] == True and command[6] == False
        is_executed = command[5] == True and command[6] == True
        
        data = {
            "id": command[0],
            "client_id": command[1],
            "is_released": is_released,
            "is_queued": is_queued,
            "is_executed": is_executed,
            "command": command[2],
            "local_port": command[3],
            "remote_port": command[4]
        }

        result.append(data)

    json_string = json.dumps(result)  # Using indent for readable formatting
    print("Content-Type: application/json")  # Set the content type to JSON
    print()  # End of headers
    print(json_string)
except Exception:
    print("Status: 500 Internal Server Error")
    print()