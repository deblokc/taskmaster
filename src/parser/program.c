/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   program.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 19:03:58 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/25 19:29:31 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <string.h>
#include <signal.h>
#include <ctype.h>

static struct s_program *cleaner(struct s_program *self)
{
	if (self->processes) {
		free(self->processes);
	}
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
		free_s_env(self->env);
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

static void	print(struct s_program* self)
{
	struct s_env	*current;

	printf("\n************************** PROGRAM **************************\n");
	printf("\tName                  :\t%s\n", self->name? self->name:"(NULL)");
	printf("\tCommand               :\t%s\n", self->command? self->command:"(NULL)");
	printf("\tNumprocs              :\t%d\n", self->numprocs);
	printf("\tPriority              :\t%d\n", self->priority);
	printf("\tAutostart             :\t%s\n", self->autostart ? "true" : "false");
	printf("\tStartsecs             :\t%d\n", self->startsecs);
	printf("\tStartretries          :\t%d\n", self->startretries);
	printf("\tAutorestart           :\t%s\n", self->autorestart == NEVER ? "never": (self->autorestart == ONERROR? "onerror" : "always"));
	if (self->exitcodes)
	{
		printf("\tExitcodes             :\n");
		for (int i = 0; self->exitcodes[i] != -1; i++)
		{
			printf("\t                       \t- %d\n", self->exitcodes[i]);
		}
	}
	else
	{
		printf("\tExitcodes             :\n\t                       \t- 0\n");
	}
	switch (self->stopsignal){
		case SIGHUP:
			printf("\tStopsignal            :\tSIGHUP\n");
			break;
		case SIGINT:
			printf("\tStopsignal            :\tSIGINT\n");
			break;
		case SIGQUIT:
			printf("\tStopsignal            :\tSIGQUIT\n");
			break;
		case SIGKILL:
			printf("\tStopsignal            :\tSIGKILL\n");
			break;
		case SIGUSR1:
			printf("\tStopsignal            :\tSIGUSR1\n");
			break;
		case SIGUSR2:
			printf("\tStopsignal            :\tSIGUSR2\n");
			break;
		default:
			printf("\tStopsignal            :\tSIGTERM\n");
			break;
	}
	printf("\tStopwaitsecs          :\t%d\n", self->stopwaitsecs);
	printf("\tStopasgroup           :\t%s\n", self->stopasgroup ? "true" : "false");
	if (self->stdoutlog)
	{
		printf("\tStdoutlog             :\ttrue\n");
		printf("\tStdoutlogfile         :\t%s\n", self->stdout_logger.logfile ? self->stdout_logger.logfile: "(default location)");
		printf("\tStdoutlogfile_maxbytes:\t%d\n", self->stdout_logger.logfile_maxbytes);
		printf("\tStdoutlogfile_backups :\t%d\n", self->stdout_logger.logfile_backups);
	}
	else
	{
		printf("\tStdoutlog             :\tfalse\n");
	}
	if (self->stderrlog)
	{
		printf("\tStderrlog             :\ttrue\n");
		printf("\tStderrlogfile         :\t%s\n", self->stderr_logger.logfile ? self->stderr_logger.logfile: "(default location)");
		printf("\tStderrlogfile_maxbytes:\t%d\n", self->stderr_logger.logfile_maxbytes);
		printf("\tStderrlogfile_backups :\t%d\n", self->stderr_logger.logfile_backups);
	}
	else
	{
		printf("\tStderrlog             :\tfalse\n");
	}
	if (self->env)
	{
		printf("\tenv                   :\t\n");
		current = self->env;
		while (current)
		{
			printf("\t                       \t- %s\n", current->value);
			current = current->next;
		}
	}
	else
	{
		printf("\tenv                   :\t(NULL)\n");
	}
	printf("\tworkingdir            :\t%s\n", self->workingdir ? self->workingdir : "(default location)");
	printf("\tumask                 :\t%o\n", (unsigned int)self->umask);
	printf("\tuser                  :\t%s\n", self->user ? self->user : "(default user)");
	printf("\tgroup                 :\t%s\n", self->group ? self->group : "(default group)");
}

static void	init_program(struct s_program *program)
{
	register_treefn_prog(program);
	program->print = print;
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

static bool add_autorestart(struct s_program *program, yaml_node_t *value, struct s_report *reporter)
{
	bool	ret = true;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing autorestart in program %s\n", program->name);
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong format for autorestart in program '%s', expected a scalar value, encountered %s\n", program->name, value->tag);
		report(reporter, false);
		ret = false;
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
		{
			snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong format for autorestart in program %s, accepted values are:\n\t- never\n\t- onerror\n\t- always\n", program->name);
			report(reporter, false);
			ret = false;
		}
	}
	return (ret);
}

static bool	add_exitcodes(struct s_program *program, yaml_document_t *document, yaml_node_t *value, struct s_report *reporter)
{
	bool		isvalid = true;
	bool		ret = true;
	yaml_node_t	*current_exit_code_node;
	long		number_exitcodes;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing exitcodes in program %s\n", program->name);
	report(reporter, false);
	if (value->type != YAML_SEQUENCE_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong format for exitcodes list in program '%s', expected an array, encountered %s\n", program->name, value->tag);
		report(reporter, false);
		ret = false;
	}
	else
	{
		number_exitcodes = value->data.sequence.items.top - value->data.sequence.items.start;
		program->exitcodes = calloc((size_t)(number_exitcodes + 1), sizeof(*(program->exitcodes)));
		if (!program->exitcodes)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not allocate space for exitcodes in program '%s'\n", program->name);
			report(reporter, true);
			ret = false;
		}
		else
		{
			program->exitcodes[number_exitcodes] = -1;
			for (int i = 0; (value->data.sequence.items.start + i) < value->data.sequence.items.top; i++)
			{
				current_exit_code_node =  yaml_document_get_node(document, *(value->data.sequence.items.start + i));
				isvalid = add_number(program->name, "exitcodes", &(program->exitcodes[i]), current_exit_code_node, 0, INT_MAX, reporter);
				if (!isvalid)
				{
					free(program->exitcodes);
					program->exitcodes = NULL;
					ret = false;
					break ;
				}
			}
		}
	}
	return (ret);
}

