/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/16 15:34:20 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include "taskmaster.h"


void administrator(struct s_process *process) {
	printf("Administrator for %s created\n", process->name);

	int pid = fork();
	if (!pid) {
		printf("%s child will try to execute %s\n", process->name, process->program->command);
		execve(process->program->command, process->program->args, process->program->env);
		perror("execve");
	}
	printf("Exiting administrator\n");
}

void test(void) {
	struct s_program prog;
	char *args[4] = {"bash", "-c", "env; echo $USER", NULL};
	char *env[3] = {"USER=tnaton", "TEST=1", NULL};

	prog.name = "ls";
	prog.command = "bash";
	prog.args = args;
	prog.numprocs = 1;
	prog.priority = 1;
	prog.autostart = true;
	prog.startsecs = 1;
	prog.startretries = 3;
	prog.autorestart = ONERROR;
	prog.exitcodes = NULL;
	prog.stopsignal = 0;
	prog.stopwaitsecs = 5;
	prog.stdoutlog = false;
	prog.stdoutlogpath = NULL;
	prog.stderrlog = false;
	prog.stderrlogpath = NULL;
	prog.env = env;
	prog.workingdir = "/usr";
	prog.umask = 0;
	prog.user = NULL;

	struct s_process proc;

	proc.name = "listing";
	proc.pid = 0;
	proc.status = STOPPED;
	proc.program = &prog;
	proc.count_restart = 0;

	administrator(&proc);
}
