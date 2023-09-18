# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/06/16 11:17:59 by tnaton            #+#    #+#              #
#    Updated: 2023/09/18 11:35:42 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.c src
vpath %.o obj
vpath %.h inc

NAME = ../../taskmasterctl

OBJDIR := obj

SRCS = main.c parser.c file_history.c

READLINE = lib/readline.a

INC = taskmasterctl.h

MOREFLAGS = -Wformat=2				\
			-Wformat-overflow=2		\
			-Wformat-truncation=2	\
			-Wstringop-overflow=4	\
			-Winit-self				\
			-ftrapv					\
			-Wdate-time

#	-Wformat=2						Check format when call to printf/scanf...
#	-Wformat-overflow=2				Check overflow of buffer with sprintf/vsprintf
#	-Wformat-truncation=2			Check output truncation with snprintf/vsnprintf
#	-Wstringop-overflow=4			Check overflow when using memcpy and strcpy (which should not happen for obvious reason)
#	-Winit-self						Check variable which initialise themself /* int i; i = i; */
#	-ftrapv							Trap signed overflow for + - * 
#	-Wdate-time						Warn if __TIME__ __DATE or __TIMESTAMP__ are encoutered to prevent bit-wise-identical compilation

CFLAGS = -Wall -Wextra -Werror -Wpedantic -O3 -g $(MOREFLAGS)

CC = gcc

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

$(NAME) : $(READLINE) $(OBJS) $(INC)
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	test -f ./lib/libyaml.so || ((test -f ../../libs/yaml-0.2.5/src/.libs/libyaml-0.so.2.0.9 || (cd ../../libs/yaml-0.2.5; ./configure; make;)) && rm -f ./lib/libyaml.so; ln -s $(PWD)/libs/yaml-0.2.5/src/.libs/libyaml-0.so.2.0.9 ./lib/libyaml.so)
	test -f inc/yaml.h || cp ../../libs/yaml-0.2.5/include/yaml.h inc/yaml.h
	$(CC) $(CFLAGS) $(OBJS) $(READLINE) -L lib -lyaml -o $@

$(OBJS): $(INC)

$(OBJS) : | $(OBJDIR)

$(OBJDIR)/%.o: %.c
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	test -f inc/yaml.h || cp ../../libs/yaml-0.2.5/include/yaml.h inc/yaml.h
	$(CC) $(CFLAGS) -I inc -o $@ -c $<

$(OBJDIR) :
	mkdir $(OBJDIR)

$(READLINE) :
	$(MAKE) -C lib

.SECONDARY: $(OBJS)

.PHONY: all
all : $(NAME)

.PHONY: clean
clean : 
	rm -rf $(OBJS) $(OBJDIR)

.PHONY: fclean
fclean:
	$(MAKE) fclean -C lib
	rm -rf $(NAME) $(OBJS) $(OBJDIR)

.PHONY: relib
relib:
	$(MAKE) re -C lib

.PHONY: re
re: relib fclean all
