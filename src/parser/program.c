/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   program.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 19:03:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/21 21:27:16 by bdetune          ###   ########.fr       */
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

static bool add_char(char const *program_name, char const *field_name, char **target, yaml_node_t *value)
{
	bool	ret = true;

	printf("Parsing %s in program %s\n", field_name, program_name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for field %s in program %s, expected a scalar value, encountered %s\n",field_name, program_name, value->tag);
	}
	else
	{
		*target = strdup((char *)value->data.scalar.value);
		if (!*target)
		{
			printf("Could not allocate %s in program %s\n", field_name, program_name);
			ret = false;
		}
	}
	return (ret);
}

static bool	add_number(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max)
{
	bool	ret = true;
	long	nb;

	printf("Parsing %s in program %s\n", field_name, program_name);
	if (value->type != YAML_SCALAR_NODE)
	{
		ret = false;
		printf("Wrong format for field %s in program %s, expected a scalar value, encountered %s\n",field_name, program_name, value->tag);
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if (!isdigit((value->data.scalar.value)[i]))
			{
				ret = false;
				printf("Wrong format for field %s in program %s, provided value is not a digit, falling back to default value %d\n", field_name, program_name, *target);
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 10);
			if (nb < min || nb > max)
			{
				printf("Wrong value for field %s in program %s, provided value must range between %ld and %ld, falling back to default value %d\n", field_name, program_name, min, max, *target);
				ret = false;
			}
			else
			{
				*target = (int)nb;
			}
		}
	}
	return (ret);
}

static bool	add_octal(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max)
{
	bool	ret = true;
	long	nb;

	printf("Parsing %s in program %s\n", field_name, program_name);
	if (value->type != YAML_SCALAR_NODE)
	{
		ret = false;
		printf("Wrong format for field %s in program %s, expected a scalar value, encountered %s\n",field_name, program_name, value->tag);
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if ((value->data.scalar.value)[i] < '0' || (value->data.scalar.value)[i] > '7')
			{
				ret = false;
				printf("Wrong format for field %s in program %s, provided value is not a digit in base 8, falling back to default value %d\n", field_name, program_name, *target);
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 8);
			if (nb < min || nb > max)
			{
				printf("Wrong value for field %s in program %s, provided value must range between %ld and %ld, falling back to default value %d\n", field_name, program_name, min, max, *target);
				ret = false;
			}
			else
			{
				*target = (int)nb;
			}
		}
	}
	return (ret);
}


