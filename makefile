
COMPILER = g++
OPS = -Wall -std=c++2a -Iinclude

test : obj/stats_test obj/ecs_test
run : obj/run
	./obj/run

.PHONY: test run

obj/stats.o : stats.h stats.cpp
	${COMPILER} ${OPS} -c stats.cpp -o obj/stats.o

obj/stats_test : obj/stats.o stats_test.cpp include/test.h
	${COMPILER} ${OPS} stats_test.cpp obj/stats.o -o obj/stats_test
	./obj/stats_test

obj/ecs_test : include/ecs.h include/ecs_test.cpp include/test.h include/util.h
	${COMPILER} ${OPS} include/ecs_test.cpp -o obj/ecs_test
	./obj/ecs_test

obj/graphics.o : include/graphics.h include/graphics.cpp include/util.h
	${COMPILER} ${OPS} -c include/graphics.cpp -o obj/graphics.o

obj/main.o : main.cpp obj/graphics.o include/glpp.h
	${COMPILER} ${OPS} -c main.cpp -o obj/main.o

obj/run : obj/main.o 
	${COMPILER} ${OPS} obj/main.o obj/graphics.o -lSDL2 -lGL -lGLEW -lGLU -o obj/run
