#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <unistd.h>

#include <pwd.h>
#include <grp.h>

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define MAX_FILE_LENGTH 255

// struktura pliku
typedef struct
{
    char name[MAX_FILE_LENGTH];
    int is_directory;
    char perms[11];
    int nlink;
    int uid_name;
    int gid_name;
    long size;
    __time_t time;
} file_details;

// doklejanie nazwy pliku/folderu do poprzedniej sciezki
char *new_path(char *old_path, char *file_name)
{
    char *p = (char *)malloc(strlen(old_path) + strlen(file_name) + 2);
    strcpy(p, old_path);
    strcat(p, "/");
    strcat(p, file_name);
    return p;
}

int compare(const void *d1, const void *d2)
{
    long date1 = (long)((file_details *)d1)->time;
    long date2 = (long)((file_details *)d2)->time;
    return date1 < date2;
}

// ilosc plikow w podanej sciezce
long files_count(char *name)
{
    long count = 0;
    struct dirent **filelist;
    int n;

    n = scandir(name, &filelist, NULL, alphasort);
    if (n < 0)
        perror("scandir");
    else
    {
        while (n--)
        {
            count++;
            free(filelist[n]);
        }
        free(filelist);
    }
    return count;
}

void get_permissions(file_details *file, struct stat *statbuf)
{
    // default
    strcpy(file->perms, "----------");

    // sprawdzanie plikow specjalnych
    if (S_ISREG(statbuf->st_mode)) file->perms[0] = '-'; // regular
    if (S_ISLNK(statbuf->st_mode)) file->perms[0] = 'l'; // link
    if (S_ISDIR(statbuf->st_mode)) file->perms[0] = 'd'; // directory

    // user
    if (statbuf->st_mode & S_IRUSR) file->perms[1] = 'r';
    if (statbuf->st_mode & S_IWUSR) file->perms[2] = 'w';
    if (statbuf->st_mode & S_IXUSR) file->perms[3] = 'x';

    // groups
    if (statbuf->st_mode & S_IRGRP) file->perms[4] = 'r';
    if (statbuf->st_mode & S_IWGRP) file->perms[5] = 'w';
    if (statbuf->st_mode & S_IXGRP) file->perms[6] = 'x';

    // others
    if (statbuf->st_mode & S_IROTH) file->perms[7] = 'r';
    if (statbuf->st_mode & S_IWOTH) file->perms[8] = 'w';
    if (statbuf->st_mode & S_IXOTH) file->perms[9] = 'x';
}

void print_file(file_details *file, int details, int recursively, int time_sort, int human)
{
    if (file->name[0] == '.')
        return;

    if (details)
    {
        struct passwd *uid_name = getpwuid(file->uid_name); // user name
        struct group *gid_name = getgrgid(file->gid_name);  // group name

        printf("%s ", file->perms);
        printf("%d ", file->nlink);
        printf("%s ", uid_name->pw_name);
        printf("%s ", gid_name->gr_name);

        if (human)
        {
            char names[6] = {'B', 'K', 'M', 'G', 'T', 'P'};
            int i = 0;
            float size = (float)file->size;
            while (size > 1024.0 && i < 6)
            {
                size /= 1024.0;
                i++;
            }
            char type = names[i];
            if ((size - (int)size) == 0)
                printf("%4.0f%-1c ", size, type);
            else
                printf("%4.1f%-1c ", size, type);
        }
        else
        {
            printf("%8ld ", (long)file->size);
        }

        printf("%.12s ", 4 + ctime(&file->time));
        printf(ANSI_COLOR_GREEN "%s\n" ANSI_COLOR_RESET, file->name);
    }
    else
    {
        printf(ANSI_COLOR_GREEN "%s " ANSI_COLOR_RESET, file->name);
    }
}

