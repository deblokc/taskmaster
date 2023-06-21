/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 19:48:45 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/21 19:48:23 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <stdio.h>

static struct s_server	*free_server(struct s_server *self)
{
	printf("Called cleaner\n");
	if (self->log_pipe[0] != 0)
		close(self->log_pipe[0]);
	if (self->log_pipe[1] != 0)
		close(self->log_pipe[1]);
	if (self->logger.logfile)
		free(self->logger.logfile);
	if (self->pidfile)
		free(self->pidfile);
	if (self->user)
		free(self->user);
	if (self->group)
		free(self->group);
	free(self);
	return (NULL);
}

void	init_server(struct s_server * server)
{
	server->cleaner = free_server;
	server->log_level = WARN;
	server->umask = 022;
	server->daemon= false;
	default_logger(&server->logger);
}

static bool add_value(struct s_server *server, yaml_document_t *document, char* key, yaml_node_t *value)
{
	bool	ret = true;

	(void) document;
	if (!strcmp("logfile", key))
		ret = add_char(NULL, "logfile", &server->logger.logfile, value);
	else if (!strcmp("logfile_maxbytes", key))
		add_number(NULL, "logfile_maxbytes", &server->logger.logfile_maxbytes, value, 100, 1024*1024*1024);
	else if (!strcmp("logfile_backups", key))
		add_number(NULL, "logfile_backups", &server->logger.logfile_backups, value, 0, 100);
	else if (!strcmp("pidfile", key))
		ret = add_char(NULL, "pidfile", &server->pidfile, value);
	else if (!strcmp("user", key))
		ret = add_char(NULL, "user", &server->user, value);
	else if (!strcmp("group", key))
		ret = add_char(NULL, "group", &server->group, value);
	else if (!strcmp("umask", key))
		add_octal(NULL, "umask", &server->umask, value, 0, 0777);
	else if (!strcmp("daemon", key))
		add_bool(NULL, "daemon", &server->daemon, value);
	else
		printf("Unknown field %s in server\n", key);
	return (ret) ;	
}

bool	parse_server(struct s_server *server, yaml_document_t *document, int value_index)
{
	bool		ret = true;
	yaml_node_t	*params_node = NULL;
	yaml_node_t	*key_node = NULL;
	yaml_node_t	*value_node = NULL;

	printf("Parsing server\n");
	params_node = yaml_document_get_node(document, value_index);
	if (params_node->type != YAML_MAPPING_NODE)
	{
		printf("Unexpected format for block server, expected map, encountered %s\n", params_node->tag);
		ret = false;
	}
	else
	{
		for (int i = 0; (params_node->data.mapping.pairs.start + i) < params_node->data.mapping.pairs.top; i++)
		{
			key_node = yaml_document_get_node(document, (params_node->data.mapping.pairs.start + i)->key);
			if (key_node->type == YAML_SCALAR_NODE)
			{
				value_node = yaml_document_get_node(document, (params_node->data.mapping.pairs.start + i)->value);
				if (!add_value(server, document, (char*)key_node->data.scalar.value, value_node))
				{
					ret = false;
					break ;
				}
			}
			else
			{
				printf("Incorrect format detected in server block\n");
			}
		}
	}
	return (ret);
}
