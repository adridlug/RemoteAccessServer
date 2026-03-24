#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>
#include "helpers.h"
ssh_session connect_to_ssh_server(char *ip, int port, char* user) {
	ssh_session session;
	int rc = -1;
	ssh_channel channel;
	session = ssh_new();

	if (session == NULL) {
		return NULL;
	}

	ssh_options_set(session, SSH_OPTIONS_HOST, ip);
	ssh_options_set(session, SSH_OPTIONS_PORT, &port);
	ssh_options_set(session, SSH_OPTIONS_USER, user);

	if (ssh_connect(session) != SSH_OK) {
		ssh_free(session);
		return NULL;
	}

	char keyPath[MAX_PATH];
	rc = get_key_path_from_exe_dir("key", keyPath);

	if (rc == 0) {
		ssh_key privkey;
		rc = ssh_pki_import_privkey_file(keyPath, NULL, NULL, NULL, &privkey);

		if(rc == SSH_OK) {
			rc = ssh_userauth_publickey(session, NULL, privkey);
		}
	}
	
	if (rc != SSH_AUTH_SUCCESS) {
		ssh_disconnect(session);
		ssh_free(session);
		return NULL;
	}

	return session;
}