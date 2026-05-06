#!/usr/bin/python
import cgi, cgitb
import login
import logging
from session_helper import create_session, get_session_cookie, get_session_username
#import debugpy

#debugpy.listen(("0.0.0.0", 5678))
#debugpy.wait_for_client()

cgitb.enable(display=0, logdir='/var/log/lighttpd')

form = cgi.FieldStorage()

username = form.getfirst("username", "")
password = form.getfirst("password", "")

if get_session_username(get_session_cookie()) or (username and password and login.login(username, password)):
    try:
        session_id = create_session(username)
        if not session_id:
            raise RuntimeError("Session creation failed")

        print("Status: 302 Found")
        print(f"Set-Cookie: session_id={session_id}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=3600")
        print("Location: /index.cgi")
        print()
        exit(0)
    except Exception as e:
        print("Status: 500 Internal Server Error")
        print()
    
print("Status: 401 Not Authorized")
print()
