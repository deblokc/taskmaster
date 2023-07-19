/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/19 18:17:31 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "taskmasterctl.h"
#include "readline.h"

/* * * * * LIST OF COMMAND IN SUPERVISORCTL * * * * 

add <name> [...]	Activates any updates in config for process/group

exit	Exit the supervisor shell.

maintail -f 	Continuous tail of supervisor main log file (Ctrl-C to exit)
maintail -100	last 100 *bytes* of supervisord main log file
maintail	last 1600 *bytes* of supervisor main log file

quit	Exit the supervisor shell.

reread 			Reload the daemon's configuration files without add/remove

signal <signal name> <name>		Signal a process
signal <signal name> <gname>:*		Signal all processes in a group
signal <signal name> <name> <name>	Signal multiple processes or groups
signal <signal name> all		Signal all processes

stop <name>		Stop a process
stop <gname>:*		Stop all processes in a group
stop <name> <name>	Stop multiple processes or groups
stop all		Stop all processes

version			Show the version of the remote supervisord process

avail			Display all configured processes

fg <process>	Connect to a process in foreground mode
		Ctrl-C to exit

open <url>	Connect to a remote supervisord process.
		(for UNIX domain socket, use unix:///socket/path)

reload 		Restart the remote supervisord.

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

pid			Get the PID of supervisord.
pid <name>		Get the PID of a single child process by name.
pid all			Get the PID of every child process, one per line.

remove <name> [...]	Removes process/group from active config

shutdown 	Shut the remote supervisord down.

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
		if (!strcmp(arg[0], "help")) {
			printf("help\t\t\tPrint a list of available commands\n");
			printf("help <command>\t\tPrint help for <command>\n");
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
		"avail", "fg", "open", "reload", "restart", "start", "tail",\
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
