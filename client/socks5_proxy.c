// #include <stdio.h>
// //#include <windows.h>
// //#include <winsock2.h>
// #include <WS2tcpip.h>
// #include "socks5_proxy.h"
// #define _WINSOCK_DEPRECATED_NO_WARNING 0x01

// #define VERSION		0x05
// #define NOAUTH		0x00
// #define NOTMETHOD	0xFF
// #define ATYP_IP		0x01
// #define ATYP_DOMAIN 0x03
// #define SUCCESS		0x00
// #define BUFFER_SIZE 0x10000



// int readn(int socket, void* buffer, int len) {
// 	int read_bytes = 0;

// 	while (read_bytes != 0) {
// 		int tmp_read_bytes = recv(socket, (int)buffer + read_bytes, len - read_bytes, 0);
// 		if (tmp_read_bytes <= 0) {
// 			return tmp_read_bytes;
// 		}

// 		read_bytes += tmp_read_bytes;
// 	}
// }

// int writen(int socket, void* buffer, int len) {
// 	int bytes_written = 0;

// 	while (bytes_written != len) {
// 		int tmp_bytes_written = send(socket, (int)buffer + bytes_written, len - bytes_written, 0);
// 		if (tmp_bytes_written < 0) {
// 			return tmp_bytes_written;
// 		}

// 		bytes_written += tmp_bytes_written;
// 	}
// }

// int authenticate(int incoming_socket) {
// 	int read_bytes;
// 	char version_method[2];
// 	int supported = 0;
// 	read_bytes = readn(incoming_socket, version_method, sizeof version_method, 0);

// 	if (read_bytes < 0) {
// 		return -1;
// 	}

// 	if (version_method[0] = !VERSION) {
// 		return -1;
// 	}

// 	for (int i = 0; i < version_method[1]; i++) {
// 		char type;
// 		read_bytes = readn(incoming_socket, &type, 1);

// 		if (read_bytes < 0) {
// 			return -1;
// 		}

// 		if (type == NOAUTH) { //not auth required 
// 			supported = 1;
// 		}
// 	}

// 	if (supported == 0) {
// 		char answer[2] = { VERSION, NOTMETHOD };
// 		writen(incoming_socket, answer, sizeof answer);
// 		return -1;
// 	}
// 	char answer[2] = { VERSION, NOAUTH };
// 	if (writen(incoming_socket, answer, sizeof answer) < 0) {
// 		return -1;
// 	}
// }

// int handle_connection_request(int incoming_socket) {
// 	int ret, local_socket;
// 	struct sockaddr_in target_addr;
// 	char target_domain_addr_size;
// 	char target_domain_addr;
// 	struct addrinfo* target_addrinfo;
	
// 	char request[4];

// 	if (readn(incoming_socket, request, sizeof request) < 0) {
// 		return -1;
// 	}
// 	char atyp = request[3];

// 	if (atyp == ATYP_IP) {
// 		char* ip = (char*)malloc(sizeof(char) * 4);
		
// 		if (readn(incoming_socket, ip, sizeof(char) * 4) < 0) {
// 			return -1;
// 		}

// 		unsigned short int target_port;
// 		if (readn(incoming_socket, &target_port, sizeof(unsigned short int))  < 0) {
// 			return -1;
// 		}

// 		memset(&target_addr, 0, sizeof(target_addr));
// 		target_addr.sin_family = AF_INET;
// 		//target_addr.sin_addr.s_addr = inet_addr(ip);
// 		InetPton(AF_INET, ip, &target_addr.sin_addr.s_addr);
// 		target_addr.sin_port = htons(target_port);

// 		local_socket = socket(AF_INET, SOCK_STREAM, 0);

// 		if (connect(local_socket, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
// 			return -1;
// 		}

// 		char answer[10] = { VERSION, SUCCESS, 0x00, ATYP_IP, ip, target_port };

// 		if (writen(incoming_socket, answer, sizeof answer) < 0) {
// 			return -1;
// 		}
// 	}
// 	else if (atyp == ATYP_DOMAIN) {

// 		if (readn(incoming_socket, &target_domain_addr_size, sizeof target_domain_addr_size) < 0) {
// 			return -1;
// 		}

// 		target_domain_addr = (char*)malloc((sizeof(char) * target_domain_addr_size) + 1);

// 		if (readn(incoming_socket, &target_domain_addr, (sizeof(char) * target_domain_addr_size)) < 0) {
// 			return -1;
// 		}

