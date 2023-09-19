# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/06/16 11:17:59 by tnaton            #+#    #+#              #
#    Updated: 2023/09/19 11:47:14 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.c src
vpath %.o obj
vpath %.h inc

NAME = ../../taskmasterctl

OBJDIR := obj

SRCS = main.c parser.c file_history.c

READLINE = lib/readline.a

YAML = lib/libyaml.a

INC = taskmasterctl.h

YAML_INC = yaml.h

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

$(NAME) : $(READLINE) $(YAML) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) $(READLINE) $(YAML) -L lib -o $@

$(OBJS): $(INC) $(YAML_INC)

$(OBJS) : | $(OBJDIR)

$(YAML_INC):
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	test -f inc/yaml.h || cp ../../libs/yaml-0.2.5/include/yaml.h inc/yaml.h

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -I inc -o $@ -c $<

$(OBJDIR) :
	mkdir $(OBJDIR)

$(READLINE) :
	$(MAKE) -C lib

$(YAML) :
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	test -f inc/yaml.h || cp ../../libs/yaml-0.2.5/include/yaml.h inc/yaml.h
	((test -f ../../libs/yaml-0.2.5/src/.libs/libyaml.a || (cd ../../libs/yaml-0.2.5; ./configure; make;)) && rm -rf ./lib/libyaml.a; ln -s $(PWD)/libs/yaml-0.2.5/src/.libs/libyaml.a ./lib/libyaml.a)

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
