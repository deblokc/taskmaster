# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/06/16 11:17:59 by tnaton            #+#    #+#              #
#    Updated: 2023/06/16 11:26:28 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.c src
vpath %.o obj
vpath %.h inc

NAME = taskmaster

OBJDIR := obj

SRCS = main.c

INC = taskmaster.h

CFLAGS = -Wall -Wextra -Werror -Wpedantic -O3 -g

CC = gcc

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

$(NAME) : $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJS): $(INC)

$(OBJS) : | $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR) :
	mkdir $(OBJDIR)

.SECONDARY: $(OBJS)

.PHONY: all
all : $(NAME)

.PHONY: clean
clean : 
	rm -rf $(OBJS) $(OBJDIR)

.PHONY: fclean
fclean:
	rm -rf $(NAME) $(OBJS) $(OBJDIR)

.PHONY: re
re: fclean all
