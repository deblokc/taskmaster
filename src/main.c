/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/26 18:34:56 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>
#include <stdio.h>
#include "taskmaster.h"
#include <limits.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/epoll.h>

int g_sig = 0;

void handle(int sig) {
	(void)sig;
	g_sig = 1;
}

int main(int ac, char **av) {
	int					ret = 0;
	struct s_server*	server = NULL;
	struct s_priority*	priorities = NULL;

	if (ac != 2)
	{
		if (write(2, "Usage: ./taskmaster CONFIGURATION-FILE\n",
					strlen("Usage: ./taskmaster CONFIGURATION-FILE\n")) == -1) {}
		ret = 1;
	}
	else
	{
		signal(SIGINT, &handle);
		server = parse_config(av[1]);
		if (!server)
		{
			printf("Could not make server from configuration file\n");
			ret = 1;
		}
		else
		{
			printf("We have a server\n");
			server->print_tree(server);


			int sock_fd = create_server();
			struct epoll_event sock;
			bzero(&sock, sizeof(sock));
			sock.data.fd = sock_fd;
			sock.events = EPOLLIN;
			int efd = epoll_create(1);
			epoll_ctl(efd, EPOLL_CTL_ADD, sock_fd, &sock);




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
				while (!g_sig) {
					check_server(sock_fd, efd);
				}

				printf("=-=-=-=-=-=-=-=-= WAITING PRIORITIES =-=-=-=-=-=-=-=-=\n");
				wait_priorities(priorities);
				priorities->destructor(priorities);
			}
			server = server->cleaner(server);
		}
	}
	return (ret);
}
