/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   report.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/11 19:41:17 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/25 18:18:32 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>


char	*get_stamp(char* stamp_str)
{
	time_t stamp;
	struct tm *local_stamp;

	time(&stamp);
	local_stamp = localtime(&stamp);
	strftime(stamp_str, 23, "[%Y-%m-%d %H:%M:%S] ", local_stamp);
	return (stamp_str);
}

bool	report(struct s_report* reporter, bool critical)
{
	bool	success = false;

	reporter->critical = critical;
	for (int i = 0; i < 100; ++i)
	{
		if (write(reporter->report_fd, reporter->buffer, strlen(reporter->buffer)) != -1) {
			success = true;
			break ;
		}
		usleep(500);
	}
	if (!success)
		perror("LOG ERROR");
	return (success);
}

void	report_critical(int fd, int report_fd)
{
	char*	line = NULL;

	if (lseek(fd, 0, SEEK_SET) == -1)
		return ;
	while ((line = get_next_line(fd)) != NULL)
	{
		if (strlen(line) > 22)
		{
			if (!strncmp(&line[22], "CRITICAL", 8))
			{
				if (report_fd == 2)
				{
					if (write(report_fd, line, strlen(line))) {}
				}
				else
				{
					if (write(report_fd, &line[22], strlen(&line[22]))) {}
				}
			}
		}
		free(line);
	}
}

void	*initial_log(void *fds)
{
	char				main_buffer[PIPE_BUF + 1];
	size_t				current_index;
	int					*reporter_pipe;	
	int					nb_events;
	int					epoll_fd;
	ssize_t				ret;
	char				buffer[PIPE_BUF + 1];
	struct epoll_event	event;

	reporter_pipe = (int *)fds;
	bzero(&event, sizeof(event));
	bzero(buffer, PIPE_BUF + 1);
	current_index = 0;
	epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		if (write(2, "CRITICAL : Could not instantiate epoll, exiting process\n", strlen("CRITICAL : Could not instantiate epoll, exiting process\n")) == -1) {};
		reporter_pipe[3] = 1;
		pthread_exit(&(reporter_pipe[3]));
	}
	event.data.fd = reporter_pipe[0];
	event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, reporter_pipe[0], &event))
	{
		if (write(2, "CRITICAL : Could not add pipe to epoll events, exiting process\n", strlen("CRITICAL : Could not add pipe to epoll events, exiting process\n")) == -1) {};
		close(epoll_fd);
		reporter_pipe[3] = 1;
		pthread_exit(&(reporter_pipe[3]));
	}
	while (true)
	{
		if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
		{
			if (errno == EINTR)
				continue ;
			if (write(2, "CRITICAL : Error while waiting for epoll event, exiting process\n", strlen("CRITICAL : Error while waiting for epoll event, exiting process\n")) == -1) {};
			event.data.fd = reporter_pipe[0];
			event.events = EPOLLIN;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, reporter_pipe[0], &event);
			close(epoll_fd);
			reporter_pipe[3] = 1;
			pthread_exit(&(reporter_pipe[3]));
		}
		if (nb_events)
		{

			ret = read(reporter_pipe[0], &main_buffer[current_index], PIPE_BUF - current_index);
			if (ret <= 0)
				continue;
			main_buffer[(size_t)ret + current_index] = '\0';
			current_index = 0;
			while (next_string(main_buffer, buffer, &current_index, true))
			{
				if (!strncmp("ENDLOG\n", &buffer[22], 7))
				{
					event.data.fd = reporter_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, reporter_pipe[0], &event);
					close(epoll_fd);
					reporter_pipe[3] = 0;
					pthread_exit(&(reporter_pipe[3]));
				}
				else
				{
					if (write(reporter_pipe[2], buffer, strlen(buffer))) {}
				}
			}
		}
	}
	reporter_pipe[3] = 1;
	pthread_exit(&(reporter_pipe[3]));
	return (NULL);
}
