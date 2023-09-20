/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/09/20 13:43:58 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "taskmasterctl.h"
#include "readline.h"

int g_sig = 0;

void help(char *full_arg)
{
	char *arg = NULL;
	int i = 0;
	while (full_arg[i] && full_arg[i] != ' ')
	{
		i++;
	}
	while (full_arg[i] && full_arg[i] == ' ')
	{
		i++;
	}
	arg = full_arg + i;

	if (!strlen(arg))
	{
		printf("default commands (type help <command>):\n");
		printf("=======================================\n");
		printf("exit maintail quit signal stop fg\n");
		printf("reload restart start tail help pid\n");
		printf("shutdown status update\n");
	}
	else
	{
		if (!strcmp(arg, "exit"))
		{
			printf("exit\tExit the taskmaster shell.\n");
		}
		else if (!strcmp(arg, "maintail"))
		{
			printf("maintail -f\t\t\tContinuous tail of taskmaster main log file (Ctrl-D to exit)\n");
			printf("maintail -100\t\t\tlast 100 *bytes* of taskmaster main log file\n");
			printf("maintail\t\t\tlast 1600 *bytes* of taskmaster main log file\n");
		}
		else if (!strcmp(arg, "quit"))
		{
			printf("quit\tExit the taskmaster shell.\n");
		}
		else if (!strcmp(arg, "signal"))
		{
			printf("signal <signal name> <name>\t\tSignal a process\n");
			printf("signal <signal name> <name> <name>\tSignal multiple processes or groups\n");
			printf("signal <signal name> all\t\tSignal all processes\n");
		}
		else if (!strcmp(arg, "stop"))
		{
			printf("stop <name>\t\tStop a process\n");
			printf("stop <name> <name>\tStop multiple processes or groups\n");
			printf("stop all\t\tStop all processes\n");
		}
		else if (!strcmp(arg, "fg"))
		{
			printf("fg <process>\tConnect to a process in foreground mode\n");
			printf("\t\tCtrl-D to exit\n");
		}
		else if (!strcmp(arg, "reload"))
		{
			printf("reload\t\tRestart the remote taskmasterd.\n");
		}
		else if (!strcmp(arg, "restart"))
		{
			printf("restart <name>\t\tRestart a process\n");
			printf("restart <name> <name>\tRestart multiple processes or groups\n");
			printf("restart all\t\tRestart all processes\n");
		}
		else if (!strcmp(arg, "start"))
		{
			printf("start <name>\t\tStart a process\n");
			printf("start <name> <name>\tStart multiple processes or groups\n");
			printf("start all\t\tStart all processes\n");
		}
		else if (!strcmp(arg, "tail"))
		{
			printf("tail [-f] <name>\t[stdout|stderr] (default stdout)\n");
			printf("Ex:\n");
			printf("tail -f <name>\t\tContinuous tail of named process stdout\n");
			printf("\t\t\tCtrl-D to exit\n");
			printf("tail -100 <name>\tlast 100 *bytes* of process stdout\n");
			printf("tail <name> stderr\tlast 1600 *bytes* of process stderr\n");
		}
		else if (!strcmp(arg, "help"))
		{
			printf("help\t\t\tPrint a list of available commands\n");
			printf("help <command>\t\tPrint help for <command>\n");
		}
		else if (!strcmp(arg, "pid"))
		{
			printf("pid\t\t\tGet the PID of taskmasterd.\n");
			printf("pid <name>\t\tGet the PID of a single child process by name.\n");
			printf("pid all\t\t\tGet the PID of every child process, one per line.\n");
		}
		else if (!strcmp(arg, "shutdown"))
		{
			printf("shutdown\t\tShut the remote taskmasterd down.\n");
		}
		else if (!strcmp(arg, "status"))
		{
			printf("status <name>\t\tGet status for a single process\n");
			printf("status <name> <name>\tGet status for multiple named processes\n");
			printf("status\t\t\tGet all process status info\n");
		}
		else if (!strcmp(arg, "update"))
		{
			printf("update\t\t\tReload config and add/remove as necessary, and will restart affected programs\n");
			printf("update all\t\tReload config and add/remove as necessary, and will restart affected programs\n");
		}
		else
		{
			printf("*** No help on %s\n", arg);
		}
	}
}

