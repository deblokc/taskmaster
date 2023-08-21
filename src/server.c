/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/21 18:30:55 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SOCK_PATH "/tmp/taskmaster.sock"

volatile _Atomic int	efd = 0;

struct s_command {
	char	*cmd;
	char	**arg;
};

bool	init_epoll(struct s_server *server, struct s_report *reporter)
{
	struct epoll_event	sock;

	bzero(&sock, sizeof(sock));
	sock.data.fd = server->socket.sockfd;
	sock.events = EPOLLIN;
	efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not init epoll for socket connection, exiting process\n");
		report(reporter, true);
		return (false);
	}
	if (epoll_ctl(efd, EPOLL_CTL_ADD, server->socket.sockfd, &sock) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not add socket to epoll instance, exiting process\n");
		report(reporter, true);
		return (false);
	}
	return (true);
}

void	create_socket(struct s_server *server, struct s_report *reporter) {
	uid_t				user;
	gid_t				group;
	struct group		*gid = NULL;
	struct passwd		*uid = NULL;
	struct sockaddr_un	addr;
	
	if (!server->socket.socketpath)
	{
		server->socket.socketpath = strdup("/tmp/taskmasterd.sock");
		if (!server->socket.socketpath)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate string for socketpath, exiting process\n");
			report(reporter, true);
			return ;
		}
	}
	server->socket.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server->socket.sockfd < 0) {
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not open socket, exiting process\n");
		report(reporter, true);
		return ;
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, server->socket.socketpath);
	if (bind(server->socket.sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not create socket, exiting process\n");
		report(reporter, true);
		return ;
	}
	if (chmod(server->socket.socketpath, 0666 & ~server->socket.umask) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not change rights of socket, exiting process\n");
		report(reporter, true);
		unlink(server->socket.socketpath);
		return ;
	}
	if (server->socket.uid)
	{
		errno = 0;
		uid = getpwnam(server->socket.uid);
		if (!uid)
		{
			if (errno == 0 || errno == ENOENT || errno == ESRCH || errno == EBADF || errno == EPERM)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Unkown user %s, cannot change ownership of socket, exiting process\n", server->socket.uid);
				report(reporter, true);
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not get information on user %s, cannot change ownership of socket, exiting process\n", server->socket.uid);
				report(reporter, true);
			}
			unlink(server->socket.socketpath);
			return ;
		}
		user = uid->pw_uid;
		group = uid->pw_gid;
		if (server->socket.gid)
		{
			gid = getgrnam(server->socket.gid);
			if (!gid)
			{
				if (errno == 0 || errno == ENOENT || errno == ESRCH || errno == EBADF || errno == EPERM)
				{
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Unkown group %s, cannot change ownership of socket, exiting process\n", server->socket.gid);
					report(reporter, true);
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not get information on group %s, cannot change ownership of socket, exiting process\n", server->socket.gid);
					report(reporter, true);
				}
				unlink(server->socket.socketpath);
				return ;
			}
			group = gid->gr_gid;
		}
		if (chown(server->socket.socketpath, user, group) == -1)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not change ownership of socket to %s:%s, exiting process\n", server->socket.uid, server->socket.gid);
			report(reporter, true);
			unlink(server->socket.socketpath);
			return ;
		}
		snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Successfully changed ownership of socket to %s:%s\n", server->socket.uid, server->socket.gid);
		report(reporter, false);

	}
	listen(server->socket.sockfd, 10);
}

struct s_client *new_client(struct s_client **list, int client_fd, struct s_report *reporter) {
	struct s_client	*runner;
	struct s_client	*new_client;

	new_client = calloc(1, sizeof(struct s_client));
	if (!new_client)
	{
		strcpy(reporter->buffer, "CRITICAL: Could not calloc new client, connection will be ignored\n");
		report(reporter, false);
		close(client_fd);
		return (NULL);
	}
	new_client->poll.data.fd = client_fd;
	bzero(new_client->buf, PIPE_BUF + 1);
	if (*list) {
		runner = *list;
		while (runner->next) {
			runner = runner->next;
		}
		runner->next = new_client;
	} else {
		*list = new_client;
	}
	return (new_client);
}

void	delete_clients(struct s_client **clients_lst)
{
	struct s_client	*client;
	struct s_client *next;

	client = *clients_lst;
	while (client)
	{
		next = client->next;
		if (client->poll.data.fd > 0)
			close(client->poll.data.fd);
		free(client);
		client = (next);
	}
	*clients_lst = NULL;
}

