#include "taskmasterctl.h"
#include "yaml.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static char *parse_document(yaml_document_t * document)
{
	char		error_message[PIPE_BUF + 1];
	char		*socket_path = NULL;
	yaml_node_t	*current_node;
	yaml_node_t	*params_node;
	yaml_node_t	*key_node;
	yaml_node_t	*value_node;

	bzero(error_message, PIPE_BUF + 1);
	current_node = yaml_document_get_root_node(document);
	if (!current_node)
	{
		write(2, "Empty configuration file provided\n", strlen("Empty configuration file provided\n"));
		return (NULL);	
	}
	if (current_node->type != YAML_MAPPING_NODE)
	{
		snprintf(error_message, PIPE_BUF, "Expected map at the root of yaml document, found: %s\n", current_node->tag);
		write(2, error_message, strlen(error_message));
		return (NULL);
	}
	for (int i = 0; (current_node->data.mapping.pairs.start + i) < current_node->data.mapping.pairs.top; ++i)
	{
		key_node = yaml_document_get_node(document, (current_node->data.mapping.pairs.start + i)->key);
		if (key_node->type != YAML_SCALAR_NODE || strcmp((char*)key_node->data.scalar.value, "socket"))
			continue ;
		params_node = yaml_document_get_node(document, (current_node->data.mapping.pairs.start + i)->value);
		if (params_node->type != YAML_MAPPING_NODE)
		{
			snprintf(error_message, PIPE_BUF, "Unexpected format for block 'socket', expected map, encountered %s\n", params_node->tag);
			write(2, error_message, strlen(error_message));
			if (socket_path)
				free(socket_path);
			return (NULL);
		}
		for (int j = 0; (params_node->data.mapping.pairs.start + j) < params_node->data.mapping.pairs.top; ++j)
		{
			key_node = yaml_document_get_node(document, (params_node->data.mapping.pairs.start + j)->key);
			if (key_node->type != YAML_SCALAR_NODE)
				continue ;
			value_node = yaml_document_get_node(document, (params_node->data.mapping.pairs.start + j)->value);
			if (!strcmp("socketpath", (char*)key_node->data.scalar.value))
			{
				if (value_node->type != YAML_SCALAR_NODE)
				{
					snprintf(error_message, PIPE_BUF, "Wrong format for socketpath, expected a scalar value, encountered %s\n", value_node->tag);
					write(2, error_message, strlen(error_message));
					if (socket_path)
						free(socket_path);
					return (NULL);
				}
				if (socket_path)
					free(socket_path);
				socket_path = strdup((char *)value_node->data.scalar.value);
				if (!socket_path)
				{
					perror("Could not copy socket path");
					return (NULL);
				}
			}
		}
	}
	if (!socket_path)
		write(2, "No socketpath encountered in configuration file\n", strlen("No socketpath encountered in configuration file\n"));
	return socket_path;
}

static char	*parse_config_yaml(FILE *config_file_handle)
{
	char			*socket_path = NULL;
	yaml_parser_t	parser;
	yaml_document_t	document;

	if (!yaml_parser_initialize(&parser))
	{
		perror("Could not initialize parser");
		return (NULL);
	}
	else
	{
		yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
		yaml_parser_set_input_file(&parser, config_file_handle);
		if (!yaml_parser_load(&parser, &document))
		{
			perror("Could not load configuration file, please verify YAML syntax");
		}
		else
		{
			socket_path = parse_document(&document);
			yaml_document_delete(&document);
		}
		yaml_parser_delete(&parser);
	}
	return (socket_path);
}

char*	parse_config(char *config_file)
{
	char	*socket_path = NULL;
	FILE	*config_file_handle = NULL;

	config_file_handle = fopen(config_file, "r");	
	if (!config_file_handle)
	{
		perror("Could not open configuration file");
		return (NULL);
	}
	socket_path = parse_config_yaml(config_file_handle);
	fclose(config_file_handle);
	return (socket_path);
}
