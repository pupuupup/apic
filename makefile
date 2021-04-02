## Simple C Makefile

SRC	=	./src/main.c
NAME	= apic
CC	=	gcc
RM	=	rm -f
OBJ	=	$(SRC:.c=.o)
CFLAGS	=	-O2 -W -Wall -Wextra -Werror
CFLAGS	+=	-I./hds/
all	:	$(NAME)
$(NAME)	:	$(OBJ)
		$(CC) -o $(NAME) $(OBJ) -lcurl
clean	:
		$(RM) $(OBJ)
fclean	:	clean
		$(RM) $(NAME)
re	:	fclean all
.PHONY	:	all clean fclean re

