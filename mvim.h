#ifndef MVIM_H
#define MVIM_H

#include <termios.h>
#include <time.h>
#include <unistd.h>

/* macro */
#define MVIM_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define MVIM_TAB_STOP 8
#define MVIM_QUIT_TIMES 3

/* data */

typedef struct ERow
{
    int size;     // 行数据字符个数
    int rsize;    // 渲染行字符个数
    char *chars;  // 实际行字符串
    char *render; // 要渲染的行字符串
} ERow;

typedef struct EditroConfig
{
    int cx, cy; // 光标位置
    int rx;
    int rowoff;      // 屏幕打印的内容相对文件内容偏移的行数
    int coloff;      // 屏幕打印的内容相对文件内容偏移的列数
    int screen_rows; // 屏幕行数
    int screen_cols; // 屏幕列数
    int num_rows;    // 要打印内容行数
    ERow *row;       // 行内容数组
    int dirty;
    char *filename;              // 文件名
    char statusmsg[80];          // 状态栏信息
    time_t statusmsg_time;       // 状态信息时间戳
    struct termios orig_termios; // 终端模式
} EditroConfig;

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
struct ABuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void die(const char *s);                               // 错误处理
void disable_raw_mode();                               // 回复终端模式
void enable_raw_mode();                                // 设置终端为原始模式
int editor_read_key();                                 // 读取按键
int get_cursor_position(int *rows, int *cols);         // 获取光标位置
int get_window_size(int *rows, int *cols);             // 获取屏幕尺寸
int editor_row_cx_to_rx(ERow *row, int cx);            // 转换实际渲染的列(制表符)
void editor_update_row(ERow *row);                     // 更新一行内容
void editor_insert_row(int at, char *s, size_t len);   // 添加一行内容
void eidtor_row_insert_char(ERow *row, int at, int c); // 插入字符
void editor_row_append_string(ERow *row, char *s, size_t len);
void editor_row_del_char(ERow *row, int at);             // 删除字符
void editor_del_char();                                  // 删除字符
void editor_free_row(ERow *row);                         // 释放一行资源
void editor_del_row(int at);                             // 删除一行
void editor_insert_char(int c);                          // 插入字符
void editor_open(const char *filename);                  // 打开文件
char *editor_rows_to_string(int *buflen);                // 将所有内容格式化为字符串
void editor_set_status_message(const char *fmt, ...);    // 设置状态栏信息
void editor_save();                                      // 保存到文件
void ab_append(struct ABuf *ab, const char *s, int len); // 添加打印内容
void ab_free(struct ABuf *ab);                           // 释放资源
void editor_scroff();                                    // 滚屏处理
void editor_draw_rows(struct ABuf *ab);                  // 打印一行
void editor_draw_status_bar(struct ABuf *ab);            // 打印状态栏
void editor_draw_message_bar(struct ABuf *ab);           // 打印信息栏
void editor_refresh_screen();                            // 刷新输出内容
void editor_move_cursor(int key);                        // 移动光标
void editor_process_keypress();                          // 处理按键
void init_editor();                                      // 初始化

#endif
