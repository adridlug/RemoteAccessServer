#include <stdio.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <windows.h>
#include <winternl.h>
#include <pthread.h>

#include "helpers.h"
#include "ssh_server_impl.h"

#define USER "client_user"

struct channel_data_struct {
    HANDLE pipe_in_handle;
    HANDLE pipe_out_handle;
    COORD console_size;
    HPCON pseudo_con_handle;
    ssh_event event;
    HANDLE console_thread_handle;
    HANDLE console_process_handle;
    LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
};

struct session_data_struct {
    ssh_channel channel;
    struct channel_data_struct *channel_data;
    int auth_attempts;
    int authenticated;
};

#include <stdio.h>

void hexdump(const void* buffer, size_t length) {
    const unsigned char* buf = (const unsigned char*)buffer;
  size_t i, j;

    for (i = 0; i < length; i += 16) {
        printf("%06zx: ", i);
        for (j = 0; j < 16; j++) {
            if (i + j < length)
                printf("%02x ", buf[i + j]);
            else
                printf("   ");
        }
        printf(" ");
        for (j = 0; j < 16 && i + j < length; j++) {
            unsigned char c = buf[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
}

static int auth_publickey(ssh_session session,
                          const char *user,
                          struct ssh_key_struct *pubkey,
                          char signature_state,
                          void *userdata) {
    struct session_data_struct *sdata = (struct session_data_struct *) userdata;

    (void) user;
    (void) session;

    if (signature_state == SSH_PUBLICKEY_STATE_NONE) {
        return SSH_AUTH_SUCCESS;
    }

    if (signature_state != SSH_PUBLICKEY_STATE_VALID) {
        return SSH_AUTH_DENIED;
    }

    // valid so far. Now look through authorized keys for a match
    ssh_key key;
    char keyPath[MAX_PATH];
    int rc = -1;
    
    if (get_key_path_from_exe_dir("local_server_key.pub", keyPath) == 0) {
        rc = ssh_pki_import_pubkey_file(keyPath, &key);
    }

    if ((rc == SSH_OK) && (key!=NULL)) {
        rc = ssh_key_cmp(key, pubkey, SSH_KEY_CMP_PUBLIC );
        ssh_key_free(key);
        if (rc == 0) {
            sdata->authenticated = 1;
            return SSH_AUTH_SUCCESS;
        }
    }
    
    // no matches
    sdata->authenticated = 0;
    return SSH_AUTH_DENIED;
}

static ssh_channel open_channel(ssh_session session, void* userdata) {
    struct session_data_struct* sdata = (struct session_data_struct*)userdata;
    sdata->channel = ssh_channel_new(session);
    return sdata->channel;
}

static int pty_request(ssh_session session, ssh_channel channel, const char* term, 
    int cols, int rows, int py, int px, void* userdata) {
    struct channel_data_struct* cdata = (struct channel_data_struct*)userdata;
    
    HRESULT hr;
    HANDLE hPipePTYIn = INVALID_HANDLE_VALUE;
    HANDLE hPipePTYOut = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&hPipePTYIn, &cdata->pipe_out_handle, NULL, 0) ||
        !CreatePipe(&cdata->pipe_in_handle, &hPipePTYOut, NULL, 0)) {
        return SSH_ERROR;
    }

    cdata->console_size.X = cols;
    cdata->console_size.Y = rows;

    hr = CreatePseudoConsole(cdata->console_size, hPipePTYIn, hPipePTYOut, 0, &cdata->pseudo_con_handle);

    if (hPipePTYIn != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipePTYIn);
    }
    
    if (hPipePTYOut != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipePTYOut);
    }

    return hr;
}

static int pty_resize(ssh_session session, ssh_channel channel, int cols, int rows, int py, int px, void* userdata) {
    struct channel_data_struct *cdata = (struct channel_data_struct*)userdata;
    
    COORD size;
    size.X = cols;
    size.Y = rows;

    // Call pseudoconsole API to inform buffer dimension update
    return ResizePseudoConsole(cdata->pseudo_con_handle, size);
}

