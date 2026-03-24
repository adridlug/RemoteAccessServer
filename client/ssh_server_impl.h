struct ssh_server_options {
	char running;
	int local_port;
};


DWORD WINAPI start_ssh_server(LPVOID params);