/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/10 19:23:52 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include "taskmasterctl.h"

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
.line = NULL
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
				if (buf[i + 1] == ']') {
					switch (buf[i + 2]) {
						case 'A': {
							return (K_ARROW_UP);
						}
						case 'B': {
							return (K_ARROW_DOWN);
						}
						case 'C': {
							return (K_ARROW_RIGHT);
						}
						case 'D': {
							return (K_ARROW_LEFT);
						}
						case '3': {
							if (buf[i + 3] == '~') {
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
				return (K_BACKSPACE);
			}
			case 0x1: {
				return (K_CTRL_A);
			}
			case 0x5: {
				return (K_CTRL_E);
			}
			case 0xc: {
				return (K_CTRL_L);
			}
			case 0xd: {
				return (K_ENTER);
			}
			case 0x9: {
				return (K_TAB);
			}
			default: {}
		}
	}
	return (0);
}

int	handle_line(struct s_readline *global) {
	size_t size;
	size_t line_len = 0;
	int j = 0;
	char buf[BUF_SIZE];
	char total[BUF_SIZE];

	bzero(buf, BUF_SIZE);
	bzero(total, BUF_SIZE);
	while (buf[0] != '\n') {
		size = read(0, buf, 4096);
		for (size_t i = 0; i < size; i++) {
			if (buf[i] >= ' ' && buf[i] <= '~') {
				global->line[line_len] = buf[i];
				line_len++;
				if (write(1, &buf[i], 1) < 0) {
					return (-1);
				}
			} else if (buf[i] == 0x7f) {
				global->line[line_len] = '\0';
				line_len--;
				if (write(1, "\b \b", 3) < 0) {
					return (-1);
				}
			} else {
				printf("%x", buf[i]);
			}
			total[j] = buf[i];
			j++;
		}
		fflush(stdout);
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
