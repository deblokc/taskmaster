/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   administrator.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 14:30:47 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/21 19:55:07 by tnaton           ###   ########.fr       */
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
#include <sys/stat.h>
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
				while (path) { // size of path + '/'    +    size of command   +   '\0'
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
		if (setsid() == -1)
		{
			snprintf(reporter.buffer, PIPE_BUF - 22, "CRITICAL: %s could not become a session leader\n", proc->name);
			report(&reporter, false);
			close(proc->log);
			close(proc->stdin[0]);
			close(proc->stdout[1]);
			close(proc->stderr[1]);
			free(command);
			free(proc->name);
			exit(1);
		}
		if (!unblock_signals_thread(&reporter))
		{
			close(proc->log);
			close(proc->stdin[0]);
			close(proc->stdout[1]);
			close(proc->stderr[1]);
			free(command);
			free(proc->name);
			exit(1);
		}
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

	// change to starting
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
	if (process->stdin[1] >= 0) {
		close(process->stdin[1]);
	}
	if (process->stdout[0] >= 0) {
		close(process->stdout[0]);
	}
	if (process->stderr[0] >= 0) {
		close(process->stderr[0]);
	}
	waitpid(process->pid, NULL, 0);
}

struct s_logging_client *new_logging_client(struct s_logging_client **list, int client_fd, struct s_report *reporter) {
	struct s_logging_client	*runner;
	struct s_logging_client	*new_client;

	new_client = calloc(1, sizeof(struct s_logging_client));
	if (!new_client)
	{
		strcpy(reporter->buffer, "CRITICAL: Could not calloc new client, connection will be ignored\n");
		report(reporter, false);
		close(client_fd);
		return (NULL);
	}
	new_client->poll.data.fd = client_fd;
	bzero(new_client->buf, PIPE_BUF + 1);
	if (*list) {
		runner = *list;
		while (runner->next) {
			runner = runner->next;
		}
		runner->next = new_client;
	} else {
		*list = new_client;
	}
	return (new_client);
}

