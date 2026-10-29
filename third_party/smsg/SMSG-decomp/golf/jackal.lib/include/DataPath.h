#ifndef DATAPATH_H
#define DATAPATH_H

extern char cd_path[256];
extern char alternate_path[256];
extern char file_path[256];
extern char game_path[520];

char *filefind_get(const char *name);

#endif
