/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/04 20:16:45 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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
	if (tcsetattr(0, TCSADRAIN, &(global->term))) {
		return (-1);
	}
	return (0);
}

struct s_readline *get_global(void) {
	static struct s_readline global = {
.buf = NULL
	};
	if (global.buf == NULL) {
		init_readline(&global);
		global.buf = (char *)calloc(sizeof(char), 4097);
	}

	return (&global);
}

char *ft_readline(char *prompt) {
	struct s_readline *global = get_global();

}

int main(int ac, char **av) {
	(void)ac;
	(void)av;

	printf("%s\n", ctermid(NULL));
	printf("%s\n", ttyname(0));

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
