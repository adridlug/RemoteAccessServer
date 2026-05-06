#!/usr/bin/python
import cgi, cgitb
import xml.etree.ElementTree as ET
import psycopg2
from db_connection import get_db_connection
from session_helper import get_session_cookie, get_session_username

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
	
	db_cursor.execute("update commands set released = true where clients_id = %s", (client_id,))
	db_connection.commit()

	print("Status: 204 No Content")
	print("Content-Type: text/plain\r\n")

except:
	print("Status: 500 Internal Server Error")
	print("Content-Type: text/plain\r\n")
	print("Database configuration error")
	raise SystemExit
finally:
	if db_cursor is not None:
		db_cursor.close()
	if db_connection is not None:
		db_connection.close()