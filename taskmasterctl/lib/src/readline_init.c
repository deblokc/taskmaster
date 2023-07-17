/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline_init.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 15:54:52 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/17 13:58:45 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string.h>
#include <stdlib.h>
#include "readline.h"

static int checkterm(void) {
	char *term = getenv("TERM");
	if (!term) {
		return (-1);
	}
	if (strcmp(term, "xterm") && strcmp(term, "xterm-256color")) {
		return (-1);
	}
	return (0);
}

static int init_readline(struct s_readline* global) {

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
		.new = NULL,
		.current = NULL,
		.cursor = 0,
		.prompt = NULL,
		.prompt_len = 0,
		.first = NULL,
		.last = NULL,
		.init = false,
	};
	if (global.init == false) {
		init_readline(&global);
		global.init = true;
	}

	return (&global);
}
