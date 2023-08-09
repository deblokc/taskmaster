/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 19:48:45 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/09 14:55:42 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <stdio.h>

static struct s_server	*free_server(struct s_server *self)
{
	if (self->log_pipe[0] >= 0)
		close(self->log_pipe[0]);
	if (self->log_pipe[1] >= 0)
		close(self->log_pipe[1]);
	if (self->logger.logfile)
		free(self->logger.logfile);
	if (self->logger.logfd >= 0)
		close(self->logger.logfd);
	if (self->pidfile)
		free(self->pidfile);
	if (self->user)
		free(self->user);
	if (self->workingdir)
		free(self->workingdir);
	if (self->env)
		free_s_env(self->env);
	if (self->config_file)
		free(self->config_file);
	if (self->bin_path)
		free(self->bin_path);
	self->socket.destructor(&self->socket);
	self->delete_tree(self);
	free(self);
	return (NULL);
}

void	init_server(struct s_server * server)
{
	register_treefn_serv(server);
	server->cleaner = free_server;
	server->loglevel = WARN;
	server->umask = 022;
	server->daemon= false;
	server->log_pipe[0] = -1;
	server->log_pipe[1] = -1;
	default_logger(&server->logger);
	init_socket(&server->socket);
}

static void add_loglevel(struct s_server *server, yaml_node_t *value, struct s_report *reporter)
{
	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing loglevel in 'server'\n");
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Wrong format for loglevel in 'server', expected a scalar value, encountered %s\n", value->tag);
		report(reporter, true);
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "CRITICAL"))
			server->loglevel = CRITICAL;
		else if (!strcmp((char *)value->data.scalar.value, "ERROR"))
			server->loglevel = ERROR;
		else if (!strcmp((char *)value->data.scalar.value, "WARN"))
			server->loglevel = WARN;
		else if (!strcmp((char *)value->data.scalar.value, "INFO"))
			server->loglevel = INFO;
		else if (!strcmp((char *)value->data.scalar.value, "DEBUG"))
			server->loglevel = DEBUG;
		else
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Wrong value for loglevel in 'server', accepted values are: CRITICAL ; ERROR ; WARN ; INFO ; DEBUG\n");
			report(reporter, true);
		}
	}
}

static void add_value(struct s_server *server, yaml_document_t *document, char* key, yaml_node_t *value, struct s_report *reporter)
{
	(void) document;
	if (!strcmp("logfile", key))
		add_char(NULL, "logfile", &server->logger.logfile, value, reporter);
	else if (!strcmp("logfile_maxbytes", key))
		add_number(NULL, "logfile_maxbytes", &server->logger.logfile_maxbytes, value, 100, 1024*1024*1024, reporter);
	else if (!strcmp("logfile_backups", key))
		add_number(NULL, "logfile_backups", &server->logger.logfile_backups, value, 0, 100, reporter);
	else if (!strcmp("pidfile", key))
		add_char(NULL, "pidfile", &server->pidfile, value, reporter);
	else if (!strcmp("user", key))
		add_char(NULL, "user", &server->user, value, reporter);
	else if (!strcmp("workingdir", key))
		add_char(NULL, "workingdir", &server->workingdir, value, reporter);
	else if (!strcmp("umask", key))
		add_octal(NULL, "umask", &server->umask, value, 0, 0777, reporter);
	else if (!strcmp("daemon", key))
		add_bool(NULL, "daemon", &server->daemon, value, reporter);
	else if (!strcmp("loglevel", key))
		add_loglevel(server, value, reporter);
	else if (!strcmp("env", key))
		parse_env(NULL, value, document, &server->env, reporter);
	else
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARNING: Encountered unknown key '%s' in server configuration\n", key);
		report(reporter, false);
	}
}

bool	parse_server(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter)
{
	static bool duplicate = false;
	bool		ret = true;
	yaml_node_t	*params_node = NULL;
	yaml_node_t	*key_node = NULL;
	yaml_node_t	*value_node = NULL;

	if (duplicate)
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARNING: Encountered two blocks server in configuration file\n");
		report(reporter, false);
	}
	duplicate = true;
	params_node = yaml_document_get_node(document, value_index);
	if (params_node->type != YAML_MAPPING_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Unexpected format for block server, expected map, encountered %s\n", params_node->tag);
		report(reporter, true);
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
				add_value(server, document, (char*)key_node->data.scalar.value, value_node, reporter);
				if (reporter->critical)
				{
					ret = false;
					break ;
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "WARNING: Incorrect format detected in server block, expected keys to be scalars, encountered: %s\n" , key_node->tag);
				report(reporter, false);
			}
		}
	}
	return (ret);
}
