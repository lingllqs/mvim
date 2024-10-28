#ifndef MVIM_H
#define MVIM_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


#define MVIM_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define MVIM_TAB_STOP 8
#define MVIM_QUIT_TIMES 3

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

struct EditorSyntax
{
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

/* data */

typedef struct EditorRow
{
    int idx;
    int size;     // 行数据字符个数
    int rsize;    // 渲染行字符个数
    char *chars;  // 实际行字符串
    char *render; // 要渲染的行字符串
    unsigned char *hl;
    int hl_open_comment;
} EditorRow;

enum EditorHighlight
{
    HL_NORMAL = 0, // 一般情况
    HL_COMMENT,    // 注释
    HL_KEYWORD1,   // 关键字
    HL_MLCOMMENT,  // 多行注释
    HL_KEYWORD2,   // 关键字
    HL_STRING,     // 字符串
    HL_NUMBER,     // 数字
    HL_MATCH       // 搜索匹配
};

typedef struct EditorConfig
{
    int cx, cy;                  // 相对整个文本的坐标
    int rx;                      // 实际渲染的坐标(制表符宽度处理)
    int rowoff;                  // 当前已滚动行数
    int coloff;                  // 当前已滚动列数
    int screen_rows;             // 屏幕行数
    int screen_cols;             // 屏幕列数
    int num_rows;                // 要打印内容行数
    EditorRow *row;              // 行内容数组
    int dirty;                   // 内容状态改变
    char *filename;              // 文件名
    char statusmsg[80];          // 状态栏信息
    time_t statusmsg_time;       // 状态信息时间戳
    struct termios orig_termios; // 终端模式
    struct EditorSyntax *syntax;
} EditorConfig;

enum EditorKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/* append buffer */
typedef struct AppendBuffer
{
    char *b; // 输出内容
    int len; // 内容总长度
} AppendBuffer;

#define ABUF_INIT {NULL, 0}

void disable_raw_mode();                                            // 回复终端模式
void enable_raw_mode();                                             // 设置终端为原始模式
int editor_read_key();                                              // 读取按键
int get_cursor_position(int *rows, int *cols);                      // 获取光标位置
int get_window_size(int *rows, int *cols);                          // 获取屏幕尺寸
int editor_row_cx_to_rx(EditorRow *row, int cx);                    // 转换实际渲染的列(制表符)
int editor_row_rx_to_cx(EditorRow *row, int rx);                    // 转换为初始的字符流
void editor_update_row(EditorRow *row);                             // 更新一行内容
void editor_insert_row(int at, char *s, size_t len);                // 添加一行内容
void eidtor_row_insert_char(EditorRow *row, int at, int c);         // 插入字符
void editor_row_append_string(EditorRow *row, char *s, size_t len); // 附加字符串
void editor_row_del_char(EditorRow *row, int at);                   // 删除字符
void editor_del_char();                                             // 删除字符
void editor_free_row(EditorRow *row);                               // 释放一行资源
void editor_del_row(int at);                                        // 删除一行
void editor_insert_char(int c);                                     // 插入字符
void editor_insert_newline();                                       // 插入新行
void editor_open(const char *filename);                             // 打开文件
char *editor_rows_to_string(int *buflen);                           // 将所有内容格式化为字符串
void editor_set_status_message(const char *fmt, ...);               // 设置状态栏信息
void editor_find_callback(char *query, int key);                    // 搜索
void editor_find();                                                 // 搜索
void editor_save();                                                 // 保存到文件
void ab_append(AppendBuffer *ab, const char *s, int len);           // 添加打印内容
void ab_free(AppendBuffer *ab);                                     // 释放资源
void editor_scroll();                                               // 滚屏处理
void editor_draw_rows(AppendBuffer *ab);                            // 打印一行
void editor_draw_status_bar(AppendBuffer *ab);                      // 打印状态栏
void editor_draw_message_bar(AppendBuffer *ab);                     // 打印信息栏
void editor_refresh_screen();                                       // 刷新输出内容
char *editor_prompt(char *prompt, void (*callback)(char *, int));   // 编辑提示
void editor_move_cursor(int key);                                   // 移动光标
void editor_process_keypress();                                     // 处理按键
void init_editor();                                                 // 初始化
void editor_update_syntax(EditorRow *row);                          // 更新语法
int editor_syntax_to_color(int hl);                                 // 应用颜色
void editor_select_syntax_highlight();                              // 选择高亮
int is_separator(int c);                                            // 分隔符判断

#endif
