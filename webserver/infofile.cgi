#!/usr/bin/python
import cgi, cgitb
import xml.etree.ElementTree as ET
import psycopg2
import os
import sys
from  shutil import copyfileobj

# Der Pfad zur Binärdatei
file_path = "/home/kali/tinyhttpd-0.1.0/htdocs/testapp.exe"

# Überprüfen, ob die Datei existiert
if not os.path.isfile(file_path):
    print("Status: 404 Not Found")
    print("Content-type: text/html\n")
    print("<h1>File not found</h1>")
else:
    # Dateigröße bestimmen
    file_size = os.path.getsize(file_path)
    file_name = os.path.basename(file_path)

    print("Content-Type: application/octet-stream")
    print(f"Content-Disposition: attachment; filename=\"{file_name}\"")
    print(f"Content-Length: {file_size}")
    print("")
    
    sys.stdout.flush()
   
    with open(file_path, "rb") as f:
        while True:
            data = f.read(8192)
            if not data:
                break
            sys.stdout.buffer.write(data)
    sys.stdout.flush()