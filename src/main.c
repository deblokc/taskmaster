/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/10 17:38:04 by bdetune          ###   ########.fr       */
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

int	early_error(char* msg, int* reporter_pipe, char* tmp_log, struct s_server *server)
{
	if (write(2, msg, strlen(msg))) {}
	if (reporter_pipe[0] > 0)
		close(reporter_pipe[0]);
	if (reporter_pipe[1] > 0)
		close(reporter_pipe[1]);
	if (reporter_pipe[2] > 0)
		close(reporter_pipe[2]);
	if (tmp_log)
		unlink(tmp_log);
	if (server)
		server->cleaner(server);
	return (1);
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

	bzero(reporter_pipe, sizeof(int) * 4);
	bzero(reporter.buffer, PIPE_BUF + 1);
	reporter.critical = false;
	if (ac != 2)
	{
		snprintf(reporter.buffer, PIPE_BUF, "Usage: %s CONFIGURATION-FILE\n", av[0]);
		return (early_error(reporter.buffer, reporter_pipe, NULL, NULL));
	}
	if (pipe2(reporter_pipe, O_DIRECT | O_NONBLOCK) == -1)
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: Could not create pipe, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, NULL, NULL));
	}
	reporter.report_fd = reporter_pipe[1];
	reporter_pipe[2] = open("/tmp/.taskmasterd_tmp.log", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
	if (reporter_pipe[2] < 0)
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not create temporary log file, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, NULL, NULL));
	}
	if (!block_signals(&reporter))
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: Could not block signals, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", NULL));
	}
	if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not create initial log thread, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", NULL));
	}
	server = parse_config(av[0], av[1], &reporter);
	if (!server || reporter.critical)
	{
		if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
			report_critical(reporter_pipe[2]);
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", server));
	}
	else
	{
		if (server->socket.enable)
		{
			create_server(server, &reporter);
			if (reporter.critical)
			{
				if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
					report_critical(reporter_pipe[2]);
				get_stamp(reporter.buffer);
				snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
				return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", server));
			}
		}
		prelude(server, &reporter);
		if (reporter.critical)
		{
			if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
				report_critical(reporter_pipe[2]);
			get_stamp(reporter.buffer);
			snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
			return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", server));
		}
		if (!end_initial_log(&reporter, &thread_ret, initial_logger))
			return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", server));
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		reporter_pipe[0] = 0;
		reporter_pipe[1] = 0;
		if (!transfer_logs(reporter_pipe[2], server, &reporter))
			return (early_error(reporter.buffer, reporter_pipe, "/tmp/.taskmasterd_tmp.log", server));
		reporter_pipe[2] = 0;
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
				if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
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
	return (ret);
}
