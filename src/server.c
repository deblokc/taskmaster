/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/09 18:38:30 by tnaton           ###   ########.fr       */
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



struct s_command {
	char	*cmd;
	char	**arg;
};

struct s_client {
	struct epoll_event	poll;
	char				buf[PIPE_BUF + 1];
	struct s_client		*next;
};

void create_server(struct s_server *server, struct s_report *reporter) {
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

void exit_admins(struct s_server *serv) {
	for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
		for (int i = 0; i < current->numprocs; i++) {
			if (write(current->processes[i].com[1], "exit", strlen("exit"))) {}
		}
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
								close(client->poll.data.fd);
								free(client);
							} else {
								struct s_client *head = list;
								while (head->next != client) {
									head = head->next;
								}
								head->next = client->next;
								close(client->poll.data.fd);
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
							snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller send command \"%s", cmd->cmd);
							if (cmd->arg) {
								for (int i = 0; cmd->arg[i]; i++) {
									snprintf(reporter.buffer + strlen(reporter.buffer), PIPE_BUF - 22, "\" \"%s", cmd->arg[i]);
								}
								snprintf(reporter.buffer + strlen(reporter.buffer), PIPE_BUF - 22, "\"\n");
							} else {
								snprintf(reporter.buffer + strlen(reporter.buffer), PIPE_BUF - 22, "\"\n");
							}
							report(&reporter, false);
							if (!strcmp(cmd->cmd, "maintail")) {      //send via logging thread
							} else if (!strcmp(cmd->cmd, "signal")) {  //administrator send signal
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent signal command\n");
								report(&reporter, false);
								if (cmd->arg) {
									char msg[4] = "sig ";
									if (!strcmp(cmd->arg[0], "SIGHUP")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGHUP\n");
										report(&reporter, false);
										msg[3] = SIGHUP;
									} else if (!strcmp(cmd->arg[0], "SIGINT")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGINT\n");
										report(&reporter, false);
										msg[3] = SIGINT;
									} else if (!strcmp(cmd->arg[0], "SIGQUIT")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGQUIT\n");
										report(&reporter, false);
										msg[3] = SIGQUIT;
									} else if (!strcmp(cmd->arg[0], "SIGKILL")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGKILL\n");
										report(&reporter, false);
										msg[3] = SIGKILL;
									} else if (!strcmp(cmd->arg[0], "SIGUSR1")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGUSR1\n");
										report(&reporter, false);
										msg[3] = SIGUSR1;
									} else if (!strcmp(cmd->arg[0], "SIGUSR2")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGUSR2\n");
										report(&reporter, false);
										msg[3] = SIGUSR2;
									} else if (!strcmp(cmd->arg[0], "SIGTERM")) {
										snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: signal to be send is SIGTERM\n");
										report(&reporter, false);
										msg[3] = SIGTERM;
									} else {
										msg[0] = '\0';
									}
									if (msg[0]) {
										if (!strcmp(cmd->arg[1], "all")) {
											for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
												for (int i = 0; i < current->numprocs; i++) {
													if (write(current->processes[i].com[1], msg, strlen(msg))) {}
												}
											}
										} else {
											for (int i = 1; cmd->arg[i]; i++) {
												for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
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
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent stop command\n");
								report(&reporter, false);
								if (cmd->arg) {
									send_command_multiproc(cmd, serv);
								}
							} else if (!strcmp(cmd->cmd, "avail")) {   //main thread return available process
							} else if (!strcmp(cmd->cmd, "fg")) {      //administrator send logging and stdin
							} else if (!strcmp(cmd->cmd, "reload")) {  //restart daemon
							} else if (!strcmp(cmd->cmd, "restart")) { //administrator stop then start process
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent start command\n");
								report(&reporter, false);
								if (cmd->arg) {
									send_command_multiproc(cmd, serv);
								}
							} else if (!strcmp(cmd->cmd, "start")) {   //administrator start process
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent start command\n");
								report(&reporter, false);
								if (cmd->arg) {
									send_command_multiproc(cmd, serv);
								}
							} else if (!strcmp(cmd->cmd, "tail")) {    //administrator send logging
							} else if (!strcmp(cmd->cmd, "clear")) {   //administrator clear logging
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent clear command\n");
								report(&reporter, false);
								if (cmd->arg) {
									send_command_multiproc(cmd, serv);
								}
							} else if (!strcmp(cmd->cmd, "pid")) {     //main thread send pid of process
								snprintf(reporter.buffer, PIPE_BUF - 22, "DEBUG: controller has sent clear command\n");
								report(&reporter, false);
								if (cmd->arg) {
									if (!strcmp(cmd->arg[0], "all")) {
										for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
											for (int i = 0; i < current->numprocs; i++) {
												snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %d\n", current->processes[i].name, current->processes[i].pid);
											}
										}
									} else {
										for (int i = 0; cmd->arg[i]; i++) {
											for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
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
								//FOR NOW SET g_sig LATER DO MORE
								g_sig = 1;
							} else if (!strcmp(cmd->cmd, "status")) {  //administrator send status
								char *status[7] = {"STOPPED", "STARTING", "RUNNING", "BACKOFF", "STOPPING", "EXITED", "FATAL"};
								if (cmd->arg) {
									for (int i = 0; cmd->arg[i]; i++) {
										for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
											if (!strncmp(current->name, cmd->arg[i], strlen(current->name))) {
												for (int j = 0; j < current->numprocs; j++) {
													if (!strcmp(cmd->arg[i], current->processes[j].name)) {
														snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %s\n", current->processes[j].name, status[current->processes[j].status]);
													}
												}
											}
										}
									}
								} else {
									for (struct s_program *current = serv->begin(serv); current; current = current->itnext(current)) {
										for (int i = 0; i < current->numprocs; i++) {
											snprintf(client->buf + strlen(client->buf), PIPE_BUF + 1, "%s : %s\n", current->processes[i].name, status[current->processes[i].status]);
										}
									}
								}
							} else if (!strcmp(cmd->cmd, "update")) {  //reparse config file
							} else {
								char *unknown_cmd = "Unknown command\n";
								memcpy(client->buf, unknown_cmd, strlen(unknown_cmd));
							}
							// execute cmd
						}
					} else if (tmp.events & EPOLLOUT) {
						// send response located in client->buf
						if (strlen(client->buf)) {
							printf("SENDING BACK DATA : >%s< \n", client->buf);
							send(tmp.data.fd, client->buf, strlen(client->buf), 0);
							bzero(client->buf, PIPE_BUF + 1);
						} else {
							printf("SENDING BACK NON ZERO RESPONSE\n");
							send(tmp.data.fd, ".\n", 2, 0);
						}
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
