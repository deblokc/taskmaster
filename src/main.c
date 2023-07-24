/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/24 19:21:58 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#define SOCK_PATH "/tmp/taskmaster.sock"

struct s_client {
	struct epoll_event	poll;
	struct s_client		*next;
};

int create_server(void) {
	int fd;
	struct sockaddr_un addr;
	
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	unlink(SOCK_PATH);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
	}
	listen(fd, 99);
	return (fd);
}

struct s_client *new_client(struct s_client *list, int client_fd) {
	if (list) {
		struct s_client *tmp = list;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = (struct s_client *)calloc(sizeof(struct s_client), 1);
		tmp->next->poll.data.fd = client_fd;
		return (tmp->next);
	} else {
		struct s_client *new = (struct s_client *)calloc(sizeof(struct s_client), 1);
		new->poll.data.fd = client_fd;
		return (new);
	}
}

void check_server(int sock_fd, int efd) {
	static struct s_client *list = NULL;
	char				buf[4096];
	struct epoll_event tmp;

	bzero(&tmp, sizeof(tmp));
	if (epoll_wait(efd, &tmp, 1, 0) > 0) { // for now only check one event every time, might change
		if (tmp.data.fd == sock_fd) {
			printf("NEW CONNECTION\n");
			struct s_client *client = new_client(list, accept(sock_fd, NULL, NULL));
			client->poll.events = EPOLLIN | EPOLLOUT;
			epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll);
			if (!list) {
				list = client;
			}
		} else {
			struct s_client *client = list;
			while (client) {
				if (tmp.data.fd == client->poll.data.fd) {
					if (tmp.events & EPOLLIN) {
						bzero(buf, 4096);
						if (read(client->poll.data.fd, buf, 4095)) {}
						printf("GOT >%s<\n", buf);
						// need to execute some action and probably send data back
					} else if (tmp.events & EPOLLOUT) {
						if (0) { // if need to send data back
							printf("SENDING DATA\n");
							send(tmp.data.fd, buf, strlen(buf), 0);
						}
					}
					return ;
				}
				client = client->next;
			}
		}
	}
}

int main(int ac, char **av) {
	if (ac != 2)
	{
		if (write(2, "Usage: ./taskmaster CONFIGURATION-FILE", strlen("Usage: ./taskmaster CONFIGURATION-FILE"))) {}
		return 0;
	}
	(void)ac;
	(void)av;
	int	major, minor, patch;

	int sock_fd = create_server();

	struct epoll_event sock;
	bzero(&sock, sizeof(sock));
	sock.data.fd = sock_fd;
	sock.events = EPOLLIN;
	int efd = epoll_create(1);
	epoll_ctl(efd, EPOLL_CTL_ADD, sock_fd, &sock);

	while (1) {
		check_server(sock_fd, efd);
	}
	yaml_get_version(&major, &minor, &patch);
	printf("Yaml version: %d.%d.%d", major, minor, patch);
	return (0);
}