void *thread_log(void *arg)
{
	struct epoll_event *sock = (struct epoll_event *)arg;
	int efd = epoll_create(1);
	sock->events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_ADD, sock->data.fd, sock);

	struct epoll_event event;

	char buf[PIPE_BUF + 1];
	bzero(buf, PIPE_BUF + 1);
	while (!g_sig)
	{
		if (epoll_wait(efd, &event, 1, 0) > 0)
		{
			if (event.data.fd == sock->data.fd)
			{
				bzero(buf, PIPE_BUF + 1);
				if (recv(event.data.fd, buf, PIPE_BUF, 0) > 0)
				{
					printf("%s", buf);
					fflush(stdout);
				}
			}
		}
	}
	fflush(stdout);
	epoll_ctl(efd, EPOLL_CTL_DEL, sock->data.fd, sock);
	close(efd);
	return NULL;
}

void tail(int efd, struct epoll_event sock)
{
	int ret;
	struct epoll_event event;
	char *line;
	char buf[PIPE_BUF + 1];
	bzero(buf, PIPE_BUF + 1);
	pthread_t thread;

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);
	pthread_create(&thread, NULL, &thread_log, &sock);
	g_sig = 0;
	while (!g_sig)
	{
		line = ft_readline("");
		if (line)
		{
			free(line);
		}
		else
		{
			g_sig = 1;
		}
	}
	pthread_join(thread, NULL);

	ret = epoll_wait(efd, &event, 1, 60 * 1000);
	if (ret > 0)
	{
		if (event.events & EPOLLOUT)
		{
			send(event.data.fd, "exit", strlen("exit"), 0);
		}
	}
	else if (ret == 0)
	{
		printf("SOCKET TIMED OUT\n");
	}
	else
	{
		perror("epoll_wait(EPOLLOUT)");
	}
	int i = 0;
	while (i < 42)
	{
		recv(sock.data.fd, buf, PIPE_BUF, MSG_DONTWAIT);
		bzero(buf, PIPE_BUF + 1);
		usleep(10);
		i++;
	}
	fflush(stdout);
}

void foreground(int efd, struct epoll_event sock)
{
	int ret;
	struct epoll_event event;
	char *line;
	char buf[PIPE_BUF + 1];
	bzero(buf, PIPE_BUF + 1);
	pthread_t thread;

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);
	pthread_create(&thread, NULL, &thread_log, &sock);
	while (!g_sig)
	{
		line = ft_readline("");
		if (line)
		{
			ret = epoll_wait(efd, &event, 1, 100);
			if (ret > 0)
			{
				if (event.events & EPOLLOUT)
				{
					bzero(buf, PIPE_BUF + 1);
					snprintf(buf, PIPE_BUF + 1, "data:%s\n", line);
					send(event.data.fd, buf, strlen(buf), 0);
				}
			}
			else if (ret == 0)
			{
				printf("socket timed out\n");
				g_sig = 1;
				return;
			}
			else
			{
				perror("epoll_wait");
				g_sig = 1;
				return;
			}
			free(line);
		}
		else
		{
			g_sig = 1;
		}
	}
	pthread_join(thread, NULL);

	ret = epoll_wait(efd, &event, 1, 60 * 1000);
	if (ret > 0)
	{
		if (event.events & EPOLLOUT)
		{
			send(event.data.fd, "exit", strlen("exit"), 0);
		}
	}
	else if (ret == 0)
	{
		printf("SOCKET TIMED OUT\n");
	}
	else
	{
		perror("epoll_wait(EPOLLOUT)");
	}
	int i = 0;
	while (i < 42)
	{
		recv(sock.data.fd, buf, PIPE_BUF, MSG_DONTWAIT);
		bzero(buf, PIPE_BUF + 1);
		usleep(10);
		i++;
	}
	fflush(stdout);
}

