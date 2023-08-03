/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/26 18:34:56 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SOCK_PATH "/tmp/taskmaster.sock"

struct s_client {
	struct epoll_event	poll;
	char				cmd[PIPE_BUF + 1];
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
		bzero(tmp->next->cmd, PIPE_BUF + 1);
		return (tmp->next);
	} else {
		struct s_client *new = (struct s_client *)calloc(sizeof(struct s_client), 1);
		new->poll.data.fd = client_fd;
		bzero(new->cmd, PIPE_BUF + 1);
		return (new);
	}
}

struct s_command *process_line(char *line) {
	char *ret = strtok(line, " \t");
	if (ret) {
		struct s_command *cmd = (struct s_command *)calloc(sizeof(struct s_command), 1);
		cmd->cmd = ret;
		int i = 0;
		while (ret) {
			ret = strtok(NULL, " \t");
			if (ret) {
				if (!cmd->arg) {
					cmd->arg = (char **)calloc(sizeof(char *), 2);
					cmd->arg[0] = ret;
					cmd->arg[1] = NULL;
				} else {
					i++;
					cmd->arg = (char **)realloc(cmd->arg, (sizeof(char *) * (i + 1)));
					cmd->arg[i - 1] = ret;
					cmd->arg[i] = NULL;
				}
			}
		}
		return cmd;
	} else {
		return NULL;
	}
}

struct s_command {
	char	*cmd;
	char	*arg;
};

void check_server(int sock_fd, int efd) {
	static struct s_client	*list = NULL;
	char					buf[PIPE_BUF + 1];
	struct epoll_event		tmp;
	char *					cmd;

	bzero(&tmp, sizeof(tmp));
	bzero(&cmd, sizeof(cmd));
	if (epoll_wait(efd, &tmp, 1, 0) > 0) { // for now only check one event every time, might change
		if (tmp.data.fd == sock_fd) {
			printf("NEW CONNECTION\n");
			struct s_client *client = new_client(list, accept(sock_fd, NULL, NULL));
			client->poll.events = EPOLLIN;// | EPOLLOUT;
			epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll);
			if (!list) {
				list = client;
			}
		} else {
			printf("GOT SMTH\n");
			struct s_client *client = list;
			while (client) {
				if (tmp.data.fd == client->poll.data.fd) {
					printf("client %d\n", tmp.data.fd);
					if (tmp.events & EPOLLIN) {
						bzero(buf, PIPE_BUF + 1);
						if (recv(client->poll.data.fd, buf, PIPE_BUF, MSG_DONTWAIT) <= 0) {
							epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
							if (client == list) {
								list = client->next;
								free(client);
							} else {
								struct s_client *head = list;
								while (head->next != client) {
									head = head->next;
								}
								head->next = client->next;
								free(client);
							}
							break ;
						}
						printf(">%s<\n", buf);
						bzero(client->cmd, PIPE_BUF + 1);
						memcpy(client->cmd, buf, strlen(buf));

						client->poll.events = EPOLLOUT;
						epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll);
						// need to execute some action and probably send data back
						struct s_command *cmd = process_line(buf);
						if (!cmd) {
						}
					} else if (tmp.events & EPOLLOUT) {
						printf("SENDING DATA\n");
						send(tmp.data.fd, "this is a response", strlen("this is a response"), 0);
						client->poll.events = EPOLLIN;
						epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll);
					}
					return ;
				}
				client = client->next;
			}
		}
	} else {
	}
}