static bool add_stopsignal(struct s_program *program, yaml_node_t *value, struct s_report *reporter)
{
	bool	ret = true;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing stopsignal in program %s\n", program->name);
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong format for stopsignal in program '%s', expected a scalar value, encountered %s\n", program->name, value->tag);
		report(reporter, false);
		ret = false;
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
		{
			snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong format for stopsignal in program '%s', accepted values are: TERM ; HUP ; INT ; QUIT ; KILL ; USR1 ; USR2\n", program->name);
			report(reporter, false);
			ret = false;
		}
	}
	return (ret);
}


static bool add_value(struct s_program *program, yaml_document_t *document, char *key, yaml_node_t *value, struct s_report *reporter)
{
	bool	ret = true;

	if (!strcmp("command", key))
		ret = add_char(program->name, "command", &program->command, value, reporter);
	else if (!strcmp("numprocs", key))
		ret = add_number(program->name, "numprocs", &program->numprocs, value, 1, 255, reporter);
	else if (!strcmp("priority", key))
		ret = add_number(program->name, "priority", &program->priority, value, 0, 999, reporter);
	else if (!strcmp("autostart", key))
		ret = add_bool(program->name, "autostart", &program->autostart, value, reporter);
	else if (!strcmp("startsecs", key))
		ret = add_number(program->name, "startsecs", &program->startsecs, value, 0, 300, reporter);
	else if (!strcmp("startretries", key))
		ret = add_number(program->name, "startretries", &program->startretries, value, 0, 10, reporter);
	else if (!strcmp("autorestart", key))
		add_autorestart(program, value, reporter);
	else if (!strcmp("exitcodes", key))
		ret = add_exitcodes(program, document, value, reporter);
	else if (!strcmp("stopsignal", key))
		add_stopsignal(program, value, reporter);
	else if (!strcmp("stopwaitsecs", key))
		ret = add_number(program->name, "stopwaitsecs", &program->stopwaitsecs, value, 0, 300, reporter);
	else if (!strcmp("stopasgroup", key))
		ret = add_bool(program->name, "stopasgroup", &program->stopasgroup, value, reporter);
	else if (!strcmp("stdoutlog", key))
		ret = add_bool(program->name, "stdoutlog", &program->stdoutlog, value, reporter);
	else if (!strcmp("stdout_logfile", key))
		ret = add_char(program->name, "stdout_logfile", &program->stdout_logger.logfile, value, reporter);
	else if (!strcmp("stdout_logfile_maxbytes", key))
		ret = add_number(program->name, "stdout_logfile_maxbytes", &program->stdout_logger.logfile_maxbytes, value, 100, 1024*1024*1024, reporter);
	else if (!strcmp("stdout_logfile_backups", key))
		ret = add_number(program->name, "stdout_logfile_backups", &program->stdout_logger.logfile_backups, value, 0, 100, reporter);
	else if (!strcmp("stderrlog", key))
		ret = add_bool(program->name, "stderrlog", &program->stderrlog, value, reporter);
	else if (!strcmp("stderr_logfile", key))
		ret = add_char(program->name, "stderr_logfile", &program->stderr_logger.logfile, value, reporter);
	else if (!strcmp("stderr_logfile_maxbytes", key))
		ret = add_number(program->name, "stderr_logfile_maxbytes", &program->stderr_logger.logfile_maxbytes, value, 100, 1024*1024*1024, reporter);
	else if (!strcmp("stderr_logfile_backups", key))
		ret = add_number(program->name, "stderr_logfile_backups", &program->stderr_logger.logfile_backups, value, 0, 100, reporter);
	else if (!strcmp("workingdir", key))
		ret = add_char(program->name, "workingdir", &program->workingdir, value, reporter);
	else if (!strcmp("umask", key))
		ret = add_octal(program->name, "umask", &program->umask, value, 0, 0777, reporter);
	else if (!strcmp("env", key))
		ret = parse_env(program->name, value, document, &program->env, reporter);
	else if (!strcmp("user", key))
		ret = add_char(program->name, "user", &program->user, value, reporter);
	else if (!strcmp("group", key))
		ret = add_char(program->name, "group", &program->group, value, reporter);
	else
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARNING: Encountered unknown key '%s' in program '%s'\n", key, program->name);
		report(reporter, false);
	}
	return (ret);
}

