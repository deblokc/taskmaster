/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 17:17:31 by bdetune           #+#    #+#             */
/*   Updated: 2023/09/18 20:08:38 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool add_char(char const *program_name, char const *field_name, char **target, yaml_node_t *value, struct s_report *reporter, int max_size)
{
	size_t	len;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing %s in %s'%s'\n", field_name, program_name?"program ":"", program_name?program_name:"server");
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format for field %s in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", value->tag);
		report(reporter, program_name ? false : true );
		return (false);
	}
	len = strlen((char *)value->data.scalar.value);
	if (max_size != -1 && len > (size_t)max_size)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Field %s in %s'%s' too long, maximum size is %d, encountered value of size %lu\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program " : "", program_name ? program_name : "server", max_size, len);
		report(reporter, program_name ? false : true );
		return (false);
	}
	*target = strdup((char *)value->data.scalar.value);
	if (!*target)
	{
		snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate %s in %s'%s'\n", field_name, program_name?"program ":"", program_name?program_name:"server");
		report(reporter, true);
		return (false);
	}
	return (true);
}

bool	add_number(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter)
{
	bool	ret = true;
	long	nb;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing %s in %s'%s'\n", field_name, program_name?"program ":"", program_name?program_name:"server");
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format for field %s in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", value->tag);
		report(reporter, program_name ? false : true );
		ret = false;
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if (!isdigit((value->data.scalar.value)[i]))
			{
				snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong value for field %s in %s'%s', provided value is not a number\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server");
				report(reporter, program_name ? false : true );
				ret = false;
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 10);
			if (nb < min || nb > max)
			{
				snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong value for field %s in %s%s, provided value must range between %ld and %ld\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", min, max);
				report(reporter, program_name ? false : true );
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

bool	add_octal(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter)
{
	bool	ret = true;
	long	nb;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing %s in %s'%s'\n", field_name, program_name?"program ":"", program_name?program_name:"server");
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format for field %s in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", value->tag);
		report(reporter, program_name ? false : true );
		ret = false;
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if ((value->data.scalar.value)[i] < '0' || (value->data.scalar.value)[i] > '7')
			{
				snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong value for field %s in %s'%s', provided value is not a number in base 8\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server");
				report(reporter, program_name ? false : true );
				ret = false;
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 8);
			if (nb < min || nb > max)
			{
				snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong value for field %s in %s%s, provided value must range between %ld and %ld\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", min, max);
				report(reporter, program_name ? false : true );
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


bool	add_bool(char const *program_name, char const *field_name, bool *target, yaml_node_t *value, struct s_report *reporter)
{
	bool	ret = true;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing %s in %s'%s'\n", field_name, program_name?"program ":"", program_name?program_name:"server");
	report(reporter, false);
	if (value->type != YAML_SCALAR_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format for field %s in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server", value->tag);
		report(reporter, program_name ? false : true );
		ret = false;
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
		{
			snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong value for field %s in %s%s, accepted values are:\n\t- For positive values: true, True, on, yes\n\t- For negative values: false, False, off, no\n", program_name ? "ERROR   " : "CRITICAL", field_name, program_name ? "program" : "", program_name ? program_name : "server");
			report(reporter, program_name ? false : true );
			ret = false;
		}
	}
	return (ret);
}

bool	parse_env(char const *program_name, yaml_node_t *map, yaml_document_t *document, struct s_env **dest, struct s_report *reporter)
{
	bool			ret = true;
	yaml_node_t		*key;
	yaml_node_t		*value;
	struct s_env	*current = NULL;
	struct s_env	*position = NULL;

	snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Parsing environment in %s'%s'\n", program_name?"program ":"", program_name?program_name:"server");
	report(reporter, false);
	if (map->type != YAML_MAPPING_NODE)
	{
		snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format for environment in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", program_name ? "program" : "", program_name ? program_name : "server", map->tag);
		report(reporter, program_name ? false : true );
		ret = false;
	}
	else
	{
		for (int i = 0; (map->data.mapping.pairs.start + i) < map->data.mapping.pairs.top; i++)
		{
			key = yaml_document_get_node(document, (map->data.mapping.pairs.start + i)->key);
			value = yaml_document_get_node(document, (map->data.mapping.pairs.start + i)->value);
			if (key->type == YAML_SCALAR_NODE)
			{
				if (value->type == YAML_SCALAR_NODE)
				{
					if (strchr((char*)key->data.scalar.value, '='))
					{
						snprintf(reporter->buffer, PIPE_BUF, "%s : Encountered illegal character '=' in environment for key %s in %s'%s'\n", program_name ? "ERROR   " : "CRITICAL", key->data.scalar.value, program_name ? "program" : "", program_name ? program_name : "server");
						report(reporter, program_name ? false : true );
						ret = false;
						break ;
					}
					if (key->data.scalar.value[0] == '\0')
					{
						snprintf(reporter->buffer, PIPE_BUF, "%s : Found empty environment key in %s'%s'\n", program_name ? "ERROR   " : "CRITICAL", program_name ? "program" : "", program_name ? program_name : "server");
						report(reporter, program_name ? false : true );
						ret = false;
						break ;
					}
					current = calloc(1, sizeof(*current));
					if (!current)
					{
						snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate environment node in %s'%s'\n", program_name?"program ":"", program_name?program_name:"server");
						report(reporter, true);
						ret = false;
						*dest = free_s_env(*dest);
						break ;
					}
					current->key = calloc(strlen((char *)key->data.scalar.value) + 1, sizeof(char));
					if (!current->key)
					{
						snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate environment node in %s'%s'\n", program_name?"program ":"", program_name?program_name:"server");
						report(reporter, true);
						ret = false;
						*dest = free_s_env(*dest);
						break ;
					}
					current->value = calloc(strlen((char *)value->data.scalar.value) + 1, sizeof(char));
					if (!current->value)
					{
						snprintf(reporter->buffer, PIPE_BUF, "CRITICAL : Could not allocate environment node in %s'%s'\n", program_name?"program ":"", program_name?program_name:"server");
						report(reporter, true);
						ret = false;
						*dest = free_s_env(*dest);
						break ;
					}
					strcpy(current->key, (char*)key->data.scalar.value);
					strcpy(current->value, (char*)value->data.scalar.value);
					if (!position)
						*dest = current;
					else
						position->next = current;
					position = current;
					current = NULL;
				}
				else
				{
					snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format in environment for key %s in %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", key->data.scalar.value, program_name ? "program" : "", program_name ? program_name : "server", value->tag);
					report(reporter, program_name ? false : true );
					ret = false;
					break ;
				}
			}
			else
			{
				snprintf(reporter->buffer, PIPE_BUF, "%s : Wrong format in environment for %s'%s', expected a scalar value, encountered %s\n", program_name ? "ERROR   " : "CRITICAL", program_name ? "program" : "", program_name ? program_name : "server", key->tag);
				report(reporter, program_name ? false : true );
				ret = false;
				break ;
			}
		}	
	}
	return (ret);
}

int	nb_procs(struct s_server *server)
{
	int	numprocs = 0;
	for (struct s_program *current = server->begin(server); current; current = current->itnext(current))
	{
		numprocs += current->numprocs;
	}
	return (numprocs);
}
