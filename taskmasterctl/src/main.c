/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/17 16:54:30 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "taskmasterctl.h"

void print_history(void);

int main(int ac, char **av) {
	(void)ac;
	(void)av;

	printf("%s\n", ctermid(NULL));
	printf("%s\n", ttyname(0));

	FILE *file = fopen_history();
	add_old_history(file);
	add_history("je suis historique !");

	char *line = ft_readline("$>");
	while (line != NULL) {
		// process line (remove space n sht)

		// add to history and read another line
		add_history(line);
		add_file_history(line, file);
		free(line);
		line = ft_readline("$>");
	}
	clear_history();
	if (file) {
		fclose(file);
	}
}
