/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/08/08 11:24:49 by bdetune           #+#    #+#             */
/*   Updated: 2023/09/25 20:48:24 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <sys/socket.h>

static void	destructor(struct s_socket *self)
{
	if (self->sockfd > 0)
	{
		close(self->sockfd);
		if (self->socketpath)
			unlink(self->socketpath);
		else
			unlink("/tmp/taskmaster.sock");
	}
	if (self->socketpath)
		free(self->socketpath);
	if (self->uid)
		free(self->uid);
	if (self->gid)
		free(self->gid);
	if (self->user)
		free(self->user);
	if (self->password)
		free(self->password);
	bzero(self, sizeof(*self));
	self->sockfd = -1;
	self->enable = false;
	self->umask = 022;
	self->destructor = destructor;
}

void	init_socket(struct s_socket *socket)
{
	bzero((void *)socket, sizeof(*socket));
	socket->sockfd = -1;
	socket->enable = false;
	socket->umask = 022;
	socket->destructor = destructor;
}


static bool add_value(struct s_socket *socket, char* key, yaml_node_t *value, struct s_report *reporter)
{
	bool	ret = true;

	if (!strcmp("enable", key))
		ret = add_bool(NULL, "enable", &socket->enable, value, reporter);
	else if (!strcmp("socketpath", key))
		ret = add_char(NULL, "socketpath", &socket->socketpath, value, reporter, PATH_SIZE);
	else if (!strcmp("umask", key))
		ret = add_octal(NULL, "umask", &socket->umask, value, 0, 0777, reporter);
	else if (!strcmp("uid", key))
		ret = add_char(NULL, "uid", &socket->uid, value, reporter, 256);
	else if (!strcmp("gid", key))
		ret = add_char(NULL, "gid", &socket->gid, value, reporter, 256);
	else if (!strcmp("user", key))
		ret = add_char(NULL, "user", &socket->user, value, reporter, 256);
	else if (!strcmp("password", key))
		ret = add_char(NULL, "password", &socket->password, value, reporter, 1024);
	else
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARNING  : Encountered unknown key '%s' in socket configuration\n", key);
		report(reporter, false);
	}
	return (ret);
}


bool	parse_socket(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter)
{
	bool		ret = true;
	yaml_node_t	*params_node = NULL;
	yaml_node_t	*key_node = NULL;
	yaml_node_t	*value_node = NULL;

	params_node = yaml_document_get_node(document, value_index);
	if (params_node->type != YAML_MAPPING_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Unexpected format for block 'socket', expected map, encountered %s, ignoring configuration block (socket connection might not be available)\n", params_node->tag);
		report(reporter, false);
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
				if (!add_value(&server->socket, (char*)key_node->data.scalar.value, value_node, reporter))
				{
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Error while parsing block 'socket', not enabling it\n");
					server->socket.destructor(&server->socket);
					report(reporter, reporter->critical);
					return (false);
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "WARNING  : Incorrect format detected in 'socket' block, expected keys to be scalars, encountered: %s\n" , key_node->tag);
				report(reporter, false);
			}
		}
	}
	if (server->socket.password && !server->socket.user)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : A password has been specified without a user in socket configuration\n");
		report(reporter, true);
	}
	if (!server->socket.password && server->socket.user)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : A user has been specified without a password in socket configuration\n");
		report(reporter, true);
	}
	return (ret);
}
