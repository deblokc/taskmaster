/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   daemon.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/08/08 13:29:40 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/09 15:58:57 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>

int	daemonize(struct s_server *server)
{
	int				fd;
	pid_t			pid;
	struct s_report	reporter;

	reporter.report_fd = server->log_pipe[1];
	if (!end_logging_thread(&reporter, server->logging_thread))
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL : Could not terminate logging thread\n");
		write_log(&server->logger, reporter.buffer);
		if (write(2, reporter.buffer, strlen(reporter.buffer))){};
		unlink(server->socket.socketpath);
		return (1);
	}
	pid = fork();
	if (pid == -1)
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL : Could not daemonize process, exiting process\n");
		write_log(&server->logger, reporter.buffer);
		if (write(2, reporter.buffer, strlen(reporter.buffer))){};
		unlink(server->socket.socketpath);
		return (1);
	}
	if (pid == 0)
	{
		if (server->loglevel == DEBUG)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "DEBUG    : First fork successful\n");
			write_log(&server->logger, reporter.buffer);
		}
		close(server->log_pipe[0]);
		server->log_pipe[0] = -1;
		pid = setsid();
		if (pid == -1)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL : Could not create a new session\n");
			report(&reporter, true);
			unlink(server->socket.socketpath);
			return (1);
		}
		pid = fork();
		if (pid == -1)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL : Could not start daemon\n");
			report(&reporter, true);
			unlink(server->socket.socketpath);
			return (1);
		}
		if (pid)
		{
			if (server->loglevel == DEBUG)
			{
				get_stamp(reporter.buffer);
				strcpy(&reporter.buffer[22], "DEBUG    : Second fork successful\n");
				write_log(&server->logger, reporter.buffer);
			}
			return (0);
		}
		else
		{
			fd = open("/dev/null", O_RDWR);
			if (fd == -1)
			{
				get_stamp(reporter.buffer);
				strcpy(&reporter.buffer[22], "ERROR    : Could not open /dev/null\n");
				write_log(&server->logger, reporter.buffer);
				close(0);
				close(1);
				close(2);
			}
			else
			{
				dup2(fd, 0);
				dup2(fd, 1);
				dup2(fd, 2);
				close(fd);
			}
			server->pid = getpid();;
			get_stamp(reporter.buffer);
			snprintf(&reporter.buffer[22], PIPE_BUF - 22, "INFO     : Daemon successfully started with PID %d\n", server->pid);
			write_log(&server->logger, reporter.buffer);
			report(&reporter, false);
			close(server->log_pipe[1]);
			server->log_pipe[1] = -1;
			if (!start_logging_thread(server, true))
			{
				unlink(server->socket.socketpath);
				return (1);
			}
			return (-1);
		}
	}
	else
	{
		int					nb_events;
		int					epoll_fd;
		ssize_t				ret;
		struct epoll_event	event;

		bzero(&event, sizeof(event));
		epoll_fd = epoll_create(1);
		if (epoll_fd == -1)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL : Could not instantiate epoll to watch daemon, exiting process\n");
			write_log(&server->logger, reporter.buffer);
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
			return (1);
		}
		close(server->log_pipe[1]);
		server->log_pipe[1] = -1;
		event.data.fd = server->log_pipe[0];
		event.events = EPOLLIN;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server->log_pipe[0], &event))
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL : Could not add pipe to epoll events to watch daemon, exiting process\n");
			write_log(&server->logger, reporter.buffer);
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
			close(epoll_fd);
			return (1);
		}
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "DEBUG    : Ready for logging daemon\n");
		if (server->loglevel == DEBUG)
		{
			write_log(&server->logger, reporter.buffer);
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
		}
		while (true)
		{
			if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
			{
				if (errno == EINTR)
					continue ;
				get_stamp(reporter.buffer);
				strcpy(&reporter.buffer[22], "CRITICAL : Error while waiting for epoll event, exiting process\n");
				write_log(&server->logger, reporter.buffer);
				if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
				event.data.fd = server->log_pipe[0];
				event.events = EPOLLIN;
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
				close(epoll_fd);
				return (1);
			}
			if (nb_events)
			{
				if (!(event.events & EPOLLIN))
				{
					printf("Epollin is: %d, received: %d\n", EPOLLIN, event.events);
					get_stamp(reporter.buffer);
					strcpy(&reporter.buffer[22], "CRITICAL : Error on pipe reporting from daemon\n");
					write_log(&server->logger, reporter.buffer);
					if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
					event.data.fd = server->log_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
					close(epoll_fd);
					return (1);
				}
				ret = read(server->log_pipe[0], reporter.buffer, PIPE_BUF);
				if (ret > 0)
				{
					reporter.buffer[ret] = '\0';
					event.data.fd = server->log_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
					close(epoll_fd);
					if (write(2, reporter.buffer, (size_t)ret)) {}
					return (0);
				}
			}
		}
	}
}
