# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/06/16 11:17:59 by tnaton            #+#    #+#              #
#    Updated: 2023/08/22 16:42:40 by bdetune          ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.c src
vpath %.o obj
vpath %.h inc

NAME = taskmaster

OBJDIR := obj

SRCS = main.c \
	   parser/parser.c \
	   parser/server.c \
	   parser/socket.c \
	   parser/logger.c \
	   parser/program.c \
	   parser/tree.c \
	   parser/utils.c \
	   parser/priorities.c \
	   parser/free_errors.c \
	   gnl/get_next_line.c \
	   gnl/get_next_line_utils.c \
	   signal.c \
	   report.c \
	   prelude_server.c \
	   log.c \
	   daemon.c \
	   administrator.c \
	   server.c \
	   launcher.c \
	   update.c

INC = taskmaster.h

MOREFLAGS = -Wformat=2				\
			-Wformat-overflow=2		\
			-Wformat-truncation=2	\
			-Wstringop-overflow=4	\
			-Winit-self				\
			-ftrapv					\
			-Wdate-time				\
			-Wconversion

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

$(NAME) : $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) -L libs -lyaml -lpthread -lcurl -o $@

$(OBJS): $(INC)

$(OBJS) : | $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -I inc -I libs/curl-8.2.1/include/curl -o $@ -c $<

$(OBJDIR) :
	mkdir -p $(OBJDIR)/parser $(OBJDIR)/gnl

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
