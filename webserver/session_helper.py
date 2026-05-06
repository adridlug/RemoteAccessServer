import db_connection
from datetime import datetime, timedelta
import os
import secrets


def create_session(username, duration_hours=1):
    """Create a session for a user and return the new session_id."""
    if not username:
        return None

    session_id = secrets.token_urlsafe(32)
    expires_at = datetime.utcnow() + timedelta(hours=duration_hours)

    conn = None
    cur = None
    try:
        conn = db_connection.get_db_connection()
        cur = conn.cursor()
        cur.execute(
            "INSERT INTO public.sessions (session_id, username, expires_at) VALUES (%s, %s, %s)",
            (session_id, username, expires_at)
        )
        conn.commit()
        return session_id
    except Exception:
        raise
    finally:
        if cur is not None:
            cur.close()
        if conn is not None:
            conn.close()


def get_session_username(session_id):
    """Returns username if session is valid, None otherwise"""
    if not session_id:
        return None
    
    try:
        conn = db_connection.get_db_connection()
        cur = conn.cursor()
        cur.execute(
            "SELECT username, expires_at FROM public.sessions WHERE session_id = %s",
            (session_id,)
        )
        result = cur.fetchone()

        if not result:
            cur.close()
            conn.close()
            return None

        username, expires_at = result
        if expires_at <= datetime.utcnow():
            # Delete expired session from DB; the browser cookie will be rejected on
            # subsequent requests since the server-side record no longer exists.
            cur.execute("DELETE FROM public.sessions WHERE session_id = %s", (session_id,))
            conn.commit()
            cur.close()
            conn.close()
            return None

        cur.close()
        conn.close()

        return username
    except Exception:
        return None


def get_session_cookie():
    """Extract session_id from HTTP_COOKIE environment variable"""
    cookies = os.environ.get("HTTP_COOKIE", "")
    for cookie in cookies.split(";"):
        name, _, value = cookie.strip().partition("=")
        if name == "session_id":
            return value
    return None


def delete_session(session_id):
    """Delete a session from the database"""
    if not session_id:
        return False
    
    try:
        conn = db_connection.get_db_connection()
        cur = conn.cursor()
        cur.execute("DELETE FROM public.sessions WHERE session_id = %s", (session_id,))
        conn.commit()
        cur.close()
        conn.close()
        return True
    except Exception:
        return False
