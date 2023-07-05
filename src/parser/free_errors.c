/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   free_errors.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:48:25 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/22 17:06:14 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdlib.h>
#include "taskmaster.h"

struct s_env *free_s_env(struct s_env *start)
{
	struct s_env	*next;

	while (start)
	{
		next = start->next;
		free(start->key);
		free(start->value);
		free(start);
		start = next;
	}
	return (NULL);
}
