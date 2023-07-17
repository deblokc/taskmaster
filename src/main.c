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

void	print_log(int fd)
{
	ssize_t	ret;
	char	buffer[PIPE_BUF];

	while ((ret = read(fd, buffer, PIPE_BUF)) > 0)
	{
		if (write(2, buffer, (size_t)ret) == -1)
			break ;
	}
}

int main(int ac, char **av) {
	int					ret = 0;
	int					reporter_pipe[2];
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
		server = parse_config(av[1], &reporter);
		print_log(reporter_pipe[0]);
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		if (!server || reporter.critical)
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
