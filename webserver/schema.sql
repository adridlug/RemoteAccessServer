--
-- PostgreSQL database dump
--

-- Dumped from database version 16.4 (Debian 16.4-1)
-- Dumped by pg_dump version 17.3 (Debian 17.3-3)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Drop existing tables if present (safe re-import)
--

DROP TABLE IF EXISTS public.commands CASCADE;
DROP TABLE IF EXISTS public.clients CASCADE;

--
-- Name: commands; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.commands (
    id integer NOT NULL,
    clients_id integer,
    cmd character varying(20),
    local_port integer DEFAULT 0,
    remote_port integer DEFAULT 0,
    released boolean DEFAULT false,
    executed boolean DEFAULT false,
    marked_as_deleted boolean DEFAULT false,
    CONSTRAINT max_local_port CHECK ((local_port <= 65535)),
    CONSTRAINT max_remote_port CHECK ((remote_port <= 65535)),
    CONSTRAINT min_local_port CHECK ((local_port >= 0)),
    CONSTRAINT min_remote_port CHECK ((remote_port >= 0))
);


ALTER TABLE public.commands OWNER TO remote_admin_db_user;

--
-- Name: commands_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.commands_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.commands_id_seq OWNER TO remote_admin_db_user;

--
-- Name: commands_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.commands_id_seq OWNED BY public.commands.id;


--
-- Name: clients; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.clients (
    id integer NOT NULL,
    hostname character varying(100),
    internalip character varying(100),
    externalip character varying(100),
    currentuser character varying(100),
    os character varying(100),
    admin character varying(1),
    lastping timestamp without time zone,
    pinginterval integer
);


ALTER TABLE public.clients OWNER TO remote_admin_db_user;

--
-- Name: clients_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.clients_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.clients_id_seq OWNER TO remote_admin_db_user;

--
-- Name: clients_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.clients_id_seq OWNED BY public.clients.id;


--
-- Name: commands id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.commands ALTER COLUMN id SET DEFAULT nextval('public.commands_id_seq'::regclass);


--
-- Name: clients id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.clients ALTER COLUMN id SET DEFAULT nextval('public.clients_id_seq'::regclass);


--
-- Name: commands commands_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.commands
    ADD CONSTRAINT commands_pkey PRIMARY KEY (id);


--
-- Name: clients clients_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.clients
    ADD CONSTRAINT clients_pkey PRIMARY KEY (id);


--
-- Name: commands commands_clients_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.commands
    ADD CONSTRAINT commands_clients_id_fkey FOREIGN KEY (clients_id) REFERENCES public.clients(id);


--
-- Name: TABLE commands; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,UPDATE ON TABLE public.commands TO remote_admin_db_user;


--
-- Name: SEQUENCE commands_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,USAGE ON SEQUENCE public.commands_id_seq TO remote_admin_db_user;


--
-- Name: TABLE clients; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,UPDATE ON TABLE public.clients TO remote_admin_db_user;


--
-- Name: SEQUENCE clients_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,USAGE ON SEQUENCE public.clients_id_seq TO remote_admin_db_user;


--
-- PostgreSQL database dump complete
--