// 		char* target_port = (char*)malloc(sizeof(unsigned short int));
// 		if (readn(incoming_socket, &target_port, sizeof(unsigned short int)) < 0) {
// 			return -1;
// 		}

// 		ret = getaddrinfo(target_domain_addr, target_port, NULL, &target_addrinfo);

// 		if (ret == EAI_NODATA) {
// 			return -1;
// 		}
// 		else if (ret == 0) {
// 			struct addrinfo* addrinfo;

// 			for (addrinfo = target_addrinfo; addrinfo != NULL; addrinfo = addrinfo->ai_next) {
// 				local_socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);

// 				if (local_socket == -1) {
// 					continue;
// 				}

// 				if (connect(local_socket, addrinfo->ai_addr, addrinfo->ai_addrlen) == 0) {
// 					freeaddrinfo(target_addrinfo);
// 					return local_socket;
// 				}
// 				else {
// 					close(local_socket);
// 				}
// 			}
// 			freeaddrinfo(target_addrinfo);
// 		}
// 	}

// 	return -1;
// }

// void process_incoming_socket(void* params) {
// 	int incoming_socket = (int)params;
// 	fd_set set;
// 	int max_socket, ret;
// 	int bytes_read;

// 	if (authenticate(incoming_socket) == -1) {
// 		return;
// 	}

// 	int local_socket = handle_connection_request(incoming_socket);
// 	if (local_socket < 0) {
// 		return;
// 	}

// 	char buffer[BUFFER_SIZE];

// 	while (1) {
// 		FD_ZERO(&set);
// 		FD_SET(local_socket, &set);
// 		FD_SET(incoming_socket, &set);
// 		max_socket = (local_socket > incoming_socket) ? local_socket : incoming_socket;

// 		if (select(max_socket + 1, &set, NULL, NULL, NULL) < 0 && errno == EINTR) {
// 			continue;
// 		}

// 		if (FD_ISSET(incoming_socket, &set)) {
// 			bytes_read = recv(incoming_socket, &buffer, BUFFER_SIZE, 0);

// 			if (bytes_read <= 0) {
// 				return;
// 			}

// 			if (writen(local_socket, &buffer, bytes_read) < 0) {
// 				return;
// 			}
// 		}

// 		if (FD_ISSET(local_socket, &set)) {
// 			bytes_read = recv(local_socket, &buffer, BUFFER_SIZE, 0);

// 			if (bytes_read <= 0) {
// 				return;
// 			}

// 			if (writen(incoming_socket, &buffer, bytes_read) < 0) {
// 				return;
// 			}
// 		}
// 	}
// }

// DWORD WINAPI start_socks5_proxy(LPVOID param) {
// 	int listen_socket, incoming_socket;
// 	int opt_val = 1;
// 	struct sockaddr_in listen_socket_addr;
// 	struct sockaddr incoming_addr;
// 	int incoming_addr_len = 0;

// 	struct socks5_proxy_param* socks5_param = (struct socks5_proxy_param*)param;

// 	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

// 	if (listen_socket < 0) {
// 		//printf("could not create listen socket");
// 		return;
// 	}

// 	if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) {
// 		//printf("could not create listen socket");
// 		return;
// 	}

// 	memset(&listen_socket_addr, 0, sizeof(struct sockaddr_in));
// 	listen_socket_addr.sin_family = AF_INET;
// 	listen_socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
// 	listen_socket_addr.sin_port = htons(socks5_param->local_port);

// 	if (bind(listen_socket, &listen_socket_addr, sizeof(listen_socket_addr)) < 0) {
// 		//printf("could not bind listen socket");
// 		return;
// 	}

// 	if (listen(listen_socket, 25) < 0) {
// 		//printf("could not start listening socket");
// 		return;
// 	}

// 	while (1) {
		
// 		incoming_socket = accept(listen_socket, &incoming_addr, &incoming_addr_len);

// 		if (incoming_socket < 0) {
// 			//printf("failed to accept incoming connection");
// 			return;
// 		}

// 		if (setsockopt(incoming_socket, SOL_SOCKET, TCP_NODELAY, &opt_val, sizeof(opt_val)) < 0) {
// 			//printf("failed to accept incoming connection");
// 			return;
// 		}

// 		process_incoming_socket(&incoming_socket);
// 	}

// }