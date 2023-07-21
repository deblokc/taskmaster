/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/21 12:32:19 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "taskmasterctl.h"
#include "readline.h"

/* * * * * LIST OF COMMAND IN taskmasterCTL * * * * 

add <name> [...]	Activates any updates in config for process/group

exit	Exit the taskmaster shell.

maintail -f 	Continuous tail of taskmaster main log file (Ctrl-C to exit)
maintail -100	last 100 *bytes* of taskmasterd main log file
maintail	last 1600 *bytes* of taskmaster main log file

quit	Exit the taskmaster shell.

reread 			Reload the daemon's configuration files without add/remove

signal <signal name> <name>		Signal a process
signal <signal name> <gname>:*		Signal all processes in a group
signal <signal name> <name> <name>	Signal multiple processes or groups
signal <signal name> all		Signal all processes

stop <name>		Stop a process
stop <gname>:*		Stop all processes in a group
stop <name> <name>	Stop multiple processes or groups
stop all		Stop all processes

version			Show the version of the remote taskmasterd process

avail			Display all configured processes

fg <process>	Connect to a process in foreground mode
		Ctrl-C to exit

open <url>	Connect to a remote taskmasterd process.
		(for UNIX domain socket, use unix:///socket/path)

reload 		Restart the remote taskmasterd.

restart <name>		Restart a process
restart <gname>:*	Restart all processes in a group
restart <name> <name>	Restart multiple processes or groups
restart all		Restart all processes
Note: restart does not reread config files. For that, see reread and update.

start <name>		Start a process
start <gname>:*		Start all processes in a group
start <name> <name>	Start multiple processes or groups
start all		Start all processes

tail [-f] <name> [stdout|stderr] (default stdout)
Ex:
tail -f <name>		Continuous tail of named process stdout
			Ctrl-C to exit.
tail -100 <name>	last 100 *bytes* of process stdout
tail <name> stderr	last 1600 *bytes* of process stderr

clear <name>		Clear a process' log files.
clear <name> <name>	Clear multiple process' log files
clear all		Clear all process' log files

help		Print a list of available actions
help <action>	Print help for <action>

pid			Get the PID of taskmasterd.
pid <name>		Get the PID of a single child process by name.
pid all			Get the PID of every child process, one per line.

remove <name> [...]	Removes process/group from active config

shutdown 	Shut the remote taskmasterd down.

status <name>		Get status for a single process
status <gname>:*	Get status for all processes in a group
status <name> <name>	Get status for multiple named processes
status			Get all process status info

update			Reload config and add/remove as necessary, and will restart affected programs
update all		Reload config and add/remove as necessary, and will restart affected programs
update <gname> [...]	Update specific groups

* * * * * * * * * * * * * * * * * * * * * * * * * * */

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
			printf("maintail -f\t\tContinuous tail of taskmaster main log file (Ctrl-C to exit)\n");
			printf("maintail -100\t\tlast 100 *bytes* of taskmaster main log file\n");
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
			printf("start all\t\t\tStart all processes\n");
		} else if (!strcmp(arg[0], "tail")) {
			printf("tail [-f] <name>\t[stdout|stderr] (default stdout)\n");
			printf("Ex:\n");
			printf("tail -f <name>\tContinuous tail of named process stdout\n");
			printf("\t\t\tCtrl-C to exit\n");
			printf("tail -100 <name>\tlast 100 *bytes* of process stdout\n");
			printf("tail <name> stderr\tlast 1600 *bytes* of process stderr\n");
		} else if (!strcmp(arg[0], "clear")) {
			printf("clear <name>\t\tClear a process' log files\n");
			printf("clear <name> <name>\t\tClear multiple process' log files\n");
			printf("clear all\t\t\tClear all process' log files\n");
		} else if (!strcmp(arg[0], "help")) {
			printf("help\t\t\tPrint a list of available commands\n");
			printf("help <command>\t\tPrint help for <command>\n");
		} else if (!strcmp(arg[0], "pid")) {
			printf("pid\t\t\tGet the PID of taskmasterd.\n");
			printf("pid <name>\tGet the PID of a single child process by name.\n");
			printf("pid all\t\tGet the PID of every child process, one per line.\n");
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

int main(int ac, char **av) {
	(void)ac;
	(void)av;
	char *tab[] = {"add", "exit", "maintail", "quit", "reread", "signal", "stop", "version",\
		"avail", "fg", "reload", "restart", "start", "tail",\
			"clear", "help", "pid", "remove", "shutdown", "status", "update", NULL};

	printf("%s\n", ctermid(NULL));
	printf("%s\n", ttyname(0));

	complete_init(tab);

	FILE *file = fopen_history();
	add_old_history(file);

	char *line = ft_readline("taskmasterctl>");
	struct s_command *cmd = NULL;
	while (line != NULL) {
		// process line (remove space n sht)
		cmd = process_line(line);
		if (cmd) {
			if (exec(cmd)) {
				free(line);
				break ;
			}
			// add to history and read another line
			add_history(line);
			add_file_history(line, file);
		}
		free(line);
		line = ft_readline("taskmasterctl>");
	}
	clear_history();
	if (file) {
		fclose(file);
	}
}
