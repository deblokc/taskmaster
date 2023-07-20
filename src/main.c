/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/17 19:49:44 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/epoll.h>

void	*initial_log(void *fds)
{
	int					*reporter_pipe;	
	int					nb_events;
	int					epoll_fd;
	ssize_t				ret;
	char				buffer[PIPE_BUF + 1];
	struct epoll_event	event;

	reporter_pipe = (int *)fds;
	epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		if (write(2, "CRITICAL: Could not instantiate epoll, exiting process\n", strlen("CRITICAL: Could not instantiate epoll, exiting process\n")) == -1) {};
		reporter_pipe[3] = 1;
		pthread_exit(&(reporter_pipe[3]));
	}
	event.data.fd = reporter_pipe[0];
	event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, reporter_pipe[0], &event))
	{
		if (write(2, "CRITICAL: Could not add pipe to epoll events, exiting process\n", strlen("CRITICAL: Could not add pipe to epoll events, exiting process\n")) == -1) {};
		close(epoll_fd);
		reporter_pipe[3] = 1;
		pthread_exit(&(reporter_pipe[3]));
	}
	while (true)
	{
		if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
		{
			if (write(2, "CRITICAL: Error while waiting for epoll event, exiting process\n", strlen("CRITICAL: Error while waiting for epoll event, exiting process\n")) == -1) {};
			event.data.fd = reporter_pipe[0];
			event.events = EPOLLIN;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, reporter_pipe[0], &event);
			close(epoll_fd);
			reporter_pipe[3] = 1;
			pthread_exit(&(reporter_pipe[3]));
		}
		if (nb_events)
		{
			ret = read(reporter_pipe[0], buffer, PIPE_BUF);
			if (ret > 0)
			{
				buffer[ret] = '\0';
				if (!strncmp("LOG", buffer, 3))
				{
					if (write(2, "########## Received signal to log all ##########\n", strlen("########## Received signal to log all ##########\n"))) {}
					event.data.fd = reporter_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, reporter_pipe[0], &event);
					close(epoll_fd);
					reporter_pipe[3] = 0;
					pthread_exit(&(reporter_pipe[3]));
				}
				else
				{
					if (write(reporter_pipe[2], buffer, (size_t)ret)) {}
				}
			}
		}
	}
	return (NULL);
}

int main(int ac, char **av) {
	void				*thread_ret;
	int					ret = 0;
	int					reporter_pipe[4];
	pthread_t			initial_logger;
	struct s_report		reporter;
	struct s_server*	server = NULL;
	struct s_priority*	priorities = NULL;

	if (ac != 2)
	{
		if (write(2, "Usage: ", strlen("Usage: ")) == -1
				|| write(2, av[0], strlen(av[0])) == -1
				|| write(2, " CONFIGURATION-FILE\n", strlen(" CONFIGURATION-FILE\n")) == -1) {}
		ret = 1;
	}
	else if (pipe2(reporter_pipe, O_DIRECT | O_NONBLOCK) == -1)
	{
		ret = 1;
	}
	else
	{
		reporter.critical = false;
		reporter.report_fd = reporter_pipe[1];
		bzero(reporter.buffer, PIPE_BUF + 1);
		bzero(reporter.stamp, 22);
		reporter_pipe[2] = open("/tmp/.taskmasterd_tmp_log", O_CREAT | O_TRUNC | O_RDWR);
		if (reporter_pipe[2] < 3)
		{
			if (write(2, "CRITICAL: could not create temporary log file, exiting process\n", strlen("CRITICAL: could not create temporary log file, exiting process\n"))) {}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			return (1);
		}
		if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
		{
			if (write(2, "CRITICAL, could not create initial log thread, exiting process\n", strlen("CRITICAL, could not create initial log thread, exiting process\n"))) {}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			unlink("/tmp/.taskmasterd_tmp_log");
			return (1);
		}
		server = parse_config(av[1], &reporter);
		if (write(reporter_pipe[0], "LOG\n", 4))
		{
			if (write(2, "CRITICAL: Log error\n", strlen("CRITICAL: Log error\n"))) {}
			pthread_join(initial_logger, &thread_ret);
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			return (1);
		}
		if (pthread_join(initial_logger, &thread_ret))
		{
			if (write(2, "CRITICAL: Could not join logging thread\n", strlen("CRITICAL: Could not join logging thread\n"))) {}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			return (1);
		}
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		close(reporter_pipe[2]);
		if (!server || reporter.critical || *(int *)thread_ret == 1)
		{
			if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) == -1) {}
			ret = 1;
		}
		else
		{
			printf("We have a server\n");
			server->print_tree(server);
			priorities = create_priorities(server);
			if (!priorities)
				printf("No priorities\n");
			else
			{
				printf("Got priorities\n");
				priorities->print_priorities(priorities);
				printf("=-=-=-=-=-=-=-=-= LAUNCHING PRIORITIES =-=-=-=-=-=-=-=-=\n");
				launch(priorities);
				// todo
				printf("=-=-=-=-=-=-=-=-= WAITING PRIORITIES =-=-=-=-=-=-=-=-=\n");
				wait_priorities(priorities);
				priorities->destructor(priorities);
			}
			server = server->cleaner(server);
		}
	}
	return (ret);
}
