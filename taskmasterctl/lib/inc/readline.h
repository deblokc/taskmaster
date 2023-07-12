/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   readline.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/12 15:48:57 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/12 17:19:21 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdbool.h>
#include <termios.h>
#include <stdio.h>

#define BUF_SIZE 4096

enum	key {
	K_ARROW_UP = 1,    //0x1b[A    UP IN HISTORY (older)
	K_ARROW_DOWN = 2,  //0x1b[B    DOWN IN HISTORY (newer)
	K_ARROW_RIGHT = 3, //0x1b[C    CURSOR TO RIGHT (reprint actual char)
	K_ARROW_LEFT = 4,  //0x1b[D    CURSOR TO LEFT ("\b")
	K_BACKSPACE = 5,   //0x7f      DELETE CHAR BACKWARD ("\b \b")
	K_DELETE = 6,      //0x1b[3~   DELETE CHAR FORWARD (reprint whole line ?)
	K_CTRL_A = 7,      //0x1       CURSOR FULL LEFT ("\b" * n)
	K_CTRL_E = 8,      //0x5       CURSOR FULL RIGHT (no clue how)
	K_CTRL_L = 9,      //0xc       CLEAR TERMINAL (how ???)
	K_ENTER = 10,      //0xd       END LINE (can be sent anywhere)
	K_TAB = 11         //0x9       AUTOCOMPLETE (T.T)
};

struct s_readline {
	char			*line;
	size_t			cursor;
	size_t			len;
	char			*prompt;
	size_t			prompt_len;
	bool			init;
	struct termios	base;
	struct termios	term;
	struct s_history	*first; // oldest line added in history
	struct s_history	*last; // newest line added in history
};

struct s_history {
	char				*line;
	struct s_history	*next;
	struct s_history	*prev;
};

struct s_readline *get_global(void);

void cursor_right(struct s_readline *global);
void cursor_left(struct s_readline *global);
void cursor_save(void);
void cursor_restore(void);
void cursor_clear_endline(void);

void exec_key(char *buf, struct s_readline *global);
