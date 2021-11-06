#include "archive.h"

const char *minor_major_num = "0000000\0";

/** Comparator for qsort function **/
int compare_str(const void *a, const void *b)
{
    return strcmp(*(const char **) a, *(const char **) b);
}

void normalize_path(char *path)
{
    size_t size = strlen(path) - 1;
    while (path[size] == '/') {
        size--;
    }
    path[size + 1] = '\0';
}

/** Append 1024 zero bytes to archive end **/
void archive_end(char *path)
{
    FILE *dest_file = fopen(path, "a");
    char *end = calloc(1024, 1);
    if (!end) {
        fclose(dest_file);
        return;
    }
    fwrite(end, 1024, 1, dest_file);
    free(end);
    fclose(dest_file);
}

void change_last_modification_time(tar_archive *tar)
{
    int time = octal_to_dec(tar->head.last_modification, 12);
    struct utimbuf utimbuf = { .modtime = time, .actime = time };
    utime(tar->head.name, &utimbuf);
}

long octal_to_dec(const char *input, short size)
{
    size -= 2;
    long res = 0;
    int exp = 1;
    while (size != -1) {
        res += (input[size] - '0') * exp;
        exp *= 8;
        size--;
    }
    return res;
}

/** Execute program by parameters **/
bool execute(param *parameters)
{
    bool state = true;
    if (parameters->stats[1]) {
        if (access(parameters->archive_name, F_OK) == 0) {
            fprintf(stderr, "Failed to create output file Operation not permitted \n");
            return false;
        }
        qsort(parameters->sources, parameters->file_count, sizeof(char *), compare_str);
        for (int i = 0; i < parameters->file_count; i++) {
            normalize_path(parameters->sources[i]);
            if (access(parameters->sources[i], F_OK) != 0) {
                fprintf(stderr, "%s: No such file or directory\n", parameters->sources[i]);
                state = false;
                continue;
            }
            if (!load_files(parameters->sources[i], parameters->archive_name, parameters->stats[0])) {
                state = false;
            }
        }
        if (access(parameters->archive_name, F_OK) == -1) {
            fprintf(stderr, "Cannot create empty archive\n");
            return false;
        }
        archive_end(parameters->archive_name);
        return state;
    }
    if (parameters->stats[2]) {
        if (access(parameters->archive_name, F_OK) == -1) {
            fprintf(stderr, "Failed to create output file Operation not permitted \n");
            return false;
        }
        return extract_archive(parameters->archive_name, parameters->stats[0]);
    }
    return false;
}

/** Extract file from archive. **/
bool extract_file(int dest_file, tar_archive *tar)
{
    long size = octal_to_dec(tar->head.file_size, 12);
    if (access(tar->head.name, F_OK) == 0) {
        fprintf(stderr, "Error extracting archive: File exists \n");
        lseek(dest_file, size + (512 - (size % 512)), SEEK_CUR); //Jump to next head
        return false;
    }
    char file_content[512];
    int new_file = open(tar->head.name, O_CREAT | O_WRONLY);
    if (new_file == -1) {
        fprintf(stderr, "Error extracting archive: Permission denied \n");
        lseek(dest_file, size + (512 - (size % 512)), SEEK_CUR); //Jump to next head
        return false;
    }
    chmod(tar->head.name, octal_to_dec(tar->head.mode, 8) & 0777);
    if (size == 0) {
        close(new_file);
        change_last_modification_time(tar);
        return true;
    }
    ssize_t loaded_size;
    if (size > 512) {
        loaded_size = read(dest_file, file_content, 512);
    } else {
        read(dest_file, file_content, size);
        write(new_file, file_content, size);
        lseek(dest_file, 512 - size, SEEK_CUR);
        close(new_file);
        change_last_modification_time(tar);
        return true;
    }
    size -= loaded_size;
    while (size > 0) {
        write(new_file, file_content, loaded_size);
        if (size > 512) {
            loaded_size = read(dest_file, file_content, 512);
        } else {
            read(dest_file, file_content, size);
            write(new_file, file_content, size);
            lseek(dest_file, 512 - size, SEEK_CUR);
            break;
        }
        size -= loaded_size;
    }
    close(new_file);
    change_last_modification_time(tar);
    return true;
}

/**creates missing directories in the path. Returns false if there are insufficient rights **/
bool check_dir(tar_archive *tar)
{
    char *p_to_path = strchr(tar->head.name + 1, '/');
    while (p_to_path) {
        *p_to_path = '\0';
        if (mkdir(tar->head.name, 0777) == -1) {
            if (errno != EEXIST) {
                fprintf(stderr, "Error extracting archive: Permission denied \n");
                *p_to_path = '/';
                return false;
            }
        }
        *p_to_path = '/';
        p_to_path = strchr(p_to_path + 1, '/');
    }
    chmod(tar->head.name, octal_to_dec(tar->head.mode, 8) & 0777);
    change_last_modification_time(tar);
    return true;
}

bool valid_checksum(tar_archive *tar)
{
    bool state = true;
    char sum_copy[8];
    strncpy(sum_copy, tar->head.check_sum, 8);
    char *new_sum = get_checksum_octal(tar);
    if (strcmp(sum_copy, new_sum) != 0) {
        state = false;
    }
    strncpy(tar->head.check_sum, sum_copy, 8);
    free(new_sum);
    return state;
}

