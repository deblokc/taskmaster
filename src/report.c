/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   report.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/11 19:41:17 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/11 20:10:25 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>

void	report(struct s_report* reporter, bool critical)
{
	reporter->critical = critical;
	if (write(reporter->report_fd, reporter->buffer, strlen(reporter->buffer)) == -1) {}
}
