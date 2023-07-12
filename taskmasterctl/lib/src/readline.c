/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 15:28:41 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 16:04:44 by tnaton           ###   ########.fr       */
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
#include "readline.h"

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
	while (strlen(buf) == 0 || buf[strlen(buf) - 1] != '\n') {
		bzero(buf, BUF_SIZE);
		size = read(0, buf, BUF_SIZE - 1);
		if (size == 0 && strlen(global->line) == 0) {
			return (1);
		}
		exec_key(buf, global);
		size = strlen(buf);
		if (strlen(global->line) + size + 1 > global->len) {
			global->len += BUF_SIZE;
			global->line = realloc(global->line, global->len);
			if (!global->line) {
				return (-1);
			}
			bzero(global->line + strlen(global->line), global->len - strlen(global->line));
		}
		for (size_t i = 0; i < size; i++) {
			if (buf[i] >= ' ' && buf[i] <= '~') {
				if (global->cursor != strlen(global->line)) { // if not at end of line
					memcpy(global->line + global->cursor + 1, global->line + global->cursor, strlen(global->line) - global->cursor + 1);
					global->line[global->cursor] = buf[i];
					cursor_save();
					if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint line with the character
					cursor_restore();
					cursor_right(global);
				} else { // if at end of line
					global->line[strlen(global->line)] = buf[i];
					global->cursor++;
					if (write(1, &buf[i], 1) < 0) {
						return (-1);
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
			ret = strdup(global->line);
			break;
		}
		default: {
			ret = NULL;
		}
	}
	free(global->line);
	global->line = NULL;
	return (ret);
}

