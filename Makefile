
default: lisod

lisod: lisod.c
	@gcc lisod.c -o lisod -Wall -Werror

clean:
	@rm lisod
