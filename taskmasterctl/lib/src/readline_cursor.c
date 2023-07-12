/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline_cursor.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 16:00:16 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 16:00:58 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "readline.h"
#include <string.h>
#include <unistd.h>

void cursor_right(struct s_readline *global) {
	if (global->cursor == strlen(global->line)) {
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

