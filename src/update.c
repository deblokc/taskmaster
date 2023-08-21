#include "taskmaster.h"
#include <stdio.h>

static struct s_program *get_programs(yaml_document_t * document, struct s_report *reporter)
{
	struct s_server	server;
	yaml_node_t		*current_node;
	yaml_node_t		*key_node;

	bzero(&server, sizeof(server));
	snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing YAML document\n");
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
						snprintf(reporter->buffer, PIPE_BUF, "DEBUG: Parsing 'programs' block\n");
						report(reporter, false);
						if (!parse_programs(&server, document, (current_node->data.mapping.pairs.start + i)->value, reporter))
							break ;
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
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not open configuration file\n");
		report(reporter, true);
		return (program_tree);
	}
	if (!yaml_parser_initialize(&parser))
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not initialize parser\n");
		report(reporter, true);
		fclose(config_file_handle);
		return (program_tree);
	}
	yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
	yaml_parser_set_input_file(&parser, config_file_handle);
	if (!yaml_parser_load(&parser, &document))
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: Could not load configuration file, please verify YAML syntax\n");
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

bool	program_issame(struct s_program *p1, struct s_program *p2)
{
	if ((p1->command && !p2->command) || (!p1->command && p2->command)
		|| (p1->args && !p2->args) || (!p1->args && p2->args)
		|| (p1->exitcodes && !p2->exitcodes) || (!p1->exitcodes && p2->exitcodes)
		|| (p1->workingdir && !p2->workingdir) || (!p1->workingdir && p2->workingdir)
		|| (p1->user && !p2->user) || (!p1->user && p2->user)
		|| (p1->group && !p2->group) || (!p1->group && p2->group)
		)
		return(false); 
	if (p1->args && p2->args)
	{
		for (int i = 0; p1->args[i]; ++i)
		{
			if (!p2->args[i] || strcmp(p1->args[i], p2->args[i]))
				return (false);
		}
	}
	if (
}

void	find_to_stop(struct s_server *server, struct s_priority **to_stop, struct s_report *reporter)
{
	struct s_priority	*old_current;
	struct s_priority	*new_current;
	struct s_program	*old_runner;
	struct s_program	*new_runner;

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
			while (old_runner)
			{
				while (new_runner && strcmp(new_runner->name, old_runner->name) < 0)
					new_runner = new_runner->next;
				if (new_runner && !strcmp(new_runner->name, old_runner->name))
				{
					if (program_issame(new_runner, old_runner))
					{
					
					}
				}
				else
				{
					old_runner = old_runner->next;
				}
			}
		}
		old_current = old_current->next;
	}
}

void	update_configuration(struct s_server *server, struct s_program *program_tree, struct s_report *reporter)
{
	struct s_program	*old_tree = server->program_tree;
	struct s_program	*tmp_tree = NULL;
	struct s_priority	*to_stop = server->priorities;

	server->priorities = NULL;
	server->program_tree = program_tree;
	if (program_tree)
	{
		server->priorities = create_priorities(server, reporter);
		if (reporter->critical)
		{
			strcpy(reporter->buffer, "CRITICAL: Could not build priorities for new programs, exiting taskmasterd\n");
			report(reporter, true);
			if (server->priorities)
				server->priorities->destructor(server->priorities);
			server->delete_tree(server);
			server->priorities = to_stop;
			server->program_tree = old_tree;
			return ;
		}
		find_to_stop(server, &to_stop, reporter);
	}
}
