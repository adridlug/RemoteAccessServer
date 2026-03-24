#!/usr/bin/python
import cgi, cgitb
import xml.etree.ElementTree as ET
import psycopg2

from db_connection import get_db_connection

try:
	db_connection = get_db_connection()
	db_cursor = db_connection.cursor()
except (ValueError, KeyError, psycopg2.Error):
	print("Status: 500 Internal Server Error")
	print("Content-Type: text/plain\r\n")
	print("Database configuration error")
	raise SystemExit

form = cgi.FieldStorage()

try:
	client_id = int(form["clientid"].value)
except (KeyError, TypeError, ValueError):
	print("Status: 400 Bad Request")
	print("Content-Type: text/plain\r\n")
	print("Invalid request parameters")
	db_cursor.close()
	db_connection.close()
	raise SystemExit

db_cursor.execute("update commands set released = true where clients_id = %s", (client_id,))
db_connection.commit()

print("Status: 204 No Content")
print("Content-Type: text/plain\r\n")

db_cursor.close()
db_connection.close()