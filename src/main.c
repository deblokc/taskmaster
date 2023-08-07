/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/07 13:27:10 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#define SOCK_PATH "/tmp/taskmaster.sock"

struct s_client
{
	struct epoll_event poll;
	struct s_client *next;
};

struct s_command
{
	char *cmd;
	char *arg;
};

bool end_initial_log(struct s_server *server, struct s_report *reporter, int *reporter_pipe, void **thread_ret, pthread_t initial_logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
	{
		if (write(2, "CRITICAL: Log error\n", strlen("CRITICAL: Log error\n")) <= 0)
		{
		}
		if (server)
			server = server->cleaner(server);
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		close(reporter_pipe[2]);
		return (false);
	}
	if (pthread_join(initial_logger, thread_ret))
	{
		if (write(2, "CRITICAL: Could not join logging thread\n", strlen("CRITICAL: Could not join logging thread\n")) <= 0)
		{
		}
		if (server)
			server = server->cleaner(server);
		close(reporter_pipe[0]);
		close(reporter_pipe[1]);
		close(reporter_pipe[2]);
		return (false);
	}
	return (true);
}

bool end_logging_thread(struct s_report *reporter, pthread_t logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
		return (false);
	if (pthread_join(logger, NULL))
		return (false);
	return (true);
}

int create_server(void)
{
	int fd;
	struct sockaddr_un addr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket");
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	unlink(SOCK_PATH);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
	}
	listen(fd, 99);
	return (fd);
}

struct s_client *new_client(struct s_client *list, int client_fd)
{
	if (list)
	{
		struct s_client *tmp = list;
		while (tmp->next)
		{
			tmp = tmp->next;
		}
		tmp->next = (struct s_client *)calloc(sizeof(struct s_client), 1);
		tmp->next->poll.data.fd = client_fd;
		return (tmp->next);
	}
	else
	{
		struct s_client *new = (struct s_client *)calloc(sizeof(struct s_client), 1);
		new->poll.data.fd = client_fd;
		return (new);
	}
}

void check_server(int sock_fd, int efd)
{
	static struct s_client *list = NULL;
	char buf[PIPE_BUF + 1];
	struct epoll_event tmp;
	char *cmd;

	bzero(&tmp, sizeof(tmp));
	bzero(&cmd, sizeof(cmd));
	if (epoll_wait(efd, &tmp, 1, 0) > 0)
	{ // for now only check one event every time, might change
		if (tmp.data.fd == sock_fd)
		{
			printf("NEW CONNECTION\n");
			struct s_client *client = new_client(list, accept(sock_fd, NULL, NULL));
			client->poll.events = EPOLLIN; // | EPOLLOUT;
			epoll_ctl(efd, EPOLL_CTL_ADD, client->poll.data.fd, &client->poll);
			if (!list)
			{
				list = client;
			}
		}
		else
		{
			printf("GOT SMTH\n");
			struct s_client *client = list;
			while (client)
			{
				if (tmp.data.fd == client->poll.data.fd)
				{
					printf("client %d\n", tmp.data.fd);
					if (tmp.events & EPOLLIN)
					{
						bzero(buf, PIPE_BUF + 1);
						if (recv(client->poll.data.fd, buf, PIPE_BUF, MSG_DONTWAIT) <= 0)
						{
							epoll_ctl(efd, EPOLL_CTL_DEL, client->poll.data.fd, &client->poll);
							if (client == list)
							{
								list = client->next;
								free(client);
							}
							else
							{
								struct s_client *head = list;
								while (head->next != client)
								{
									head = head->next;
								}
								head->next = client->next;
								free(client);
							}
						}
						cmd = strdup(buf);
						printf(">%s<\n", cmd);
						client->poll.events = EPOLLOUT;
						epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll);
						// need to execute some action and probably send data back
					}
					else if (tmp.events & EPOLLOUT)
					{
						printf("SENDING DATA\n");
						send(tmp.data.fd, "this is a response", strlen("this is a response"), 0);
						client->poll.events = EPOLLIN;
						epoll_ctl(efd, EPOLL_CTL_MOD, client->poll.data.fd, &client->poll);
					}
					return;
				}
				client = client->next;
			}
		}
	}
	else
	{
	}
}

