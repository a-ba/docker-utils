#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DOCKER_IMAGE "docker-sandbox-img"

#define PROGNAME "docker-sandbox"

#define	LIST_SIZE 256
struct list {
	int nb;
	const char* elem[LIST_SIZE];
};

const char* DEFAULT_VOLUMES[] =
{
	"/lib",
	"/lib64",
	"/usr/lib",
	NULL
};

const char* DOCKER_CANDIDATES[] =
{
	"/usr/local/bin/docker",
	"/usr/bin/docker",
	NULL
};

int endswith(const char* haystack, const char* needle)
{
	int lh = strlen(haystack);
	int ln = strlen(needle);

	int offset = lh - ln;
	return (offset >= 0) &&
		(strcmp(&haystack[offset], needle) == 0);
}


void print_help()
{
	puts(	"usage: docker-sandbox [ OPTIONS ] COMMAND [ ARGS ... ]\n"
		"options:\n"
		"  -v VOLUME[:ro]  mount VOLUME as an external volume\n"
		"  --network NET   use alternative network mode\n"
		"  -i              keep stdin open\n"
		"  -t              allocate tty\n"
		"  -h              print help\n"
	);		
}

void die(const char* fmt, ...)
{
	va_list ap;

	fputs(PROGNAME ": error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputs("\n", stderr);

	exit(1);
}
void warning(const char* fmt, ...)
{
	va_list ap;

	fputs(PROGNAME ": warning: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputs("\n", stderr);
}

const char* append (struct list* cmd, const char* value)
{
	if (cmd->nb >= LIST_SIZE)
	{
		die("command too long");
	}

	cmd->elem[cmd->nb++] = value;

	return value;
}

void add_volume(struct list* cmd, const char* requested_path)
{

	char path[PATH_MAX];

	int read_only = endswith(requested_path, ":ro");
	if(read_only)
	{
		int len = strlen(requested_path);
		((char*)requested_path)[len-3] = '\0';
	}

	if (realpath(requested_path, path) == NULL)
	{
		warning("ignored volume '%s' (%s)", requested_path, strerror(errno));
		return;
	}

	if (access(path, F_OK) == -1) {
		warning("ignored volume '%s' (%s)", requested_path, strerror(errno));
		return;
	}

	char* arg = malloc(strlen(path)*2 + 5);
	sprintf(arg, "%s:%s%s", path, path,
			(read_only ? ":ro" : ""));

	append(cmd, "-v");
	append(cmd, arg);
}

int is_executable_file(const char* path)
{
	if (access(path, X_OK))
	{
		return 0;
	}

	struct stat st;

	return ((stat(path, &st) == 0) && S_ISREG(st.st_mode));
}

const char* find_docker()
{
	const char** p;
	
	for (p=DOCKER_CANDIDATES ; *p ; *p++)
	{
		if (faccessat(-1, *p, X_OK, AT_EACCESS) == 0) {
			return *p;
		}
	}
	die("docker command not found");
}


const char* which(const char* command)
{
	if (*command == 0) {
		die("empty command");
	}

	if (strstr(command, "/")) {
		if (is_executable_file(command)) {
			return strdup(command);
		} else {
			die("not an executable: %s", command);
		}
	}


	// -> lookup in PATH
	char buff[PATH_MAX];
	const int clen = strlen(command);
	const char *path, *end;

	for (	path = getenv("PATH") ;
		path ;
		path = end ? (end+1) : NULL)
	{
		end = strstr(path, ":");
		int plen = end ? (end-path) : strlen(path);
		if(	   (plen == 0)
			|| (plen + clen + 2 > PATH_MAX))
		{
			continue;
		}
		strncpy(buff, path, plen);
		buff[plen] = '/';
		strcpy(&buff[plen+1], command);

		if (is_executable_file(buff))
		{
			return strdup(buff);
		}
	}

	die("command not found: %s", command);
}

struct option long_options[] = {

	{"help",	no_argument,		NULL,	'h'},
	{"volume",	required_argument,	NULL,	'v'},
	{"network",	required_argument,	NULL,	'N'},

	{"tty",		no_argument,		NULL,	0},
	{"stdin",	no_argument,		NULL,	0},
	{"cap-drop",	required_argument,	NULL,	0},
	{0, 		0,			NULL,	0},
	};

void forward_opt(struct list* cmd, int opt)
{
	char* arg = malloc(3);
	arg[0] = '-';
	arg[1] = opt;
	arg[2] = 0;

	append(cmd, arg);

	if (optarg) {
		append(cmd, optarg);
	}
}

void forward_opt_long(struct list* cmd, int longindex)
{
	const char* name = long_options[longindex].name;
	char* arg = malloc(strlen(name)+3);
	sprintf(arg, "--%s", name);
	append(cmd, arg);

	if (optarg) {
		append(cmd, optarg);
	}
}

void redir_to_dev_null()
{
	int fd = open("/dev/null", O_WRONLY | O_TRUNC);
	if (fd == -1) {
		die("unable to open /dev/null (%s)", strerror(errno));
	}
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);
}

void ensure_docker_image(const char* docker_path)
{
	int pid, status;

	switch(pid = fork())
	{
	case -1:
		die("fork error (%s)", strerror(errno));
	case 0:
		redir_to_dev_null();
		execl(docker_path, "docker", "inspect", DOCKER_IMAGE, NULL);
		die("unable to run docker (%s)", strerror(errno));
	}
	
	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && (WEXITSTATUS(status) == 0))
	{
		// image exists
		return;
	}

	// build image
	int pipes[2];
	if (pipe(pipes)) {
		die("pipe() error (%s)", strerror(errno));
	}
	int rd = pipes[0];
	int wr = pipes[1];

	switch(pid = fork())
	{
	case -1:
		die("fork error (%s)", strerror(errno));
	case 0:
		close(wr);
		dup2(rd, STDIN_FILENO);
		close(rd);
		redir_to_dev_null();
		execl(docker_path, "docker", "build", "-t", DOCKER_IMAGE, "-", NULL);
		die("unable to run docker (%s)", strerror(errno));
	}
	close(rd);
	static const char buff[] = "FROM scratch\nCMD []\n";
	write(wr, buff, strlen(buff));
	close(wr);

	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && (WEXITSTATUS(status) == 0))
	{
		return;
	}

	// TODO write a different message docker is not available
	die("unable to build docker-sandbox image");
}

