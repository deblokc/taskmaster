/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 19:48:45 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/19 19:56:42 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <stdio.h>

static struct s_server	*free_server(struct s_server *server)
{
	printf("Called cleaner\n");
	if (server->log_pipe[0] != 0)
		close(server->log_pipe[0]);
	if (server->log_pipe[1] != 0)
		close(server->log_pipe[1]);
	if (server->logger.logfile)
		free(server->logger.logfile);
	if (server->user)
		free(server->user);
	free(server);
	return (NULL);
}

bool	init_server(struct s_server * server)
{
	bool	ret = true;

	server->cleaner = free_server;
	server->log_level = WARN;
	server->umask = 022;
	if (!default_logger(server))
		ret = false;
	return (ret);
}

bool	parse_server(struct s_server *server, yaml_document_t *document, int value_index)
{
	bool		ret = true;
	yaml_node_t	*value_node = NULL;

	(void)server;
	value_node = yaml_document_get_node(document, value_index);
	if (value_node->type != YAML_MAPPING_NODE)
	{
		printf("Unexpected format for block server, expected map, encountered %s\n", value_node->tag);
		ret = false;
	}
	else
	{
		printf("Ready to parse server block\n");
	}
	return (ret);
}
