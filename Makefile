image:
	docker build -t utility-dev .

shell: 
	docker run --rm -it --pid=host -v "$(PWD)":/src utility-dev bash

build:
	@clang -Wall -Wextra -g -o utility utility.c -lncurses

run: build
	@./spp -s