int main(int argc, char* argv[])
{
	struct list cmd;
	int use_host_net = 0;

	cmd.nb = 0;

	append(&cmd, "docker");
	append(&cmd, "run");
	append(&cmd, "--rm");

	{
		const char **path;
		for (path=DEFAULT_VOLUMES ; *path ; path++)
		{
			add_volume(&cmd, *path);
		}
	}

	int opt, longindex;
	while ((opt = getopt_long(argc, argv, "+tiv:h", long_options, &longindex)) != -1)
	{
		switch(opt)
		{
		case 0:
			forward_opt_long(&cmd, longindex);
			break;
		case 't':
		case 'i':
			forward_opt(&cmd, opt);
			break;
		case 'v':
			add_volume(&cmd, optarg);
			break;
		case 'N':
			if (strcmp(optarg, "host") != 0) {
				die("invalid network --network=%s (allowed value is 'host', default is 'none')", optarg);
			}
			use_host_net = 1;
			break;
		case 'h':
			print_help();
			return 0;
		default:
			return 1;
		}
	}

	// network
	if (use_host_net) {
		append(&cmd, "--network=host");
	} else {
		append(&cmd, "--network=none");
	}

	// workdir
	{
		const char* cwd = realpath(".", NULL);
		if (cwd == NULL)
		{
			die("current workdir not usable (%s)", strerror(errno));
		}
		append(&cmd, "-w");
		append(&cmd, cwd);
	}

	// user
	{
		char user[64];
		sprintf(user, "--user=%d:%d", getuid(), getgid());
		append(&cmd, user);
	}

	// command volume
	// FIXME: argv[0] is not preserved -> should keep the same basename
	const char* cmd_path;
	{
		if (optind == argc) {
			die("must provide a command");
		}

		cmd_path = realpath(which(argv[optind]), NULL);
		if (cmd_path == NULL)
		{
			die("invalid command (%s)", strerror(errno));
		}
		optind++;

		add_volume(&cmd, cmd_path);
	}

	// image
	append (&cmd, DOCKER_IMAGE);
	

	// command + arguments
	append(&cmd, cmd_path);

	for ( ; optind<argc ; optind++)
	{
		append(&cmd, argv[optind]);
	
	}

	append(&cmd, NULL);

	const char* docker_path = find_docker();

	ensure_docker_image(docker_path);

	char* const* env = { NULL };
	execve(docker_path, (char*const*)cmd.elem, env);
	die("unable to execute docker (%s)", strerror(errno));
}
