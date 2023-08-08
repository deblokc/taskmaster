/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/08 19:33:08 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

//#define _GNU_SOURCE
#include "taskmaster.h"
#include <fcntl.h>
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
	struct s_report reporter;
	reporter.report_fd = proc->log;
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
		snprintf(reporter.buffer, PIPE_BUF - 22, "CRITICAL: %s command \"%s\" not found\n", proc->name, proc->program->command);
		report(&reporter, false);
	} else {
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s execveing command \"%s\"\n", proc->name, proc->program->command);
		report(&reporter, false);
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
		struct s_report reporter;
		reporter.report_fd = process->log;
		if (process->count_restart < process->program->startretries) {
			process->count_restart++;
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s restarting for the %d time\n", process->name, process->count_restart);
			report(&reporter, false);
			return true;
		} else {
			snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s is now in FATAL\n", process->name);
			report(&reporter, false);
			process->status = FATAL;
			return false;
		}
	}
	return false;
}

void closeall(struct s_process *process, int epollfd) {
	struct s_report reporter;
	reporter.report_fd = process->log;
	snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s exited\n", process->name);
	report(&reporter, false);
	epoll_ctl(epollfd, EPOLL_CTL_DEL, process->stdout[0], NULL);
	epoll_ctl(epollfd, EPOLL_CTL_DEL, process->stderr[0], NULL);
	close(process->stdin[1]);
	close(process->stdout[0]);
	close(process->stderr[0]);
	waitpid(process->pid, NULL, 0);
}

