/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline_key.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 16:03:22 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 16:05:14 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "readline.h"
#include <unistd.h>
#include <string.h>

static int getkey(char *buf) {
	for (int i = 0; buf[i]; i++) {
		switch (buf[i]) {
			case 0x1b: {
				if (buf[i + 1] == '[') {
					switch (buf[i + 2]) {
						case 'A': {
							memcpy(buf + i, buf + i + 3, strlen(buf + i + 3) + 1);
							return (K_ARROW_UP);
						}
						case 'B': {
							memcpy(buf + i, buf + i + 3, strlen(buf + i + 3) + 1);
							return (K_ARROW_DOWN);
						}
						case 'C': {
							memcpy(buf + i, buf + i + 3, strlen(buf + i + 3) + 1);
							return (K_ARROW_RIGHT);
						}
						case 'D': {
							memcpy(buf + i, buf + i + 3, strlen(buf + i + 3) + 1);
							return (K_ARROW_LEFT);
						}
						case '3': {
							if (buf[i + 3] == '~') {
								memcpy(buf + i, buf + i + 4, strlen(buf + i + 4) + 1);
								return (K_DELETE);
							}
						}
						default: {}
					}
				} else {
					break ;
				}
			}
			case 0x7f: {
				memcpy(buf + i, buf + i + 1, strlen(buf + i) + 1);
				return (K_BACKSPACE);
			}
			case 0x1: {
				memcpy(buf + i, buf + i + 1, strlen(buf + i) + 1);
				return (K_CTRL_A);
			}
			case 0x5: {
				memcpy(buf + i, buf + i + 1, strlen(buf + i) + 1);
				return (K_CTRL_E);
			}
			case 0xc: {
				memcpy(buf + i, buf + i + 1, strlen(buf + i) + 1);
				return (K_CTRL_L);
			}
			case 0xd: {
				buf[i] = '\n';
				return (K_ENTER);
			}
			case 0x9: {
				memcpy(buf + i, buf + i + 1, strlen(buf + i) + 1);
				return (K_TAB);
			}
			default: {}
		}
	}
	return (0);
}


void exec_key(char *buf, struct s_readline *global) {
	switch (getkey(buf)) {
		case K_ARROW_UP: {
			// history up
			break;
		}
		case K_ARROW_DOWN: {
			// history down
			break;
		}
		case K_ARROW_RIGHT: {
			cursor_right(global);
			break;
		}
		case K_ARROW_LEFT: {
			cursor_left(global);
			break;
		}
		case K_BACKSPACE: {
			if (global->cursor == 0) {
				return ;
			}
			cursor_left(global);
			cursor_save();
			memcpy(global->line + global->cursor, global->line + global->cursor + 1, strlen(global->line) - global->cursor + 1); // move everything after deleted char one to the left
			cursor_clear_endline();
			if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint end of line
			cursor_restore();
			break;
		}
		case K_DELETE: {
			if (global->cursor == strlen(global->line)) {
				return ;
			}
			cursor_save();
			memcpy(global->line + global->cursor, global->line + global->cursor + 1, strlen(global->line) - global->cursor + 1); // move everything after deleted char one to the left
			cursor_clear_endline();
			if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint end of line
			cursor_restore();
			break;
		}
		case K_CTRL_A: {
			for (size_t i = 0; global->cursor; i++) {
				cursor_left(global);
			}
			break;
		}
		case K_CTRL_E: {
			for (size_t i = 0; global->cursor < strlen(global->line); i++) {
				cursor_right(global);
			}
			break;
		}
		case K_CTRL_L: {
			if (write(1, "\33[2J", 4)) {} // clear screen
			printf("\33[0;%ldH", global->prompt_len + global->cursor + 1); // move cursor to first row
			cursor_save();
			if (write(1, "\33[H", 3)) {} // go to left corner
			if (global->prompt) {
				if (write(1, global->prompt, global->prompt_len)) {} // reprint line
			}
			if (write(1, global->line, strlen(global->line))) {}
			cursor_restore();
			break;
		}
		case K_ENTER: {
			// already handled by getkey because it is easier
			break;
		}
		case K_TAB: {
			// autocomplete T.T
			break;
		}
		default: {}
	}
}


