/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/21 15:50:12 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "taskmasterctl.h"
#include "readline.h"

int g_sig = 0;

void help(char *full_arg) {
	char *arg = NULL;
	int i = 0;
	while (full_arg[i] && full_arg[i] != ' ') {
		i++;
	}
	while (full_arg[i] && full_arg[i] == ' ') {
		i++;
	}
	arg = full_arg + i;

	if (!strlen(arg)) {
		printf("default commands (type help <command>):\n");
		printf("=======================================\n");
		printf("exit maintail quit signal stop avail fg\n");
		printf("reload restart start tail clear help pid\n");
		printf("shutdown status update\n");
	} else {
		if (!strcmp(arg, "exit")) {
			printf("exit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg, "maintail")) {
			printf("maintail -f\t\t\tContinuous tail of taskmaster main log file (Ctrl-C to exit)\n");
			printf("maintail -100\t\t\tlast 100 *bytes* of taskmaster main log file\n");
			printf("maintail\t\t\tlast 1600 *bytes* of taskmaster main log file\n");
		} else if (!strcmp(arg, "quit")) {
			printf("quit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg, "signal")) {
			printf("signal <signal name> <name>\t\tSignal a process\n");
			printf("signal <signal name> <name> <name>\tSignal multiple processes or groups\n");
			printf("signal <signal name> all\t\tSignal all processes\n");
		} else if (!strcmp(arg, "stop")) {
			printf("stop <name>\t\tStop a process\n");
			printf("stop <name> <name>\tStop multiple processes or groups\n");
			printf("stop all\t\tStop all processes\n");
		} else if (!strcmp(arg, "avail")) {
			printf("avail\t\t\tDisplay all configured processes\n");
		} else if (!strcmp(arg, "fg")) {
			printf("fg <process>\tConnect to a process in foreground mode\n");
			printf("\t\tCtrl-C to exit\n");
		} else if (!strcmp(arg, "reload")) {
			printf("reload\t\tRestart the remote taskmasterd.\n");
		} else if (!strcmp(arg, "restart")) {
			printf("restart <name>\t\tRestart a process\n");
			printf("restart <name> <name>\tRestart multiple processes or groups\n");
			printf("restart all\t\tRestart all processes\n");
			printf("Note: restart does not reread config files. For that, see reread and update.\n");
		} else if (!strcmp(arg, "start")) {
			printf("start <name>\t\tStart a process\n");
			printf("start <name> <name>\tStart multiple processes or groups\n");
			printf("start all\t\tStart all processes\n");
		} else if (!strcmp(arg, "tail")) {
			printf("tail [-f] <name>\t[stdout|stderr] (default stdout)\n");
			printf("Ex:\n");
			printf("tail -f <name>\t\tContinuous tail of named process stdout\n");
			printf("\t\t\tCtrl-C to exit\n");
			printf("tail -100 <name>\tlast 100 *bytes* of process stdout\n");
			printf("tail <name> stderr\tlast 1600 *bytes* of process stderr\n");
		} else if (!strcmp(arg, "clear")) {
			printf("clear <name>\t\t\tClear a process' log files\n");
			printf("clear <name> <name>\t\tClear multiple process' log files\n");
			printf("clear all\t\t\tClear all process' log files\n");
		} else if (!strcmp(arg, "help")) {
			printf("help\t\t\tPrint a list of available commands\n");
			printf("help <command>\t\tPrint help for <command>\n");
		} else if (!strcmp(arg, "pid")) {
			printf("pid\t\t\tGet the PID of taskmasterd.\n");
			printf("pid <name>\t\tGet the PID of a single child process by name.\n");
			printf("pid all\t\t\tGet the PID of every child process, one per line.\n");
		} else if (!strcmp(arg, "shutdown")) {
			printf("shutdown\t\tShut the remote taskmasterd down.\n");
		} else if (!strcmp(arg, "status")) {
			printf("status <name>\t\tGet status for a single process\n");
			printf("status <name> <name>\tGet status for multiple named processes\n");
			printf("status\t\t\tGet all process status info\n");
		} else if (!strcmp(arg, "update")) {
			printf("update\t\t\tReload config and add/remove as necessary, and will restart affected programs\n");
			printf("update all\t\tReload config and add/remove as necessary, and will restart affected programs\n");
		} else {
			printf("*** No help on %s\n", arg);
		}
	}
}

void *thread_log(void *arg) {
	struct epoll_event *sock = (struct epoll_event *)arg;
	int efd = epoll_create(1);
	sock->events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_ADD, sock->data.fd, sock);

	struct epoll_event event;

	char buf[PIPE_BUF + 1];
	bzero(buf, PIPE_BUF + 1);
	while (!g_sig) {
		if (epoll_wait(efd, &event, 1, 0) > 0) {
			if (event.data.fd == sock->data.fd) {
				bzero(buf, PIPE_BUF + 1);
				if (recv(event.data.fd, buf, PIPE_BUF, 0) > 0) {
					printf("%s", buf);
				}
			}
		}
	}
	fflush(stdout);
	epoll_ctl(efd, EPOLL_CTL_DEL, sock->data.fd, sock);
	close(efd);
	return NULL;
}

void foreground(int efd, struct epoll_event sock) {
	int ret;
	struct epoll_event event;
	char *line;
	char buf[PIPE_BUF + 1];
	bzero(buf, PIPE_BUF + 1);
	pthread_t thread;

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);
	pthread_create(&thread, NULL, &thread_log, &sock);
	while (!g_sig) {
		line = ft_readline("");
		if (line) {
			ret = epoll_wait(efd, &event, 1, 100);
			if (ret > 0) {
				if (event.events & EPOLLOUT) {
					bzero(buf, PIPE_BUF + 1);
					snprintf(buf, PIPE_BUF + 1, "data:%s\n", line);
					send(event.data.fd, buf, strlen(buf), 0);
				}
			} else if (ret == 0) {
				printf("socket timed out\n");
				g_sig = 1;
				return ;
			} else {
				perror("epoll_wait");
				g_sig = 1;
				return ;
			}
			free(line);
		} else {
			g_sig = 1;
		}
	}
	pthread_join(thread, NULL);

	ret = epoll_wait(efd, &event, 1, 60 * 1000);
	if (ret > 0) {
		if (event.events & EPOLLOUT) {
			send(event.data.fd, "exit", strlen("exit"), 0);
		}
	} else if (ret == 0) {
		printf("SOCKET TIMED OUT\n");
	} else {
		perror("epoll_wait(EPOLLOUT)");
	}
	int i = 0;
	while (i < 42) { 
		recv(sock.data.fd, buf, PIPE_BUF, MSG_DONTWAIT);
		bzero(buf, PIPE_BUF + 1);
		usleep(10);
		i++;
	}
	fflush(stdout);
}