bool extract_archive(char *path, bool show)
{
    tar_archive *tar = calloc(512, 1);
    if (!tar) {
        return false;
    }
    int dest_file = open(path, O_RDONLY);
    bool state = true;
    while (read(dest_file, &tar->head, 512) > 0) {
        if (!(*tar->head.name)) { //End of archive
            break;
        }
        if (show)
            fprintf(stderr, "%s\n", tar->head.name);
        if (!valid_checksum(tar)) {
            fprintf(stderr, "Error extracting archive: Protocol not supported \n");
            state = false;
            break;
        }
        if (!check_dir(tar)) {
            state = false;
            continue;
        }
        if (tar->head.flag[0] == '0') {
            if (!extract_file(dest_file, tar)) {
                state = false;
            }
        }
    }
    free(tar);
    close(dest_file);
    return state;
}

bool load_files(char *file_path, char *archive_path, bool show)
{
    if (show)
        fprintf(stderr, "%s\n", file_path);
    DIR *dir = opendir(file_path);
    if (!dir) {
        return append_to_archive(file_path, archive_path, false);
    }
    closedir(dir);
    bool state = true;
    if (!append_to_archive(file_path, archive_path, true)) {
        state = false;
    }
    if (!load_files_rec(file_path, archive_path, show)) {
        state = false;
    }
    return state;
}

/** Recursively load all files and append them to the archive **/
bool load_files_rec(char *file_path, char *archive_path, bool show)
{
    bool state = true;
    struct dirent **file_list;
    int size = scandir(file_path, &file_list, NULL, alphasort);
    for (int i = 0; i < size; i++) {
        char path[PATH_SIZE];
        if (is_dir(file_list[i]->d_type)) {
            if (strcmp(file_list[i]->d_name, ".") == 0 || strcmp(file_list[i]->d_name, "..") == 0) {
                free(file_list[i]);
                continue;
            }
            snprintf(path, PATH_SIZE, "%s/%s", file_path, file_list[i]->d_name);
            if (show)
                fprintf(stderr, "%s\n", path);
            if (!append_to_archive(path, archive_path, true)) {
                state = false;
            }
            if (!load_files_rec(path, archive_path, show)) {
                state = false;
            }
            free(file_list[i]);
            continue;
        }
        snprintf(path, PATH_SIZE, "%s/%s", file_path, file_list[i]->d_name);
        if (show)
            fprintf(stderr, "%s\n", path);

        if (!append_to_archive(path, archive_path, false)) {
            state = false;
        }
        free(file_list[i]);
    }
    if (file_list) {
        free(file_list);
    }

    return state;
}

/** Append archived file content into archive **/
void append_content_to_archive(const char *path, FILE *dest_file)
{
    int file = open(path, O_RDONLY);
    size_t size;
    size_t last_size = 0;
    char file_content[512];
    while ((size = read(file, &file_content, 512)) != 0) {
        fwrite(file_content, size, 1, dest_file);
        last_size = size;
    }
    if (last_size != 0) {
        int remind = 512 - (int) last_size;
        char *reminder_content = calloc(remind, 1);
        fwrite(reminder_content, remind, 1, dest_file);
        free(reminder_content);
    }
    close(file);
}

void append_head_to_archive(tar_archive *tar, char *path, char *archive_path, bool is_directory)
{
    FILE *dest_file = fopen(archive_path, "a");
    fwrite(&(tar->head), 512, 1, dest_file);
    if (!is_directory) {
        append_content_to_archive(path, dest_file);
    }
    fclose(dest_file);
}

bool append_to_archive(char *path, char *archive_path, bool is_directory)
{
    tar_archive *tar = calloc(512, 1);
    if (!tar) {
        return false;
    }
    if (access(path, R_OK) == -1) {
        fprintf(stderr, "%s: Permission denied\n", path);
        return false;
    }
    struct stat stats;
    if (stat(path, &stats)) {
        fprintf(stderr, "Stat error \n");
        return false;
    }
    strncpy(tar->head.name, path, 100);
    if (!is_directory) {
        snprintf(tar->head.file_size, 12, "%011o", (int) stats.st_size);
        tar->head.flag[0] = '0';
    } else {
        strcat(tar->head.name, "/");
        snprintf(tar->head.file_size, 12, "%011o", 0);
        tar->head.flag[0] = '5';
    }
    snprintf(tar->head.mode, 8, "%07o", stats.st_mode & 0777);
    snprintf(tar->head.owner_id, 8, "%07o", stats.st_uid);
    snprintf(tar->head.group_id, 8, "%07o", stats.st_gid);
    snprintf(tar->head.last_modification, 12, "%011o", (int) stats.st_mtime);
    strcpy(tar->head.magic_num, "ustar\0");
    strcpy(tar->head.ustar_ver, "00");
    struct passwd *pw = getpwuid(stats.st_uid);
    struct group *gr = getgrgid(stats.st_gid);
    if (pw && pw->pw_name) {
        strncpy(tar->head.owner_str, pw->pw_name, 32);
    }
    if (gr && gr->gr_name) {
        strncpy(tar->head.group_str, gr->gr_name, 32);
    }
    strcpy(tar->head.major_num, minor_major_num);
    strcpy(tar->head.minir_num, minor_major_num);
    char *check_sum = get_checksum_octal(tar);
    strcpy(tar->head.check_sum, check_sum);
    append_head_to_archive(tar, path, archive_path, is_directory);
    free(tar);
    free(check_sum);
    return true;
}

char *get_checksum_octal(tar_archive *archived_item)
{
    memset(archived_item->head.check_sum, ' ', 8);
    unsigned int result = 0;
    const unsigned char *const p_to_archived_item = (const unsigned char *) &archived_item->head;
    for (short i = 0; i < 500; i++) {
        result += p_to_archived_item[i];
    }
    char *res_str = calloc(8, 1);
    if (!res_str) {
        return NULL;
    }
    snprintf(res_str, 8, "%06o", result);
    res_str[6] = '\0';
    res_str[7] = ' ';
    return res_str;
}