int handle_command(struct s_process *process, char *buf, int epollfd) {
	struct s_report reporter;
	reporter.report_fd = process->log;

	if (!strcmp(buf, "exit")) {
		// exit command
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to exit\n", process->name);
		report(&reporter, false);
		if (process->status == STARTING || process->status == RUNNING || process->status == BACKOFF) { // if process is not stopped start stop process
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was not already stopped, sending him signal %d to stop him\n", process->name, process->program->stopsignal);
			report(&reporter, false);
			process->status = STOPPING;
			killpg(process->pid, process->program->stopsignal);
			if (gettimeofday(&process->stop, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				return -1;
			}
			process->bool_exit = true;
		} else { // else if is already stopped exit taskmaster
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was already stopped, exiting administrator\n", process->name);
			report(&reporter, false);
			return -1; // might need some more cleaning before this
		}
	} else if (!strcmp(buf, "stop")) {
		// stop command
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to stop\n", process->name);
		report(&reporter, false);
		if (process->status == STARTING || process->status == RUNNING || process->status == BACKOFF) { // if process is not stopped start stop process
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was not already stopped, sending him signal %d to stop him\n", process->name, process->program->stopsignal);
			report(&reporter, false);
			process->status = STOPPING;
			killpg(process->pid, process->program->stopsignal);
			if (gettimeofday(&process->stop, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				return -1;
			}
		} else { // if already stopped do nothing
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was stopped, did nothing\n", process->name);
			report(&reporter, false);
		}
	} else if (!strcmp(buf, "start")) {
		// stop command
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to start\n", process->name);
		report(&reporter, false);
		if (process->status == STOPPED || process->status == EXITED || process->status == FATAL) { // if process is stopped start it
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was stopped, starting it\n", process->name);
			report(&reporter, false);
			process->bool_start = true;
		} else { // if not stopped do nothing
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was running, did nothing\n", process->name);
			report(&reporter, false);
		}
	} else if (!strcmp(buf, "restart")) {
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to restart\n", process->name);
		report(&reporter, false);
		if (process->status == STARTING || process->status == RUNNING || process->status == BACKOFF) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s was not stopped, stopping it for restart\n", process->name);
			report(&reporter, false);
			process->status = STOPPING;
			killpg(process->pid, process->program->stopsignal);
			if (gettimeofday(&process->stop, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				return -1;
			}
		}
		process->bool_start = true;
	} else if (!strncmp(buf, "sig", 3)) {
		// sig command
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to send sig %d to process", process->name, (int)buf[3]);
		report(&reporter, false);
		killpg(process->pid, (int)buf[3]);
	} else if (!strncmp(buf, "fg", 2)) {
		int fd = atoi(buf + 2);
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to foregroung client with fd %d\n", process->name, fd);
		report(&reporter, false);
		if (fd <= 0) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "CRITICAL: %s's new client has corrupted fd %d, ignoring this client\n", process->name, fd);
			report(&reporter, false);
			return (0);
		}
		struct s_logging_client *client = new_logging_client(&(process->list), fd, &reporter);
		if (!client) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "CRITICAL: %s's new client could not be created, ignoring this client\n", process->name);
			report(&reporter, false);
			return (0);
		}
		epoll_ctl(efd, EPOLL_CTL_DEL, fd, &client->poll);
		// sending "old" log
		size_t stdout_logsize = 0;
		size_t stderr_logsize = 0;
		struct stat tmp;

		if (process->stdoutlog) {
			if (!fstat(process->stdout_logger.logfd, &tmp)) {
				stdout_logsize = (size_t)tmp.st_size;
				if (stdout_logsize > PIPE_BUF / 2) {
					stdout_logsize = PIPE_BUF / 2;
				}
			}
		}
		if (process->stderrlog) {
			if (!fstat(process->stderr_logger.logfd, &tmp)) {
				stderr_logsize = (size_t)tmp.st_size;
				if (stderr_logsize > PIPE_BUF / 2) {
					stderr_logsize = PIPE_BUF / 2;
				}
			}
		}
		if (stdout_logsize + stderr_logsize > 0) {
			client->log = (char *)calloc(sizeof(char), stdout_logsize + stderr_logsize);
			if (process->stdoutlog) {
				lseek(process->stdout_logger.logfd, -(int)stdout_logsize, SEEK_END);
				if (read(process->stdout_logger.logfd, client->log, stdout_logsize)) {}
			}
			if (process->stderrlog) {
				lseek(process->stderr_logger.logfd, -(int)stderr_logsize, SEEK_END);
				if (read(process->stderr_logger.logfd, client->log + strlen(client->log), stderr_logsize)) {}
			}
		} else {
			snprintf(client->buf, PIPE_BUF, "\n");
		}
		client->poll.events = EPOLLOUT | EPOLLIN;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &client->poll);
		// add fd to epoll and send it log
	} else if (!strncmp(buf, "tail", 4)) {
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has received order to tail\n", process->name);
		report(&reporter, false);
	}
	return (0);
}

