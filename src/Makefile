SRC := .
INCLUDE := -I$(SRC)/include
BUILD := ../build
TARGET := $(BUILD)/mvim

CFLAGS := -Wall     # 输出警告信息
CFLAGS += -Wextra   # 输出更多的警告信息
CFLAGS += -pedantic # 严格按照语法语义进行编译
CFLAGS += -std=c99  # C语言版本
CFLAGS += -O2       # 优化级别

DEBUG := -g              # 如果需要调试信息
STRIP := strip $(TARGET) # 如果需要使用 strip 来减小可执行文件的大小

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(SOURCES:$(SRC)/%.c=$(BUILD)/%.o)

$(TARGET): $(OBJECTS)
	$(CC) $(INCLUDE) $(OBJECTS) -o $@
	$(STRIP)

$(BUILD)/%.o: $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

.PHONY: clean run

run:
	$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)
