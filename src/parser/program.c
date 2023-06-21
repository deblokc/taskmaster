/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   program.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 19:03:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/21 19:47:56 by bdetune          ###   ########.fr       */
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
	program->stdoutlog = true;
	default_logger(&program->stdout_logger);
	program->stderrlog = true;
	default_logger(&program->stderr_logger);
	program->umask = 022;
}

static bool add_command(struct s_program *program, yaml_node_t *value)
{
	bool	ret = true;

	printf("Parsing command in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for command in program %s\n", program->name);
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

static int	is_valid_number(char* number, long min, long max)
{
	long	ret = 0;

	for (int i = 0; number[i]; i++)
	{
		if (!isdigit(number[i]))
		{
			ret = -1;
			break ;
		}
	}
	if (!ret)
	{
		ret = strtol(number, NULL, 10);
		if (ret < min || ret > max)
			ret = -1;
	}
	return ((int)ret);
}

static void add_numprocs(struct s_program *program, yaml_document_t *document, yaml_node_t *value)
{
	int		numprocs;

	(void) document;
	printf("Parsing numprocs in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for number of processes in program %s, falling back to default value %d\n", program->name, program->numprocs);
	}
	else
	{
		numprocs = is_valid_number((char *)value->data.scalar.value, 1, 255);
		if (numprocs == -1)
			printf("Wrong value for number of processes in program %s, accepted value must range between 1 and 255, falling back to default value %d\n", program->name, program->numprocs);
		else
			program->numprocs = numprocs;
	}
}

static void add_priority(struct s_program *program, yaml_node_t *value)
{
	int		priority;

	printf("Parsing priority in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for priority in program %s, falling back to default value %d\n", program->name, program->priority);
	}
	else
	{
		priority = is_valid_number((char *)value->data.scalar.value, 0, 999);
		if (priority == -1)
			printf("Wrong value for priority in program %s, accepted value must range between 0 and 999, falling back to default value %d\n", program->name, program->priority);
		else
			program->priority = priority;
	}
}

static void add_autostart(struct s_program *program, yaml_node_t *value)
{
	printf("Parsing autostart in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for autostart in program %s, falling back to default value %s\n", program->name, program->autostart?"true":"false");
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "true")
				|| !strcmp((char *)value->data.scalar.value, "True")
				|| !strcmp((char *)value->data.scalar.value, "on")
				|| !strcmp((char *)value->data.scalar.value, "yes"))
			program->autostart = true;
		else if (!strcmp((char *)value->data.scalar.value, "false")
				|| !strcmp((char *)value->data.scalar.value, "False")
				|| !strcmp((char *)value->data.scalar.value, "off")
				|| !strcmp((char *)value->data.scalar.value, "no"))
			program->autostart = true;
		else
			printf("Wrong format for autostart in program %s, accepted values are:\n\t- For positive values: true, True, on, yes\n\t- For negative values: false, False, off, no\nfalling back to default value %s\n", program->name, program->autostart?"true":"false");
	}
}

static void add_startsecs(struct s_program *program, yaml_node_t *value)
{
	int		startsecs;

	printf("Parsing startsecs in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for startsecs in program %s, falling back to default value %d\n", program->name, program->startsecs);
	}
	else
	{
		startsecs = is_valid_number((char *)value->data.scalar.value, 0, 300);
		if (startsecs == -1)
			printf("Wrong value for startsecs in program %s, accepted value must range between 0 and 300, falling back to default value %d\n", program->name, program->startsecs);
		else
			program->startsecs = startsecs;
	}
}

static void add_startretries(struct s_program *program, yaml_node_t *value)
{
	int		startretries;

	printf("Parsing startretries in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for startretries in program %s, falling back to default value %d\n", program->name, program->startretries);
	}
	else
	{
		startretries = is_valid_number((char *)value->data.scalar.value, 0, 10);
		if (startretries == -1)
			printf("Wrong value for startretries in program %s, accepted value must range between 0 and 10, falling back to default value %d\n", program->name, program->startretries);
		else
			program->startretries = startretries;
	}
}

static void add_autorestart(struct s_program *program, yaml_node_t *value)
{
	printf("Parsing autorestart in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for autorestart in program %s, falling back to default value %s\n", program->name, program->autorestart==NEVER?"never":(program->autorestart==ONERROR?"onerror":"always"));
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "never"))
			program->autorestart = NEVER;
		else if (!strcmp((char *)value->data.scalar.value, "onerror"))
			program->autorestart = ONERROR;
		else if (!strcmp((char *)value->data.scalar.value, "always"))
			program->autorestart = ALWAYS;
		else
			printf("Wrong format for autorestart in program %s, accepted values are:\n\t- never\n\t- onerror\n\t- always\nfalling back to default value %s\n", program->name, program->autorestart==NEVER?"never":(program->autorestart==ONERROR?"onerror":"always"));
	}
}