void list_dir(char *program, char *name, int details, int recursively, int time_sort, int human)
{
    DIR *dir;
    struct dirent *dp;
    struct stat statbuf;

    if ((dir = opendir(name)) == NULL)
    {
        printf("%s: cannot access '%s': No such file or directory\n", program, name);
        exit(1);
    }

    if (recursively)
        printf("%s:\n", name);

    int files = files_count(name);

    if (files == 2)
        return;

    // lista wszystkich plikow w katalogu
    file_details *filelist = malloc(sizeof(file_details) * files);

    int file_opened = 0;
    int dir_amount = 0;
    int dir_iter = 0;

    while ((dp = readdir(dir)) != NULL)
    {
        char *new = new_path(name, dp->d_name);
        file_details *file = filelist + file_opened;
        
        if (stat(new, &statbuf))
        {
            perror("Error with opening file");
            free(new);
            continue;
        }

        strcpy(file->name, dp->d_name);
        file->is_directory = S_ISDIR(statbuf.st_mode) ? 1 : 0;
        get_permissions(file, &statbuf);
        file->nlink = statbuf.st_nlink;
        file->uid_name = statbuf.st_uid;
        file->gid_name = statbuf.st_gid;
        file->size = statbuf.st_size;
        file->time = statbuf.st_mtime;   

        if (file->is_directory) 
            dir_amount++;

        file_opened++;
        free(new);
    }

    if (time_sort)
        qsort(filelist, file_opened, sizeof(file_details), compare);

    for (int i = 0; i < files; ++i)
        print_file(filelist + i, details, recursively, time_sort, human);

    printf("\n");

    if (dir_amount == 2 || recursively == 0)
    {
        free(filelist);
        closedir(dir);
        return;
    }

    file_details *dir_list = (file_details *)malloc(sizeof(file_details) * (dir_amount));

    for (int i = 0; i < file_opened; ++i)
    {
        if (filelist[i].is_directory) 
        {
            if (strcmp(filelist[i].name, ".") == 0 || strcmp(filelist[i].name, "..") == 0)
                continue;
            dir_list[dir_iter++] = filelist[i];
        }
    }

    free(filelist);
    closedir(dir);

    for (int i = 0; i < dir_iter; ++i)
    {
        char *new = new_path(name, dir_list[i].name);
        list_dir(program, new, details, recursively, time_sort, human);
        free(new);
    }
    free(dir_list);
}

int main(int argc, char **argv)
{
    int details = 0;
    int recursively = 0;
    int time_sort = 0;
    int human = 0;
    int path = 0;
    int version = 0;
    int help = 0;
    int error = 0;

    for (int i = 1; i < argc; ++i)
        if (argv[i][0] != '-')
            path = i;

    while (1)
    {
        static struct option long_options[] =
            {
                {"version", no_argument, 0, 0},
                {"help", no_argument, 0, 0},
                {"", no_argument, 0, 'l'},
                {"recursive", no_argument, 0, 'R'},
                {"", no_argument, 0, 't'},
                {"human-readable", no_argument, 0, 'h'},
                {0, 0, 0, 0}};

        // getopt_long stores the option index here.
        int option_index = 0;

        int c = getopt_long(argc, argv, "lRth", long_options, &option_index);

        // Detect the end of the options.
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            if (strcmp(long_options[option_index].name, "version") == 0)
                version = 1;
            if (strcmp(long_options[option_index].name, "help") == 0)
                help = 1;
            break;

        case 'l':
            details = 1;
            break;

        case 'R':
            recursively = 1;
            break;

        case 't':
            time_sort = 1;
            break;

        case 'h':
            human = 1;
            break;

        case '?':
            error = 1;
            break;

        default:
            printf("Try '%s --help' for more information.\n", argv[0]);
            exit(0);
        }
    }

    if (!error)
    {
        if (version || help)
        {
            if (version)
            {
                printf("%s version 1.0\nCopyright (C) 2019 Jakub Fladzinski\n", argv[0]);
            }
            else
            {
                printf("Usage: %s [OPTION]... [FILE]...\nMandatory arguments to long options are mandatory for short options too.\n", argv[0]);
                printf("\t-h, --human-readable\t\t with -l print human readable sizes\n");
                printf("\t-l\t\t\t\t use a long listing format\n");
                printf("\t-R, --recursive\t\t\t list subdirectiories recursively\n");
                printf("\t-t, --recursive\t\t\t sort by modification time, newest first\n");
            }
        }
        else
        {
            if (!path)
                list_dir(argv[0], ".", details, recursively, time_sort, human);
            else
                list_dir(argv[0], argv[path], details, recursively, time_sort, human);
        }
    }

    return 0;
}
