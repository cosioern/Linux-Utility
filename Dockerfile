FROM ubuntu:26.04
RUN apt-get update && apt-get install -y clang make gdb valgrind strace libncurses-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src