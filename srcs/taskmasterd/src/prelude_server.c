/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   prelude_server.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/25 18:58:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/09/20 20:38:44 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

void	create_pid_file(struct s_server *server, struct s_report *reporter)
{
	int	fd;
	
	if (!server->pidfile)
		fd = open("taskmasterd.pid", O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0666 & ~server->umask);
	else
		fd = open(server->pidfile, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0666 & ~server->umask);
	if (fd  < 0)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not create pid file, exiting process\n");
		report(reporter, true);
		return ;
	}
	snprintf(reporter->buffer, PIPE_BUF, "%d\n", server->pid);
	if (write(fd, reporter->buffer, strlen(reporter->buffer)) == -1)
	{
		if (server->pidfile)
			unlink(server->pidfile);
		else
			unlink("taskmasterd.pid");
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not write pid to file, exiting process\n");
		report(reporter, true);
	}
	close(fd);
}

void update_umask(struct s_server *server)
{
	server->logger.umask = server->umask;
	for (struct s_program* current = server->begin(server); current; current = current->itnext(current))
	{
		current->stdout_logger.umask = server->umask;
		current->stderr_logger.umask = server->umask;
	}
}

void	prelude(struct s_server *server, struct s_report *reporter)
{
#ifdef DISCORD
	char*			discord_channel = NULL;
	char*			discord_token = NULL;
#endif
	struct passwd	*ret = NULL;
	struct s_env	*current = NULL;

	if (server->user)
	{
		errno = 0;
		ret = getpwnam(server->user);
		if (!ret)
		{
			if (errno == 0 || errno == ENOENT || errno == ESRCH || errno == EBADF || errno == EPERM)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Unkown user %s\n", server->user);
				report(reporter, true);
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not get information on user %s\n", server->user);
				report(reporter, true);
			}
			return ;
		}
		if (setuid(ret->pw_uid) == -1 || setgid(ret->pw_gid) == -1)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not change user to %s\n", server->user);
			report(reporter, true);
			free(ret);
			return ;
		}
		ret = NULL;
	}
	current = server->env;
	while (current)
	{
		if (setenv(current->key, current->value, 1))
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not update env in 'server'\n");
			report(reporter, true);
			return ;
		}
		current = current->next;
	}
	if (server->workingdir && chdir(server->workingdir) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not change working directory of 'server' to %s\n", server->workingdir);
		report(reporter, true);
		return ;
	}
	if (server->umask != 022)
	{
		umask((mode_t)server->umask);
	}
	if (!server->logger.logfile)
	{
		server->logger.logfile = strdup("taskmasterd.log");
		if (!server->logger.logfile)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate space\n");
			report(reporter, true);
			return ;
		}
	}
	if ((server->logger.logfd = open(server->logger.logfile, O_CREAT | O_APPEND | O_RDWR | O_NOCTTY, 0666 & ~server->umask)) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not open logfile\n");
		report(reporter, true);
		return ;
	}
	update_umask(server);
#ifdef DISCORD
	if (server->log_discord)
	{
		if (!server->discord_channel)
		{
			discord_channel = getenv("DISCORD_CHANNEL");
			if (!discord_channel)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Discord logging is enabled, however no channel was specified, feature will be disabled\n");
				report(reporter, false);
				server->log_discord = false;
				return ;
			}
			server->discord_channel = strdup(discord_channel);
			if (!server->discord_channel)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate string for variable discord_channel\n");
				report(reporter, true);
				server->log_discord = false;
				return ;
			}
		}
		if (!server->discord_token)
		{
			discord_token = getenv("DISCORD_TOKEN");
			if (!discord_token)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Discord logging is enabled, however no token was specified, feature will be disabled\n");
				report(reporter, false);
				server->log_discord = false;
				return ;
			}
			server->discord_token = strdup(discord_token);
			if (!server->discord_token)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate string for variable discord_token\n");
				report(reporter, true);
				server->log_discord = false;
				return ;
			}
		}
	}
#endif
}
