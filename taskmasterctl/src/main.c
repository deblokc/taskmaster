/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/11 20:26:16 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include "taskmasterctl.h"

int log_fd;

void hexdump(char *buf) {
	dprintf(log_fd, ">");
	for (size_t i = 0; buf[i]; i++) {
		dprintf(log_fd, "%x", buf[i]);
	}
	dprintf(log_fd, "<\n");
}

int checkterm(void) {
	char *term = getenv("TERM");
	if (!term) {
		return (-1);
	}
	if (strcmp(term, "xterm") && strcmp(term, "xterm-256color")) {
		return (-1);
	}
	return (0);
}

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

int init_readline(struct s_readline* global) {

	if (checkterm()) {
		return (-1);
	}
	if (tcgetattr(0, &global->term)) {
		return (-1);
	}
	memcpy(&(global->base), &(global->term), sizeof(struct termios));
	global->term.c_lflag = ~(ICANON); // deactivate cannonical mode
	global->term.c_lflag = ~(ECHO); // deactivate automatic print of char inputed
	global->term.c_lflag |= ISIG; // cancel signal
	global->term.c_cc[VMIN] = 1; // need only one char to do smg
	global->term.c_cc[VTIME] = 0; // doesnt need timeout AKA busy wait 4ever
	return (0);
}

struct s_readline *get_global(void) {
	static struct s_readline global = {
		.cursor = 0,
		.line = NULL,
		.len = 0,
		.prompt = NULL,
		.prompt_len = 0,
	};
	if (global.line == NULL) {
		init_readline(&global);
		global.line = (char *)calloc(sizeof(char), 4096);
	}

	return (&global);
}

int getkey(char *buf) {
	for (int i = 0; buf[i]; i++) {
		switch (buf[i]) {
			case 0x1b: {
				if (buf[i + 1] == '[') {
					switch (buf[i + 2]) {
						case 'A': {
							memcpy(buf, buf + 3, strlen(buf + 3) + 1 + 1);
							return (K_ARROW_UP);
						}
						case 'B': {
							memcpy(buf, buf + 3, strlen(buf + 3) + 1);
							return (K_ARROW_DOWN);
						}
						case 'C': {
							memcpy(buf, buf + 3, strlen(buf + 3) + 1);
							return (K_ARROW_RIGHT);
						}
						case 'D': {
							memcpy(buf, buf + 3, strlen(buf + 3) + 1);
							return (K_ARROW_LEFT);
						}
						case '3': {
							if (buf[i + 3] == '~') {
								memcpy(buf, buf + 4, strlen(buf + 4) + 1);
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
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_BACKSPACE);
			}
			case 0x1: {
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_CTRL_A);
			}
			case 0x5: {
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_CTRL_E);
			}
			case 0xc: {
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_CTRL_L);
			}
			case 0xd: {
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_ENTER);
			}
			case 0x9: {
				memcpy(buf, buf + 1, strlen(buf + 1));
				return (K_TAB);
			}
			default: {}
		}
	}
	return (0);
}

void cursor_right(struct s_readline *global) {
	if (global->cursor == global->len) {
		return ;
	}
	if (write(1, "\33[C", 3)) {}
	global->cursor++;
}

void cursor_left(struct s_readline *global) {
	if (global->cursor == 0) {
		return ;
	}
	if (write(1, "\33[D", 3)) {}
	global->cursor--;
}

void cursor_save(void) {
	if (write(1, "\33[s", 3)) {} // save position
}

void cursor_restore(void) {
	if (write(1, "\33[u", 3)) {} // bring back
}

void cursor_clear_endline(void) {
	if (write(1, "\33[K", 3)) {} // clear line from cursor
}

