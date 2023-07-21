/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/21 17:43:46 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#define SOCK_PATH "/tmp/taskmaster.sock"

void create_server(void) {
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

	struct epoll_event sock;
	bzero(&sock, sizeof(sock));
	sock.data.fd = fd;
	sock.events = EPOLLIN;
	int efd = epoll_create(1);
	epoll_ctl(efd, EPOLL_CTL_ADD, fd, &sock);
	char  buf[4096];

	struct epoll_event client;
	bzero(&client, sizeof(client));
	struct epoll_event tmp;
	printf("ENTERING WHILE\n");
	while (1) {
		bzero(&tmp, sizeof(tmp));
		if (epoll_wait(efd, &tmp, 10, 0) > 0) {
			if (tmp.data.fd == fd) {
				printf("NEW CONNECTION\n");
				client.data.fd = accept(fd, NULL, NULL);
				client.events = EPOLLIN;
				epoll_ctl(efd, EPOLL_CTL_ADD, client.data.fd, &client);
			} else if (tmp.data.fd == client.data.fd) {
				bzero(buf, 4096);
				if (read(fd, buf, 4095)) {}
				printf("GOT >%s<\n", buf);
			}
		}
/*		bzero(buf, 4096);
		if (read(0, buf, 4095) <= 0) {
			break;
		}
		if (write(fd, buf, strlen(buf))) {}
		printf("SENT >%s<\n", buf);
		*/
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

	create_server();
	yaml_get_version(&major, &minor, &patch);
	printf("Yaml version: %d.%d.%d", major, minor, patch);
	return (0);
}
