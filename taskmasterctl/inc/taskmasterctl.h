/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmasterctl.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:45:18 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/05 16:12:58 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <termios.h>
#include <stdio.h>

#define BUF_SIZE 4096

enum	key {
	K_ARROW_UP = 1,
	K_ARROW_DOWN = 2,
	K_ARROW_RIGHT = 3,
	K_ARROW_LEFT = 4,
	K_BACKSPACE = 5,
	K_DELETE = 6,
	K_CTRL_A = 7,
	K_CTRL_D = 8,
	K_CTRL_L = 9,
	K_TAB = 10
};

struct s_readline {
	char *line;
	struct termios	base;
	struct termios	term;
};

FILE *fopen_history(void);
void add_old_history(FILE *file);
void add_file_history(char *line, FILE *file);

