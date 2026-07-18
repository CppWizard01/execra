comp = g++
flags = -std=c++17 -Wall -Wextra -Werror -Iinclude

src = src/main.cpp src/shell.cpp src/input.cpp src/exec.cpp src/parser.cpp src/history.cpp
target = build/execra

all: $(target)

$(target): $(src)
	@mkdir -p build
	$(comp) $(flags) $(src) -o $(target)

clean:
	rm -f $(target)
