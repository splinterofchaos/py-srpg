
COMPILER = g++
EXTRA = 
OPS = -Wall -std=c++2a -Iinclude `pkg-config --cflags freetype2` \
			${EXTRA}

test : obj/stats_test obj/ecs_test
run : obj/run
	./obj/run

.PHONY: test run

obj/stats.o : stats.h stats.cpp
	${COMPILER} ${OPS} -c stats.cpp -o obj/stats.o

obj/stats_test : stats.h stats_test.cpp include/test.h
	${COMPILER} ${OPS} stats_test.cpp obj/stats.o -o obj/stats_test
	./obj/stats_test

obj/ecs_test : include/ecs.h include/ecs_test.cpp include/test.h include/util.h
	${COMPILER} ${OPS} include/ecs_test.cpp -o obj/ecs_test
	./obj/ecs_test

obj/graphics.o : include/graphics.h include/graphics.cpp include/util.h
	${COMPILER} ${OPS} -c include/graphics.cpp -o obj/graphics.o

obj/shaders.o : shaders.* include/graphics.h include/util.h
	${COMPILER} ${OPS} -c shaders.cpp -o obj/shaders.o

obj/font.o : font.cpp font.h include/glpp.h include/util.h
	${COMPILER} ${OPS} -c font.cpp -o obj/font.o

obj/math.o : include/math.h include/math.cpp
	${COMPILER} ${OPS} -c include/math.cpp -o obj/math.o

obj/grid.o : grid.h grid.cpp
	${COMPILER} ${OPS} -c grid.cpp -o obj/grid.o

obj/game.o : game.h game.cpp components.h font.h grid.h shaders.h
	${COMPILER} ${OPS} -c game.cpp -o obj/game.o

obj/action.o : action.h action.cpp components.h game.h include/timer.h \
						   include/util.h
	${COMPILER} ${OPS} -c action.cpp -o obj/action.o

obj/dijkstra.o : dijkstra.* game.h
	${COMPILER} ${OPS} -c dijkstra.cpp -o obj/dijkstra.o

obj/main.o : main.cpp include/*.h *.h
	${COMPILER} ${OPS} -c main.cpp -o obj/main.o

obj/run : obj/main.o obj/graphics.o obj/shaders.o obj/font.o obj/math.o \
					obj/action.o obj/grid.o obj/game.o obj/dijkstra.o
	${COMPILER} ${OPS} obj/*.o -lSDL2 -lfreetype -lGL -lGLEW -lGLU -o obj/run
