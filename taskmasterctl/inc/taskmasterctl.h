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

#define BUF_SIZE 4096

struct s_readline {
	char *line;
	struct termios	base;
	struct termios	term;
};

FILE *fopen_history(void);
void add_old_history(FILE *file);
void add_file_history(char *line, FILE *file);

