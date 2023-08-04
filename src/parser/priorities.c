/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   priorities.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/04 18:26:30 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/25 18:26:12 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <stdio.h>
#include <stdlib.h>

static struct s_priority*	itnext(struct s_priority* self)
{
	struct s_priority*	ret = NULL;

	if (self)
	{
		ret = self->next;
	}
	return (ret);
}

static void	print_priority(struct s_priority* self)
{
	if (self)
	{
		printf("************************* PRIORITY %d *************************", self->priority);
		for (struct s_program* current = self->begin; current; current = current->next)
		{
			current->print(current);
		}
		printf("\n");
	}
}

static void destructor(struct s_priority *self)
{
	struct s_priority*	next;

	while (self)
	{
		next = self->next;
		free(self);
		self = next;
	}
}

static void print_priorities(struct s_priority *head)
{
	printf("######################### LIST OF PRIORITIES #########################\n");
	for (struct s_priority* current = head; current; current = current->itnext(current))
	{
		current->print_priority(current);
	}
}

static void	init_priority(struct s_priority *self)
{
	if (self)
	{
		self->destructor = destructor;
		self->itnext = itnext;
		self->print_priority = print_priority;
		self->print_priorities = print_priorities;
	}
}

static struct s_priority* get_location(struct s_priority* first, int priority)
{
	struct s_priority*	current;

	current = first;
	while (current)
	{
		if (current->priority == priority)
			break ;
		current = current->next;
	}
	return (current);
}

static void	insert_back(struct s_priority* target, struct s_program *current)
{
	struct s_program*	runner;
	
	runner = target->begin;
	if (!runner)
	{
		target->begin = current;
		current->next = NULL;
	}
	else
	{
		while (runner->next)
		{
			runner = runner->next;
		}
		runner->next = current;
		current->next = NULL;
	}
}

static void	insert_priority(struct s_priority** head, struct s_priority* location)
{
	struct s_priority*	current;

	if (!head || !*head)
	{
		return ;
	}
	else if (location->priority < (*head)-> priority)
	{
		location->next = *head;
		*head = location;
	}
	else
	{
		current = *head;
		while (current->next && current->next->priority < location->priority)
		{
			current = current->next;
		}
		if (!current->next)
		{
			current->next = location;
		}
		else
		{
			location->next = current->next;
			current->next = location;
		}
	}
}

static void	add_priority(struct s_priority** head, struct s_program *current, struct s_report *reporter)
{
	struct s_priority*	location;
	
	if (head && current->autostart)
	{
		if (!*head)
		{
			*head = calloc(1, sizeof(**head));
			if (!*head)
			{
				snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: could not allocate space for priority node\n");
				report(reporter, true);
				return ;
			}
			init_priority(*head);
			(*head)->priority = current->priority;
			(*head)->begin = current;
		}
		else
		{
			location = get_location(*head, current->priority);
			if (location)
			{
				insert_back(location, current);
			}
			else
			{
				location = calloc(1, sizeof(*location));
				if (!location)
				{
					snprintf(reporter->buffer, PIPE_BUF, "CRITICAL: could not allocate space for priority node\n");
					report(reporter, true);
					return ;
				}
				init_priority(location);
				location->priority = current->priority;
				insert_priority(head, location);
				insert_back(location, current);
			}
		}
	}
}

struct s_priority*	create_priorities(struct s_server* server, struct s_report *reporter)
{
	struct s_priority*	head = NULL;
	for (struct s_program* current = server->begin(server); current; current = current->itnext(current))
	{
		add_priority(&head, current, reporter);
		if (reporter->critical)
		{
			if (head)
				head->destructor(head);
			head = NULL;
			break ;
		}
	}
	return (head);
}
