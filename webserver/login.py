import hashlib
import psycopg2

from db_connection import get_db_connection


def login(username: str, password: str) -> bool:
    try:
        db_connection = get_db_connection()
        db_cursor = db_connection.cursor()
    except (ValueError, KeyError, psycopg2.Error):
        return False

    try:
        db_cursor.execute(
            "SELECT passwordhash, salt FROM public.users WHERE username = %s",
            (username,)
        )
        row = db_cursor.fetchone()
    except psycopg2.Error:
        return False
    finally:
        db_cursor.close()
        db_connection.close()

    if row is None:
        return False

    stored_hash = row[0].strip()
    salt = row[1].strip()

    computed_hash = hashlib.sha256((password + salt).encode()).hexdigest()

    return computed_hash == stored_hash
