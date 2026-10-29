#include "jgldefs.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "DataPath.h"
#include "JackalUtil.h"

char cd_path[256] = {0};
char alternate_path[256] = {0};
char file_path[256] = {0};
char game_path[2 * MAX_PATH] = {0};

void filefind_set_alternate(const char *dir)
{
    alternate_path[0] = 0;
    if (dir) {
        if (!strchr(dir, ':') && *dir != '\\')
            strcat(alternate_path, game_path);
        strcat(alternate_path, dir);
    }
}

char *filefind_get(const char *name)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (!name)
        return 0;
    if (name == file_path)
        return file_path;
    if (name[1] == ':') {
        ClearString(file_path);
        AppendString(file_path, name);
        h = FindFirstFileA(file_path, &fd);
        FindClose(h);
        return h != INVALID_HANDLE_VALUE ? file_path : 0;
    }
    if (alternate_path[0]) {
        ClearString(file_path);
        AppendString(file_path, alternate_path);
        AppendString(file_path, name);
        h = FindFirstFileA(file_path, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            FindClose(h);
            return file_path;
        }
    }
    ClearString(file_path);
    AppendString(file_path, game_path);
    AppendString(file_path, name);
    h = FindFirstFileA(file_path, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        FindClose(h);
        return file_path;
    }
    if (cd_path[0]) {
        ClearString(file_path);
        AppendString(file_path, cd_path);
        AppendString(file_path, name);
        h = FindFirstFileA(file_path, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            FindClose(h);
            return file_path;
        }
    }
    return 0;
}

long GetDataFileSize(const char *name)
{
    char *path = filefind_get(name);
    if (path == 0)
        return -1;
    FILE *f = fopen(path, "rb");
    if (f == 0)
        return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

void SetDataDirs(const char *dirs)
{
    unsigned int n = strcspn(dirs, ";");
    strncpy(alternate_path, dirs, n);
    alternate_path[n] = 0;
    if (dirs[n] == ';')
        strcpy(cd_path, dirs + n + 1);
}
