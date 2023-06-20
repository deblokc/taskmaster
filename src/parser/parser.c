/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 18:00:54 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/20 16:28:11 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static bool parse_document(struct s_server *server, yaml_document_t * document)
{
	bool		ret = true;
	yaml_node_t	*current_node;
	yaml_node_t	*key_node;

	printf("Parsing document\n");
	current_node = yaml_document_get_root_node(document);
	if (!current_node)
	{
		ret = false;
		if (write(2, "Empty yaml configuration file encountered\n", strlen("Empty yaml configuration file encountered\n"))) {}
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
						if (!parse_programs(server, document, (current_node->data.mapping.pairs.start + i)->value))
						{
							ret = false;
							break ;
						}
					}
					else if (!strcmp((char*)key_node->data.scalar.value, "server"))
					{
						if (!parse_server(server, document, (current_node->data.mapping.pairs.start + i)->value))
						{
							ret = false;
							break ;
						}
					}
					else
					{
						printf("Unknown key encountered: %s\n",  key_node->data.scalar.value);
					}
				}
				else
				{
					printf("Wrong format of node\n");
				}
			}
		}
		else
		{
			printf("Expected map at the root of yaml document, found: %s\n", current_node->tag);
			ret = false;
		}
	}

	return ret;
}

static bool	parse_config_yaml(struct s_server * server, FILE *config_file_handle)
{
	bool			ret = true;
	yaml_parser_t	parser;
	yaml_document_t	document;

	if (!yaml_parser_initialize(&parser))
	{
		printf("Could not initialize parser\n");
		ret = false;
	}
	else
	{
		yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
		yaml_parser_set_input_file(&parser, config_file_handle);
		if (!yaml_parser_load(&parser, &document))
		{
			printf("Could not load document, please verify YAML syntax\n");
			ret = false;
		}
		else
			ret = parse_document(server, &document);
		yaml_document_delete(&document);
		yaml_parser_delete(&parser);
	}
	return (ret);	
}

struct s_server*	parse_config(char* config_file)
{
	FILE				*config_file_handle = NULL;
	struct s_server		*server = NULL;

	server = calloc(1, sizeof(*server));
	if (server)
	{
		init_server(server);
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
	else
	{
		printf("Could not allocate server\n");
	}
	return (server);
}
