# Thanks to https://spin.atomicobject.com/2016/08/26/makefile-c-projects/ for
# the advice on how to write this file properly.

TARGET_EXEC ?= a.out

OBJ_DIR ?= ./obj

SRCS := $(shell find -maxdepth 1 -name "*.cpp"; find include -name "*.cpp")
OBJS := $(SRCS:%=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := -Iinclude

CPPFLAGS ?= $(INC_FLAGS) -g -std=c++2a -Wall -MP -MMD \
						`pkg-config --cflags freetype2`

LDFLAGS := -lSDL2 -lfreetype -lGL -lGLEW -lGLU

$(TARGET_EXEC): $(OBJS) $(OBJ_DIR)/./main.cpp.o
	$(CXX) $(OBJS) $(LDFLAGS) -o $@ 

$(OBJ_DIR)/./main.cpp.o: main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c main.cpp -o $@

$(OBJ_DIR)/%.cpp.o: %.cpp %.h
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run : $(TARGET_EXEC)
	./a.out

gdb : $(TARGET_EXEC)
	gdb a.out

.PHONY: clean run gdb

clean:
	$(RM) -r $(OBJ_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
