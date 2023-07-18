/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/18 15:26:15 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/18 15:35:39 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef READLINE_H
# define READLINE_H

char	*readline(char *prompt);
void	add_history(char *line);
void	clear_history(void);
void	complete_init(char **tab_complete);

#endif
