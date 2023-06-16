/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 18:00:54 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/16 20:25:20 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static struct s_server	*free_server(struct s_server *server)
{
	if (server->log_pipe[0] != 0)
		close(server->log_pipe[0]);
	if (server->log_pipe[1] != 0)
		close(server->log_pipe[1]);
	free(server);
	return (NULL);
}

static bool	parse_config_yaml(struct s_server * server, FILE *config_file_handle)
{
	bool			ret = true;
	yaml_parser_t	parser;

	(void)server;
	if (!yaml_parser_initialize(&parser))
	{
		ret = false;
	}
	else
	{
		yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
		yaml_parser_set_input_file(&parser, config_file_handle);
		yaml_parser_delete(&parser);
	}
	return (ret);	
}

struct s_server*	parse_config(char* config_file)
{
	FILE				*config_file_handle = NULL;
	struct s_server		*server = NULL;

	(void)config_file;
	server = calloc(1, sizeof(*server));
	if (server)
	{
		server->cleaner = free_server;
		if (write(2, "Building server\n", strlen("Building server\n")) == -1){}
		else if (pipe2(server->log_pipe, O_DIRECT | O_NONBLOCK))
		{
			if (write(2, "Error while piping\n", strlen("Error while piping\n")) == -1){}
			server = server->cleaner(server);
		}
		else
		{
			config_file_handle = fopen(config_file, "r");	
			if (!config_file_handle)
			{
				if (write(2, "Error while opening configuration file\n", 
						strlen("Error while opening configuration file\n")) == -1) {}
				server = server->cleaner(server);
			}
			else
			{
				server->config_file = config_file;
				if (!parse_config_yaml(server, config_file_handle))
					server = server->cleaner(server);
				fclose(config_file_handle);
			}
		}
	}
	return (server);
}