static void	add_bool(char const *program_name, char const *field_name, bool *target, yaml_node_t *value)
{
	printf("Parsing %s in program %s\n", field_name, program_name);
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for field %s in program %s, expected a scalar value, encountered %s\n",field_name, program_name, value->tag);
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "true")
				|| !strcmp((char *)value->data.scalar.value, "True")
				|| !strcmp((char *)value->data.scalar.value, "on")
				|| !strcmp((char *)value->data.scalar.value, "yes"))
			*target = true;
		else if (!strcmp((char *)value->data.scalar.value, "false")
				|| !strcmp((char *)value->data.scalar.value, "False")
				|| !strcmp((char *)value->data.scalar.value, "off")
				|| !strcmp((char *)value->data.scalar.value, "no"))
			*target = false;
		else
			printf("Wrong format for field %s in program %s, accepted values are:\n\t- For positive values: true, True, on, yes\n\t- For negative values: false, False, off, no\nfalling back to default value %s\n", field_name, program_name, *target?"true":"false");
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
	bool		isvalid = true;
	bool		ret = true;
	yaml_node_t	*current_exit_code_node;
	long		number_exitcodes;

	printf("Parsing exitcodes in program %s\n", program->name);
	if (value->type != YAML_SEQUENCE_NODE)
	{
		printf("Wrong format for exitcodes list in program %s, expected array, encountered %s, falling back to default value 0\n", program->name, value->tag);
	}
	else
	{
		number_exitcodes = value->data.sequence.items.top - value->data.sequence.items.start;
		program->exitcodes = calloc((size_t)(number_exitcodes + 1), sizeof(*(program->exitcodes)));
		if (!program->exitcodes)
		{
			printf("Could not allocate space for exitcodes in program %s\n", program->name);
			ret = false;
		}
		else
		{
			program->exitcodes[number_exitcodes] = -1;
			for (int i = 0; (value->data.sequence.items.start + i) < value->data.sequence.items.top; i++)
			{
				current_exit_code_node =  yaml_document_get_node(document, *(value->data.sequence.items.start + i));
				isvalid = add_number(program->name, "exitcodes", &(program->exitcodes[i]), current_exit_code_node, 0, INT_MAX);
				if (!isvalid)
				{
					free(program->exitcodes);
					program->exitcodes = NULL;
					break ;
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


static bool add_value(struct s_program *program, yaml_document_t *document, char *key, yaml_node_t *value)
{
	bool	ret = true;

	if (!strcmp("command", key))
		ret = add_char(program->name, "command", &program->command, value);
	else if (!strcmp("numprocs", key))
		add_number(program->name, "numprocs", &program->numprocs, value, 1, 255);
	else if (!strcmp("priority", key))
		add_number(program->name, "priority", &program->priority, value, 0, 999);
	else if (!strcmp("autostart", key))
		add_bool(program->name, "autostart", &program->autostart, value);
	else if (!strcmp("startsecs", key))
		add_number(program->name, "startsecs", &program->startsecs, value, 0, 300);
	else if (!strcmp("startretries", key))
		add_number(program->name, "startretries", &program->startretries, value, 0, 10);
	else if (!strcmp("autorestart", key))
		add_autorestart(program, value);
	else if (!strcmp("exitcodes", key))
		ret = add_exitcodes(program, document, value);
	else if (!strcmp("stopsignal", key))
		add_stopsignal(program, value);
	else if (!strcmp("stopwaitsecs", key))
		add_number(program->name, "stopwaitsecs", &program->stopwaitsecs, value, 0, 300);
	else if (!strcmp("stdoutlog", key))
		add_bool(program->name, "stdoutlog", &program->stdoutlog, value);
	else if (!strcmp("stdout_logfile", key))
		ret = add_char(program->name, "stdout_logfile", &program->stdout_logger.logfile, value);
	else if (!strcmp("stdout_logfile_maxbytes", key))
		add_number(program->name, "stdout_logfile_maxbytes", &program->stdout_logger.logfile_maxbytes, value, 100, 1024*1024*1024);
	else if (!strcmp("stdout_logfile_backups", key))
		add_number(program->name, "stdout_logfile_backups", &program->stdout_logger.logfile_backups, value, 0, 100);
	else if (!strcmp("stderrlog", key))
		add_bool(program->name, "stderrlog", &program->stderrlog, value);
	else if (!strcmp("stderr_logfile", key))
		ret = add_char(program->name, "stderr_logfile", &program->stderr_logger.logfile, value);
	else if (!strcmp("stderr_logfile_maxbytes", key))
		add_number(program->name, "stderr_logfile_maxbytes", &program->stderr_logger.logfile_maxbytes, value, 100, 1024*1024*1024);
	else if (!strcmp("stderr_logfile_backups", key))
		add_number(program->name, "stderr_logfile_backups", &program->stderr_logger.logfile_backups, value, 0, 100);
	else if (!strcmp("workingdir", key))
		ret = add_char(program->name, "workingdir", &program->workingdir, value);
	else if (!strcmp("umask", key))
		add_octal(program->name, "umask", &program->umask, value, 0, 0777);
	else if (!strcmp("user", key))
		ret = add_char(program->name, "user", &program->user, value);
	else
		printf("Unknown field %s in program %s\n", key, program->name);
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
