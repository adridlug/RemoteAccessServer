#!/usr/bin/python
import cgi, cgitb
import json
import psycopg2
from psycopg2.extras import RealDictCursor
from datetime import datetime
import logging
import sys

from db_connection import get_db_connection

# Configure logging
logging.basicConfig(
    filename='/var/log/lighttpd/error.log',
    level=logging.DEBUG,
    format='[%(asctime)s] %(levelname)s: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

# Enable CGI debugging
cgitb.enable(display=0, logdir='/var/log/lighttpd')

def send_json_response(data, status_code=200):
    """Send JSON response with proper headers"""
    if status_code != 200:
        print(f"Status: {status_code}")
    print("Content-Type: application/json")
    print("Cache-Control: no-cache, no-store, must-revalidate")
    print("Pragma: no-cache")
    print("Expires: 0")
    print()
    print(json.dumps(data))

def send_error(log_message, client_message="Internal server error", status_code=500):
    """Send error response as JSON with generic client message"""
    logging.error(log_message)
    send_json_response({"error": client_message, "success": False}, status_code)

try:
    # Connect to database
    try:
        db_connection = get_db_connection()
        db_cursor = db_connection.cursor(cursor_factory=RealDictCursor)
    except (ValueError, KeyError, psycopg2.Error) as e:
        logging.error(f"Database connection failed: {str(e)}", exc_info=True)
        send_error("Database connection failed", "Internal server error", 500)
        sys.exit(1)

    try:
        # Fetch all clients
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
            try:
                # Parse timestamp
                last_ping_str = client['lastping']
                if isinstance(last_ping_str, str):
                    # Handle various timestamp formats
                    try:
                        last_ping = datetime.strptime(last_ping_str, "%Y-%m-%d %H:%M:%S.%f")
                    except ValueError:
                        last_ping = datetime.strptime(last_ping_str, "%Y-%m-%d %H:%M:%S")
                else:
                    last_ping = last_ping_str

                ping_interval = client['pinginterval']
                
                # Calculate if client is alive
                # pinginterval is in milliseconds, convert to seconds
                elapsed_seconds = (datetime.now() - last_ping).total_seconds()
                interval_seconds = ping_interval / 1000.0 if ping_interval else 60
                is_alive = elapsed_seconds <= (interval_seconds * 1.5)  # 1.5x grace period
                
                # Build client data object
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
                
            except Exception as e:
                logging.error(f"Error processing client {client.get('id', 'unknown')}: {str(e)}", exc_info=True)
                continue

        logging.info(f"Successfully retrieved {len(result)} clients")
        send_json_response(result)

    finally:
        db_cursor.close()
        db_connection.close()

except Exception as e:
    logging.error(f"Unexpected error in getinfos.cgi: {str(e)}", exc_info=True)
    send_error("Unexpected error in getinfos.cgi", "Internal server error", 500)

