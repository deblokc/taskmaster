/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/16 16:57:18 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "taskmaster.h"

void child_exec(struct s_process *proc) {
		dup2(proc->stdin[1], 0);
		dup2(proc->stdout[1], 1);
		dup2(proc->stderr[1], 2);
		close(proc->stdin[0]);
		close(proc->stdout[0]);
		close(proc->stderr[0]);
//		printf("%s child will try to execute %s\n", proc->name, proc->program->command);
		execve(proc->program->command, proc->program->args, proc->program->env);
		perror("execve");
		close(proc->stdin[1]);
		close(proc->stdout[1]);
		close(proc->stderr[1]);
}

void exec(struct s_process *process) {
	if (pipe(process->stdin)) {
		return ;
	}
	if (pipe(process->stdout)) {
		return ;
	}
	if (pipe(process->stderr)) {
		return ;
	}
	int pid = fork();
	if (!pid) {
		child_exec(process);
	}
	char buf[4096];
	if (write(process->stdin[0], "/bin/ls\n", 9) > 0) {
		printf("SENT /bin/ls to process\n");
	}
	if (read(process->stdout[0], buf, 4095) > 0) {
		printf("PIPED STDOUT : >%s<\n", buf);
	}

}

void administrator(struct s_process *process) {
	printf("Administrator for %s created\n", process->name);
	exec(process);
	printf("Exiting administrator\n");
}

void test(void) {
	struct s_program prog;
	char *args[2] = {"/bin/bash", NULL};
	char *env[3] = {"USER=tnaton", "TEST=1", NULL};

	prog.name = "ls";
	prog.command = "/bin/bash";
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
