
COMPILER = g++
OPS = -Wall -std=c++2a -Iinclude `pkg-config --cflags freetype2`

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

obj/shaders.o : shaders.* include/graphics.h include/util.h
	${COMPILER} ${OPS} -c shaders.cpp -o obj/shaders.o

obj/main.o : main.cpp include/*.h
	${COMPILER} ${OPS} -c main.cpp -o obj/main.o

obj/run : obj/main.o obj/graphics.o obj/shaders.o
	${COMPILER} ${OPS} obj/*.o -lSDL2 -lfreetype -lGL -lGLEW -lGLU -o obj/run