int main(int ac, char **av)
{
	void *thread_ret;
	int ret = 0;
	int reporter_pipe[4];
	pthread_t initial_logger;
	pthread_t logger;
	struct s_report reporter;
	struct s_server *server = NULL;
	struct s_priority *priorities = NULL;

	if (ac != 2)
	{
		if (write(2, "Usage: ", strlen("Usage: ")) == -1 || write(2, av[0], strlen(av[0])) == -1 || write(2, " CONFIGURATION-FILE\n", strlen(" CONFIGURATION-FILE\n")) == -1)
		{
		}
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
			if (write(2, "CRITICAL: could not create temporary log file, exiting process\n", strlen("CRITICAL: could not create temporary log file, exiting process\n")))
			{
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			return (1);
		}
		if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
		{
			if (write(2, "CRITICAL, could not create initial log thread, exiting process\n", strlen("CRITICAL, could not create initial log thread, exiting process\n")))
			{
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			unlink("/tmp/.taskmasterd_tmp.log");
			return (1);
		}
		server = parse_config(av[1], &reporter);
		if (!server || reporter.critical)
		{
			if (!end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger))
			{
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				return (1);
			}
			if (*(int *)thread_ret != 1)
				report_critical(reporter_pipe[2]);
			if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
			{
			}
			if (server)
				server = server->cleaner(server);
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			close(reporter_pipe[2]);
			ret = 1;
		}
		else
		{
			prelude(server, &reporter);
			if (reporter.critical)
			{
				if (!end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger))
				{
					if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
					{
					}
					return (1);
				}
				if (*(int *)thread_ret != 1)
					report_critical(reporter_pipe[2]);
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				if (server)
					server = server->cleaner(server);
				close(reporter_pipe[0]);
				close(reporter_pipe[1]);
				close(reporter_pipe[2]);
				return (1);
			}
			if (!end_initial_log(server, &reporter, reporter_pipe, &thread_ret, initial_logger))
			{
				if (write(2, "CRITICAL: Could not start taskmasterd, exiting process\n", strlen("CRITICAL: Could not start taskmasterd, exiting process\n")) <= 0)
				{
				}
				return (1);
			}
			close(reporter_pipe[0]);
			close(reporter_pipe[1]);
			transfer_logs(reporter_pipe[2], server);
			if (pipe2(server->log_pipe, O_DIRECT | O_NONBLOCK) == -1)
			{
				get_stamp(reporter.buffer);
				strcpy(&reporter.buffer[22], "CRITICAL: Could not open pipe for logging\n");
				if (write(2, reporter.buffer, strlen(reporter.buffer)) <= 0) {}
				write_log(&server->logger, reporter.buffer);
				server = server->cleaner(server);
				return (1);
			}
			if (pthread_create(&logger, NULL, main_logger, server))
			{
				get_stamp(reporter.buffer);
				strcpy(&reporter.buffer[22], "CRITICAL: Could not initiate logging thread\n");
				if (write(2, reporter.buffer, strlen(reporter.buffer)) <= 0) {}
				write_log(&server->logger, reporter.buffer);
				server = server->cleaner(server);
				return (1);
			}
			reporter.report_fd = server->log_pipe[1];
			priorities = create_priorities(server, &reporter);
			int sock_fd = create_server();
			struct epoll_event sock;
			bzero(&sock, sizeof(sock));
			sock.data.fd = sock_fd;
			sock.events = EPOLLIN;
			int efd = epoll_create(1);
			epoll_ctl(efd, EPOLL_CTL_ADD, sock_fd, &sock);
			strcpy(reporter.buffer, "INFO: Starting taskmasterd\n");
			report(&reporter, false);
			if (!priorities)
			{
				strcpy(reporter.buffer, "DEBUG: No priorities to start\n");
				report(&reporter, false);
			}
			else
			{
				strcpy(reporter.buffer, "DEBUG: Launching priorities\n");
				report(&reporter, false);
				launch(priorities);

				// todo
				while (1)
				{
					check_server(sock_fd, efd);
				}

				printf("=-=-=-=-=-=-=-=-= WAITING PRIORITIES =-=-=-=-=-=-=-=-=\n");
				wait_priorities(priorities);
				priorities->destructor(priorities);
			}
			if (!end_logging_thread(&reporter, logger))
			{
				if (write(2, "Could not terminate logging thread\n", strlen("Could not terminate logging thread\n"))){}
			}
			server = server->cleaner(server);
		}
	}
	return (ret);
}
