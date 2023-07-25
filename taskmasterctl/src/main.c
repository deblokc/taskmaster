/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/25 18:18:03 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <limits.h>
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

void help(char *arg) {
	if (!arg) {
		printf("default commands (type help <command>):\n");
		printf("=======================================\n");
		printf("TOUTES LES COMMANDES QUON AURA\n");
	} else {
		if (!strcmp(arg, "add")) {
			printf("add <name> [...]\tActivates any updates in config for process/group\n");
		} else if (!strcmp(arg, "exit")) {
			printf("exit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg, "maintail")) {
			printf("maintail -f\t\t\tContinuous tail of taskmaster main log file (Ctrl-C to exit)\n");
			printf("maintail -100\t\t\tlast 100 *bytes* of taskmaster main log file\n");
			printf("maintail\t\t\tlast 1600 *bytes* of taskmaster main log file\n");
		} else if (!strcmp(arg, "quit")) {
			printf("quit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg, "reread")) {
			printf("reread\t\t\tReload the daemon's configuration files without add/remove\n");
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
		} else if (!strcmp(arg, "remove")) {
			printf("remove <name> [...] Removes process from active config\n");
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

void remote_exec(struct s_command *cmd, int efd, struct epoll_event sock) {
	struct epoll_event tmp;

	/*
	 *  THE LOGIC WITH WHICH IVE MADE THIS CTL IS THAT IT IS AN EMPTY SHELL.
	 *  THIS MEANS IT WILL ALWAYS FIRST `SEND A REQUEST` (OR MORE ACCURATELLY
	 *  TRANSFER THE COMMAND) AND THEN `PRINT THE RESPONSE` WITHOUT ANY THOUGH
	 */

	printf("remote exec\n");

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);

	int ret = epoll_wait(efd, &tmp, 1, 60 * 1000); // wait for max a minute
	if (ret > 0) {
		if (tmp.events & EPOLLOUT) { // SEND PHASE
			send(tmp.data.fd, cmd->cmd, strlen(cmd->cmd), 0);
			printf("SENT %s\n", cmd->cmd);
			if (cmd->arg) {
				for (int i = 0; i < 10; i++) {
					if (send(tmp.data.fd, cmd->arg, strlen(cmd->arg), 0) > 0) {
						printf("SENT %s\n", cmd->arg);
						break ;
					}
					usleep(10);
				}
			}
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
			printf("%s\n", buf); // DUMBLY PRINT RESPONSE
		}
	} else if (ret == 0) {
		printf("SOCKET TIMED OUT\n");
	} else {
		perror("epoll_wait(EPOLLIN)");
	}
}

int exec(struct s_command *cmd, int efd, struct epoll_event sock) {
	int ret = 0;

	printf("cmd : %s\n", cmd->cmd);
	if (cmd->arg) {
		printf("arg : %s\n", cmd->arg);
	}
	if (!strcmp(cmd->cmd, "help")) {
		help(cmd->arg);
	} else if (!strcmp(cmd->cmd, "exit") || !strcmp(cmd->cmd, "quit")) {
		ret = 1;
	} else {
		remote_exec(cmd, efd, sock);
	}
	free(cmd->cmd);
	free(cmd->arg);
	free(cmd);
	return (ret);
}

struct s_command *process_line(char *line) {
	int i = 0;
	int j = 0;
	while (line[i] && line[i] == ' ') {
		i++;
	}
	j = i;
	while (line[j] && line[j] != ' ') {
		j++;
	}
	char *ret = (char *)calloc(sizeof(char), j - i + 1);
	memcpy(ret, line + i, j - i);
	if (ret) {
		struct s_command *cmd = (struct s_command *)calloc(sizeof(struct s_command), 1);
		cmd->cmd = ret;
		if (strlen(line + j)) {
			cmd->arg = (char *)calloc(sizeof(char), strlen(line + j) + 1);
			cmd->arg = memcpy(cmd->arg, line + j, strlen(line + j));
		} else {
			cmd->arg = NULL;
		}
		/*
		int i = 0;
		while (ret) {
			ret = strtok(NULL, " \t");
			if (ret) {
				if (!cmd->arg) {
					cmd->arg = (char **)calloc(sizeof(char *), 2);
					cmd->arg = ret;
					cmd->arg[1] = NULL;
				} else {
					i++;
					cmd->arg = (char **)realloc(cmd->arg, (sizeof(char *) * (i + 1)));
					cmd->arg[i - 1] = ret;
					cmd->arg[i] = NULL;
				}
			}
		}
		*/
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
	struct s_command *cmd = NULL;
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
