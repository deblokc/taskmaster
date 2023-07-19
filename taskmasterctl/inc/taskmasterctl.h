/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmasterctl.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:45:18 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/19 16:33:19 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

struct s_command {
	char	*cmd;
	char	**arg;
};

FILE *fopen_history(void);
void add_old_history(FILE *file);
void add_file_history(char *line, FILE *file);

char *ft_readline(char *prompt);