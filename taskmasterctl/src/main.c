/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 15:17:03 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 15:30:59 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "taskmasterctl.h"

int main(int ac, char **av) {
	(void)ac;
	(void)av;

	printf("%s\n", ctermid(NULL));
	printf("%s\n", ttyname(0));

	char *test = ft_readline("test>");
	free(test);

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