int remote_exec(char *cmd, int efd, struct epoll_event sock)
{
	struct epoll_event tmp;
	int to_ret = 0;

	/*
	 *  THE LOGIC WITH WHICH IVE MADE THIS CTL IS THAT IT IS AN EMPTY SHELL.
	 *  THIS MEANS IT WILL ALWAYS FIRST `SEND A REQUEST` (OR MORE ACCURATELLY
	 *  TRANSFER THE COMMAND) AND THEN `PRINT THE RESPONSE` WITHOUT ANY THOUGH
	 */
	if (strlen(cmd) > PIPE_BUF)
	{
		printf("line too long...\n");
		return 1;
	}

	sock.events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);

	int ret = epoll_wait(efd, &tmp, 1, 60 * 1000); // wait for max a minute
	if (ret > 0)
	{
		if (tmp.events & EPOLLOUT)
		{ // SEND PHASE
			if (send(tmp.data.fd, cmd, strlen(cmd), 0) <= 0) {
				to_ret = 1;
			}
		}
	}
	else if (ret == 0)
	{
		printf("SOCKET TIMED OUT\n");
	}
	else
	{
		perror("epoll_wait(EPOLLOUT)");
	}

	sock.events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock.data.fd, &sock);

	ret = epoll_wait(efd, &tmp, 1, 60 * 1000); // wait for max a minute
	if (ret > 0)
	{
		if (tmp.events & EPOLLIN)
		{ // RECV PHASE
			char buf[PIPE_BUF + 1];
			bzero(buf, PIPE_BUF + 1);
			recv(tmp.data.fd, buf, PIPE_BUF, 0);
			if (!strncmp(buf, "fg", 2))
			{
				printf("%s", buf + 2);
				foreground(efd, sock);
				g_sig = 0;
			}
			else if (!strncmp(buf, "tail", 4))
			{
				printf("%s", buf + 4);
				tail(efd, sock);
				g_sig = 0;
			}
			else
			{
				printf("%s", buf); // DUMBLY PRINT RESPONSE
				fflush(stdout);
			}
		}
	}
	else if (ret == 0)
	{
		to_ret = 1;
		printf("SOCKET TIMED OUT\n");
	}
	else
	{
		to_ret = 1;
		perror("epoll_wait(EPOLLIN)");
	}
	if (!strcmp(cmd, "shutdown")) {
		to_ret = 1;
	}
	return (to_ret);
}

void get_cmd(char *cmd, char *full_cmd)
{
	if (strlen(full_cmd) < 4)
	{
		return;
	}
	else if (full_cmd[4] != ' ' && full_cmd[4] != '\0')
	{
		return;
	}
	else
	{
		memcpy(cmd, full_cmd, 4);
	}
}

int exec(char *full_cmd, int efd, struct epoll_event sock)
{
	int ret = 0;
	char cmd[5];

	bzero(cmd, 5);
	get_cmd(cmd, full_cmd);
	if (strlen(cmd))
	{
		if (!strcmp(cmd, "help"))
		{
			help(full_cmd);
		}
		else if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit"))
		{
			ret = 1;
		}
		else
		{
			ret = remote_exec(full_cmd, efd, sock);
		}
	}
	else
	{
		ret = remote_exec(full_cmd, efd, sock);
	}
	return (ret);
}

char *process_line(char *line)
{
	int i = 0;
	while (line[i] && line[i] == ' ')
	{
		i++;
	}
	char *cmd = line + i;
	if (strlen(cmd))
	{
		return cmd;
	}
	else
	{
		return NULL;
	}
}

void sig_handler(int sig)
{
	(void)sig;
	g_sig = 1;
}

int open_socket(char *socket_path)
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
	strcpy(addr.sun_path, socket_path);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("connect");
		close(fd);
		return (-1);
	}
	return (fd);
}

static char *strjoin(const char *s1, const char *s2, const char *s3)
{
	if (!s1 || !s2 || !s3)
	{
		return NULL;
	}
	char *result = (char *)calloc(sizeof(char), (strlen(s1) + strlen(s2) + strlen(s3) + 1));

	if (result)
	{
		strcpy(result, s1);
		strcat(result, s2);
		strcat(result, s3);
	}
	return result;
}

