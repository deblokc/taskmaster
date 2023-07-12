/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmasterctl.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:45:18 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 15:54:33 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

FILE *fopen_history(void);
void add_old_history(FILE *file);
void add_file_history(char *line, FILE *file);

char *ft_readline(char *prompt);
