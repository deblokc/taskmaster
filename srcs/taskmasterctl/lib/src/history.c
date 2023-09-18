/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   history.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 16:46:05 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/18 15:14:26 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "readline_private.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct s_history *history_new(char *line) {
	struct s_history *tmp = (struct s_history *)calloc(sizeof(struct s_history), 1);
	if (!tmp) {
		return (NULL);
	}
	tmp->len = 0;
	while (!tmp->len || tmp->len < strlen(line)) {
		tmp->len += BUF_SIZE;
		if (tmp->line) {
			free(tmp->line);
		}
		tmp->line = (char *)calloc(sizeof(char), tmp->len);
		if (!tmp->line) {
			free(tmp);
			return (NULL);
		}
	}
	memcpy(tmp->line, line, strlen(line));
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

void print_line_history(struct s_readline *global) {
	cursor_at_line_begin();
	cursor_clear_endline();
	if (global->prompt) {
		if (write(1, global->prompt, strlen(global->prompt))) {}
	}
	if (global->current->line) {
		if (write(1, global->current->line, strlen(global->current->line))) {}
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

void print_node(struct s_history *node) {
	printf("_____NODE_____\n");
	printf("-----PREV-----\n");
	if (node->prev) {
		printf("%s\n", node->prev->line);
	} else {
		printf("%p\n", (void *)node->prev);
	}
	printf("-----LINE-----\n");
	printf("%s\n", node->line);
	printf("-----NEXT-----\n");
	if (node->next) {
		printf("%s\n", node->next->line);
	} else {
		printf("%p\n", (void *)node->next);
	}
	printf("______________\n");
}

void print_history(void) {
	struct s_readline *global = get_global();
	struct s_history *tmp = global->first;


	printf("################# PRINTING HISTORY IN ORDER ##################\n");
	while (tmp) {
		if (!tmp->line) {
			printf("%p\n", tmp->line);
		} else {
			print_node(tmp);
		}
		tmp = tmp->next;
	}
	printf("##############################################################\n");
}
