/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/10 15:31:50 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>


void	create_pid_file(struct s_server *server, struct s_report *reporter)
{
	int	fd;
	
	if (!server->pidfile)
		fd = open("taskmasterd.pid", O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0666 & ~server->umask);
	else
		fd = open(server->pidfile, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0666 & ~server->umask);
	if (fd  < 0)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not create pid file, exiting process\n");
		report(reporter, true);
		return ;
	}
	snprintf(reporter->buffer, PIPE_BUF, "%d\n", server->pid);
	if (write(fd, reporter->buffer, strlen(reporter->buffer)) == -1)
	{
		if (server->pidfile)
			unlink(server->pidfile);
		else
			unlink("taskmasterd.pid");
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not write pid to file, exiting process\n");
		report(reporter, true);
	}
	close(fd);
}

bool end_initial_log(struct s_server *server, struct s_report *reporter, int *reporter_pipe, void **thread_ret, pthread_t initial_logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
	{
		if (write(2, "CRITICAL: Log error\n", strlen("CRITICAL: Log error\n")) <= 0)
		{
		}
		if (server)
			server = server->cleaner(server);
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		close(reporter_pipe[2]);
		return (false);
	}
	if (pthread_join(initial_logger, thread_ret))
	{
		if (write(2, "CRITICAL: Could not join logging thread\n", strlen("CRITICAL: Could not join logging thread\n")) <= 0)
		{
		}
		if (server)
			server = server->cleaner(server);
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		close(reporter_pipe[2]);
		return (false);
	}
	return (true);
}

bool end_logging_thread(struct s_report *reporter, pthread_t logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
		return (false);
	if (pthread_join(logger, NULL))
		return (false);
	return (true);
}

static void	cleanup(struct s_server *server, struct s_priority *priorities, bool remove_pidfile, struct s_report *reporter)
{
	if (!end_logging_thread(reporter, server->logging_thread))
	{
		if (!server->daemon && write(2, "Could not terminate logging thread\n", strlen("Could not terminate logging thread\n")))
		{
		}
	}
	if (priorities)
		priorities->destructor(priorities);
	if (remove_pidfile)
	{
		if (!server->pidfile)
			unlink("taskmasterd.pid");
		else
			unlink(server->pidfile);
	}
	if (server->socket.enable && server->socket.socketpath)
		unlink(server->socket.socketpath);
	server->cleaner(server);
}


int main(int ac, char **av)
{
	void *thread_ret;
	int ret = 0;
	int reporter_pipe[4];
	pthread_t initial_logger;
	struct s_report reporter;
	struct s_server *server = NULL;
	struct s_priority *priorities = NULL;

	if (ac != 2)
	{
		if (write(2, "Usage: ", strlen("Usage: ")) == -1 || write(2, av[0], strlen(av[0])) == -1 || write(2, " CONFIGURATION-FILE\n", strlen(" CONFIGURATION-FILE\n")) == -1)
		{
		}
		ret = 1;
	}
	else if (pipe2(reporter_pipe, O_DIRECT | O_NONBLOCK) == -1)
	{
		if (write(2, "CRITICAL: Could not create pipe\n", strlen("CRITICAL: Could not create pipe\n"))) {}
		ret = 1;
	}
	else
	{
		reporter.critical = false;
		reporter.report_fd = reporter_pipe[1];
		bzero(reporter.buffer, PIPE_BUF + 1);
		bzero(reporter.stamp, 22);
		reporter_pipe[2] = open("/tmp/.taskmasterd_tmp.log", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
		if (reporter_pipe[2] < 3)
		{
			if (write(2, "CRITICAL: could not create temporary log file, exiting process\n", strlen("CRITICAL: could not create temporary log file, exiting process\n")))
			{
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			return (1);
		}
		if (!block_signals(&reporter))
		{
			if (write(2, "CRITICAL: could not block signals, exiting process\n", strlen("CRITICAL, could not block signals, exiting process\n")))
			{
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			unlink("/tmp/.taskmasterd_tmp.log");
			return (1);
		}
		if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
		{
			if (write(2, "CRITICAL: could not create initial log thread, exiting process\n", strlen("CRITICAL, could not create initial log thread, exiting process\n")))
			{
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			unlink("/tmp/.taskmasterd_tmp.log");
			return (1);
		}
		server = parse_config(av[0], av[1], &reporter);
		if (!server || reporter.critical)
		{
			if (!end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger))
			{
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				return (1);
			}
			if (*(int *)thread_ret != 1)
				report_critical(reporter_pipe[2]);
			if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
			{
			}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			ret = 1;
		}
		else
		{
			if (server->socket.enable)
			{
				create_server(server, &reporter);
				if (reporter.critical)
				{
					if (end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
						report_critical(reporter_pipe[2]);
					if (server)
							server = server->cleaner(server);
					close(reporter_pipe[0]);
					close(reporter_pipe[1]);
					close(reporter_pipe[2]);
					return (1);
				}
			}
			prelude(server, &reporter);
			if (reporter.critical)
			{
				if (end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
					report_critical(reporter_pipe[2]);
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				if (server)
					server = server->cleaner(server);
				close(reporter_pipe[0]);
				close(reporter_pipe[1]);
				close(reporter_pipe[2]);
				return (1);
			}
			if (!end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger))
			{
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				return (1);
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			transfer_logs(reporter_pipe[2], server);
			if (!start_logging_thread(server, false))
			{
				server = server->cleaner(server);
				return (1);
			}
			reporter.report_fd = server->log_pipe[1];
			priorities = create_priorities(server, &reporter);
			if (reporter.critical)
			{
				strcpy(reporter.buffer, "CRITICAL: Could not build priorities, exiting taskmasterd\n");
				report(&reporter, true);
				if (!end_logging_thread(&reporter, server->logging_thread))
				{
					if (write(2, "Could not terminate logging thread\n", strlen("Could not terminate logging thread\n")))
					{
					}
				}	
				server = server->cleaner(server);
				return (1);
			}
			if (server->daemon)
			{
				ret = daemonize(server);
				if (ret != -1)
				{
					if (priorities)
						priorities->destructor(priorities);
					server->cleaner(server);
					exit(ret);
				}
			}
			else
			{
				server->pid = getpid();
			}
			reporter.report_fd = server->log_pipe[1];
			create_pid_file(server, &reporter);
			if (reporter.critical)
			{
				cleanup(server, priorities, false, &reporter);
				return (1);
			}
			strcpy(reporter.buffer, "INFO: Starting taskmasterd\n");
			report(&reporter, false);
			if (!priorities)
			{
				strcpy(reporter.buffer, "DEBUG: No priorities to start\n");
				report(&reporter, false);
			}
			else
			{
				strcpy(reporter.buffer, "DEBUG: Launching priorities\n");
				report(&reporter, false);
				launch(priorities, server->log_pipe[1]);
			}
			if (server->socket.enable)
			{
				struct epoll_event sock;
				int efd;
				bzero(&sock, sizeof(sock));
				sock.data.fd = server->socket.sockfd;
				sock.events = EPOLLIN;
				efd = epoll_create(1);
				epoll_ctl(efd, EPOLL_CTL_ADD, server->socket.sockfd, &sock);
				while (!g_sig)
				{
					check_server(server->socket.sockfd, efd, server);
				}
				exit_admins(server);
			}
			else
			{
				while (!g_sig)
				{
				}
			}
			if (priorities)
			{
				strcpy(reporter.buffer, "DEBUG: Waiting priorities\n");
				report(&reporter, false);
				wait_priorities(priorities);
			}
			cleanup(server, priorities, true, &reporter);
		}
	}
	return (ret);
}
