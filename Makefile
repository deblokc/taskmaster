# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/09/16 12:07:07 by tnaton            #+#    #+#              #
#    Updated: 2023/09/19 18:39:59 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

TASKMASTERD = taskmasterd

TASKMASTERD_PATH = srcs/taskmasterd

TASKMASTERD_SRCS = $(addprefix $(TASKMASTERD_PATH)/src/, $(shell grep -e '[a-z]\.c' $(TASKMASTERD_PATH)/Makefile | tr -d '\\ \n' | cut -c 6-))

TASKMASTERD_INC = $(addprefix $(TASKMASTERD_PATH)/inc/, $(shell grep 'INC =' $(TASKMASTERD_PATH)/Makefile | cut -c 7-))

TASKMASTERCTL = taskmasterctl

TASKMASTERCTL_PATH = srcs/taskmasterctl

TASKMASTERCTL_SRCS = $(addprefix $(TASKMASTERCTL_PATH)/src/, $(shell grep -e 'SRCS =' $(TASKMASTERCTL_PATH)/Makefile | cut -c 8-))

TASKMASTERCTL_INC = $(addprefix $(TASKMASTERCTL_PATH)/inc/, $(shell grep 'INC =' $(TASKMASTERCTL_PATH)/Makefile | cut -c 7-))

.PHONY: all
all: $(TASKMASTERD) $(TASKMASTERCTL)

$(TASKMASTERD): $(TASKMASTERD_SRCS) $(TASKMASTERD_INC)
	$(MAKE) -C $(TASKMASTERD_PATH)

$(TASKMASTERCTL): $(TASKMASTERCTL_SRCS) $(TASKMASTERCTL_INC)
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
