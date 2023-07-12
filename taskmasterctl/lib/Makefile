# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/07/12 15:42:00 by tnaton            #+#    #+#              #
#    Updated: 2023/07/12 16:08:35 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.c src
vpath %.o obj
vpath %.h inc

READLINE = readline.a

OBJDIR := obj

SRCS = readline.c readline_cursor.c readline_init.c readline_key.c

INC = readline.h

CFLAGS = -Wall -Wextra -Werror -Wpedantic -O3

CC = gcc

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

$(READLINE) : $(OBJS) $(INC)
	ar rcs $@ $?

$(OBJS): $(INC)

$(OBJS): | $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -I inc -o $@ -c $<

$(OBJDIR) :
	mkdir $(OBJDIR)

.SECONDARY: $(OBJS)

.PHONY: all
all: $(READLINE)

.PHONY: clean
clean:
	rm -rf $(OBJDIR)

.PHONY: fclean
fclean:
	rm -rf $(READLINE) $(OBJDIR)

.PHONY: re
re: fclean all