static bool	add_exitcodes(struct s_program *program, yaml_document_t *document, yaml_node_t *value)
{
	bool		ret = true;
	yaml_node_t	*current_exit_code_node;
	int			current_exit_code;
	long		number_exitcodes;

	printf("Parsing exitcodes in program %s\n", program->name);
	if (value->type != YAML_SEQUENCE_NODE)
	{
		printf("Wrong format for exitcodes list in program %s, expected array, encountered %s, falling back to default value 0\n", program->name, value->tag);
	}
	else
	{
		number_exitcodes = value->data.sequence.items.top - value->data.sequence.items.start;
		program->exitcodes = malloc(sizeof(*(program->exitcodes)) * (number_exitcodes + 1));
		if (!program->exitcodes)
		{
			printf("Could not allocate space for exitcodes in program %s\n", program->name);
			ret = false;
		}
		else
		{
			memset(program->exitcodes, -1, number_exitcodes + 1);
			for (int i = 0; (value->data.sequence.items.start + i) < value->data.sequence.items.top; i++)
			{
				current_exit_code_node =  yaml_document_get_node(document, *(value->data.sequence.items.start + i));
				if (current_exit_code_node->type != YAML_SCALAR_NODE)
				{
					printf("Wrong format in list of exitcodes in program %s, expected scalar value, encountered %s, falling back to default value 0\n", program->name, current_exit_code_node->tag);
					free(program->exitcodes);
					program->exitcodes = NULL;
					break;
				}
				else
				{
					current_exit_code = is_valid_number((char*)current_exit_code_node->data.scalar.value, 0, INT_MAX);
					if (current_exit_code == -1)
					{
						printf("Wrong value in list of exitcodes in program %s, accepted values range from 0 to %d, falling back to default value 0\n", program->name, INT_MAX);
						free(program->exitcodes);
						program->exitcodes = NULL;
						break;
					}
					else
					{
						program->exitcodes[i] = current_exit_code;
					}
				}
			}
		}
	}
	return (ret);
}

static void add_stopsignal(struct s_program *program, yaml_node_t *value)
{
	printf("Parsing stopsignal in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for stopsignal in program %s, falling back to default value TERM\n", program->name);
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "TERM"))
			program->stopsignal = SIGTERM;
		else if (!strcmp((char *)value->data.scalar.value, "HUP"))
			program->stopsignal = SIGHUP;
		else if (!strcmp((char *)value->data.scalar.value, "INT"))
			program->stopsignal = SIGINT;
		else if (!strcmp((char *)value->data.scalar.value, "QUIT"))
			program->stopsignal = SIGQUIT;
		else if (!strcmp((char *)value->data.scalar.value, "KILL"))
			program->stopsignal = SIGKILL;
		else if (!strcmp((char *)value->data.scalar.value, "USR1"))
			program->stopsignal = SIGUSR1;
		else if (!strcmp((char *)value->data.scalar.value, "USR2"))
			program->stopsignal = SIGUSR2;
		else
			printf("Wrong format for stopsignal in program %s, accepted values are:\n\t- TERM\n\t- HUP\n\t- INT\n\t- QUIT\n\t- KILL\n\t- USR1\n\t- USR2\nfalling back to default value TERM\n", program->name);
	}
}

static void add_stopwaitsecs(struct s_program *program, yaml_node_t *value)
{
	int		stopwaitsecs;

	printf("Parsing stopwaitsecs in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for stopwaitsecs in program %s, falling back to default value %d\n", program->name, program->stopwaitsecs);
	}
	else
	{
		stopwaitsecs = is_valid_number((char *)value->data.scalar.value, 0, 300);
		if (stopwaitsecs == -1)
			printf("Wrong value for stopwaitsecs in program %s, accepted value must range between 0 and 300, falling back to default value %d\n", program->name, program->stopwaitsecs);
		else
			program->stopwaitsecs = stopwaitsecs;
	}
}

static void add_stdoutlog(struct s_program *program, yaml_node_t *value)
{
	printf("Parsing stdoutlog in program %s\n", program->name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for stdoutlog in program %s, falling back to default value %s\n", program->name, program->stdoutlog?"true":"false");
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "true")
				|| !strcmp((char *)value->data.scalar.value, "True")
				|| !strcmp((char *)value->data.scalar.value, "on")
				|| !strcmp((char *)value->data.scalar.value, "yes"))
			program->stdoutlog = true;
		else if (!strcmp((char *)value->data.scalar.value, "false")
				|| !strcmp((char *)value->data.scalar.value, "False")
				|| !strcmp((char *)value->data.scalar.value, "off")
				|| !strcmp((char *)value->data.scalar.value, "no"))
			program->stdoutlog = true;
		else
			printf("Wrong format for stdoutlog in program %s, accepted values are:\n\t- For positive values: true, True, on, yes\n\t- For negative values: false, False, off, no\nfalling back to default value %s\n", program->name, program->stdoutlog?"true":"false");
	}
}


static bool add_value(struct s_program *program, yaml_document_t *document, char *key, yaml_node_t *value)
{
	bool	ret = true;

	if (!strcmp("command", key))
		ret = add_command(program, value);
	else if (!strcmp("numprocs", key))
		add_numprocs(program, document, value);
	else if (!strcmp("priority", key))
		add_priority(program, value);
	else if (!strcmp("autostart", key))
		add_autostart(program, value);
	else if (!strcmp("startsecs", key))
		add_startsecs(program, value);
	else if (!strcmp("startretries", key))
		add_startretries(program, value);
	else if (!strcmp("autorestart", key))
		add_autorestart(program, value);
	else if (!strcmp("exitcodes", key))
		ret = add_exitcodes(program, document, value);
	else if (!strcmp("stopsignal", key))
		add_stopsignal(program, value);
	else if (!strcmp("stopwaitsecs", key))
		add_stopwaitsecs(program, value);
	else if (!strcmp("stdoutlog", key))
		add_stdoutlog(program, value);
	else if (!strcmp("stdout_logfile", key))
		add_stdoutlog(program, value);







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

