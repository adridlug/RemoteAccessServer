#!/usr/bin/python
import cgi, cgitb
import psycopg2

from core.db_connection import get_db_connection
from webserver.core.session_helper import get_session_cookie, get_session_username

try:
	user_name = get_session_username(get_session_cookie())
	if not user_name:
		print("Status: 401 Unauthorized")
		print()
		exit(0)
	
	db_connection = get_db_connection()
	db_cursor = db_connection.cursor()

	form = cgi.FieldStorage()

	client_id = int(form["clientid"].value)
	cmd_id = int(form["cmdid"].value)

	db_cursor.execute("update commands set marked_as_deleted = true where id = %s and clients_id = %s", (cmd_id, client_id))

	db_connection.commit()

	print("Status: 204 No Content")
	print("Content-Type: text/plain\r\n")

except Exception:
	print("Status: 500 Internal Server Error")
	print()
finally:
	if db_cursor is not None:
		db_cursor.close()
	if db_connection is not None:
		db_connection.close()