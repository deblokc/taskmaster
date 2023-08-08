/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/08 15:36:26 by tnaton           ###   ########.fr       */
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

struct s_command {
	char	*cmd;
	char	**arg;
};

struct s_client {
	struct epoll_event	poll;
	char				buf[PIPE_BUF + 1];
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
		bzero(tmp->next->buf, PIPE_BUF + 1);
		return (tmp->next);
	} else {
		struct s_client *new = (struct s_client *)calloc(sizeof(struct s_client), 1);
		new->poll.data.fd = client_fd;
		bzero(new->buf, PIPE_BUF + 1);
		return (new);
	}
}

struct s_command *process_line(char *line) {
	char *ret = strtok(line, " \t");
	if (ret) {
		struct s_command *cmd = (struct s_command *)calloc(sizeof(struct s_command), 1);
		cmd->cmd = ret;
		size_t i = 0;
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

void check_server(int sock_fd, int efd, struct s_server *serv) {
	static struct s_client	*list = NULL;
	char					buf[PIPE_BUF + 1];
	struct epoll_event		tmp;
	char					*cmd;

	(void)serv;
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
			struct s_report reporter;
			reporter.report_fd = serv->log_pipe[1];

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
						client->poll.events = EPOLLOUT;
						epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll);
						struct s_command *cmd = process_line(buf);
						if (!cmd) {
							char *fatal_error = "Fatal Error in process_line\n";
							memcpy(client->buf, fatal_error, strlen(fatal_error));
						} else {
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller send command %s", cmd->cmd);
							for (int i = 0; cmd->arg[i]; i++) {
								strcat(reporter.buffer, " ");
								strcat(reporter.buffer, cmd->arg[i]);
							}
							strcat(reporter.buffer, "\n");
							report(&reporter, false);
							if (!strcmp(cmd->cmd, "maintail")) {      //send via logging thread
							} else if (!strcmp(cmd->cmd, "signal")) {  //administrator send signal
							} else if (!strcmp(cmd->cmd, "stop")) {    //administrator stop process
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent stop command\n");
								report(&reporter, false);
								if (cmd->arg) {
									if (!strcmp(cmd->arg[0], "all")) {
										// send stop to every process
									} else {
										for (int i = 0; cmd->arg[i]; i++) {
											for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
												if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
													for (int j = 0; j < current->numprocs; j++) {
														// add check
														if (write(current->processes[j].com[1], "stop", strlen("stop"))) {}
													}
												}
											}
										}
									}
								}
							} else if (!strcmp(cmd->cmd, "avail")) {   //main thread return available process
							} else if (!strcmp(cmd->cmd, "fg")) {      //administrator send logging and stdin
							} else if (!strcmp(cmd->cmd, "reload")) {  //restart daemon
							} else if (!strcmp(cmd->cmd, "restart")) { //administrator stop then start process
							} else if (!strcmp(cmd->cmd, "start")) {   //administrator start process
							} else if (!strcmp(cmd->cmd, "tail")) {    //administrator send logging
							} else if (!strcmp(cmd->cmd, "clear")) {   //administrator clear logging
							} else if (!strcmp(cmd->cmd, "pid")) {     //main thread send pid of process
							} else if (!strcmp(cmd->cmd, "shutdown")) {//main thread stop all process & exit
							} else if (!strcmp(cmd->cmd, "status")) {  //administrator send status
							} else if (!strcmp(cmd->cmd, "update")) {  //reparse config file
							} else {
								char *unknown_cmd = "Unknown command\n";
								memcpy(client->buf, unknown_cmd, strlen(unknown_cmd));
							}
							// execute cmd
						}
					} else if (tmp.events & EPOLLOUT) {
						// send response located in client->buf
						printf("SENDING DATA\n");
						send(tmp.data.fd, client->buf, strlen(client->buf), 0);
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
