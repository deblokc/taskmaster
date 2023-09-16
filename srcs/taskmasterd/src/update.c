/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   update.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/08/21 18:53:01 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/25 16:31:18 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <stdio.h>

static struct s_program *get_programs(yaml_document_t * document, struct s_report *reporter)
{
	struct s_server	server;
	yaml_node_t		*current_node;
	yaml_node_t		*key_node;

	bzero(&server, sizeof(server));
	init_server(&server);
	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing YAML document\n");
	report(reporter, false);
	current_node = yaml_document_get_root_node(document);
	if (!current_node)
	{
		snprintf(reporter->buffer, PIPE_BUF, "WARN: empty YAML configuration file encountered\n");
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
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing 'programs' block\n");
						report(reporter, false);
						if (!parse_programs(&server, document, (current_node->data.mapping.pairs.start + i)->value, reporter))
							break ;
					}
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "ERROR    : Wrong node format encountered in root YAML map: %s\n", key_node->data.scalar.value);
					report(reporter, false);
				}
			}
		}
		else
		{
			snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Expected map at the root of yaml document, found: %s\n", current_node->tag);
			report(reporter, true);
		}
	}
	return (server.program_tree);
}


struct s_program	*get_current_programs(struct s_server *server, struct s_report *reporter)
{
	FILE				*config_file_handle = NULL;
	yaml_parser_t		parser;
	yaml_document_t		document;
	struct s_program	*program_tree = NULL;

	config_file_handle = fopen(server->config_file, "r");
	if (!config_file_handle)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not open configuration file\n");
		report(reporter, true);
		return (program_tree);
	}
	if (!yaml_parser_initialize(&parser))
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not initialize parser\n");
		report(reporter, true);
		fclose(config_file_handle);
		return (program_tree);
	}
	yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
	yaml_parser_set_input_file(&parser, config_file_handle);
	if (!yaml_parser_load(&parser, &document))
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not load configuration file, please verify YAML syntax\n");
		report(reporter, true);
	}
	else
	{
		program_tree = get_programs(&document, reporter);
		yaml_document_delete(&document);
	}
	yaml_parser_delete(&parser);
	fclose(config_file_handle);
	return (program_tree);
}

static bool	program_issame(struct s_program *p1, struct s_program *p2)
{
	int				i;
	struct s_env	*p1_env_runner;
	struct s_env	*p2_env_runner;

	if ((p1->command && !p2->command) || (!p1->command && p2->command)
		|| (p1->args && !p2->args) || (!p1->args && p2->args)
		|| (p1->exitcodes && !p2->exitcodes) || (!p1->exitcodes && p2->exitcodes)
		|| (p1->workingdir && !p2->workingdir) || (!p1->workingdir && p2->workingdir)
		|| (p1->user && !p2->user) || (!p1->user && p2->user)
		|| (p1->group && !p2->group) || (!p1->group && p2->group)
		|| (p1->stdout_logger.logfile && !p2->stdout_logger.logfile) || (!p1->stdout_logger.logfile && p2->stdout_logger.logfile)
		|| (p1->stderr_logger.logfile && !p2->stderr_logger.logfile) || (!p1->stderr_logger.logfile && p2->stderr_logger.logfile)
		|| (p1->env && !p2->env) || (!p1->env && p2->env)
		)
		return(false);
	if (p1->command && p2->command && strcmp(p1->command, p2->command))
		return (false);
	if (p1->args && p2->args)
	{
		i = 0;
		for (; p1->args[i]; ++i)
		{
			if (!p2->args[i] || strcmp(p1->args[i], p2->args[i]))
				return (false);
		}
		if (p2->args[i] != NULL)
			return (false);
	}
	if (p1->exitcodes && p2->exitcodes)
	{
		i = 0;
		for (; p1->exitcodes[i] != -1; ++i)
		{
			if (p1->exitcodes[i] != p2->exitcodes[i])
				return (false);
		}
		if (p2->exitcodes[i] != -1)
			return (false);
	}
	if (p1->stdout_logger.logfile && p2->stdout_logger.logfile && strcmp(p1->stdout_logger.logfile, p2->stdout_logger.logfile))
		return (false);
	if (p1->stderr_logger.logfile && p2->stderr_logger.logfile && strcmp(p1->stderr_logger.logfile, p2->stderr_logger.logfile))
		return (false);
	if (p1->workingdir && p2->workingdir && strcmp(p1->workingdir, p2->workingdir))
		return (false);
	if (p1->user && p2->user && strcmp(p1->user, p2->user))
		return (false);
	if (p1->group && p2->group && strcmp(p1->group, p2->group))
		return (false);
	p1_env_runner = p1->env;
	p2_env_runner = p2->env;
	while (p1_env_runner)
	{
		if (!p2_env_runner)
			return (false);
		if (strcmp(p1_env_runner->value, p2_env_runner->value))
			return (false);
		p1_env_runner = p1_env_runner->next;
		p2_env_runner = p2_env_runner->next;
	}
	if (p2_env_runner)
		return (false);
	if (p1->numprocs != p2->numprocs
		|| p1->priority != p2->priority
		|| p1->autostart != p2->autostart
		|| p1->startsecs != p2->startsecs
		|| p1->startretries != p2->startretries
		|| p1->autorestart != p2->autorestart
		|| p1->stopsignal != p2->stopsignal
		|| p1->stopwaitsecs != p2->stopwaitsecs
		|| p1->stopasgroup != p2->stopasgroup
		|| p1->stdoutlog != p2->stdoutlog
		|| p1->stdout_logger.logfile_maxbytes != p2->stdout_logger.logfile_maxbytes
		|| p1->stdout_logger.logfile_backups != p2->stdout_logger.logfile_backups
		|| p1->stdout_logger.umask != p2->stdout_logger.umask
		|| p1->stderrlog != p2->stderrlog
		|| p1->stderr_logger.logfile_maxbytes != p2->stderr_logger.logfile_maxbytes
		|| p1->stderr_logger.logfile_backups != p2->stderr_logger.logfile_backups
		|| p1->stderr_logger.umask != p2->stderr_logger.umask
		|| p1->umask != p2->umask)
	{
		return (false);
	}
	return (true);
}