int check_auth(int efd, struct epoll_event *sock)
{
	struct epoll_event event;
	char buf[5];
	bzero(buf, 5);

	if (epoll_wait(efd, &event, 1, 30 * 1000) <= 0) {
		printf("TIMEOUT EPOLL_WAIT\n");
		return (1);
	}
	if (read(event.data.fd, buf, 4) < 4) {
		printf("ERROR\n");
		return (1);
	}
	sock->events = EPOLLOUT;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock->data.fd, sock);
	if (!strcmp("okay", buf)) {
		return (0);
	}
	char *username = ft_readline("username :");
	ft_readline_shadow(true);
	char *password = ft_readline("password :");
	ft_readline_shadow(false);

	char *tosend = strjoin(username, "\n", password);
	if (!tosend) {
		return (1);
	}
	if (epoll_wait(efd, &event, 1, 30 * 1000) <= 0) {
		printf("TIMEOUT EPOLL_WAIT\n");
		return (1);
	}
	if (write(event.data.fd, tosend, strlen(tosend))) {
	}
	sock->events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock->data.fd, sock);
	if (epoll_wait(efd, &event, 1, 30 * 1000) <= 0) {
		printf("TIMEOUT EPOLL_WAIT\n");
		return (1);
	}
	if (read(event.data.fd, buf, 4) < 4) {
		return (1);
	}
	if (!strcmp("okay", buf)) {
		sock->events = EPOLLOUT;
		epoll_ctl(efd, EPOLL_CTL_MOD, sock->data.fd, sock);
		return (0);
	}
	return (1);
	
}

int main(int ac, char **av)
{
	char error_message[PIPE_BUF + 1];
	char *socket_path = NULL;
	char *tab[] = {"add", "exit", "maintail", "quit", "reread", "signal", "stop",
				   "avail", "fg", "reload", "restart", "start", "tail",
				   "clear", "help", "pid", "remove", "shutdown", "status", "update", NULL};

	bzero(error_message, PIPE_BUF + 1);
	if (ac > 2)
	{
		snprintf(error_message, PIPE_BUF, "Usage:\n%s\nOR\n%s CONFIGURATION-FILE\n", av[0], av[0]);
		if (write(2, error_message, strlen(error_message)))
		{
		}
		return (1);
	}
	if (ac == 1)
	{
		socket_path = strdup("/tmp/taskmaster.sock");
		if (!socket_path)
		{
			perror("Could not copy socket path");
			return (1);
		}
	}
	else
	{
		socket_path = parse_config(av[1]);
		if (!socket_path)
			return (1);
	}
	int socket = open_socket(socket_path);
	if (socket == -1)
	{
		free(socket_path);
		return (1);
	}
	struct epoll_event sock;
	bzero(&sock, sizeof(sock));
	sock.data.fd = socket;
	sock.events = EPOLLIN;
	int efd = epoll_create(1);
	epoll_ctl(efd, EPOLL_CTL_ADD, socket, &sock);

	if (check_auth(efd, &sock)) {
		printf("Error in authentification !\n");
		free(socket_path);
		return (1);
	}
	// SETUP EPOLL, IT CAN NOW BE CALLED IN NEEDED FUNCTION
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &sig_handler);
	complete_init(tab);

	FILE *file = fopen_history();
	add_old_history(file);

	char *line = ft_readline("taskmasterctl>");
	char *cmd = NULL;
	while (line != NULL)
	{
		// process line (remove space n sht)
		char *tmp = strdup(line);
		cmd = process_line(line);
		if (cmd)
		{
			if (exec(cmd, efd, sock))
			{
				free(line);
				free(tmp);
				break;
			}
			// add to history and read another line
			add_history(tmp);
			add_file_history(tmp, file);
		}
		free(line);
		free(tmp);
		line = ft_readline("taskmasterctl>");
	}
	clear_history();
	if (file)
	{
		fclose(file);
	}
	free(socket_path);
	close(efd);
	close(sock.data.fd);
	return (0);
}
