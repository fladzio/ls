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

int tab = 0;

typedef struct
{
	struct dirent *dp;
	struct stat statbuf;
} data;

char *new_path(char *old_path, char *file_name)
{
	char *p = (char *)malloc(strlen(old_path) + strlen(file_name) + 2);
	strcpy(p, old_path);
	strcat(p, "/");
	strcat(p, file_name);
	return p;
}

void list_dir(char *program, char *name, int details, int recursively, int time_sort, int human);

int compare(const void *d1, const void *d2)
{
	long date1 = ((data *)d1)->statbuf.st_mtime;
	long date2 = ((data *)d2)->statbuf.st_mtime;
	return date1 < date2;
}

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

void get_permissions(struct stat *statbuf, char perms[])
{
	// default
	strcpy(perms, "----------");

	// sprawdzanie plikow specjalnych
	if (S_ISREG(statbuf->st_mode))
		perms[0] = '-'; // regular
	if (S_ISLNK(statbuf->st_mode))
		perms[0] = 'l'; // link
	if (S_ISDIR(statbuf->st_mode))
		perms[0] = 'd'; // directory

	// user
	if (statbuf->st_mode & S_IRUSR)
		perms[1] = 'r';
	if (statbuf->st_mode & S_IWUSR)
		perms[2] = 'w';
	if (statbuf->st_mode & S_IXUSR)
		perms[3] = 'x';

	// groups
	if (statbuf->st_mode & S_IRGRP)
		perms[4] = 'r';
	if (statbuf->st_mode & S_IWGRP)
		perms[5] = 'w';
	if (statbuf->st_mode & S_IXGRP)
		perms[6] = 'x';

	// others
	if (statbuf->st_mode & S_IROTH)
		perms[7] = 'r';
	if (statbuf->st_mode & S_IWOTH)
		perms[8] = 'w';
	if (statbuf->st_mode & S_IXOTH)
		perms[9] = 'x';
}

void print_file(char *program, char *name, data *data, int details, int recursively, int time_sort, int human)
{
	char perms[11]; // string od uprawnien
	if (data->dp->d_name[0] == '.')
		return;
	if (details)
	{
		struct passwd *uid_name = getpwuid(data->statbuf.st_uid); // user name
		struct group *gid_name = getgrgid(data->statbuf.st_gid);  // group name

		get_permissions(&data->statbuf, perms);

		if (recursively)
		{
			for (int i = 0; i <= tab; ++i)
			{
				if (i != tab)
					printf("|   ");
				else
					printf("|-> ");
			}
		}

		printf("%s ", perms);
		printf("%ld ", data->statbuf.st_nlink);
		printf("%s ", uid_name->pw_name);
		printf("%s ", gid_name->gr_name);

		if (human)
		{
			char names[6] = {'B', 'K', 'M', 'G', 'T', 'P'};
			int iterator = 0;
			float size = (float)data->statbuf.st_size;
			while (size > 1024.0 && iterator < 6)
			{
				size /= 1024.0;
				iterator++;
			}
			char type = names[iterator];
			if ((size - (int)size) == 0)
				printf("%4.0f%-1c ", size, type);
			else
				printf("%4.1f%-1c ", size, type);
		}
		else
		{
			printf("%8ld ", (long)data->statbuf.st_size);
		}

		printf("%.12s ", 4 + ctime(&data->statbuf.st_mtime));
		printf(ANSI_COLOR_GREEN "%s\n" ANSI_COLOR_RESET, data->dp->d_name);

		if (recursively && perms[0] == 'd')
		{
			tab++;
			char *full_name = new_path(name, data->dp->d_name);
			list_dir(program, full_name, details, recursively, time_sort, human);
		}
	}
	else
	{
		printf("%s ", data->dp->d_name);
	}
}

void list_dir(char *program, char *name, int details, int recursively, int time_sort, int human)
{
	DIR *dir;

	if ((dir = opendir(name)) == NULL)
	{
		printf("%s: cannot access '%s': No such file or directory\n", program, name);
		exit(1);
	}

	int files = files_count(name);

	int i = -1;
	data *filelist = (data *)malloc(sizeof(data) * files + 1);

	while ((filelist[++i].dp = readdir(dir)) != NULL)
	{
		char *heh = new_path(name, filelist[i].dp->d_name);
		if (filelist[i].dp->d_name[0] != '.')
		{
			if (stat(heh, &filelist[i].statbuf))
			{
				perror("Cannot open the file!");
				free(heh);
				continue;
			}
		}
		free(heh);
	}

	if (time_sort)
		qsort(filelist, files, sizeof(data), compare);

	for (i = 0; i < files; ++i)
		print_file(program, name, filelist + i, details, recursively, time_sort, human);

	if (!details)
		printf("\n");

	tab--;

	free(filelist);
	closedir(dir);
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
