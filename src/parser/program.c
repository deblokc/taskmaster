/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   program.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 19:03:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/20 17:48:43 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <string.h>
#include <signal.h>
#include <ctype.h>

static struct s_program *cleaner(struct s_program *self)
{
	if (self->processes)
		free(self->processes);
	if (self->name)
		free(self->name);
	if (self->command)
		free(self->command);
	if (self->args)
	{
		for (int i = 0; (self->args)[i]; i++)
		{
			free((self->args)[i]);
		}
		free(self->args);
	}
	if (self->exitcodes)
		free(self->exitcodes);
	if (self->env)
	{
		for (int i = 0; (self->env)[i]; i++)
		{
			free((self->env)[i]);
		}
		free(self->env);
	}
	if (self->workingdir)
		free(self->workingdir);
	if (self->user)
		free(self->user);
	if (self->stdout_logger.logfile)
		free(self->stdout_logger.logfile);
	if (self->stderr_logger.logfile)
		free(self->stderr_logger.logfile);
	free(self);
	return (NULL);
}

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

static bool add_command(struct s_program *program, yaml_document_t *document, yaml_node_t *value)
{
	bool	ret = true;

	printf("Parsing command\n");
	(void) document;
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for command in program %s\n", program->name);
		ret = false;
	}
	else
	{
		program->command = strdup((char *)value->data.scalar.value);
		if (!program->command)
		{
			printf("Could not allocate command in program %s\n", program->name);
			ret = false;
		}
	}
	return (ret);
}

static bool add_numprocs(struct s_program *program, yaml_document_t *document, yaml_node_t *value)
{
	bool	ret = true;
	long	numprocs;

	(void) document;
	printf("Parsing numprocs\n");
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for number of processes in program %s\n", program->name);
		ret = false;
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if (!isdigit((value->data.scalar.value)[i]))
			{
				printf("Non-digit character detected for number of processes in program %s\n", program->name);
				ret = false;
				break ;
			}
		}
		if (ret)
		{
			numprocs = strtol((char *)value->data.scalar.value, NULL, 10);
			if (numprocs < 1 || numprocs > 255)
			{
				printf("Incorrect number of processes detected in program %s\n", program->name);
				ret = false;
			}
			else
			{
				program->numprocs = (int)numprocs;
			}
		}
	}
	return (ret);
}

static bool add_value(struct s_program *program, yaml_document_t *document, char *key, yaml_node_t *value)
{
	bool	ret = true;

	if (!strcmp("command", key))
		ret = add_command(program, document, value);
	else if (!strcmp("numprocs", key))
		ret = add_numprocs(program, document, value);
//	else if (!strcmp("priority", key))
//		ret = add_priority(program, document, value);
//	if (!strcmp(command, key))


	return (ret);
}

static bool parse_values(struct s_program *program, yaml_document_t *document, yaml_node_t *params)
{
	bool		ret = true;
	yaml_node_t *key;
	yaml_node_t *value;
	for (int i = 0; (params->data.mapping.pairs.start + i) < params->data.mapping.pairs.top; i++)
	{
		key = yaml_document_get_node(document, (params->data.mapping.pairs.start + i)->key);
		value = yaml_document_get_node(document, (params->data.mapping.pairs.start + i)->value);
		if (key->type != YAML_SCALAR_NODE)
		{
			printf("Invalid key type encountered for parameters in program %s\n", program->name);
			ret = false;
			break ;
		}
		if (!add_value(program, document, (char *)key->data.scalar.value, value))
		{
			ret = false;
			break ;
		}

	}
	return (ret);
}

static bool	add_program(struct s_server *server, yaml_document_t *document, yaml_node_t *name, yaml_node_t *params)
{
	bool				ret = true;
	struct s_program	*program = NULL;

	(void)server;
	if (params->type != YAML_MAPPING_NODE)
	{
		printf("Wrong node type for program %s, expected map, encountered: %s\n", name->data.scalar.value, params->tag);
		ret = false;
	}
	else
	{
		program = calloc(1, sizeof(*program));
		if (!program)
		{
			printf("Could not allocate program\n");
			ret = false;
		}
		else
		{
			program->cleaner = cleaner;
			program->name = strdup((char *)name->data.scalar.value);
			if (!program->name)
			{
				printf("Could not allocate program name\n");
				program->cleaner(program);
				ret = false;
			}
			else
			{
				init_program(program);
				if (!parse_values(program, document, params))
				{
					printf("Error while parsing program %s\n", program->name);
					program->cleaner(program);
				}
				else
				{
					printf("Valid program %s\n", program->name);
					program->cleaner(program);
				}
			}
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

	printf("Parsing programs\n");
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

