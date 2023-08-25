/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tree.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 17:17:11 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/25 19:29:59 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <string.h>
#include "taskmaster.h"

static struct s_program*	next(struct s_program* self)
{
	struct s_program*	current;

	if (self && self->right)
	{
		current = self->right;
		while (current->left)
			current = current->left;
	}
	else if (self == NULL || self->parent == NULL)
	{
			current = NULL;
	}
	else
	{
		while (self->parent && self->parent->right == self)
			self = self->parent;
		if (self->parent == NULL)
			current = NULL;
		else
			current = self->parent;
	}
	return (current);
}

static struct s_program*	begin(struct s_server* server)
{
	struct s_program*	current;

	current = server->program_tree;
	if (current)
	{
		while(current->left)
			current = current->left;
	}
	return (current);
}

static struct s_program*	free_next_node(struct s_program* begin)
{
	struct s_program*	ret = NULL;

	if (begin)
	{
		while (begin->left || begin->right)
		{
			if (begin->left)
				begin = begin->left;
			else
				begin = begin->right;
		}
		if (begin->parent)
		{
			ret = begin->parent;
			if (begin->parent->left == begin)
				begin->parent->left = NULL;
			else
				begin->parent->right = NULL;
		}
		begin->cleaner(begin);
	}
	return (ret);
}

static void	delete_tree(struct s_server* server)
{
	struct s_program *current;

	current = server->program_tree;
	current = free_next_node(server->program_tree);
	while (current)
		current = free_next_node(server->program_tree);
	server->program_tree = NULL;
}

static struct s_program* find_location(struct s_program *begin, struct s_program *to_insert)
{
	int					diff;
	struct s_program*	target;

	target = begin;
	while (target->left || target->right)
	{
		diff = strcmp(target->name, to_insert->name);
		if (!diff)
			break ;
		if (diff < 0)
		{
			if (!target->right)
				break ;
			target = target->right;
		}
		else
		{
			if (!target->left)
				break ;
			target = target->left;
		}
	}
	return (target);
}


static void	replace_node(struct s_program* target, struct s_program *to_insert)
{
	to_insert->left = target->left;
	if (target->left)
		target->left->parent = to_insert;
	to_insert->right = target->right;
	if (target->right)
		target->right->parent = to_insert;
	to_insert->parent = target->parent;
	if (target->parent)
	{
		if (target->parent->left == target)
			target->parent->left = to_insert;
		else
			target->parent->right = to_insert;
	}
	target->cleaner(target);
}

static void	insert(struct s_server* server, struct s_program* program, struct s_report *reporter)
{
	int					diff;
	struct s_program*	target = NULL;

	if (!server->program_tree)
		server->program_tree = program;
	else
	{
		target = find_location(server->program_tree, program);
		diff = strcmp(target->name, program->name);
		if (!diff)
		{
			snprintf(reporter->buffer, PIPE_BUF, "WARNING  : Redefinition of program %s, discarding previous definition in favor of the latest one\n", program->name);
			report(reporter, false);
			if (server->program_tree == target)
			{
				snprintf(reporter->buffer, PIPE_BUF, "DEBUG    : Switching root of program tree\n");
				report(reporter, false);
				server->program_tree = program;
			}
			replace_node(target, program);
		}
		else if (diff < 0)
		{
			target->right = program;
			program->parent = target;
		}
		else
		{
			target->left = program;
			program->parent = target;
		}
	}
}

static void	print_tree(struct s_server* self)
{
	printf("############# Program Tree #############");
	for (struct s_program* current = self->begin(self); current; current = current->itnext(current))
	{
		printf("\t");
		current->print(current);
	}
}

void	register_treefn_serv(struct s_server *self)
{
	self->insert = insert;
	self->delete_tree = delete_tree;
	self->begin = begin;
	self->print_tree = print_tree;
}

void	register_treefn_prog(struct s_program *self)
{
	self->itnext = next;
}
