# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/09/16 12:07:07 by tnaton            #+#    #+#              #
#    Updated: 2023/09/18 11:35:13 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

TASKMASTERD = taskmasterd

TASKMASTERCTL = taskmasterctl

TASKMASTERD_PATH = srcs/taskmasterd

TASKMASTERCTL_PATH = srcs/taskmasterctl

.PHONY: all
all: $(TASKMASTERD) $(TASKMASTERCTL)

$(TASKMASTERD):
	$(MAKE) -C $(TASKMASTERD_PATH)

$(TASKMASTERCTL):
	$(MAKE) -C $(TASKMASTERCTL_PATH)

.PHONY: clean
clean :
	$(MAKE) clean -C $(TASKMASTERD_PATH)
	$(MAKE) clean -C $(TASKMASTERCTL_PATH)

.PHONY: fclean
fclean :
	$(MAKE) fclean -C $(TASKMASTERD_PATH)
	$(MAKE) fclean -C $(TASKMASTERCTL_PATH)

.PHONY: re
re: fclean all
