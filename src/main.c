/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/25 19:15:46 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

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
		reporter_pipe[2] = open("/tmp/.taskmasterd_tmp.log", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
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
			unlink("/tmp/.taskmasterd_tmp.log");
			return (1);
		}
		server = parse_config(av[1], &reporter);
		strcpy(reporter.buffer, "ENDLOG\n");
		if (!report(&reporter, false))
		{
			if (write(2, "CRITICAL: Log error\n", strlen("CRITICAL: Log error\n")) <= 0) {}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			return (1);
		}
		if (pthread_join(initial_logger, &thread_ret))
		{
			if (write(2, "CRITICAL: Could not join logging thread\n", strlen("CRITICAL: Could not join logging thread\n")) <= 0) {}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			return (1);
		}
		if (!server || reporter.critical || *(int *)thread_ret == 1)
		{
			if (*(int *)thread_ret != 1)
			{
				report_critical(reporter_pipe[2]);
			}
			if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0) {}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			ret = 1;
		}
		else
		{
			//prelude(server);
			//	server->print_tree(server);
			priorities = create_priorities(server, &reporter);
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
