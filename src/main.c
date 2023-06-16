/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/16 16:12:04 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"

int main(int ac, char **av) {
	if (ac != 2)
	{
		write(2, "Usage: ./taskmaster CONFIGURATION-FILE", strlen
	}
	(void)ac;
	(void)av;
	int	major, minor, patch;

	yaml_get_version(&major, &minor, &patch);
	printf("Yaml version: %d.%d.%d", major, minor, patch);
	return (0);
}
