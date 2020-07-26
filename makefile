
COMPILER = g++
OPS = -Wall -std=c++2a

test : obj/vec_test obj/stats_test obj/ecs_test

.PHONY: test

obj/vec_test : vec.h vec_test.cpp test.h
	${COMPILER} ${OPS} vec_test.cpp -o obj/vec_test
	./obj/vec_test

obj/stats.o : stats.h stats.cpp
	${COMPILER} ${OPS} -c stats.cpp -o obj/stats.o

obj/stats_test : obj/stats.o stats_test.cpp test.h
	${COMPILER} ${OPS} stats_test.cpp obj/stats.o -o obj/stats_test
	./obj/stats_test

obj/ecs_test : ecs.h ecs_test.cpp test.h util.h
	${COMPILER} ${OPS} ecs_test.cpp -o obj/ecs_test
	./obj/ecs_test
