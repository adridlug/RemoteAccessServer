#!/usr/bin/python
import cgi, cgitb
import sys
import os
import json
import psycopg2
from psycopg2.extras import RealDictCursor
from datetime import datetime
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from core.db_connection import get_db_connection
from core.session_helper import get_session_cookie, get_session_username

def send_json_response(data, status_code=200):
    if status_code != 200:
        print(f"Status: {status_code}")
    print("Content-Type: application/json")
    print("Cache-Control: no-cache, no-store, must-revalidate")
    print("Pragma: no-cache")
    print("Expires: 0")
    print()
    print(json.dumps(data))

try:
    user_name = get_session_username(get_session_cookie())

    if not user_name:
        send_json_response({"error": "Unauthorized", "success": False}, 401)
        exit(0)
    
    db_connection = get_db_connection()
    db_cursor = db_connection.cursor(cursor_factory=RealDictCursor)
  
    db_cursor.execute("""
        SELECT id, hostname, internalip, 
                COALESCE(externalip, '') as externalip,
                currentuser, 
                COALESCE(os, '') as os,
                admin, lastping, pinginterval 
        FROM clients
        ORDER BY lastping DESC
    """)
    
    clients = db_cursor.fetchall()
    result = []

    for client in clients:
        
        last_ping_str = client['lastping']
        if isinstance(last_ping_str, str):
            try:
                last_ping = datetime.strptime(last_ping_str, "%Y-%m-%d %H:%M:%S.%f")
            except ValueError:
                last_ping = datetime.strptime(last_ping_str, "%Y-%m-%d %H:%M:%S")
        else:
            last_ping = last_ping_str

        ping_interval = client['pinginterval']
        
        elapsed_seconds = (datetime.now() - last_ping).total_seconds()
        interval_seconds = ping_interval / 1000.0 if ping_interval else 60
        is_alive = elapsed_seconds <= (interval_seconds * 1.5)  # 1.5x grace period
        
        data = {
            "id": client['id'],
            "hostname": client['hostname'] or "Unknown",
            "internal_ip": client['internalip'] or "Unknown",
            "external_ip": client['externalip'] or "N/A",
            "current_user": client['currentuser'] or "Unknown",
            "os": client['os'] or "Unknown",
            "admin": bool(client['admin']),
            "last_ping": str(client['lastping']),
            "ping_interval": ping_interval,
            "is_alive": is_alive
        }
        result.append(data)
                
    send_json_response(result)

except Exception as e:
     send_json_response({"error": "Internal server error", "success": False}, 500)
finally:
    if db_cursor is not None:
        db_cursor.close()
    if db_connection is not None:
        db_connection.close()