void exec_key(int keycode, struct s_readline *global) {
	switch (keycode) {
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
			memcpy(global->line + global->cursor, global->line + global->cursor + 1, global->len - global->cursor + 1); // move everything after deleted char one to the left
			cursor_clear_endline();
			if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint end of line
			cursor_restore();
			global->len--;
			break;
		}
		case K_DELETE: {
			if (global->cursor == global->len) {
				return ;
			}
			cursor_save();
			memcpy(global->line + global->cursor, global->line + global->cursor + 1, global->len - global->cursor + 1); // move everything after deleted char one to the left
			cursor_clear_endline();
			if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint end of line
			cursor_restore();
			global->len--;
			break;
		}
		case K_CTRL_A: {
			for (size_t i = 0; global->cursor; i++) {
				cursor_left(global);
			}
			break;
		}
		case K_CTRL_E: {
			for (size_t i = 0; global->cursor < global->len; i++) {
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
			if (write(1, global->line, global->len)) {}
			cursor_restore();
			break;
		}
		case K_ENTER: {
			break;
		}
		case K_TAB: {
			// autocomplete T.T
			break;
		}
		default: {}
	}
}

int	handle_line(struct s_readline *global) {
	size_t size;
	int j = 0;
	char buf[BUF_SIZE];
	char total[BUF_SIZE];

	bzero(buf, BUF_SIZE);
	bzero(total, BUF_SIZE);
	while (buf[0] != '\n') {
		size = read(0, buf, 4096);
		dprintf(log_fd, "before getkey : ");
		hexdump(buf);
		int tmp = getkey(buf);
		exec_key(tmp, global);
		dprintf(log_fd, "after getkey  : ");
		hexdump(buf);
		size = strlen(buf);
		dprintf(log_fd, "len : %ld\ncursor : %ld\n", global->len, global->cursor);
		for (size_t i = 0; i < size; i++) {
			if (buf[i] >= ' ' && buf[i] <= '~') {
				if (global->cursor != global->len) {
					memcpy(global->line + global->cursor + 1, global->line + global->cursor, global->len - global->cursor + 1);
					global->line[global->cursor] = buf[i];
					global->len++;
					cursor_save();
					if (write(1, global->line + global->cursor, strlen(global->line + global->cursor))) {} // reprint line with the character
					cursor_restore();
					cursor_right(global);
				} else {
					global->line[global->len] = buf[i];
					global->len++;
					global->cursor++;
					if (write(1, &buf[i], 1) < 0) {
						return (-1);
					}
				}
			}
			total[j] = buf[i];
			j++;
		}
		fflush(stdout);
		bzero(buf + strlen(buf), BUF_SIZE - strlen(buf));
	}
	printf("\n\n>%s<\n\n", global->line);
	for (int i = 0; total[i]; i++) {
		if (total[i] >= ' ' && total[i] <= '~') {
			printf("%c", total[i]);
		} else {
			printf("0x%x", total[i]);
		}
	}
	printf("\n");
	return (0);

}

char *ft_readline(char *prompt) {
	struct s_readline *global = get_global();

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
	if (handle_line(global)) {
		return NULL;
	}
	if (write(1, "\n", 1) < 0) {
		return NULL;
	}
	if (readline_resetattr(global)) {
		return NULL;
	}
	return NULL;
}

int main(int ac, char **av) {
	(void)ac;
	(void)av;

	log_fd = open("./log", O_CREAT | O_TRUNC | O_RDWR, 0644);
	printf("%d\n", log_fd);
	if (write(log_fd, "=-=-=-=-=LOG=-=-=-=-=\n", strlen("=-=-=-=-=LOG=-=-=-=-=\n"))){}

	printf("%s\n", ctermid(NULL));
	printf("%s\n", ttyname(0));

	(void)ft_readline("test>");

	FILE *file = fopen_history();
	add_old_history(file);
	char *line = readline("$>");
	while (line != NULL) {
		// process line (remove space n sht)

		// add to history and read another line
		add_history(line);
		add_file_history(line, file);
		free(line);
		line = readline("$>");
	}
	rl_clear_history();
	if (file) {
		fclose(file);
	}
}
