/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/16 20:25:36 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "taskmaster.h"

void child_exec(struct s_process *proc) {
		dup2(proc->stdin[0], 0);
		dup2(proc->stdout[1], 1);
		dup2(proc->stderr[1], 2);

		close(proc->stdin[1]);
		close(proc->stdout[0]);
		close(proc->stderr[0]);

		execve(proc->program->command, proc->program->args, proc->program->env);
		perror("execve");

		close(proc->stdin[1]);
		close(proc->stdout[1]);
		close(proc->stderr[1]);
}

int exec(struct s_process *process) {

	if (pipe(process->stdin)) {
		return 1;
	}
	if (pipe(process->stdout)) {
		return 1;
	}
	if (pipe(process->stderr)) {
		return 1;
	}

	if ((process->pid = fork()) < 0) {
		return 1;
	}
	if (!process->pid) {
		child_exec(process);
	}
	return (0);

/*
	int epollfd;
	struct epoll_event in, out;

	epollfd = epoll_create(1);

	in.events = EPOLLOUT;
	in.data.fd = process->stdin[1];
	out.events = EPOLLIN;
	out.data.fd = process->stdout[0];

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, process->stdin[1], &in) == -1) {
		perror("epoll_ctl");
	}
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, process->stdout[0], &out) == -1) {
		perror("epoll_ctl");
	}

	struct epoll_event events[2];
	printf("epoll_waiting\n");
	while (1) {
		printf("waiting event...\n");
		int nfds = epoll_wait(epollfd, events, 2, -1);
		if (nfds == -1) {
			perror("epoll_wait");
		}
		printf("got an event !\n");

		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == process->stdin[1]) {
				printf("ready to write in pipe\n");
				if (write(process->stdin[1], "ls\n", 3) > 0) {
					printf("Wrote to pipe\n");
				}
			} else if (events[i].data.fd == process->stdout[0]) {
				printf("ready to read from pipe\n");
				char buf[4096];
				if (read(process->stdout[0], buf, 4096) > 0) {
					printf("READ : >%s<\n", buf);
					return ;
				}
			} else {
				printf("WTF\n");
			}
		}
	}
*/
}

bool should_start(struct s_process *process) {
	if (process->program->autostart) {
		process->program->autostart = false; // ?
		return true;
	} else if (process->status == EXITED) {
		if (process->program->autorestart == ALWAYS) {
			return true;
		} else if (process->program->autorestart == ONERROR) {
			// check if exit status is expected or not
		} else {
			return false;
		}
	} else if (process->status == BACKOFF) {
		if (process->count_restart < process->program->startretries) {
			process->count_restart++;
			return true;
		} else {
			process->status = FATAL;
			return false;
		}
	}
	return false;
}

void administrator(struct s_process *process) {
	printf("Administrator for %s created\n", process->name);

	bool start = false; // this bool serve for autostart and start signal sent by controller
	if (process->program->autostart) {
		start = true;
	}

	while (1) {
		if (process->status == EXITED || process->status == BACKOFF || start) {
			if (should_start(process) || start) {
				start = false;
				if (exec(process)) {
					printf("FATAL ERROR\n");
					return ;
				}
			}
		}
	}
	printf("Exiting administrator\n");
	return ;
}

void test(void) {
	struct s_program prog;
	char *args[2] = {"/usr/bin/telnet", NULL};
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
