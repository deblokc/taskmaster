/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 18:00:54 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/08 11:25:26 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static bool parse_document(struct s_server *server, yaml_document_t * document, struct s_report *reporter)
{
	bool		ret = true;
	yaml_node_t	*current_node;
	yaml_node_t	*key_node;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing YAML document\n");
	report(reporter, false);
	current_node = yaml_document_get_root_node(document);
	if (!current_node)
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARN: empty YAML configuration file encountered, taskmasterd will start with default server and no program\n");
		report(reporter, false);
	}
	else
	{
		if (current_node->type == YAML_MAPPING_NODE)
		{
			for (int i = 0; (current_node->data.mapping.pairs.start + i) < current_node->data.mapping.pairs.top; i++)
			{
				key_node = yaml_document_get_node(document, (current_node->data.mapping.pairs.start + i)->key);
				if (key_node->type == YAML_SCALAR_NODE)
				{
					if (!strcmp((char *)key_node->data.scalar.value, "programs"))
					{
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing 'programs' block\n");
						report(reporter, false);
						if (!parse_programs(server, document, (current_node->data.mapping.pairs.start + i)->value, reporter))
						{
							ret = false;
							break ;
						}
					}
					else if (!strcmp((char*)key_node->data.scalar.value, "server"))
					{
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing 'server' block\n");
						report(reporter, false);
						if (!parse_server(server, document, (current_node->data.mapping.pairs.start + i)->value, reporter))
						{
							ret = false;
							break ;
						}
					}
					else if (!strcmp((char*)key_node->data.scalar.value, "ssocket"))
					{
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing 'socket' block\n");
						report(reporter, false);
						if (!parse_socket(server, document, (current_node->data.mapping.pairs.start + i)->value, reporter))
						{
							ret = false;
							break ;
						}
					}
					else
					{
						snprintf(reporter->buffer, PIPE_BUF, "WARN: Unknown key encountered in configuration file: %s\n", key_node->data.scalar.value);
						report(reporter, false);
					}
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong node format encountered in root YAML map: %s\n", key_node->data.scalar.value);
					report(reporter, false);
				}
			}
		}
		else
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Expected map at the root of yaml document, found: %s\n", current_node->tag);
			report(reporter, true);
			ret = false;
		}
	}

	return ret;
}

static bool	parse_config_yaml(struct s_server * server, FILE *config_file_handle, struct s_report *reporter)
{
	bool			ret = true;
	yaml_parser_t	parser;
	yaml_document_t	document;

	if (!yaml_parser_initialize(&parser))
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not initialize parser\n");
		report(reporter, true);
		ret = false;
	}
	else
	{
		yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
		yaml_parser_set_input_file(&parser, config_file_handle);
		if (!yaml_parser_load(&parser, &document))
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not load configuration file, please verify YAML syntax\n");
			report(reporter, true);
			ret = false;
		}
		else
		{
			ret = parse_document(server, &document, reporter);
			yaml_document_delete(&document);
		}
		yaml_parser_delete(&parser);
	}
	return (ret);	
}


static void	resolve_configfile_path(struct s_server* server, char* config_file, struct s_report *reporter)
{
	char*	config_trimmed;
	char*	cwd;
	int		i;
	size_t	len;

	i = 0;
	while ((config_file[i] >= '\t' && config_file[i] <= '\r') || config_file[i] == ' ')
		i++;
	config_trimmed = &config_file[i];
	if (config_trimmed[0] == '/')
	{
		server->config_file = strdup(config_trimmed);
		if (!server->config_file)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate path for current working directory\n");
			report(reporter, true);
		}
	}
	else
	{
		cwd = getcwd(NULL, 4096);
		if (!cwd)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate path for current working directory\n");
			report(reporter, true);
		}
		else
		{
			server->config_file = calloc((strlen(cwd) + strlen(config_trimmed) + 2), sizeof(char));
			if (!server->config_file)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate path for config file\n");
				report(reporter, true);
			}
			else
			{
				strcpy(server->config_file, cwd);
				len = strlen(server->config_file);
				server->config_file[len] = '/';
				strcpy(&server->config_file[len + 1], config_trimmed);
			}
			free(cwd);
		}
	}
	if (!reporter->critical)
	{
		snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Path to configuration file resolved: %s\n", server->config_file);
		report(reporter, false);
	}
}

struct s_server*	parse_config(char* config_file, struct s_report* reporter)
{
	FILE				*config_file_handle = NULL;
	struct s_server		*server = NULL;

	server = calloc(1, sizeof(*server));
	if (server)
	{
		init_server(server);
		snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Buiding server\n");
		report(reporter, false);
		config_file_handle = fopen(config_file, "r");	
		if (!config_file_handle)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not open configuration file\n");
			report(reporter, true);
			server = server->cleaner(server);
		}
		else
		{
			resolve_configfile_path(server, config_file, reporter);
			if (!reporter->critical)
			{
				if (!parse_config_yaml(server, config_file_handle, reporter))
					server = server->cleaner(server);
			}
			fclose(config_file_handle);
		}
	}
	else
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate server\n");
		report(reporter, true);
	}
	return (server);
}
