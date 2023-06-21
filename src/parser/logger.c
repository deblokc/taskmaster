/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   logger.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/19 18:56:48 by bdetune           #+#    #+#             */
/*   Updated: 2023/06/21 19:22:47 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <string.h>

void	default_logger(struct s_logger *logger)
{
	logger->logfile_maxbytes = 5*1024*1024;
	logger->logfile_backups = 10;
}