static int exec_request(ssh_session session, ssh_channel channel, const char* command, void* userdata) {
    struct channel_data_struct* cdata = (struct channel_data_struct*)userdata;
    return SSH_ERROR;
}

static int data_function(ssh_session session, ssh_channel channel, void* data, uint32_t len, int is_stderr, void *userdata) {
    struct channel_data_struct* cdata = (struct channel_data_struct*)userdata;
    int rc;
    DWORD bytes_written;
    rc = WriteFile(cdata->pipe_out_handle, (LPCVOID)data, len, &bytes_written, NULL);
    if (!rc) {
        return SSH_ERROR;
    }

    return bytes_written;
}

static int subsystem_request(ssh_session session, ssh_channel channel, const char* subsystem, void* userdata) {
    struct channel_data_struct* cdata = (struct channel_data_struct*)userdata;
    return SSH_ERROR;
}

static int shell_request(ssh_session session, ssh_channel channel, void* userdata) {
   
    struct channel_data_struct* cdata = (struct channel_data_struct*)userdata;
    STARTUPINFOEXW startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));

    startup_info.StartupInfo.cb = sizeof(startup_info);

    size_t size;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
   

    startup_info.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, size);
    
    if (!startup_info.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    BOOL hr = InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0, &size);

    if (!hr) {
        HeapFree(GetProcessHeap(), 0, startup_info.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    cdata->attribute_list = startup_info.lpAttributeList;

    hr = UpdateProcThreadAttribute(
        startup_info.lpAttributeList, 
        0, 
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, 
        cdata->pseudo_con_handle, 
        sizeof(cdata->pseudo_con_handle),
        NULL, 
        NULL);
    
    if (!hr) {
        HeapFree(GetProcessHeap(), 0, startup_info.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    PCWSTR childApplication =  L"C:\\windows\\system32\\cmd.exe";
    const size_t bytes_required = wcslen(childApplication) + 1;
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * bytes_required);

    if (!cmdLineMutable) {
        HeapFree(GetProcessHeap(), 0, startup_info.lpAttributeList);
        return E_OUTOFMEMORY;
    }

    wcscpy_s(cmdLineMutable, bytes_required, childApplication);

    PROCESS_INFORMATION proc_info;
    memset(&proc_info, 0, sizeof(proc_info));

    hr = CreateProcessW(
        NULL,
        cmdLineMutable,
        NULL,
        NULL,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        NULL,
        NULL,
        &startup_info.StartupInfo,
        &proc_info);

    HeapFree(GetProcessHeap(), 0, cmdLineMutable); 

    cdata->console_thread_handle = proc_info.hThread;
    cdata->console_process_handle = proc_info.hProcess;
    
    return hr ? SSH_OK : HRESULT_FROM_WIN32(GetLastError());
}

static DWORD WINAPI poll_pipe_in(LPVOID userdata) {
    struct session_data_struct *session_data = (struct session_data_struct*)userdata;
    char buffer[256];
    do {
        memset(&buffer, 0, sizeof(buffer));
        DWORD bytes_read;
        
        if (ReadFile(session_data->channel_data->pipe_in_handle, buffer, 256, &bytes_read, NULL)) {

            if (ssh_channel_write(session_data->channel, buffer, bytes_read) == SSH_ERROR) { 
                ssh_channel_close(session_data->channel);            
            }
        }
    } while (ssh_channel_is_open(session_data->channel));

    return 0;
}


void handle_session(ssh_event event, ssh_session session, struct ssh_server_options* options) {
    int auth_poll_interval_cnt;
    int rc;
    HANDLE poll_pipe_in_thread_handle = NULL;
    COORD con_size;
    con_size.X = -1;
    con_size.Y = -1;

    struct channel_data_struct channel_data = {
        .pipe_in_handle = INVALID_HANDLE_VALUE,
        .pipe_out_handle = INVALID_HANDLE_VALUE,
        .pseudo_con_handle = INVALID_HANDLE_VALUE,
        .console_size = con_size,
        .console_process_handle = INVALID_HANDLE_VALUE,
        .console_thread_handle = INVALID_HANDLE_VALUE,
        .attribute_list = NULL
    };

    struct session_data_struct session_data = {
        .channel = NULL,
        .channel_data = &channel_data,
        .auth_attempts = 0,
        .authenticated = 0
    };


    struct ssh_server_callbacks_struct server_callbacks = {
       .userdata = &session_data,
       //.auth_password_function = auth_with_pwd,
       .auth_pubkey_function = auth_publickey,
       .channel_open_request_session_function = open_channel,
    };

    struct ssh_channel_callbacks_struct channel_callbacks = {
        .userdata = &channel_data,
        .channel_pty_request_function = pty_request,
        .channel_pty_window_change_function = pty_resize,
        .channel_shell_request_function = shell_request,
        .channel_exec_request_function = exec_request,
        .channel_data_function = data_function,
        .channel_subsystem_request_function = subsystem_request
    };


    ssh_set_auth_methods(session, SSH_AUTH_METHOD_PUBLICKEY);

    ssh_callbacks_init(&server_callbacks);
    ssh_callbacks_init(&channel_callbacks);

    ssh_set_server_callbacks(session, &server_callbacks);

    if (ssh_handle_key_exchange(session)) {
        return;
    }

    ssh_event_add_session(event, session);

    //do authentication
    auth_poll_interval_cnt = 0;
    
    while (session_data.authenticated == 0 || session_data.channel == NULL) {

        if (session_data.auth_attempts >= 3 || auth_poll_interval_cnt >= 100) {
            return;
        }

        if (ssh_event_dopoll(event, 100) == SSH_ERROR) {
            break;
        }

        auth_poll_interval_cnt++;
    }

    ssh_set_channel_callbacks(session_data.channel, &channel_callbacks);

    do {
        if (ssh_event_dopoll(event, 100) == SSH_ERROR) {
            ssh_channel_close(session_data.channel);
            break;
        }

        if (channel_data.event != NULL) {
             continue;
        }

        channel_data.event = event;
        
        //pty and shell request seems to arrive after first event poll. so lets start threade here to 
        //continuously read from pipe
        poll_pipe_in_thread_handle = start_thread(poll_pipe_in, &session_data);
        
    } while (options->running);

    ssh_channel_close(session_data.channel);

    if (poll_pipe_in_thread_handle != NULL) {
        WaitForSingleObject(poll_pipe_in_thread_handle, INFINITE);
        CloseHandle(poll_pipe_in_thread_handle);
    }

    ssh_channel_free(session_data.channel);

    if (channel_data.console_thread_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(channel_data.console_thread_handle);
    }

    if (channel_data.console_process_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(channel_data.console_process_handle);
    }

    if (channel_data.attribute_list != NULL) {
        DeleteProcThreadAttributeList(channel_data.attribute_list);
        HeapFree(GetProcessHeap(), 0, channel_data.attribute_list);
    }
    
    if (channel_data.pseudo_con_handle != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(channel_data.pseudo_con_handle);
    }
}

DWORD WINAPI start_ssh_server(LPVOID params)
{
    struct ssh_server_options* options = (struct ssh_server_options*)params;

    char keyPath[MAX_PATH];
    int rc;
    ssh_bind sshbind;
    ssh_session session;
    int port = options->local_port;
    ssh_event event;

    sshbind = ssh_bind_new();

    if (sshbind == NULL) {
        return -1;
    }

    if (get_key_path_from_exe_dir("host_key", keyPath) != 0) {
        return -1;
    }

    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, "127.0.0.1");
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, keyPath);
    
    rc = ssh_bind_listen(sshbind);

    if (rc < 0) {
        return -1;
    }

    rc = ssh_bind_listen(sshbind);

    if (rc < 0) {
        return -1;
    }

    while (options->running) {
        session = ssh_new();

        if (session == NULL) {
            return -1;
        }

        if (ssh_bind_accept(sshbind, session) != SSH_OK) {
            goto DISCONNECT;
        }

        event = ssh_event_new();

        handle_session(event, session, options);

        ssh_event_free(event);

    DISCONNECT:
        ssh_disconnect(session);
        ssh_free(session);
    }

    return 0;
}