void handle_logging_client(struct s_process *process, struct epoll_event event, int epollfd) {
	struct s_report reporter;
	reporter.report_fd = process->log;
	struct s_logging_client	*client = process->list;
	while (client && client->poll.data.fd != event.data.fd) {
		client = client->next;
	}
	if (!client) {
		return ;
	}
	if (event.events & EPOLLOUT) {
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: sending something to client %d\n", client->poll.data.fd);
		report(&reporter, false);
		if (strlen(client->buf)) {
			send(event.data.fd, client->buf, strlen(client->buf), 0);
			bzero(client->buf, PIPE_BUF + 1);
		} else {
			send(event.data.fd, client->log, strlen(client->log), 0);
			free(client->log);
			client->log = NULL;
		}
		client->poll.events = EPOLLIN;
		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll)) {
			snprintf(reporter.buffer, PIPE_BUF, "ERROR: Could not modify client event in epoll_ctl STARTING for client %d\n", client->poll.data.fd);
			report(&reporter, false);
		}
	} else if (event.events & EPOLLIN) {
		char buf[PIPE_BUF + 1];
		bzero(buf, PIPE_BUF + 1);
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s received data from client %d\n", process->name, client->poll.data.fd);
		report(&reporter, false);
		if (recv(client->poll.data.fd, buf, PIPE_BUF, MSG_DONTWAIT) <= 5) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: Client %d disconnected from %s\n", client->poll.data.fd, process->name);
			report(&reporter, false);
			epoll_ctl(epollfd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);

			client->poll.events = EPOLLIN;
			if (epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll) < 0) {
				snprintf(reporter.buffer, PIPE_BUF, "DEBUG: Could not add client %d back to main thread's epoll\n", client->poll.data.fd);
				report(&reporter, false);
			}
			if (client == process->list) {
				process->list = client->next;
				bzero(client->buf, PIPE_BUF + 1);
				free(client);
			} else {
				struct s_logging_client *head = process->list;
				while (head->next != client) {
					head = head->next;
				}
				head->next = client->next;
				bzero(client->buf, PIPE_BUF + 1);
				free(client);
			}
			return ;
		}
		char msg[PIPE_BUF / 2];
		bzero(msg, PIPE_BUF / 2);
		memcpy(msg, buf, PIPE_BUF / 2);
		msg[(PIPE_BUF / 2) - 1] = '\0';
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s received \"%s\" from client %d\n", process->name, msg, client->poll.data.fd);
		report(&reporter, false);
		snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s is sending \"%s\" to its process\n", process->name, msg + 5);
		report(&reporter, false);
		if (write(process->stdin[1], buf + 5, strlen(buf + 5)) < 0) {
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s could not write to its process\n", process->name);
			report(&reporter, false);
		}
	}
}

void send_clients(struct s_process *process, int epollfd, char *buf) {
	struct s_logging_client *tmp = process->list;
	char trunc[PIPE_BUF - 20 + 1];

	memcpy(trunc, buf, PIPE_BUF - 20 + 1);
	while (tmp) {
		snprintf(tmp->buf + strlen(tmp->buf), PIPE_BUF + 1 - strlen(tmp->buf), "%s", trunc);
		tmp->poll.events = EPOLLIN | EPOLLOUT;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, tmp->poll.data.fd, &(tmp->poll));
		tmp = tmp->next;
	}
}

