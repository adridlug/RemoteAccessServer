#ifndef HELPERS_H
#define HELPERS_H

#include <windows.h>

int get_key_path_from_exe_dir(const char* keyName, char* keyPathOut);

#endif // HELPERS_H
#include <windows.h>
HANDLE start_thread(LPTHREAD_START_ROUTINE function, LPVOID param);
void get_pc_infos(int update_interval, char* info_buffer);
int extractValue(const char* input, const char* pos);