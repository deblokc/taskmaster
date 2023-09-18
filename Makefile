# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/09/16 12:07:07 by tnaton            #+#    #+#              #
#    Updated: 2023/09/18 12:35:03 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

TASKMASTERD = taskmasterd

TASKMASTERCTL = taskmasterctl

TASKMASTERD_PATH = srcs/taskmasterd

TASKMASTERCTL_PATH = srcs/taskmasterctl

TASKMASTERD_SRCS = $(addprefix $(TASKMASTERD_PATH)/src/, $(shell grep -e '[a-z]\.c' $(TASKMASTERD_PATH)/Makefile | tr -d '\\ \n' | cut -c 6-))

TASKMASTERCTL_SRCS = $(addprefix $(TASKMASTERCTL_PATH)/src/, $(shell grep -e 'SRCS =' $(TASKMASTERCTL_PATH)/Makefile | cut -c 8-))

.PHONY: all
all: $(TASKMASTERD) $(TASKMASTERCTL)

$(TASKMASTERD): $(TASKMASTERD_SRCS)
	$(MAKE) -C $(TASKMASTERD_PATH)

$(TASKMASTERCTL): $(TASKMASTERCTL_SRCS)
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
