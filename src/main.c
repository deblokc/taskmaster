/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/22 19:12:15 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>
#include "taskmaster.h"

int main(int ac, char **av) {
	int					ret = 0;
	struct s_server*	server = NULL;

	if (ac != 2)
	{
		if (write(2, "Usage: ./taskmaster CONFIGURATION-FILE\n",
					strlen("Usage: ./taskmaster CONFIGURATION-FILE\n")) == -1) {}
		ret = 1;
	}
	else
	{
		server = parse_config(av[1]);
		if (!server)
		{
			printf("Could not make server from configuration file\n");
			ret = 1;
		}
		else
		{
			printf("We have a server\n");
			server->print_tree(server);
			server = server->cleaner(server);
		}
	}
	return (ret);
}
