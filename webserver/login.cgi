#!/usr/bin/python
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core.session_helper import get_session_cookie, get_session_username

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LOGIN_HTML = os.path.join(SCRIPT_DIR, "html", "login.html")

try:
    session_id = get_session_cookie()
    username = get_session_username(session_id)
    if username:
        print("Status: 302 Found")
        print("Location: /index.cgi")
        print()
        exit(0)

    with open(LOGIN_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    print("Status: 200 OK")
    print("Content-Type: text/html; charset=utf-8")
    print()
    print(html)
except Exception:
    print("Status: 500 Internal Server Error")
    print()