static void	find_to_stop(struct s_server *server, struct s_priority **to_stop)
{
	struct s_priority	*old_current;
	struct s_priority	*new_current;
	struct s_program	*old_runner;
	struct s_program	*new_runner;
	struct s_program	*previous_old;
	struct s_program	*previous_new;
	struct s_program	*next_old;
	struct s_program	*next_new;

	if (!to_stop || !*to_stop)
		return ;
	old_current = *to_stop;
	new_current = server->priorities;
	while (old_current)
	{
		while (new_current && new_current->priority < old_current->priority)
			new_current = new_current->next;
		if (new_current && new_current->priority == old_current->priority)
		{
			old_runner = old_current->begin;
			new_runner = new_current->begin;
			previous_old = NULL;
			previous_new = NULL;
			while (old_runner)
			{
				while (new_runner && strcmp(new_runner->name, old_runner->name) < 0)
				{
					previous_new = new_runner;
					new_runner = new_runner->next;
				}
				if (new_runner && !strcmp(new_runner->name, old_runner->name) && program_issame(new_runner, old_runner))
				{
					if (!previous_old)
						old_current->begin = old_runner->next;
					else
						previous_old->next = old_runner->next;
					if (!previous_new)
						new_current->begin = old_runner;
					else
						previous_new->next = old_runner;
					next_old = old_runner->next;
					next_new = new_runner->next;
					if (server->program_tree == new_runner)
						server->program_tree = old_runner;
					old_runner->next = new_runner->next;
					old_runner->left = new_runner->left;
					old_runner->right = new_runner->right;
					old_runner->parent = new_runner->parent;
					if (old_runner->parent)
					{
						if (old_runner->parent->left == new_runner)
							old_runner->parent->left = old_runner;
						else
							old_runner->parent->right = old_runner;

					}
					if (old_runner->left)
						old_runner->left->parent = old_runner;
					if (old_runner->right)
						old_runner->right->parent = old_runner;
					new_runner->cleaner(new_runner);
					new_runner = next_new;
					old_runner = next_old;
				}
				else
				{
					previous_old = old_runner;
					old_runner = old_runner->next;
				}
			}
		}
		old_current = old_current->next;
	}
}

static void	free_to_stop(struct s_priority *priorities)
{
	struct s_priority	*current;
	struct s_priority	*prio_next;
	struct s_program	*prog;
	struct s_program	*prog_next;

	current = priorities;
	while (current)
	{
		prog = current->begin;
		while (prog)
		{
			prog_next = prog->next;
			prog->cleaner(prog);
			prog = prog_next;
		}
		prio_next = current->next;
		free(current);
		current = prio_next;
	}
}

void	update_configuration(struct s_server *server, struct s_program *program_tree, struct s_report *reporter)
{
	struct s_program	*old_tree = server->program_tree;
	struct s_priority	*to_stop = server->priorities;

	server->priorities = NULL;
	server->program_tree = program_tree;
	for (struct s_program* current = server->begin(server); current; current = current->itnext(current))
	{
		current->stdout_logger.umask = server->umask;
		current->stderr_logger.umask = server->umask;
	}
	if (program_tree)
	{
		strcpy(reporter->buffer, "DEBUG    : Finding programs to stop\n");
		report(reporter, false);
		server->priorities = create_priorities(server, reporter);
		if (reporter->critical)
		{
			strcpy(reporter->buffer, "CRITICAL : Could not build priorities for new programs, exiting taskmasterd\n");
			report(reporter, true);
			if (server->priorities)
				server->priorities->destructor(server->priorities);
			server->delete_tree(server);
			server->priorities = to_stop;
			server->program_tree = old_tree;
			return ;
		}
		find_to_stop(server, &to_stop);
	}
	else
	{
		strcpy(reporter->buffer, "DEBUG    : No new programs\n");
		report(reporter, false);
	}
	exit_admins(to_stop);
	wait_priorities(to_stop);
	free_to_stop(to_stop);
	block_signals_thread(reporter);
	launch(server->priorities, reporter->report_fd);
	unblock_signals_thread(reporter);
}