void *administrator(void *arg) {
	struct s_process *process = (struct s_process *)arg;
	process->list = NULL;
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
	process->bool_start = false; // this bool serve for autostart and start signal sent by controller
	process->bool_exit = false;
	int					nfds = 0;
	struct epoll_event	events[3], in;
	int					epollfd = epoll_create(1);

	process->stop.tv_sec = 0;           // NEED TO SET W/ gettimeofday WHEN CHANGING STATUS TO STOPPING
	process->stop.tv_usec = 0;          //

	if (process->program->autostart) {
		process->bool_start = true;
	}

	in.events = EPOLLIN;
	in.data.fd = process->com[0];
	epoll_ctl(epollfd, EPOLL_CTL_ADD, in.data.fd, &in); // listen for main communication

	while (1) {
		if (process->status == EXITED || process->status == BACKOFF || (process->status != STOPPING && process->bool_start)) {
			if (should_start(process) || process->bool_start) {
				process->bool_start = false;
				snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STARTING\n", process->name);
				report(&reporter, false);
				snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s has raw cmd %s\n", process->name, process->program->command);
				report(&reporter, false);
				if (exec(process, epollfd)) {
					snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in exec\n", process->name);
					report(&reporter, false);
					closeall(process, epollfd);
					close(epollfd);
					if (process->stdout_logger.logfd > 0) {
						close(process->stdout_logger.logfd);
					}
					if (process->stderr_logger.logfd > 0) {
						close(process->stderr_logger.logfd);
					}
					return NULL;
				}
			} else {
				snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s will not instantly restart\n", process->name);
				report(&reporter, false);
			}
		}
		if (process->status == STARTING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				killpg(process->pid, SIGKILL);
				closeall(process, epollfd);
				close(epollfd);
				if (process->stdout_logger.logfd > 0) {
					close(process->stdout_logger.logfd);
				}
				if (process->stderr_logger.logfd > 0) {
					close(process->stderr_logger.logfd);
				}
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
							send_clients(process, epollfd, buf);
						} else {
							if (process->bool_exit) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s has STOPPED\n", process->name);
								report(&reporter, false);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							} else {
								snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
								report(&reporter, false);
								closeall(process, epollfd);
								process->status = BACKOFF;
								break ;
							}
						}
					}
					if (events[i].data.fd == process->stderr[0]) { // if process print in stderr
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stderr[0], buf, PIPE_BUF) > 0) {
							if (process->stderrlog) {
								write_process_log(&(process->stderr_logger), buf);
							}
							send_clients(process, epollfd, buf);
						} else {
							if (process->bool_exit) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s has STOPPED\n", process->name);
								report(&reporter, false);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							} else {
								snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s is now in BACKOFF\n", process->name);
								report(&reporter, false);
								closeall(process, epollfd);
								process->status = BACKOFF;
								break ;
							}
						}
					}
					if (events[i].data.fd == in.data.fd) {
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(in.data.fd, buf, PIPE_BUF) > 0) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator received something from main thread while STARTING\n", process->name);
							report(&reporter, false);
							if (handle_command(process, buf, epollfd)) {
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s's administrator could not read from main thread, exiting to avoid hanging\n", process->name);
							report(&reporter, false);
							process->status = STOPPING;
							if (gettimeofday(&process->stop, NULL)) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
								report(&reporter, false);
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
							process->bool_exit = true;
						}
					}
					if (process->list) {
						handle_logging_client(process, events[0], epollfd);
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

			if (nfds) {
				for (int i = 0; i < nfds; i++) {
				/* handles fds as needed */
					if (events[i].data.fd == process->stdout[0]) { // if process print in stdout
						bzero(buf, PIPE_BUF + 1);
						if (read(process->stdout[0], buf, PIPE_BUF) > 0) {
							if (process->stdoutlog) {
								write_process_log(&(process->stdout_logger), buf);
							}
							send_clients(process, epollfd, buf);
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
							send_clients(process, epollfd, buf);
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
							if (handle_command(process, buf, epollfd)) {
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s's administrator could not read from main thread, exiting to avoid hanging\n", process->name);
							report(&reporter, false);
							process->status = STOPPING;
							if (gettimeofday(&process->stop, NULL)) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
								report(&reporter, false);
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
							process->bool_exit = true;
						}
					}
					if (process->list) {
						handle_logging_client(process, events[i], epollfd);
					}
				}
			} else {
				snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s epoll_wait encountered an issue : \"%d\"", process->name, errno);
				report(&reporter, false);
			}
		} else if (process->status == STOPPING) {
			struct timeval tmp;
			if (gettimeofday(&tmp, NULL)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
				report(&reporter, false);
				killpg(process->pid, SIGKILL);
				closeall(process, epollfd);
				close(epollfd);
				if (process->stdout_logger.logfd > 0) {
					close(process->stdout_logger.logfd);
				}
				if (process->stderr_logger.logfd > 0) {
					close(process->stderr_logger.logfd);
				}
				return NULL;
			}

			long long stop_micro = ((process->stop.tv_sec * 1000) + (process->stop.tv_usec / 1000));
			long long tmp_micro = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000));

			if ((long long)(process->program->stopwaitsecs * 1000) <= (long long)(tmp_micro - stop_micro)) {
				snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s did not stop before %ds, sending SIGKILL\n", process->name, process->program->stopwaitsecs);
				report(&reporter, false);
				process->status = STOPPED;
				close(process->stdout[0]);
				close(process->stderr[0]);
				close(process->stdin[1]);
				process->stdout[0] = -1;
				process->stderr[0] = -1;
				process->stdin[1] = -1;
				killpg(process->pid, SIGKILL);
				// need to epoll_wait 0 to return instantly
				tmp_micro = (process->program->stopwaitsecs * 1000);
				stop_micro = 0;
			}
			snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s stopping epoll time to wait : %lld\n", process->name, ((long long)process->program->stopwaitsecs * 1000) - (tmp_micro - stop_micro));
			report(&reporter, false);
			nfds = epoll_wait(epollfd, events, 3, (int)((process->program->stopwaitsecs * 1000) - (tmp_micro - stop_micro)));
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
							send_clients(process, epollfd, buf);
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							report(&reporter, false);
							process->stop.tv_sec = 0;
							process->stop.tv_usec = 0;
							if (process->bool_exit) { // if need to exit administrator
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator exited\n", process->name);
								report(&reporter, false);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
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
							send_clients(process, epollfd, buf);
						} else {
							closeall(process, epollfd);
							process->status = STOPPED;
							snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
							report(&reporter, false);
							process->stop.tv_sec = 0;
							process->stop.tv_usec = 0;
							if (process->bool_exit) { // if need to exit administrator
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator exited\n", process->name);
								report(&reporter, false);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
							break ;
						}
					}
					if (events[i].data.fd == in.data.fd) {
						char buf[PIPE_BUF + 1];
						bzero(buf, PIPE_BUF + 1);
						if (read(in.data.fd, buf, PIPE_BUF) > 0) {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator received something from main thread while RUNNING\n", process->name);
							report(&reporter, false);
							if (handle_command(process, buf, epollfd)) {
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s's administrator could not read from main thread, exiting to avoid hanging\n", process->name);
							report(&reporter, false);
							process->status = STOPPING;
							if (gettimeofday(&process->stop, NULL)) {
								snprintf(reporter.buffer, PIPE_BUF - 22, "WARNING: %s got a fatal error in gettimeofday\n", process->name);
								report(&reporter, false);
								killpg(process->pid, SIGKILL);
								closeall(process, epollfd);
								close(epollfd);
								if (process->stdout_logger.logfd > 0) {
									close(process->stdout_logger.logfd);
								}
								if (process->stderr_logger.logfd > 0) {
									close(process->stderr_logger.logfd);
								}
								return NULL;
							}
							process->bool_exit = true;
						}
					}
					if (process->list) {
						handle_logging_client(process, events[i], epollfd);
					}
				}
			} else { // if timeout
				process->status = STOPPED;
				snprintf(reporter.buffer, PIPE_BUF - 22, "INFO: %s is now STOPPED\n", process->name);
				report(&reporter, false);
				process->stop.tv_sec = 0;
				process->stop.tv_usec = 0;
				closeall(process, epollfd);
				if (process->bool_exit) { // if need to exit administrator
					snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: %s's administrator exited\n", process->name);
					report(&reporter, false);
					close(epollfd);
					if (process->stdout_logger.logfd > 0) {
						close(process->stdout_logger.logfd);
					}
					if (process->stderr_logger.logfd > 0) {
						close(process->stderr_logger.logfd);
					}
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
						if (handle_command(process, buf, epollfd)) {
							close(epollfd);
							if (process->stdout_logger.logfd > 0) {
								close(process->stdout_logger.logfd);
							}
							if (process->stderr_logger.logfd > 0) {
								close(process->stderr_logger.logfd);
							}
							return NULL;
						}
					} else {
						snprintf(reporter.buffer, PIPE_BUF - 22, "ERROR: %s's administrator could not read from main thread, exiting to avoid hanging\n", process->name);
						report(&reporter, false);
						close(epollfd);
						if (process->stdout_logger.logfd > 0) {
							close(process->stdout_logger.logfd);
						}
						if (process->stderr_logger.logfd > 0) {
							close(process->stderr_logger.logfd);
						}
						return NULL;
					}
				}
				if (process->list) {
					handle_logging_client(process, events[i], epollfd);
				}
			}
		}
	}
	return NULL;
}
