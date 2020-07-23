
COMPILER = g++
OPS = -Wall -std=c++2a

vec_test : vec.h vec_test.cpp
	${COMPILER} ${OPS} vec_test.cpp -o obj/vec_test
	./obj/vec_test
