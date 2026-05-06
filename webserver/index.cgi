#!/usr/bin/python
import os
from session_helper import get_session_cookie, get_session_username

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INDEX_HTML = os.path.join(SCRIPT_DIR, "index.html")

try:
    session_id = get_session_cookie()
    username = get_session_username(session_id)
    if not username:
        print("Status: 302 Found")
        print("Location: /login.cgi")
        print()
        exit(0)

    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    print("Status: 200 OK")
    print("Content-Type: text/html; charset=utf-8")
    print()
    print(html)
except Exception:
    print("Status: 500 Internal Server Error")
    print()
