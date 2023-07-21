/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/21 16:21:59 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "taskmasterctl.h"
#include "readline.h"

int g_sig = 0;

void help(char **arg) {
	if (!arg) {
		printf("default commands (type help <command>):\n");
		printf("=======================================\n");
		printf("TOUTES LES COMMANDES QUON AURA\n");
	} else {
		if (!strcmp(arg[0], "add")) {
			printf("add <name> [...]\tActivates any updates in config for process/group\n");
		} else if (!strcmp(arg[0], "exit")) {
			printf("exit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg[0], "maintail")) {
			printf("maintail -f\t\t\tContinuous tail of taskmaster main log file (Ctrl-C to exit)\n");
			printf("maintail -100\t\t\tlast 100 *bytes* of taskmaster main log file\n");
			printf("maintail\t\t\tlast 1600 *bytes* of taskmaster main log file\n");
		} else if (!strcmp(arg[0], "quit")) {
			printf("quit\tExit the taskmaster shell.\n");
		} else if (!strcmp(arg[0], "reread")) {
			printf("reread\t\t\tReload the daemon's configuration files without add/remove\n");
		} else if (!strcmp(arg[0], "signal")) {
			printf("signal <signal name> <name>\t\tSignal a process\n");
			printf("signal <signal name> <name> <name>\tSignal multiple processes or groups\n");
			printf("signal <signal name> all\t\tSignal all processes\n");
		} else if (!strcmp(arg[0], "stop")) {
			printf("stop <name>\t\tStop a process\n");
			printf("stop <name> <name>\tStop multiple processes or groups\n");
			printf("stop all\t\tStop all processes\n");
		} else if (!strcmp(arg[0], "version")) {
			printf("version\t\t\tShow the version of the remote taskmasterd process\n");
		} else if (!strcmp(arg[0], "avail")) {
			printf("avail\t\t\tDisplay all configured processes\n");
		} else if (!strcmp(arg[0], "fg")) {
			printf("fg <process>\tConnect to a process in foreground mode\n");
			printf("\t\tCtrl-C to exit\n");
		} else if (!strcmp(arg[0], "reload")) {
			printf("reload\t\tRestart the remote taskmasterd.\n");
		} else if (!strcmp(arg[0], "restart")) {
			printf("restart <name>\t\tRestart a process\n");
			printf("restart <name> <name>\tRestart multiple processes or groups\n");
			printf("restart all\t\tRestart all processes\n");
			printf("Note: restart does not reread config files. For that, see reread and update.\n");
		} else if (!strcmp(arg[0], "start")) {
			printf("start <name>\t\tStart a process\n");
			printf("start <name> <name>\tStart multiple processes or groups\n");
			printf("start all\t\tStart all processes\n");
		} else if (!strcmp(arg[0], "tail")) {
			printf("tail [-f] <name>\t[stdout|stderr] (default stdout)\n");
			printf("Ex:\n");
			printf("tail -f <name>\t\tContinuous tail of named process stdout\n");
			printf("\t\t\tCtrl-C to exit\n");
			printf("tail -100 <name>\tlast 100 *bytes* of process stdout\n");
			printf("tail <name> stderr\tlast 1600 *bytes* of process stderr\n");
		} else if (!strcmp(arg[0], "clear")) {
			printf("clear <name>\t\t\tClear a process' log files\n");
			printf("clear <name> <name>\t\tClear multiple process' log files\n");
			printf("clear all\t\t\tClear all process' log files\n");
		} else if (!strcmp(arg[0], "help")) {
			printf("help\t\t\tPrint a list of available commands\n");
			printf("help <command>\t\tPrint help for <command>\n");
		} else if (!strcmp(arg[0], "pid")) {
			printf("pid\t\t\tGet the PID of taskmasterd.\n");
			printf("pid <name>\t\tGet the PID of a single child process by name.\n");
			printf("pid all\t\t\tGet the PID of every child process, one per line.\n");
		} else if (!strcmp(arg[0], "remove")) {
			printf("remove <name> [...] Removes process from active config\n");
		} else if (!strcmp(arg[0], "shutdown")) {
			printf("shutdown\t\tShut the remote taskmasterd down.\n");
		} else if (!strcmp(arg[0], "status")) {
			printf("status <name>\t\tGet status for a single process\n");
			printf("status <name> <name>\tGet status for multiple named processes\n");
			printf("status\t\t\tGet all process status info\n");
		} else if (!strcmp(arg[0], "update")) {
			printf("update\t\t\tReload config and add/remove as necessary, and will restart affected programs\n");
			printf("update all\t\tReload config and add/remove as necessary, and will restart affected programs\n");
		} else {
			printf("*** No help on %s\n", arg[0]);
		}
	}
}

int exec(struct s_command *cmd) {
	int ret = 0;
/*
	printf("cmd : %s\n", cmd->cmd);
	if (cmd->arg) {
		printf("arg :");

		int i = 0;
		while (cmd->arg[i]) {
			printf(" %s", cmd->arg[i]);
			i++;
		}
		printf("\n");
	}
*/
	if (!strcmp(cmd->cmd, "help")) {
		help(cmd->arg);
	} else if (!strcmp(cmd->cmd, "exit") || !strcmp(cmd->cmd, "quit")) {
		ret = 1;
	}
	free(cmd->arg);
	free(cmd);
	return (ret);
}

struct s_command *process_line(char *line) {
	char *ret = strtok(line, " \t");
	if (ret) {
		struct s_command *cmd = (struct s_command *)calloc(sizeof(struct s_command), 1);
		cmd->cmd = ret;
		int i = 0;
		while (ret) {
			ret = strtok(NULL, " \t");
			if (ret) {
				if (!cmd->arg) {
					cmd->arg = (char **)calloc(sizeof(char *), 2);
					cmd->arg[0] = ret;
					cmd->arg[1] = NULL;
				} else {
					i++;
					cmd->arg = (char **)realloc(cmd->arg, (sizeof(char *) * (i + 1)));
					cmd->arg[i - 1] = ret;
					cmd->arg[i] = NULL;
				}
			}
		}
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
	char *tab[] = {"add", "exit", "maintail", "quit", "reread", "signal", "stop", "version",\
				"avail", "fg", "reload", "restart", "start", "tail",\
			"clear", "help", "pid", "remove", "shutdown", "status", "update", NULL};

	int socket = open_socket();
	signal(SIGINT, &sig_handler);
	complete_init(tab);

	FILE *file = fopen_history();
	add_old_history(file);

	char *line = ft_readline("taskmasterctl>");
	struct s_command *cmd = NULL;
	int ret = 0;
	char buf[4096];
	while (line != NULL) {
		bzero(buf, 4096);
		ret = recv(socket, buf, 4095, MSG_DONTWAIT);
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			printf("nothing to read\n");
		} else if (ret < 0) {
			perror("recv");
		} else {
			printf("GOT >%s<\n", buf);
		}
		// process line (remove space n sht)
		char *tmp = strdup(line);
		cmd = process_line(line);
		if (cmd) {
			if (exec(cmd)) {
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
