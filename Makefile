comp = g++
flags = -std=c++17 -Wall -Wextra -Werror

src = src/main.cpp
target = build/execra

all = $(target)

$(target) : $(src)
	$(comp) $(flags) $(src) -o $(target)

clean :
	rm -f $(target)