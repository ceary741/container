#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <errno.h>
#include <err.h>
#include <limits.h>

#define STACK_SIZE (1024 * 1024)
static char container_stack[STACK_SIZE];
static char new_hostname[512] = "container";
static char container_name[128];

extern char **environ;

static void
proc_setgroups_write(pid_t child_pid, char *str)
{
	char setgroups_path[PATH_MAX];
	int fd;

	snprintf(setgroups_path, PATH_MAX, "/proc/%jd/setgroups",
			(intmax_t) child_pid);

	fd = open(setgroups_path, O_RDWR);
	if (fd == -1) {
		if (errno != ENOENT)
			fprintf(stderr, "ERROR: open %s: %s\n", setgroups_path,
					strerror(errno));
		return;
	}

	if (write(fd, str, strlen(str)) == -1)
		fprintf(stderr, "ERROR: write %s: %s\n", setgroups_path,
				strerror(errno));

	close(fd);
}

static void
update_map(char *mapping, char *map_file)
{
	int fd;
	size_t map_len;     /* Length of 'mapping' */

	/* Replace commas in mapping string with newlines. */

	map_len = strlen(mapping);
	//	for (size_t j = 0; j < map_len; j++)
	//		if (mapping[j] == ',')
	//			mapping[j] = '\n';

	fd = open(map_file, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "ERROR: open %s: %s\n", map_file,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (write(fd, mapping, map_len) != map_len) {
		fprintf(stderr, "ERROR: write %s: %s\n", map_file,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	close(fd);
}

//void write_file(char *path, char *str) {
//	int fd = open(path, O_WRONLY);
//	if (fd == -1) {
//		perror("open");
//		exit(EXIT_FAILURE);
//	}
//	if (write(fd, str, strlen(str)) == -1) {
//		perror("write");
//		close(fd);
//		exit(EXIT_FAILURE);
//	}
//	close(fd);
//}

static int
container_main(void* arg) {
	char ch;
	close(((int*)arg)[1]);
	if (read(((int*)arg)[0], &ch, 1) != 0) {
		fprintf(stderr,
				"Failure in child: read from pipe returned != 0\n");
		exit(EXIT_FAILURE);
	}
	close(((int*)arg)[0]);

	printf("Container - inside the container!\n");
	printf("Container - my PID is %d\n", getpid());
	printf("Container - my UID is %d\n", getuid());

	// 设置新的root文件系统
	if(chroot(container_name) == -1) {
		err(EXIT_FAILURE, "chroot");
	}
	chdir("/");

	mount("proc", "/proc", "proc", 0, "");

	if(sethostname(container_name, strlen(container_name)) != 0) {
		perror("Failed to change hostname");
	}

	execl("/bin/bash", "/bin/bash", "-c", "bash", NULL);

	return 0;
}

int main(int argc, char **argv) {
	const int MAP_BUF_SIZE = 128;
	char map_buf[MAP_BUF_SIZE];
	char map_path[PATH_MAX];
	int pipe_fd[2];
	char command[512];

	pid_t container_pid;
	int container_num = atoi(argv[2]);

	strcpy(container_name, argv[1]);

	printf("Parent - starting a container\n");

	pid_t pid = getpid();
	sprintf(command, "./cgroup.sh %jd %s", 
			(intmax_t) pid, container_name);
	system(command);

	if (pipe(pipe_fd) == -1) {
		err(EXIT_FAILURE, "pipe");
	}

	container_pid = clone(container_main, container_stack+STACK_SIZE, 
			CLONE_NEWPID | CLONE_NEWNS | SIGCHLD | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWNET, pipe_fd);
	if (container_pid == -1) {
		err(EXIT_FAILURE, "clone");
	}

	printf("PID of child created by clone() is %jd\n",
			(intmax_t) container_pid);

	sprintf(command, "./net.sh %jd %s 192.168.31.%d veth%d veth%d", 
			(intmax_t) container_pid, container_name, container_num+2, container_num*2, container_num*2+1);
	system(command);

	snprintf(map_path, PATH_MAX, "/proc/%jd/uid_map",
			(intmax_t) container_pid);
	snprintf(map_buf, MAP_BUF_SIZE, "0 %jd 1",
			(intmax_t) getuid());
	update_map(map_buf, map_path);

	proc_setgroups_write(container_pid, "deny");
	snprintf(map_path, PATH_MAX, "/proc/%jd/gid_map",
			(intmax_t) container_pid);
	snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1",
			(intmax_t) getgid());
	update_map(map_buf, map_path);

	/* Close the write end of the pipe, to signal to the child that we
	   have updated the UID and GID maps. */

	close(pipe_fd[1]);

	waitpid(container_pid, NULL, 0);
	printf("Parent - container stopped\n");
	return 0;
}

