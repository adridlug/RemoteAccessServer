#!/usr/bin/python
import cgi, cgitb
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from core.login import login
from core.session_helper import create_session, get_session_cookie, get_session_username

try:
    form = cgi.FieldStorage()

    username = form.getfirst("username", "")
    password = form.getfirst("password", "")

    if get_session_username(get_session_cookie()) or (username and password and login(username, password)):
        
        session_id = create_session(username)
        if not session_id:
            raise RuntimeError("Session creation failed")

        print("Status: 302 Found")
        print(f"Set-Cookie: session_id={session_id}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=3600")
        print("Location: /index.cgi")
        print()
        exit(0)
       
    print("Status: 401 Not Authorized")
    print()

except Exception:
    print("Status: 500 Internal Server Error")
    print()