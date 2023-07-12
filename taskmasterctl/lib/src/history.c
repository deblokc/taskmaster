/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   history.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 16:46:05 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 17:42:45 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "readline.h"
#include <stdlib.h>
#include <string.h>

struct s_history *history_new(char *line) {
	struct s_history *tmp = (struct s_history *)calloc(sizeof(struct s_history), 1);
	tmp->line = strdup(line);
	tmp->next = NULL;
	tmp->prev = NULL;
	return (tmp);
}

void clear_history(void) {
	struct s_readline *global = get_global();
	struct s_history *current = global->first;
	struct s_history *tmp = NULL;

	while (current) {
		tmp = current;
		current = current->next;
		free(tmp->line);
		free(tmp);
	}
}

void add_history(char *line) {
	struct s_readline *global;

	if (!line) {
		return ;
	}
	global = get_global();
	if (!global->first) {
		global->first = history_new(line);
		global->last = global->first;
	} else {
		struct s_history *tmp = history_new(line);
		global->last->next = tmp;
		tmp->prev = global->last;
		global->last = tmp;
	}
}
