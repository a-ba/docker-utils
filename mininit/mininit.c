#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2, 0};

int child_pid = 0;

void signal_handler(int sig)
{
	kill(child_pid, sig);
}

void install_signal_handlers()
{
	struct sigaction act;
	if (sigemptyset(&act.sa_mask)) {
		perror("sigemptyset()");
		return;
	}
	act.sa_handler		= signal_handler;
	act.sa_flags		= 0;
	act.sa_restorer		= NULL;

	const int *s;
	for (s=signals ; *s ; s++) {
		if(sigaction(*s, &act, NULL)) {
			perror("sigaction()");
		}
	}
}

int main_loop()
{
	install_signal_handlers();

	for(;;) {
		int pid, status;

		pid = wait(&status);

		if ((pid == -1) && (errno != EINTR))
		{
			perror("wait error");
			return 255;
		}
		else if (pid == child_pid)
		{
			if (WIFEXITED(status)) {
				// normal exit
				return WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				// killed
				int sig = WTERMSIG(status);
				fprintf(stderr, "killed by signal %d (%s)\n", sig, strsignal(sig));
				return 255;
			} else {
				// unreachable
				fprintf(stderr, "bad exit status");
				fflush(stderr);
				abort();
			}
		}
	}
}

int exec_child(int argc, char* argv[]) 
{
	memmove(&argv[0], &argv[1], (argc-1) * sizeof(char*));
	argv[argc-1] = NULL;

	execvp(argv[0], argv);

	perror("exec error");
	return 255;
}

int main (int argc, char* argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s COMMAND [ ARGS ... ]\n", argv[0]);
		return 255;
	}

	child_pid = fork();
	if (child_pid == -1) {
		perror("fork error");
		return 255;
	} else if (child_pid == 0) {
		return exec_child(argc, argv);
	} else {
		return main_loop();
	}
}