void exit_admins(struct s_priority *priorities) {
	struct s_priority	*current;
	struct s_program	*prog;

	current = priorities;
	while (current)
	{
		prog = current->begin;
		while (prog) {
			if (prog->processes)
			{
				for (int i = 0; i < prog->numprocs; i++) {
					if (write(prog->processes[i].com[1], "exit", strlen("exit"))) {}
				}
			}
			prog = prog->next;
		}
		current = current->next;
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
					i++;
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

void send_command_multiproc(struct s_command *cmd, struct s_server *serv) {
	if (!strcmp(cmd->arg[0], "all")) {
		for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
			for (int i = 0; i < current->numprocs; i++) {
				if (write(current->processes[i].com[1], cmd->cmd, strlen(cmd->cmd))) {}
			}
		}
	} else {
		for (int i = 0; cmd->arg[i]; i++) {
			for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
				if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
					for (int j = 0; j < current->numprocs; j++) {
						if (!strcmp(current->processes[j].name, cmd->arg[i])) {
							if (write(current->processes[j].com[1], cmd->cmd, strlen(cmd->cmd))) {}
						}
					}
				}
			}
		}
	}
}

void check_server(struct s_server *server, struct epoll_event *events, int nb_events, struct s_client **clients_lst, struct s_report *reporter)
{
	struct s_client		*client;
	int					client_socket;
	char				buf[PIPE_BUF + 1];
	struct s_command	*cmd;

	for (int i = 0; i < nb_events; ++i)
	{
		if (events[i].data.fd == server->socket.sockfd) {
			strcpy(reporter->buffer, "INFO: Received new connection on socket\n");
			report(reporter, false);
			if ((client_socket = accept(server->socket.sockfd, NULL, NULL)) != -1)
			{
				client = new_client(clients_lst, client_socket, reporter);
				if (!client)
					continue ;
				client->poll.events = EPOLLIN;// | EPOLLOUT;
				if (epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll) == -1)
				{
					strcpy(reporter->buffer, "CRITICAL: Could not add new client to epoll list\n");
					report(reporter, false);
					close(client->poll.data.fd);
					client->poll.data.fd = -1;
				}
			}
		} else {
			client = *clients_lst;
			while (client && events[i].data.fd != client->poll.data.fd) {
				client = client->next;
			}
			if (!client)
				continue ;
			if (events[i].events & EPOLLIN) {
				snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Received data from client with socket fd %d\n", client->poll.data.fd);
				report(reporter, false);
				bzero(buf, PIPE_BUF + 1);
				if (recv(client->poll.data.fd, buf, PIPE_BUF, MSG_DONTWAIT) <= 0) {
					snprintf(reporter->buffer, PIPE_BUF, "INFO: Client disconnected\n");
					report(reporter, false);
					epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
					if (client == *clients_lst) {
						*clients_lst = client->next;
						close(client->poll.data.fd);
						free(client);
					} else {
						struct s_client *head = *clients_lst;
						while (head->next != client) {
							head = head->next;
						}
						head->next = client->next;
						close(client->poll.data.fd);
						free(client);
					}
					continue ;
				}
				snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Data received from client with socket fd %d\n", client->poll.data.fd);
				report(reporter, false);
				client->poll.events = EPOLLOUT;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll) == -1)
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR: Could not modify client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
				}
				cmd = process_line(buf);
				if (!cmd) {
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Fatal error while parsing command received from client with socket fd %d\n", client->poll.data.fd);
					memcpy(client->buf, reporter->buffer, strlen(reporter->buffer));
					report(reporter, false);
				} else {
					snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: client with socket fd %d sent command \"%s", client->poll.data.fd, cmd->cmd);
					if (cmd->arg) {
						for (int i = 0; cmd->arg[i]; i++) {
							snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "\" \"%s", cmd->arg[i]);
						}
						snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "\"\n");
					} else {
						snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "\"\n");
					}
					report(reporter, false);
					if (!strcmp(cmd->cmd, "maintail")) {      //send via logging thread
					} else if (!strcmp(cmd->cmd, "signal")) {  //administrator send signal
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent signal command\n");
						report(reporter, false);
						if (cmd->arg) {
							char msg[4] = "sig ";
							if (!strcmp(cmd->arg[0], "SIGHUP")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGHUP\n");
								report(reporter, false);
								msg[3] = SIGHUP;
							} else if (!strcmp(cmd->arg[0], "SIGINT")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGINT\n");
								report(reporter, false);
								msg[3] = SIGINT;
							} else if (!strcmp(cmd->arg[0], "SIGQUIT")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGQUIT\n");
								report(reporter, false);
								msg[3] = SIGQUIT;
							} else if (!strcmp(cmd->arg[0], "SIGKILL")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGKILL\n");
								report(reporter, false);
								msg[3] = SIGKILL;
							} else if (!strcmp(cmd->arg[0], "SIGUSR1")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGUSR1\n");
								report(reporter, false);
								msg[3] = SIGUSR1;
							} else if (!strcmp(cmd->arg[0], "SIGUSR2")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGUSR2\n");
								report(reporter, false);
								msg[3] = SIGUSR2;
							} else if (!strcmp(cmd->arg[0], "SIGTERM")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGTERM\n");
								report(reporter, false);
								msg[3] = SIGTERM;
							} else {
								msg[0] = '\0';
							}
							if (msg[0]) {
								if (!strcmp(cmd->arg[1], "all")) {
									for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
										for (int i = 0; i < current->numprocs; i++) {
											if (write(current->processes[i].com[1], msg, strlen(msg))) {}
										}
									}
								} else {
									for (int i = 1; cmd->arg[i]; i++) {
										for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
											if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
												for (int j = 0; j < current->numprocs; j++) {
													if (!strcmp(current->processes[j].name, cmd->arg[i])) {
														if (write(current->processes[j].com[1], msg, strlen(msg))) {}
													}
												}
											}
										}
									}
								}
							}
						}
					} else if (!strcmp(cmd->cmd, "stop")) {    //administrator stop process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent stop command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
						}
					} else if (!strcmp(cmd->cmd, "avail")) {   //main thread return available process
					} else if (!strcmp(cmd->cmd, "fg")) {      //administrator send logging and stdin
					} else if (!strcmp(cmd->cmd, "reload")) {
						if (cmd->arg) {
							free(cmd->arg);
						}
						free(cmd);
						g_sig = SIGHUP;
						return ;
					} else if (!strcmp(cmd->cmd, "restart")) { //administrator stop then start process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent start command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
						}
					} else if (!strcmp(cmd->cmd, "start")) {   //administrator start process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent start command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
						}
					} else if (!strcmp(cmd->cmd, "tail")) {    //administrator send logging
					} else if (!strcmp(cmd->cmd, "clear")) {   //administrator clear logging
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent clear command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
						}
					} else if (!strcmp(cmd->cmd, "pid")) {     //main thread send pid of process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG: controller has sent pid command\n");
						report(reporter, false);
						if (cmd->arg) {
							if (!strcmp(cmd->arg[0], "all")) {
								for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
									for (int i = 0; i < current->numprocs; i++) {
										snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %d\n", current->processes[i].name, current->processes[i].pid);
									}
								}
							} else {
								for (int i = 0; cmd->arg[i]; i++) {
									for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
										if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
											for (int j = 0; j < current->numprocs; j++) {
												if (!strcmp(cmd->arg[i], current->processes[j].name)) {
													snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %d\n", current->processes[j].name, current->processes[j].pid);
												}
											}
										}
									}
								}
							}
						} else {
							snprintf(client->buf, PIPE_BUF + 1, "%d\n", getpid());
						}
					} else if (!strcmp(cmd->cmd, "shutdown")) {//main thread stop all process & exit
						if (cmd->arg) {
							free(cmd->arg);
						}
						free(cmd);
						g_sig = SIGTERM;
						return ;
					} else if (!strcmp(cmd->cmd, "status")) {  //administrator send status
						char *status[7] = {"STOPPED", "STARTING", "RUNNING", "BACKOFF", "STOPPING", "EXITED", "FATAL"};
						if (cmd->arg) {
							for (int i = 0; cmd->arg[i]; i++) {
								for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
									if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
										if (!current->processes)
											continue ;
										for (int j = 0; j < current->numprocs; j++) {
											if (!strcmp(cmd->arg[i], current->processes[j].name)) {
												snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %s\n", current->processes[j].name, status[current->processes[j].status]);
											}
										}
									}
								}
							}
						} else {
							for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
								if (!current->processes)
									continue ;
								for (int i = 0; i < current->numprocs; i++) {
									snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %s\n", current->processes[i].name, status[current->processes[i].status]);
								}
							}
						}
					} else if (!strcmp(cmd->cmd, "update")) {
						if (cmd->arg) {
							free(cmd->arg);
						}
						free(cmd);
						g_sig = -1;
						return ;
					} else {
						char *unknown_cmd = "Unknown command\n";
						memcpy(client->buf, unknown_cmd, strlen(unknown_cmd));
					}
					if (cmd->arg) {
						free(cmd->arg);
					}
					free(cmd);
					// execute cmd
				}
			} else if (events[i].events & EPOLLOUT) {
				// send response located in client->buf
				if (strlen(client->buf)) {
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Sending to client with socket fd %d", client->poll.data.fd);
					report(reporter, false);
					send(events[i].data.fd, client->buf, strlen(client->buf), 0);
					bzero(client->buf, PIPE_BUF + 1);
				} else {
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Sending to client with socket fd %d NON ZERO RESPONSE", client->poll.data.fd);
					report(reporter, false);
					send(events[i].data.fd, ".\n", 2, 0);
				}
				client->poll.events = EPOLLIN;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll))
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR: Could not modify client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "INFO: Client disconnected");
				report(reporter, false);
				epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
				if (client == *clients_lst) {
					*clients_lst = client->next;
					close(client->poll.data.fd);
					free(client);
				} else {
					struct s_client *head = *clients_lst;
					while (head->next != client) {
						head = head->next;
					}
					head->next = client->next;
					close(client->poll.data.fd);
					free(client);
				}
			}
		}
	}
}
