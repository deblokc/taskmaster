/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/07 14:30:01 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

//#define _GNU_SOURCE
#include "taskmaster.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

char *getcmd(struct s_process *proc) {
		char *command = NULL;
		if (!access(proc->program->command, X_OK)) { // if no need to search w/ path (aka absolute path or relative
			command = strdup(proc->program->command);
		} else {
			char *envpath = strdup(getenv("PATH"));
			if (envpath) {
				char *path;
				path = strtok(envpath, ":");
				while (path) {// size of path + '/'    +    size of command   +   '\0'
					size_t size = (strlen(path) + 1 + strlen(proc->program->command) + 1);
					command = (char *)calloc(size, sizeof(char));
					if (!command) {
						break ;
					}
					strcat(command, path);
					strcat(command, "/");
					strcat(command, proc->program->command);
					if (!access(command, X_OK)) {
						break ;
					}
					free(command);
					command = NULL;
					path = strtok(NULL, ":");
				}
				free(envpath);
			} else {
				free(command);
				return (NULL);
			}
		}
		return (command);
}

void child_exec(struct s_process *proc) {
		// dup every standard stream to pipe
		dup2(proc->stdin[0], 0);
		dup2(proc->stdout[1], 1);
		dup2(proc->stderr[1], 2);

		// close ends of pipe which doesnt belong to us
		close(proc->stdin[1]);
		close(proc->stdout[0]);
		close(proc->stderr[0]);
		close(proc->com[0]);
		close(proc->com[1]);
	
		if (proc->program->env) {
			struct s_env*	begin;

			begin = proc->program->env;
			while (begin)
			{
				putenv(strdup(begin->value));
				begin = begin->next;
			}
		}
		char *command = getcmd(proc);

		if (!command) {
			char buf[PIPE_BUF - 22];
			snprintf(buf, PIPE_BUF - 22, "CRITICAL: %s command \"%s\" not found\n", proc->name, proc->program->command);
			if (write(proc->log, buf, strlen(buf))) {}
		} else {
			char buf[PIPE_BUF - 22];
			snprintf(buf, PIPE_BUF - 22, "DEBUG: %s execveing command \"%s\"", proc->name, proc->program->command);
			if (write(proc->log, buf, strlen(buf))) {}
			execve(command, proc->program->args, environ);
			perror("execve");
		}
		// on error dont leak fds
		close(proc->log);
		close(proc->stdin[0]);
		close(proc->stdout[1]);
		close(proc->stderr[1]);
		free(command);
		free(proc->name);
		exit(1);
}

int exec(struct s_process *process, int epollfd) {

	struct epoll_event out, err;

	if (pipe(process->stdin)) {
		return 1;
	}
	if (pipe(process->stdout)) {
		return 1;
	}
	if (pipe(process->stderr)) {
		return 1;
	}

	// set listening to stdout
	out.events = EPOLLIN;
	out.data.fd = process->stdout[0];

	// set listening to stderr
	err.events = EPOLLIN;
	err.data.fd = process->stderr[0];

	// register out
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, process->stdout[0], &out) == -1) {
		perror("epoll_ctl");
	}

	// register err
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, process->stderr[0], &err) == -1) {
		perror("epoll_ctl");
	}

	// fork for exec
	if ((process->pid = fork()) < 0) {
		return 1;
	}
	if (!process->pid) {
		// child execve
		close(epollfd);
		child_exec(process);
	}

	// close the ends of the pipe which doesnt belong to us
	close(process->stdin[0]);
	close(process->stdout[1]);
	close(process->stderr[1]);

	// set start of process
	if (gettimeofday(&process->start, NULL)) {
		return 1;
	}

	// change to running
	process->status = STARTING;

	return 0;
}

static bool should_start(struct s_process *process) {
	if (process->status == EXITED) {
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
			char buf[PIPE_BUF - 22];
			snprintf(buf, PIPE_BUF - 22, "DEBUG: %s restarting for the %d time\n", process->name, process->count_restart);
			if (write(process->log, buf, strlen(buf))) {}
			return true;
		} else {
			process->status = FATAL;
			return false;
		}
	}
	return false;
}

