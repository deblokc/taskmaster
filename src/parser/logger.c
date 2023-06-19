/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   logger.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 18:56:48 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/19 18:57:28 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <string.h>

bool	default_logger(struct s_server *server)
{
	bool	ret = true;
	char*	logfile = NULL;

	server->logger.logfile_maxbytes = 5*1024*1024;
	server->logger.logfile_backups = 10;
	logfile = calloc(15, sizeof(*logfile));
	if (!logfile)
		ret = false;
	else
		logfile = strncpy(logfile, "taskmaster.log", 15);
	server->logger.logfile = logfile;
	return (ret);
}

