/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/09/20 20:56:56 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */
#include "taskmaster.h"
#include <fcntl.h>
#include <limits.h>
#include <libscrypt.h>
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
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not init epoll for socket connection, exiting process\n");
		report(reporter, true);
		return (false);
	}
	if (epoll_ctl(efd, EPOLL_CTL_ADD, server->socket.sockfd, &sock) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not add socket to epoll instance, exiting process\n");
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
		server->socket.socketpath = strdup(SOCK_PATH);
		if (!server->socket.socketpath)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate string for socketpath, exiting process\n");
			report(reporter, true);
			return ;
		}
	}
	server->socket.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server->socket.sockfd < 0) {
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not open socket, exiting process\n");
		report(reporter, true);
		return ;
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, server->socket.socketpath);
	if (bind(server->socket.sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not create socket, exiting process\n");
		report(reporter, true);
		close(server->socket.sockfd);
		server->socket.sockfd = -1;
		return ;
	}
	if (chmod(server->socket.socketpath, 0666 & ~server->socket.umask) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not change rights of socket, exiting process\n");
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
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Unkown user %s, cannot change ownership of socket, exiting process\n", server->socket.uid);
				report(reporter, true);
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not get information on user %s, cannot change ownership of socket, exiting process\n", server->socket.uid);
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
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Unkown group %s, cannot change ownership of socket, exiting process\n", server->socket.gid);
					report(reporter, true);
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not get information on group %s, cannot change ownership of socket, exiting process\n", server->socket.gid);
					report(reporter, true);
				}
				unlink(server->socket.socketpath);
				return ;
			}
			group = gid->gr_gid;
		}
		if (chown(server->socket.socketpath, user, group) == -1)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not change ownership of socket to %s:%s, exiting process\n", server->socket.uid, server->socket.gid);
			report(reporter, true);
			unlink(server->socket.socketpath);
			return ;
		}
		snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Successfully changed ownership of socket to %s:%s\n", server->socket.uid, server->socket.gid);
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
		strcpy(reporter->buffer, "CRITICAL : Could not calloc new client, connection will be ignored\n");
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

