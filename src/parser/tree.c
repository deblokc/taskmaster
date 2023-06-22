/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tree.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 17:17:11 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/22 17:43:35 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

	current = server->program_tree
}

static void	empty_tree(struct s_server* server)
{
	
}
