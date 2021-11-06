#ifndef HW05_ARCHIVE_H
#define HW05_ARCHIVE_H
#include "stdbool.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "string.h"
#include "pwd.h"
#include "unistd.h"
#include "grp.h"
#include "fcntl.h"
#include <utime.h>

#define PATH_SIZE 512
#define is_dir(in) ((in) == 4)

typedef struct head
{
    char name[100];
    char mode[8];
    char owner_id[8];
    char group_id[8];
    char file_size[12];
    char last_modification[12];
    char check_sum[8];
    char flag[1];
    char link_name[100];
    char magic_num[6];
    char ustar_ver[2];
    char owner_str[32];
    char group_str[32];
    char major_num[8];
    char minir_num[8];
    char prefix[155];
    char other[12];
} header;

typedef struct tar_archive
{
    header head;
} tar_archive;

typedef struct param
{
    bool stats[3]; //view = 0, create, open = 2
    char *archive_name;
    char **sources;
    int file_count;
} param;

bool execute(param *parameters);
bool load_files(char *file_path, char *archive_path, bool show);
bool load_files_rec(char *file_path, char *archive_path, bool show);
bool append_to_archive(char *path, char *archive_path, bool is_directory);
long octal_to_dec(const char *input, short size);
bool extract_archive(char *path, bool show);
char *get_checksum_octal(tar_archive *archived_item);

#endif //HW05_ARCHIVE_H