static bool parse_values(struct s_program *program, yaml_document_t *document, yaml_node_t *params, struct s_report *reporter)
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
			snprintf(reporter->buffer, PIPE_BUF, "ERROR: Incorrect format detected for parameters in program '%s', expected keys to be scalars, encountered: %s\n", program->name, key->tag);
			report(reporter, false);
			ret = false;
			break ;
		}
		if (!add_value(program, document, (char *)key->data.scalar.value, value, reporter))
		{
			ret = false;
			break ;
		}

	}
	return (ret);
}

static void	add_program(struct s_server *server, yaml_document_t *document, yaml_node_t *name, yaml_node_t *params, struct s_report *reporter)
{
	struct s_program	*program = NULL;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing program '%s'\n", name->data.scalar.value);
	report(reporter, false);
	if (params->type != YAML_MAPPING_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "ERROR: Wrong node type for program '%s', expected map, encountered: %s, ignoring entry\n", name->data.scalar.value, params->tag);
		report(reporter, false);
	}
	else
	{
		program = calloc(1, sizeof(*program));
		if (!program)
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: could not allocate node for program '%s'\n", name->data.scalar.value);
			report(reporter, true);
		}
		else
		{
			program->cleaner = cleaner;
			program->name = strdup((char *)name->data.scalar.value);
			if (!program->name)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: could not allocate spce for program name for program '%s'\n", name->data.scalar.value);
				report(reporter, true);
				program->cleaner(program);
			}
			else
			{
				init_program(program);
				if (!parse_values(program, document, params, reporter))
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR: ignoring program '%s'\n", name->data.scalar.value);
					report(reporter, false);
					program->cleaner(program);
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "DEBUG: inserting valid program '%s' in tree\n", name->data.scalar.value);
					report(reporter, false);
					server->insert(server, program, reporter);
				}
			}
		}
	}
}

bool	parse_programs(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter)
{
	bool		ret = true;
	yaml_node_t	*value_node = NULL;
	yaml_node_t	*key_node = NULL;
	yaml_node_t	*program_node = NULL;

	value_node = yaml_document_get_node(document, value_index);
	if (value_node->type != YAML_MAPPING_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Unexpected format for block programs, expected map, encountered %s\n", value_node->tag);
		report(reporter, true);
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
				add_program(server, document, key_node, program_node, reporter);
				if (reporter->critical)
				{
					ret = false;
					break ;
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "WARNING: Incorrect format detected in programs block, expected keys to be scalars, encountered: %s\n" , key_node->tag);
				report(reporter, false);
			}
		}
	}
	return (ret);
}
