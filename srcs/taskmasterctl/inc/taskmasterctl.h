/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmasterctl.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:45:18 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/25 15:45:45 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TASKMASTERCTL_H
# define TASKMASTERCTL_H

# include <stdio.h>
# include <limits.h>

struct s_command {
	char	*cmd;
	char	*arg;
};

char	*parse_config(char *config_file);
FILE	*fopen_history(void);
void	add_old_history(FILE *file);
void	add_file_history(char *line, FILE *file);

char	*ft_readline(char *prompt);

#endif