void	delete_clients(struct s_client **clients_lst, struct s_report *reporter)
{
	(void)reporter;
	struct s_client	*client;
	struct s_client *next;

	client = *clients_lst;
	while (client)
	{
		next = client->next;
		if (client->poll.data.fd > 0)
			close(client->poll.data.fd);
		if (client->log)
			free(client->log);
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

int auth(struct s_client *client, char *buf, struct s_server *server, struct s_report *reporter) {
	if (buf) {
		char *token = strtok(buf, "\n");
		if (strcmp(token, server->socket.user)) {
			snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Wrong username\n");
			report(reporter, false);
			return (1);
		}
		token = strtok(NULL, "\n");
		char dup[SCRYPT_MCF_LEN + 1];
		bzero(dup, SCRYPT_MCF_LEN + 1);
		memcpy(dup, server->socket.password, SCRYPT_MCF_LEN);
		if (libscrypt_check(dup, token) < 1) {
			snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Wrong password\n");
			report(reporter, false);
			return (1);
		}
		client->auth = true;
	}
	return (0);
}

void check_server(struct s_server *server, struct epoll_event *events, int nb_events, struct s_client **clients_lst, struct s_report *reporter)
{
	struct s_client		*client;
	int					client_socket;
	char				buf[PIPE_BUF + 1];
	struct s_command	*cmd;

	for (int event_i = 0; event_i < nb_events; ++event_i)
	{
		if (events[event_i].data.fd == server->socket.sockfd) {
			strcpy(reporter->buffer, "INFO     : Received new connection on socket\n");
			report(reporter, false);
			if ((client_socket = accept(server->socket.sockfd, NULL, NULL)) != -1)
			{
				client = new_client(clients_lst, client_socket, reporter);
				if (!client)
					continue ;
				client->poll.events = EPOLLOUT;
				if (epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll) == -1)
				{
					strcpy(reporter->buffer, "CRITICAL : Could not add new client to epoll list\n");
					report(reporter, false);
					close(client->poll.data.fd);
					client->poll.data.fd = -1;
				}
				if (server->socket.password) {
					client->auth = false;
					snprintf(client->buf, PIPE_BUF, "need");
				} else {
					client->auth = true;
					snprintf(client->buf, PIPE_BUF, "okay");
				}
			}
		} else {
			client = *clients_lst;
			while (client && events[event_i].data.fd != client->poll.data.fd) {
				client = client->next;
			}
			if (!client)
				continue ;
			if (events[event_i].events & EPOLLIN) {
				snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Received data from client with socket fd %d\n", client->poll.data.fd);
				report(reporter, false);
				bzero(buf, PIPE_BUF + 1);
				if ((recv(client->poll.data.fd, buf, PIPE_BUF, MSG_DONTWAIT) <= 0)) {
					snprintf(reporter->buffer, PIPE_BUF, "INFO     : Client disconnected\n");
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
				client->poll.events = EPOLLOUT;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll) == -1)
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR    : Could not modify client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
				}
				snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Data received from client with socket fd %d\n", client->poll.data.fd);
				report(reporter, false);
				if (client->auth) {
					cmd = process_line(buf);
				} else {
					char trunc[PIPE_BUF / 4];
					bzero(trunc, PIPE_BUF / 4);
					memcpy(trunc, buf, strlen(buf));
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Client %d wants to authentificate with >%s<\n", client->poll.data.fd, trunc);
					report(reporter, false);
					if (auth(client, buf, server, reporter)) {
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Client %d was denied access\n", client->poll.data.fd);
						report(reporter, false);
						snprintf(client->buf, PIPE_BUF, "nope");
					} else {
						snprintf(client->buf, PIPE_BUF, "okay");
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Client %d succesfully logged in\n", client->poll.data.fd);
						report(reporter, false);
					}
					continue ;
				}
				if (!cmd) {
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Fatal error while parsing command received from client with socket fd %d\n", client->poll.data.fd);
					memcpy(client->buf, reporter->buffer, strlen(reporter->buffer));
					report(reporter, false);
				} else {
					snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : client with socket fd %d sent command '%s", client->poll.data.fd, cmd->cmd);
					if (cmd->arg) {
						for (int i = 0; cmd->arg[i]; i++) {
							snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "' '%s", cmd->arg[i]);
						}
						snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "'\n");
					} else {
						snprintf(reporter->buffer + strlen(reporter->buffer), PIPE_BUF - 22, "'\n");
					}
					report(reporter, false);
					if (!strcmp(cmd->cmd, "maintail")) {      //send via logging thread
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent maintail command\n");
						report(reporter, false);
						size_t size = 0;
						if (cmd->arg) {
							if (!strcmp(cmd->arg[0], "-f")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : starting infinitemaintailling\n");
								report(reporter, false);
								snprintf(reporter->buffer, PIPE_BUF - 22, "maintail %d\n", client->poll.data.fd);
								report(reporter, false);
								if (epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll) == -1)
								{
									snprintf(reporter->buffer, PIPE_BUF, "ERROR    : Could not remove client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
									report(reporter, false);
								}
								// register client
							} else {
								size = (size_t)atoi(cmd->arg[0] + 1);
								if (size >= INT_MAX) {
									snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : maintail size is 0 or below\n");
									report(reporter, false);
									snprintf(client->buf, PIPE_BUF + 1, "invalid maintail size\n");
								}
							}
						} else {
							size = 1600;
						}
						if (size > 0 && size < INT_MAX) {
							snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : sending client %d request for maintail of %d size\n", client->poll.data.fd, (int)size);
							report(reporter, false);
							snprintf(reporter->buffer, PIPE_BUF - 22, "getlog %d %d\n", (int)size, client->poll.data.fd);
							report(reporter, false);
							if (epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll) == -1)
							{
								snprintf(reporter->buffer, PIPE_BUF, "ERROR    : Could not remove client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
								report(reporter, false);
							}
						}
					} else if (!strcmp(cmd->cmd, "signal")) {  //administrator send signal
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent signal command\n");
						report(reporter, false);
						if (cmd->arg) {
							char msg[5] = "sig ";
							if (!strcmp(cmd->arg[0], "SIGHUP")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGHUP\n");
								report(reporter, false);
								msg[3] = SIGHUP;
							} else if (!strcmp(cmd->arg[0], "SIGINT")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGINT\n");
								report(reporter, false);
								msg[3] = SIGINT;
							} else if (!strcmp(cmd->arg[0], "SIGQUIT")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGQUIT\n");
								report(reporter, false);
								msg[3] = SIGQUIT;
							} else if (!strcmp(cmd->arg[0], "SIGKILL")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGKILL\n");
								report(reporter, false);
								msg[3] = SIGKILL;
							} else if (!strcmp(cmd->arg[0], "SIGUSR1")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGUSR1\n");
								report(reporter, false);
								msg[3] = SIGUSR1;
							} else if (!strcmp(cmd->arg[0], "SIGUSR2")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGUSR2\n");
								report(reporter, false);
								msg[3] = SIGUSR2;
							} else if (!strcmp(cmd->arg[0], "SIGTERM")) {
								snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : signal to be send is SIGTERM\n");
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
								snprintf(client->buf, PIPE_BUF, "signal %s sent to", cmd->arg[0]);
								int i = 1;
								while (cmd->arg[i]) {
									snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), " %s", cmd->arg[i]);
									i++;
								}
								snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), "\n");
							}
						}
					} else if (!strcmp(cmd->cmd, "stop")) {    //administrator stop process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent stop command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
							snprintf(client->buf, PIPE_BUF, "stop request sent to");
							int i = 0;
							while (cmd->arg[i]) {
								snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), " %s", cmd->arg[i]);
								i++;
							}
							snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), "\n");
						}
					} else if (!strcmp(cmd->cmd, "fg")) {      //administrator send logging and stdin
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent fg command\n");
						report(reporter, false);
						if (cmd->arg) {
							for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
								if (!strncmp(current->name, cmd->arg[0], strlen(current->name))) {
									if (!current->processes)
										continue ;
									for (int i = 0; i < current->numprocs; i++) {
										if (!strcmp(current->processes[i].name, cmd->arg[0])) {
											bzero(buf, PIPE_BUF + 1);
											snprintf(buf, PIPE_BUF + 1, "fg %d", client->poll.data.fd);
											if (write(current->processes[i].com[1], buf, strlen(buf))) {}
											epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
											break ;
										}
									}
									break ;
								}
							}
						}
					} else if (!strcmp(cmd->cmd, "reload")) {
						snprintf(client->buf, PIPE_BUF, "reloading\n");
						if (cmd->arg) {
							free(cmd->arg);
						}
						free(cmd);
						g_sig = SIGHUP;
						return ;
					} else if (!strcmp(cmd->cmd, "restart")) { //administrator stop then start process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent start command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
							snprintf(client->buf, PIPE_BUF, "restart request sent to");
							int i = 0;
							while (cmd->arg[i]) {
								snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), " %s", cmd->arg[i]);
								i++;
							}
							snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), "\n");
						}
					} else if (!strcmp(cmd->cmd, "start")) {   //administrator start process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent start command\n");
						report(reporter, false);
						if (cmd->arg) {
							send_command_multiproc(cmd, server);
							snprintf(client->buf, PIPE_BUF, "start request sent to");
							int i = 0;
							while (cmd->arg[i]) {
								snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), " %s", cmd->arg[i]);
								i++;
							}
							snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), "\n");
	
						}
					} else if (!strcmp(cmd->cmd, "tail")) {    //administrator send logging
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent tail command\n");
						report(reporter, false);
						if (cmd->arg) {
							char newbuf[PIPE_BUF + 1];
							bzero(newbuf, PIPE_BUF + 1);
							char *name = NULL;
							if (cmd->arg[0][0] == '-') { // if specified size
								if (!strcmp(cmd->arg[0], "-f")) {
									// infinite tail
									if (cmd->arg[1]) {
										name = cmd->arg[1];
										if (cmd->arg[2]) { // if specified output
											if (!strcmp(cmd->arg[2], "stdout")) {
												// send to cmd->arg[1] infinite bytes of stdout
												snprintf(newbuf, PIPE_BUF + 1, "tail f 1 %d", client->poll.data.fd);
											} else if (!strcmp(cmd->arg[2], "stderr")) {
												// send to cmd->arg[1] infinite bytes of stderr
												snprintf(newbuf, PIPE_BUF + 1, "tail f 2 %d", client->poll.data.fd);
											}
										} else { // default output is stdout
											//send to cmd->arg[1] infinite bytes of stdout
											snprintf(newbuf, PIPE_BUF + 1, "tail f 1 %d", client->poll.data.fd);
										}
									} else {
										// no program to send to, not doing it
									}
								} else {
									int val = atoi(cmd->arg[0] + 1);
									// if val is positiv/non-null
									if (val > 0 && val < INT_MAX) {
										if (cmd->arg[1]) {
											name = cmd->arg[1];
											if (cmd->arg[2]) { // if specified output
												if (!strcmp(cmd->arg[2], "stdout")) {
													// send to cmd->arg[1] n bytes of stdout
													snprintf(newbuf, PIPE_BUF + 1, "tail %s 1 %d", cmd->arg[0] + 1, client->poll.data.fd);
												} else if (!strcmp(cmd->arg[2], "stderr")) {
													// send to cmd->arg[1] n bytes of stderr
													snprintf(newbuf, PIPE_BUF + 1, "tail %s 2 %d", cmd->arg[0] + 1, client->poll.data.fd);
												}
											} else { // default output is stdout
												//send to cmd->arg[1] n bytes of stdout
												snprintf(newbuf, PIPE_BUF + 1, "tail %s 1 %d", cmd->arg[0] + 1, client->poll.data.fd);
											}
										} else {
											// no program to send to, not doing it
										}
									}
								}
							} else { // if no size default 1600 bytes
								name = cmd->arg[0];
								if (cmd->arg[1]) {
									if (!strcmp(cmd->arg[1], "stdout")) {
										// send to cmd->arg[0] 1600 bytes of stdout
										snprintf(newbuf, PIPE_BUF + 1, "tail 1600 1 %d", client->poll.data.fd);
									} else if (!strcmp(cmd->arg[1], "stderr")) {
										// send to cmd->arg[0] 1600 bytes of stderr
										snprintf(newbuf, PIPE_BUF + 1, "tail 1600 2 %d", client->poll.data.fd);
									}
								} else { // default output is stdout
									// send to cmd->arg[0] 1600 bytes of stdout
									snprintf(newbuf, PIPE_BUF + 1, "tail 1600 1 %d", client->poll.data.fd);
								}
							}
							if (!name)
								return ;
							for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
								if (!strncmp(current->name, name, strlen(current->name))) {
									if (!current->processes)
										continue ;
									for (int i = 0; i < current->numprocs; i++) {
										if (!strcmp(current->processes[i].name, name)) {
											if (write(current->processes[i].com[1], newbuf, strlen(newbuf))) {}

											epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
											break ;
										}
									}
									break ;
								}
							}
						}
					} else if (!strcmp(cmd->cmd, "pid")) {     //main thread send pid of process
						snprintf(reporter->buffer, PIPE_BUF - 22, "DEBUG    : controller has sent pid command\n");
						report(reporter, false);
						if (cmd->arg) {
							if (!strcmp(cmd->arg[0], "all")) {
								for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
									if (!current->processes)
										continue ;
									for (int i = 0; i < current->numprocs; i++) {
										snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%-15s : %d\n", current->processes[i].name, current->processes[i].pid);
									}
								}
							} else {
								for (int i = 0; cmd->arg[i]; i++) {
									for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
										if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
											if (!current->processes)
												continue ;
											for (int j = 0; j < current->numprocs; j++) {
												if (!strcmp(cmd->arg[i], current->processes[j].name)) {
													snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%-15s : %d\n", current->processes[j].name, current->processes[j].pid);
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
										struct timeval now;
										gettimeofday(&now, NULL);
										for (int j = 0; j < current->numprocs; j++) {
											if (!strcmp(cmd->arg[i], current->processes[j].name)) {
												int hour = 0;
												int minute = 0;
												int second = 0;
												int stat = current->processes[j].status;
												if (stat == STARTING || stat == RUNNING || stat == STOPPING) {
													long start = current->processes[j].sec_start;
													hour = (int)(now.tv_sec- start) / 3600;
													minute = (int)((now.tv_sec - start) / 60) % 60;
													second = (int)(now.tv_sec - start) % 60;
												}
												snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%-15s : %-8s\tpid : %-5d uptime : %02d:%02d:%02d\n", current->processes[j].name, status[current->processes[j].status], current->processes[j].pid, hour, minute, second);
	
											}
										}
									}
								}
							}
						} else {
							for (struct s_program *current = server->begin(server); current; current = current->itnext(current)) {
								if (!current->processes)
									continue ;
								struct timeval now;
								gettimeofday(&now, NULL);
								for (int i = 0; i < current->numprocs; i++) {
									int hour = 0;
									int minute = 0;
									int second = 0;
									int stat = current->processes[i].status;
									if (stat == STARTING || stat == RUNNING || stat == STOPPING) {
										long start = current->processes[i].sec_start;
										hour = (int)(now.tv_sec- start) / 3600;
										minute = (int)((now.tv_sec - start) / 60) % 60;
										second = (int)(now.tv_sec - start) % 60;
									}
									snprintf(client->buf + strlen(client->buf), PIPE_BUF - strlen(client->buf), "%-15s : %-8s\tpid : %d\tuptime : %02d:%02d:%02d\n", current->processes[i].name, status[current->processes[i].status], current->processes[i].pid, hour, minute, second);
								}
							}
						}
					} else if (!strcmp(cmd->cmd, "update")) {
						snprintf(client->buf, PIPE_BUF, "updating\n");
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
			} else if (events[event_i].events & EPOLLOUT) {
				// send response located in client->buf
				if (strlen(client->buf)) {
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Sending to client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
					send(events[event_i].data.fd, client->buf, strlen(client->buf), 0);
					bzero(client->buf, PIPE_BUF + 1);
				} else if (client->log) {
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Sending log to client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
					send(events[event_i].data.fd, client->log, strlen(client->log), 0);
					free(client->log);
					client->log = NULL;
				} else {
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Sending to client with socket fd %d NON ZERO RESPONSE\n", client->poll.data.fd);
					report(reporter, false);
					send(events[event_i].data.fd, ".\n", 2, 0);
				}
				client->poll.events = EPOLLIN;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll))
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR    : Could not modify client events in epoll list for client with socket fd %d\n", client->poll.data.fd);
					report(reporter, false);
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "INFO     : Client disconnected");
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