void remote_exec(char *cmd, int efd, struct epoll_event sock) {
	struct epoll_event tmp;

	/*
	 *  THE LOGIC WITH WHICH IVE MADE THIS CTL IS THAT IT IS AN EMPTY SHELL.
	 *  THIS MEANS IT WILL ALWAYS FIRST `SEND A REQUEST` (OR MORE ACCURATELLY
	 *  TRANSFER THE COMMAND) AND THEN `PRINT THE RESPONSE` WITHOUT ANY THOUGH
	 */
	if (strlen(cmd) > PIPE_BUF) {
		printf("line too long...\n");
		return ;
	}

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);

	int ret = epoll_wait(efd, &tmp, 1, 60 * 1000); // wait for max a minute
	if (ret > 0) {
		if (tmp.events & EPOLLOUT) { // SEND PHASE
			send(tmp.data.fd, cmd, strlen(cmd), 0);
		}
	} else if (ret == 0) {
		printf("SOCKET TIMED OUT\n");
	} else {
		perror("epoll_wait(EPOLLOUT)");
	}

	sock.events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);

	ret = epoll_wait(efd, &tmp, 1, 60 * 1000); // wait for max a minute
	if (ret > 0) {
		if (tmp.events & EPOLLIN) { // RECV PHASE
			char buf[PIPE_BUF + 1];
			bzero(buf, PIPE_BUF + 1);
			recv(tmp.data.fd, buf, PIPE_BUF, 0);
			if (!strncmp(buf, "fg", 2)) {
				printf("%s", buf + 2);
				foreground(efd, sock);
				g_sig = 0;
			} else {
				printf("%s", buf); // DUMBLY PRINT RESPONSE
			}
		}
	} else if (ret == 0) {
		printf("SOCKET TIMED OUT\n");
	} else {
		perror("epoll_wait(EPOLLIN)");
	}
}

void get_cmd(char *cmd, char *full_cmd) {
	if (strlen(full_cmd) < 4) {
		return ;
	} else if (full_cmd[4] != ' ' && full_cmd[4] != '\0') {
		return ;
	} else {
		memcpy(cmd, full_cmd, 4);
	}
}

int exec(char *full_cmd, int efd, struct epoll_event sock) {
	int ret = 0;
	char cmd[5];

	bzero(cmd, 5);
	get_cmd(cmd, full_cmd);
	if (strlen(cmd)) {
		if (!strcmp(cmd, "help")) {
			help(full_cmd);
		} else if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) {
			ret = 1;
		} else {
			remote_exec(full_cmd, efd, sock);
		}
	} else {
		remote_exec(full_cmd, efd, sock);
	}
	return (ret);
}

char *process_line(char *line) {
	int i = 0;
	while (line[i] && line[i] == ' ') {
		i++;
	}
	char *cmd = line + i;
	if (strlen(cmd)) {
		return cmd;
	} else {
		return NULL;
	}
}

void sig_handler(int sig) {
	(void)sig;
	g_sig = 1;
}

#define SOCK_PATH "/tmp/taskmaster.sock"

int open_socket(void) {
	int fd;
	struct sockaddr_un addr;
	
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
	}
	return (fd);
}

int main(int ac, char **av) {
	(void)ac;
	(void)av;
	char *tab[] = {"add", "exit", "maintail", "quit", "reread", "signal", "stop",\
				"avail", "fg", "reload", "restart", "start", "tail",\
			"clear", "help", "pid", "remove", "shutdown", "status", "update", NULL};

	int socket = open_socket();
	struct epoll_event sock;
	bzero(&sock, sizeof(sock));
	sock.data.fd = socket;
	sock.events = 0;
	int efd = epoll_create(1);
	epoll_ctl(efd, EPOLL_CTL_ADD, socket, &sock);

	// SETUP EPOLL, IT CAN NOW BE CALLED IN NEEDED FUNCTION

	signal(SIGINT, &sig_handler);
	complete_init(tab);

	FILE *file = fopen_history();
	add_old_history(file);

	char *line = ft_readline("taskmasterctl>");
	char *cmd = NULL;
	while (line != NULL) {
		// process line (remove space n sht)
		char *tmp = strdup(line);
		cmd = process_line(line);
		if (cmd) {
			if (exec(cmd, efd, sock)) {
				free(line);
				free(tmp);
				break ;
			}
			// add to history and read another line
			add_history(tmp);
			add_file_history(tmp, file);
		}
		free(line);
		free(tmp);
		line = ft_readline("taskmasterctl>");
	}
	clear_history();
	if (file) {
		fclose(file);
	}
}