void closeall(struct s_process *process, int epollfd) {
	char buf[PIPE_BUF - 22];
	snprintf(buf, PIPE_BUF - 22, "DEBUG: %s exited\n", process->name);
	if (write(process->log, buf, strlen(buf))) {}
	epoll_ctl(epollfd, EPOLL_CTL_DEL, process->stdout[0], NULL);
	epoll_ctl(epollfd, EPOLL_CTL_DEL, process->stderr[0], NULL);
	close(process->stdin[1]);
	close(process->stdout[0]);
	close(process->stderr[0]);
	waitpid(process->pid, NULL, -1);
}

void *administrator(void *arg) {
	struct s_process *process = (struct s_process *)arg;
	char buf[PIPE_BUF - 22];
	snprintf(buf, PIPE_BUF - 22, "DEBUG: Administrator for %s created\n", process->name);
	if (write(process->log, buf, strlen(buf))) {}

	bool				start = false; // this bool serve for autostart and start signal sent by controller
	int					nfds = 0;
	struct epoll_event	events[3], in;
	int					epollfd = epoll_create(1);
	struct timeval		stop;  //

	stop.tv_sec = 0;           // NEED TO SET W/ gettimeofday WHEN CHANGING STATUS TO STOPPING
	stop.tv_usec = 0;          //

	if (process->program->autostart) {
		start = true;
	}

	in.events = EPOLLIN;
	in.data.fd = 0;
	(void)in;
	// ALL THIS IS TEMPORARY -- WILL USE PIPE WITH MAIN THREAD INSTEAD
	//epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &in);

	while (1) {
		if (process->status == EXITED || process->status == BACKOFF || start) {
			if (should_start(process) || start) {
				start = false;
				snprintf(buf, PIPE_BUF - 22, "INFO: %s is now STARTING\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
				snprintf(buf, PIPE_BUF - 22, "DEBUG: %s has raw cmd %s\n", process->name, process->program->command);
				if (write(process->log, buf, strlen(buf))) {}
				if (exec(process, epollfd)) {
					snprintf(buf, PIPE_BUF - 22, "WARNING: %s got a fatal error in exec\n", process->name);
					if (write(process->log, buf, strlen(buf))) {}
					return NULL;
				}
			} else {

				snprintf(buf, PIPE_BUF - 22, "INFO: %s could not restart, exiting\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
				usleep(1000);
				return NULL;
			}
		}
		if (process->status == STARTING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(buf, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
				return NULL;
			}

			long long start_micro = ((process->start.tv_sec * 1000) + (process->start.tv_usec / 1000));
			long long tmp_micro = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000));

			if (((long long)process->program->startsecs * 1000) <= (tmp_micro - start_micro)) {
				process->status = RUNNING;
				snprintf(buf, PIPE_BUF - 22, "INFO: %s is now RUNNING\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
				// need to epoll_wait 0 to return instantly
				tmp_micro = ((long long)process->program->startsecs * 1000);
				start_micro = 0;
			}
			snprintf(buf, PIPE_BUF - 22, "DEBUG: %s starting epoll time to wait : %lld\n", process->name, ((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro));
			if (write(process->log, buf, strlen(buf))) {}
			if (((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro) > INT_MAX) { // if time to wait is bigger than an int, we wait as much as possible and come again if we got no event
				nfds = epoll_wait(epollfd, events, 3, INT_MAX);
			} else {
				nfds = epoll_wait(epollfd, events, 3, (int)(((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro)));
			}
			if (nfds) { // if not timeout
				for (int i = 0; i < nfds; i++) {
				/* handles fds as needed */
					if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
							printf("%s : >%s<\n", process->name, buf);
						} else {
							snprintf(buf, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
							if (write(process->log, buf, strlen(buf))) {}
							closeall(process, epollfd);
							process->status = BACKOFF;
							break ;
						}
					}
					if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
							printf("%s ERROR : >%s<\n", process->name, buf);
						} else {
							snprintf(buf, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
							if (write(process->log, buf, strlen(buf))) {}
							closeall(process, epollfd);
							process->status = BACKOFF;
							break ;
						}
					}
				}
			} else if (((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro) > INT_MAX) { // if timeout and not because time to wait is bigger than an int
				process->status = RUNNING;
				snprintf(buf, PIPE_BUF - 22, "INFO: %s is now RUNNING\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
			}
		} else if (process->status == RUNNING) {
			nfds = epoll_wait(epollfd, events, 3, -1); // busy wait for smth to do

			for (int i = 0; i < nfds; i++) {
			/* handles fds as needed */
				if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
					char buf[PIPE_BUF + 1];
					bzero(buf, PIPE_BUF + 1);
					if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
						printf("%s : >%s<\n", process->name, buf);
					} else {
						closeall(process, epollfd);
						process->status = EXITED;
						snprintf(buf, PIPE_BUF - 22, "INFO: %s is now EXITED\n", process->name);
						if (write(process->log, buf, strlen(buf))) {}
						break ;
					}
				}
				if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
					char buf[PIPE_BUF + 1];
					bzero(buf, PIPE_BUF + 1);
					if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
						printf("%s ERROR : >%s<\n", process->name, buf);
					} else {
						closeall(process, epollfd);
						process->status = EXITED;
						snprintf(buf, PIPE_BUF - 22, "INFO: %s is now EXITED\n", process->name);
						if (write(process->log, buf, strlen(buf))) {}
						break ;
					}
				}
			}
		} else if (process->status == STOPPING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(buf, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				if (write(process->log, buf, strlen(buf))) {}
				return NULL;
			}

			long long start_micro = ((stop.tv_sec * 1000) + (stop.tv_usec / 1000));
			long long tmp_micro = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000));

			if ((long long)(process->program->stopwaitsecs * 1000) <= (long long)(tmp_micro - start_micro)) {
				snprintf(buf, PIPE_BUF - 22, "WARNING: %s did not stop before %ds, sending SIGKILL\n", process->name, process->program->stopwaitsecs);
				if (write(process->log, buf, strlen(buf))) {}
				process->status = STOPPED;
				// kill(process->pid, SIGKILL);
				// need to epoll_wait 0 to return instantly
				tmp_micro = (process->program->stopwaitsecs * 1000);
				start_micro = 0;
			}
			snprintf(buf, PIPE_BUF - 22, "DEBUG: %s stopping epoll time to wait : %lld\n", process->name, ((long long)process->program->stopwaitsecs * 1000) - (tmp_micro - start_micro));
			if (write(process->log, buf, strlen(buf))) {}
			nfds = epoll_wait(epollfd, events, 3, (int)((process->program->stopwaitsecs * 1000) - (tmp_micro - start_micro)));
			if (nfds) { // if not timeout
				for (int i = 0; i < nfds; i++) {
				/* handles fds as needed */
					if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
							printf("%s : >%s<\n", process->name, buf);
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(buf, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							if (write(process->log, buf, strlen(buf))) {}
							stop.tv_sec = 0;
							stop.tv_usec = 0;
							break ;
						}
					}
					if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
							printf("%s ERROR : >%s<\n", process->name, buf);
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(buf, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							if (write(process->log, buf, strlen(buf))) {}
							stop.tv_sec = 0;
							stop.tv_usec = 0;
							break ;
						}
					}
				}
			} else { // if timeout
				process->status = STOPPED;
				// kill(process->pid, SIGKILL);
				stop.tv_sec = 0;
				stop.tv_usec = 0;
			}
		} else if (process->status == STOPPED || process->status == EXITED || process->status == FATAL) {
			nfds = epoll_wait(epollfd, events, 3, -1); // busy wait for main thread to send instruction

			for (int i = 0; i < nfds; i++) {
				// do shit
			}
		}
	}
	return NULL;
}