void *administrator(void *arg) {
	struct s_process *process = (struct s_process *)arg;
	struct s_report reporter;
	reporter.report_fd = process->log;
	snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: Administrator for %s created\n", process->name);
	report(&reporter, false);

	if (process->stdoutlog) {
		if ((process->stdout_logger.logfd = open(process->stdout_logger.logfile, O_CREAT | O_TRUNC | O_RDWR, 0666 & ~(process->stdout_logger.umask))) < 0) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s could not open %s for stdout logging\n", process->name, process->stdout_logger.logfile);
			report(&reporter, false);
		}
	}
	if (process->stderrlog) {
		if ((process->stderr_logger.logfd = open(process->stderr_logger.logfile, O_CREAT | O_TRUNC | O_RDWR, 0666 & ~(process->stderr_logger.umask))) < 0) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s could not open %s for stderr logging\n", process->name, process->stderr_logger.logfile);
			report(&reporter, false);
		}
	}
	bool				start = false; // this bool serve for autostart and start signal sent by controller
	bool				exit = false;
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
	in.data.fd = process->com[0];
	epoll_ctl(epollfd, EPOLL_CTL_ADD, in.data.fd, &in); // listen for main communication

	while (1) {
		if (process->status == EXITED || process->status == BACKOFF || (process->status != STOPPING && start)) {
			if (should_start(process) || start) {
				start = false;
				snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STARTING\n", process->name);
				report(&reporter, false);
				snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has raw cmd %s\n", process->name, process->program->command);
				report(&reporter, false);
				if (exec(process, epollfd)) {
					snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in exec\n", process->name);
					report(&reporter, false);
					return NULL;
				}
			} else {
				snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s will not instantly restart\n", process->name);
				report(&reporter, false);
				return NULL;
			}
		}
		if (process->status == STARTING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				return NULL;
			}

			long long start_micro = ((process->start.tv_sec * 1000) + (process->start.tv_usec / 1000));
			long long tmp_micro = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000));

			if (((long long)process->program->startsecs * 1000) <= (tmp_micro - start_micro)) {
				process->status = RUNNING;
				snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now RUNNING\n", process->name);
				report(&reporter, false);
				// need to epoll_wait 0 to return instantly
				tmp_micro = ((long long)process->program->startsecs * 1000);
				start_micro = 0;
			}
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s starting epoll time to wait : %lld\n", process->name, ((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro));
			report(&reporter, false);
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
							if (process->stdoutlog) {
								write_process_log(&(process->stdout_logger), buf);
							}
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
							report(&reporter, false);
							closeall(process, epollfd);
							process->status = BACKOFF;
							break ;
						}
					}
					if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
							if (process->stderrlog) {
								write_process_log(&(process->stderr_logger), buf);
							}
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
							report(&reporter, false);
							closeall(process, epollfd);
							process->status = BACKOFF;
							break ;
						}
					}
				}
			} else if (((long long)process->program->startsecs * 1000) - (tmp_micro - start_micro) > INT_MAX) { // if timeout and not because time to wait is bigger than an int
				process->status = RUNNING;
				snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now RUNNING\n", process->name);
				report(&reporter, false);
			}
		} else if (process->status == RUNNING) {
			nfds = epoll_wait(epollfd, events, 3, -1); // busy wait for smth to do
			char buf[PIPE_BUF + 1];

			for (int i = 0; i < nfds; i++) {
			/* handles fds as needed */
				if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
					bzero(buf, PIPE_BUF + 1);
					if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
						if (process->stdoutlog) {
							write_process_log(&(process->stdout_logger), buf);
						}
					} else {
						closeall(process, epollfd);
						process->status = EXITED;
						snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now EXITED\n", process->name);
						report(&reporter, false);
						break ;
					}
				}
				if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
					bzero(buf, PIPE_BUF + 1);
					if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
						if (process->stderrlog) {
							write_process_log(&(process->stderr_logger), buf);
						}
					} else {
						closeall(process, epollfd);
						process->status = EXITED;
						snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now EXITED\n", process->name);
						report(&reporter, false);
						break ;
					}
				}
				if (events[i].data.fd == in.data.fd) {
					bzero(buf, PIPE_BUF + 1);
					if (read(in.data.fd, buf, PIPE_BUF) > 0) {
						snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator received something from main thread while RUNNING\n", process->name);
						report(&reporter, false);
						if (!strcmp(buf, "exit")) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to exit\n", process->name);
							report(&reporter, false);
							if (process->status == STARTING || process->status == RUNNING || process->status == BACKOFF) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was not already stopped, sending him signal %d to stop him\n", process->name, process->program->stopsignal);
								report(&reporter, false);
								process->status = STOPPING;
								kill(process->pid, process->program->stopsignal);
								if (gettimeofday(&stop, NULL)) {
									snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
									report(&reporter, false);
									return NULL;
								}
							} else {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was already stopped, exiting administrator\n", process->name);
								report(&reporter, false);
							}
							exit = true;
						} else if (!strcmp(buf, "stop")) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to stop\n", process->name);
							report(&reporter, false);
							if (process->status == STARTING || process->status == RUNNING || process->status == BACKOFF) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was not already stopped, sending him signal %d to stop him\n", process->name, process->program->stopsignal);
								report(&reporter, false);
								process->status = STOPPING;
								kill(process->pid, process->program->stopsignal);
								if (gettimeofday(&stop, NULL)) {
									snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
									report(&reporter, false);
									return NULL;
								}
							} else {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was stopped, did nothing\n", process->name);
								report(&reporter, false);
							}
						} else if (!strcmp(buf, "start")) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to start\n", process->name);
							report(&reporter, false);
							if (process->status == STOPPED || process->status == EXITED || process->status == FATAL) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was stopped, starting it\n", process->name);
								report(&reporter, false);
								start = true;
							} else {
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was running, did nothing\n", process->name);
								report(&reporter, false);
							}
						} else if (!strncmp(buf, "sig", 3)) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to send sig %d to process", process->name, (int)buf[3]);
							report(&reporter, false);
							kill(process->pid, (int)buf[3]);
						}
					} else {
						snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s's administrator could not read from main thread, exiting to avoid hanging\n", process->name);
						report(&reporter, false);
						process->status = STOPPING;
						if (gettimeofday(&stop, NULL)) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
							report(&reporter, false);
							return NULL;
						}
						exit = true;
					}
				}
			}
		} else if (process->status == STOPPING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				return NULL;
			}

			long long start_micro = ((stop.tv_sec * 1000) + (stop.tv_usec / 1000));
			long long tmp_micro = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000));

			if ((long long)(process->program->stopwaitsecs * 1000) <= (long long)(tmp_micro - start_micro)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s did not stop before %ds, sending SIGKILL\n", process->name, process->program->stopwaitsecs);
				report(&reporter, false);
				process->status = STOPPED;
				// kill(process->pid, SIGKILL);
				// need to epoll_wait 0 to return instantly
				tmp_micro = (process->program->stopwaitsecs * 1000);
				start_micro = 0;
			}
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s stopping epoll time to wait : %lld\n", process->name, ((long long)process->program->stopwaitsecs * 1000) - (tmp_micro - start_micro));
			report(&reporter, false);
			nfds = epoll_wait(epollfd, events, 3, (int)((process->program->stopwaitsecs * 1000) - (tmp_micro - start_micro)));
			if (nfds) { // if not timeout
				for (int i = 0; i < nfds; i++) {
				/* handles fds as needed */
					if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
							if (process->stdoutlog) {
								write_process_log(&(process->stdout_logger), buf);
							}
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							report(&reporter, false);
							stop.tv_sec = 0;
							stop.tv_usec = 0;
							if (exit) { // if need to exit administrator
								return NULL;
							}
							break ;
						}
					}
					if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
							if (process->stderrlog) {
								write_process_log(&(process->stderr_logger), buf);
							}
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							report(&reporter, false);
							stop.tv_sec = 0;
							stop.tv_usec = 0;
							if (exit) { // if need to exit administrator
								return NULL;
							}
							break ;
						}
					}
				}
			} else { // if timeout
				process->status = STOPPED;
				stop.tv_sec = 0;
				stop.tv_usec = 0;
				if (exit) { // if need to exit administrator
					return NULL;
				}
			}
		} else if (process->status == STOPPED || process->status == EXITED || process->status == FATAL) {
			char buf[PIPE_BUF + 1];
			bzero(buf, PIPE_BUF + 1);
			nfds = epoll_wait(epollfd, events, 3, -1); // busy wait for main thread to send instruction
			for (int i = 0; i < nfds; i++) {
				if (events[i].data.fd == in.data.fd) {
					if (read(in.data.fd, buf, PIPE_BUF) > 0) {
						snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator received something from main thread while STOPPED\n", process->name);
						report(&reporter, false);
						if (!strcmp(buf, "exit")) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to exit\n", process->name);
							report(&reporter, false);
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was already stopped, exiting administrator\n", process->name);
							report(&reporter, false);
							return NULL;
						}
					} else {
					}
				}
			}
		}
	}
	return NULL;
}
