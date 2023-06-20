/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   program.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 19:03:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/20 14:57:18 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <string.h>
#include <signal.h>

static void	init_program(struct s_program *program)
{
	program->numprocs = 1;
	program->priority = 999;
	program->autostart = true;
	program->startsecs = 3;
	program->startretries = 3;
	program->autorestart = ONERROR;
	program->stopsignal = SIGTERM;
	program->stopwaitsecs = 5;
	program->stdoutlog = false;
	program->stderrlog = false;
	program->umask = 022;
}

bool	add_program(struct s_server *server, yaml_document_t *document, yaml_node_t *name, yaml_node_t *params)
{
	bool				ret = true;
	struct s_program	*program = NULL;

	(void)server;
	(void)document;
	(void)params;
	program = calloc(1, sizeof(*program));
	if (!program)
	{
		printf("Could not allocate program\n");
		ret = false;
	}
	else
	{
		program->name = strdup((char *)name->data.scalar.value);
		if (!program->name)
		{
			printf("Could not allocate program name\n");
			ret = false;
		}
		else
		{
			init_program(program);
			printf("Ready to parse program %s\n", program->name);
			free(program->name);
			free(program);
		}
	}
	return (ret);
}

bool	parse_programs(struct s_server *server, yaml_document_t *document, int value_index)
{
	bool		ret = true;
	yaml_node_t	*value_node = NULL;
	yaml_node_t	*key_node = NULL;
	yaml_node_t	*program_node = NULL;

	value_node = yaml_document_get_node(document, value_index);
	if (value_node->type != YAML_MAPPING_NODE)
	{
		printf("Unexpected format for block programs, expected map, encountered %s\n", value_node->tag);
		ret = false;
	}
	else
	{
		for (int i = 0; (value_node->data.mapping.pairs.start + i) < value_node->data.mapping.pairs.top; i++)
		{
			key_node = yaml_document_get_node(document, (value_node->data.mapping.pairs.start + i)->key);
			if (key_node->type == YAML_SCALAR_NODE)
			{
				program_node = yaml_document_get_node(document, (value_node->data.mapping.pairs.start + i)->value);
				if (!add_program(server, document, key_node, program_node))
				{
					ret = false;
					break ;
				}
			}
			else
			{
				printf("Incorrect format detected in programs block\n");
			}
		}
	}
	return (ret);
}

