/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 15:28:41 by tnaton            #+#    #+#             */
/*   Updated: 2023/09/18 20:00:59 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include "readline_private.h"

int readline_setattr(struct s_readline* global) {
	if (tcsetattr(0, TCSADRAIN, &(global->term))) {
		return (-1);
	}
	return (0);
}

int readline_resetattr(struct s_readline* global) {
	if (tcsetattr(0, TCSADRAIN, &(global->base))) {
		return (-1);
	}
	return (0);
}

int	handle_line(struct s_readline *global) {
	size_t size;
	char buf[BUF_SIZE];

	bzero(buf, BUF_SIZE);
	if (!global->new) {
		global->new = (struct s_history *)calloc(sizeof(struct s_history), 1);
		if (!global->new) {
			return (-1);
		}
		global->new->line = (char *)calloc(sizeof(char), BUF_SIZE);
		if (!global->new->line) {
			return (-1);
		}
		bzero(global->new->line, BUF_SIZE);
		global->new->len = BUF_SIZE;
		global->new->prev = NULL;
		global->new->next = NULL;
	}
	global->current = global->new;
	global->cursor = 0;
	while (strlen(buf) == 0 || buf[strlen(buf) - 1] != '\n') {
		bzero(buf, BUF_SIZE);
		size = read(0, buf, BUF_SIZE - 1);
		if (size == 0 && strlen(global->current->line) == 0) {
			return (1);
		}
		exec_key(buf, global); // <=== CAN CHANGE CURRENT
		size = strlen(buf);
		if (strlen(global->current->line) + size + 1 > global->current->len) {
			global->current->len += BUF_SIZE;
			global->current->line = realloc(global->current->line, global->current->len);
			if (!global->current->line) {
				return (-1);
			}
			bzero(global->current->line + strlen(global->current->line), global->current->len - strlen(global->current->line));
		}
		for (size_t i = 0; i < size; i++) {
			if (buf[i] >= ' ' && buf[i] <= '~') {
				if (global->cursor != strlen(global->current->line)) { // if not at end of line
					memcpy(global->current->line + global->cursor + 1, global->current->line + global->cursor, strlen(global->current->line) - global->cursor + 1);
					global->current->line[global->cursor] = buf[i];
					if (!global->shadow) {
						cursor_save();
						if (write(1, global->current->line + global->cursor, strlen(global->current->line + global->cursor))) {} // reprint line with the character
						cursor_restore();
						cursor_right(global);
					}
				} else { // if at end of line
					global->current->line[strlen(global->current->line)] = buf[i];
					global->cursor++;
					if (!global->shadow) {
						if (write(1, &buf[i], 1) < 0) {
							return (-1);
						}
					}
				}
			}
		}
		fflush(stdout);
	}
	return (0);
}

char *ft_readline(char *prompt) {
	struct s_readline *global = get_global();
	int ret_line = 0;
	char *ret;

	if (readline_setattr(global)) {
		return NULL;
	}
	if (prompt) {
		global->prompt = prompt;
		global->prompt_len = strlen(prompt);
		if (write(1, prompt, strlen(prompt)) < 0) {
			return NULL;
		}
	}
	ret_line = handle_line(global);
	if (readline_resetattr(global)) {
		return NULL;
	}
	if (write(1, "\n", 1) < 0) {
		return NULL;
	}
	switch (ret_line) {
		case 0: {
			ret = strdup(global->current->line);
			break;
		}
		default: {
			ret = NULL;
		}
	}
	if (global->new) {
		global->current = NULL;
		free(global->new->line);
		free(global->new);
		global->new = NULL;
		if (global->last)
			global->last->next = NULL;
	}
	return (ret);
}

