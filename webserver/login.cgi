#!/usr/bin/python
import cgi, cgitb
import login
#import debugpy

#debugpy.listen(("0.0.0.0", 5678))
#debugpy.wait_for_client()

cgitb.enable(display=0, logdir='/var/log/lighttpd')

form = cgi.FieldStorage()

username = form.getfirst("username", "")
password = form.getfirst("password", "")

if not username or not password or not login.login(username, password):
    print("Status: 401 Not Authorized")
    print("Content-Type: text/plain")
    print()
    print("Wrong credentials")
else:
    print("Status: 200 OK")
    print("Content-Type: text/plain")
    print()
    print("Login successful")
