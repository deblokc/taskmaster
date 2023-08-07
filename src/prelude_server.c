/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   prelude_server.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/25 18:58:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/07 16:05:31 by bdetune          ###   ########.fr       */
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

static void update_umask(struct s_server *server)
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
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Unkown user %s\n", server->user);
				report(reporter, true);
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not get information on user %s\n", server->user);
				report(reporter, true);
			}
			return ;
		}
		if (setuid(ret->pw_uid) == -1)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not change user to %s\n", server->user);
			report(reporter, true);
			free(ret);
			return ;
		}
		free(ret);
		ret = NULL;
	}
	current = server->env;
	while (current)
	{
		if (!putenv(current->value))
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not update env in 'server'\n");
			report(reporter, true);
			return ;
		}
		current = current->next;
	}
	if (server->workingdir && chdir(server->workingdir) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not change working directory of 'server' to %s\n", server->workingdir);
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
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate space\n");
			report(reporter, true);
			return ;
		}
	}
	if ((server->logger.logfd = open(server->logger.logfile, O_CREAT | O_APPEND | O_RDWR, 0666 & ~server->umask)) == -1)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not open logfile\n");
		report(reporter, true);
		return ;
	}
	update_umask(server);
}
