#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "./include/mvim.h"
#include "./include/utils.h"

struct EditorConfig E;

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};

char *C_HL_keywords[] = {"switch", "if",      "while",   "for",    "break",     "continue", "return", "else",
                         "struct", "union",   "typedef", "static", "enum",      "class",    "case",   "int|",
                         "long|",  "double|", "float|",  "char|",  "unsigned|", "signed|",  "void|",  NULL};

struct EditorSyntax HLDB[] = {
    {"c", C_HL_extensions, C_HL_keywords, "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};

/* 回复终端输入模式 */
void disable_raw_mode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    {
        die("tcsetattr");
    }
}

/* 启用原始输入模式 */
void enable_raw_mode()
{
    /* 获取标准输入属性 */
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    /* 程序正常退出 exit()，或者通过 main 函数 return，执行指定函数。*/
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &=
        ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // 控制输出流 | 忽略回车换行 | 忽略 SIGINT | 奇偶校验 | 去除数据第8位
    raw.c_oflag &= ~(OPOST);                       // 对输出进行标准处理，如 \r 转换为 \r\n
    raw.c_cflag |= (CS8);                          // 设置每个字符 8bit 数据
    raw.c_lflag &= ~(ECHO | IEXTEN | ICANON |
                     ISIG); // 关闭回显 | 关闭扩展处理功能 | 关闭行缓冲(不需要输入会车) | 关闭信号，如 SIGINT
    raw.c_cc[VMIN] = 0;     // 指定 read() 调用读取最少字符数，0 表示立即返回
    raw.c_cc[VTIME] = 1;    // 指定 read() 调用超时时间，单位为十分之一秒(1/10 second)

    // 应用属性，TCSAFLUSH 表示丢弃所用缓冲区的数据
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        die("tcsetattr");
    }
}

/* 读取按键 */
int editor_read_key()
{
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN) // EAGAIN 表示系统资源暂时无法获取等原因，可以稍后继续尝试
            die("read");
    }

    /* 处理控制流字符 */
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) // 读取下一个字符
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) // 读取后面第二个字符
            return '\x1b';

        /* 处理控制流字符标志'[' */
        if (seq[0] == '[')
        {
            /* '[' 字符后的字符为数字 */
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) // 读取下一个字符
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY; // "\x1b[1~" 表示键盘上的 HOME 按键
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            /* '[' 后字符的字符不是数字 */
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP; // "\x1b[A" 表示向上箭头
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        /* \x1b 后的字符不是 '[' */
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY; // "\x1bOH" 在一些旧终端中也表示 HOME 按键
            case 'F':
                return END_KEY;
            }
        }
        /* escape 字符 */
        return '\x1b';
    }
    /* 处理一般字符 */
    else
    {
        return c;
    }
}

/* 获取光标位置 */
int get_cursor_position(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) // 查询光标位置，响应格式为 "\x1b[{ROW};{COLUMN}R"，R 表示结束
        return -1;

    /* 读取响应信息 */
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;

    /* 将响应信息中的光标位置信息返回 */
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

/* 获取窗口大小 */
int get_window_size(int *rows, int *cols)
{
    struct winsize ws;

    /*
     * 如果通过 ioctl 获取窗口大小失败则
     * 使用获取右下角光标位置的方式来获取窗口行列
     * Terminal Input Outpu Control Get Window Size
     */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) /* 设置一个足够大的数使光标跳转到右下角 */
            return -1;
        return get_cursor_position(rows, cols);

        return -1;
    }
    else
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

int editor_row_cx_to_rx(EditorRow *row, int cx)
{
    int rx = 0; // 实际渲染的列数
    int j;
    for (j = 0; j < cx; j++)
    {
        /* 处理 tab 字符 */
        if (row->chars[j] == '\t')
            rx += (MVIM_TAB_STOP - 1) - (rx % MVIM_TAB_STOP);
        rx++;
    }
    return rx;
}

int editor_row_rx_to_cx(EditorRow *row, int rx)
{
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++)
    {
        if (row->chars[cx] == '\t')
            cur_rx += (MVIM_TAB_STOP - 1) - (cur_rx % MVIM_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

void editor_update_row(EditorRow *row)
{
    int tabs = 0;
    int j;
    /* 计算 tab 个数 */
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs * (MVIM_TAB_STOP) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        /* 处理制表符 */
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            /* 补全 tab 的空格数 */
            while (idx % MVIM_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        /* 普通字符 */
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editor_update_syntax(row);
}

/* 记录一行的信息，包括字符串长度合具体内容 */
void editor_insert_row(int at, char *s, size_t len)
{
    if (at < 0 || at > E.num_rows)
        return;

    E.row = realloc(E.row, sizeof(EditorRow) * (E.num_rows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(EditorRow) * (E.num_rows - at));
    for (int j = at + 1; j <= E.num_rows; j++)
        E.row[j].idx++;

    E.row[at].idx = at;

    E.row[at].size = len; // 新行的字符长度
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len); // 新行的内容
    E.row[at].chars[len] = '\0';     // 最后一个字符结束标志

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editor_update_row(&E.row[at]); // 实际渲染的行需要处理，加上制表符的空格数

    E.num_rows++; // 行数加一
    E.dirty++;
}

void eidtor_row_insert_char(EditorRow *row, int at, int c)
{
    if (at < 0 || at > row->size)
        at = row->size;

    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // 将 at 位置后面的内容后移

    row->size++;
    row->chars[at] = c;
    editor_update_row(row);
    E.dirty++;
}

void editor_row_append_string(EditorRow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editor_update_row(row);
    E.dirty++;
}

/* 插入字符 */
void editor_insert_char(int c)
{
    if (E.cy == E.num_rows)
    {
        editor_insert_row(E.num_rows, "", 0);
    }
    eidtor_row_insert_char(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editor_insert_newline()
{
    if (E.cx == 0)
    {
        editor_insert_row(E.cy, "", 0);
    }
    else
    {
        EditorRow *row = &E.row[E.cy];
        editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editor_update_row(row);
    }
    E.cy++;
    E.cx = 0;
}

/* 删除字符 */
void editor_row_del_char(EditorRow *row, int at)
{
    if (at < 0 || at >= row->size)
        return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editor_update_row(row);
    E.dirty++;
}

void editor_del_char()
{
    if (E.cy == E.num_rows)
        return;
    if (E.cx == 0 && E.cy == 0)
        return;
    EditorRow *row = &E.row[E.cy]; // 获取所在行
    /* 光标前还有字符 */
    if (E.cx > 0)
    {
        editor_row_del_char(row, E.cx - 1);
        E.cx--;
    }
    /* 光标前没有字符 */
    else
    {
        E.cx = E.row[E.cy - 1].size;
        editor_row_append_string(&E.row[E.cy - 1], row->chars, row->size);
        editor_del_row(E.cy);
        E.cy--;
    }
}

void editor_free_row(EditorRow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editor_del_row(int at)
{
    if (at < 0 || at >= E.num_rows)
        return;
    editor_free_row(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(EditorRow) * (E.num_rows - at - 1));
    for (int j = at; j < E.num_rows - 1; j++)
        E.row[j].idx--;
    E.num_rows--;
    E.dirty++;
}

void editor_open(const char *filename)
{
    free(E.filename);
    E.filename = strdup(filename);

    editor_select_syntax_highlight();

    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    /* 将文件内容读取到 E.row 中 */
    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        /* 减去回车换行的个数 */
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;

        /* 读取一行内容 */
        editor_insert_row(E.num_rows, line, linelen);
    }

    free(line);
    fclose(fp);
    E.dirty = 0;
}

char *editor_rows_to_string(int *buflen)
{
    int totlen = 0;
    int j;
    /* 获取所有内容总长度，每一行预留一个换行符 */
    for (j = 0; j < E.num_rows; j++)
        totlen += E.row[j].size + 1;

    *buflen = totlen;
    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.num_rows; j++)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void editor_set_status_message(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL); // 获取 1970年1月1日 到现在的秒数
}

void editor_find_callback(char *query, int key)
{
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;
    if (saved_hl)
    {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP)
    {
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }
    if (last_match == -1)
        direction = 1;
    int current = last_match;

    /* 遍历每一行 */
    int i;
    for (i = 0; i < E.num_rows; i++)
    {
        current += direction;
        if (current == -1)
            current = E.num_rows - 1;
        else if (current == E.num_rows)
            current = 0;
        EditorRow *row = &E.row[current];
        char *match = strstr(row->render, query); // 匹配字符串
        if (match)
        {
            last_match = current;
            E.cy = current;
            /* 光标跳转过去 */
            E.cy = i;
            E.cx = editor_row_rx_to_cx(row, match - row->render);
            E.cx = match - row->render;
            E.rowoff = E.num_rows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editor_find()
{
    /* 保存坐标 */
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_callback);
    if (query)
    {
        free(query);
    }
    /* 取消查询时恢复坐标 */
    else
    {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

void editor_save()
{
    if (E.filename == NULL)
    {
        E.filename = editor_prompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL)
        {
            editor_set_status_message("Save aborted");
            return;
        }
        editor_select_syntax_highlight();
    }

    int len;
    char *buf = editor_rows_to_string(&len);
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644); // 以读写的方式打开文件，没有就创建一个 | rw-r--r--
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.dirty = 0;
                editor_set_status_message("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}

/* 生成所有需要打印信息 */
void ab_append(AppendBuffer *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (NULL == new)
        return;

    memcpy(&new[ab->len], s, len); // 将新的内容添加到尾部

    /* 更新数据 */
    ab->b = new;
    ab->len = ab->len + len;
}

/* 释放资源 */
void ab_free(AppendBuffer *ab)
{
    free(ab->b);
}

/* 滚屏 */
void editor_scroll()
{
    E.rx = 0;

    if (E.cy < E.num_rows)
    {
        E.rx = editor_row_cx_to_rx(&E.row[E.cy], E.cx);
    }

    /* 往上滚动 */
    if (E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }

    /* 往下滚动 */
    if (E.cy >= E.rowoff + E.screen_rows)
    {
        E.rowoff = E.cy - E.screen_rows + 1;
    }

    /* 往左滚动 */
    if (E.rx < E.coloff)
    {
        E.coloff = E.rx;
    }

    /* 往右滚动 */
    if (E.rx >= E.coloff + E.screen_cols)
    {
        E.coloff = E.rx - E.screen_cols + 1;
    }
}

/* 输出数据到屏幕 */
void editor_draw_rows(AppendBuffer *ab)
{
    int y;
    for (y = 0; y < E.screen_rows; y++)
    {
        int filerow = y + E.rowoff; // 文件行位置 = 当前屏幕行数 + 已经隐藏的内容的行数
        if (filerow >= E.num_rows)
        {
            /* 没有文本内容输入时打印版本信息 */
            if (E.num_rows == 0 && y == E.screen_rows / 3)
            {
                char welcom[80];
                int welcomlen = snprintf(welcom, sizeof(welcom), "Kilo Editor -- Version %s", MVIM_VERSION);
                if (welcomlen > E.screen_cols)
                    welcomlen = E.screen_cols;

                int padding = (E.screen_cols - welcomlen) / 2;
                if (padding)
                {
                    ab_append(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    ab_append(ab, " ", 1);
                ab_append(ab, welcom, welcomlen);
            }
            /* 有文本输入 */
            else
            {
                ab_append(ab, "~", 1);
            }
        }
        else
        {
            int len = E.row[filerow].rsize - E.coloff; // 获取要打印的行的实际内容长度
            if (len < 0)
                len = 0;
            if (len > E.screen_cols)
                len = E.screen_cols;
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++)
            {
                if (iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    ab_append(ab, "\x1b[7m", 4);
                    ab_append(ab, &sym, 1);
                    ab_append(ab, "\x1b[m", 3);
                    if (current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        ab_append(ab, buf, clen);
                    }
                }
                else if (hl[j] == HL_NORMAL)
                {
                    if (current_color != -1)
                    {

                        ab_append(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    ab_append(ab, &c[j], 1);
                }
                else
                {
                    int color = editor_syntax_to_color(hl[j]);
                    if (color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab_append(ab, buf, clen);
                    }
                    ab_append(ab, &c[j], 1);
                }
            }
            ab_append(ab, "\x1b[39m", 5);
        }
        ab_append(ab, "\x1b[K", 3); // 2K: 清除整行 1K: 清除光标左边 0K: 清除光标右边(默认)
        ab_append(ab, "\r\n", 2);   // 换行
    }
}

/* 打印状态栏 */
void editor_draw_status_bar(AppendBuffer *ab)
{
    ab_append(ab, "\x1b[7m", 4); // 反转前景和背景颜色
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.num_rows,
                       E.dirty ? "(modified)" : "");

    int rlen =
        snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.num_rows);

    if (len > E.screen_cols)
        len = E.screen_cols;
    ab_append(ab, status, len);

    /* 在状态栏末端打印 rstatus */
    while (len < E.screen_cols)
    {
        if (E.screen_cols - len == rlen)
        {
            ab_append(ab, rstatus, rlen);
            break;
        }
        else
        {
            ab_append(ab, " ", 1);
            len++;
        }
    }
    ab_append(ab, "\x1b[m", 3); // 重置颜色配置
    ab_append(ab, "\r\n", 2);   // 换行
}

/* 输出信息栏 */
void editor_draw_message_bar(AppendBuffer *ab)
{
    ab_append(ab, "\x1b[K", 3); // 清除行内光标右边内容
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screen_cols)
        msglen = E.screen_cols;

    /* 5 秒更新一次状态信息 */
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        ab_append(ab, E.statusmsg, msglen);
}

/* 输出最新屏幕内容 */
void editor_refresh_screen()
{
    /* 处理滚动产生的 rowoff, coloff 和 rx 改变 */
    editor_scroll();

    /* 定义一个打印信息结构体并初始化 */
    AppendBuffer ab = ABUF_INIT;

    ab_append(&ab, "\x1b[H", 3);    // 光标位置设置为左上角
    ab_append(&ab, "\x1b[?25l", 6); // 隐藏光标

    editor_draw_rows(&ab);
    editor_draw_status_bar(&ab);
    editor_draw_message_bar(&ab);

    /* 设置光标位置为实际相对屏幕位置 */
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6); // 显示光标

    /* 输出屏幕内容 */
    write(STDOUT_FILENO, ab.b, ab.len);

    /* 释放资源 */
    ab_free(&ab);
}

char *editor_prompt(char *prompt, void (*callback)(char *, int))
{
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';
    while (1)
    {
        editor_set_status_message(prompt, buf);
        editor_refresh_screen();
        int c = editor_read_key();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if (buflen != 0)
                buf[--buflen] = '\0';
        }
        else if (c == '\x1b')
        {
            editor_set_status_message("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        }
        else if (c == '\r')
        {
            if (buflen != 0)
            {
                editor_set_status_message("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        }
        else if (!iscntrl(c) && c < 128)
        {
            if (buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if (callback)
            callback(buf, c);
    }
}

/* 移动光标 */
void editor_move_cursor(int key)
{
    EditorRow *row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];

    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        /* 移动到上一行的末尾 */
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size; // 光标所在列为当前光标所在行的内容的大小(最后一个字符的右边)
        }
        break;
    case ARROW_DOWN:
        /* 向下移动没有超过内容的行数 */
        if (E.cy < E.num_rows)
            E.cy++;
        break;
    case ARROW_RIGHT:
        /* 行内有内容并且光标所在列小于内容长度 */
        if (row && E.cx < row->size)
        {
            E.cx++;
        }
        /* 移动到下一行开头 */
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    }

    row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy]; // 获取当前行
    int rowlen = row ? row->size : 0;                 // 获取当前行内容长度

    /* 限制光标往右移(没有字符的位置) */
    if (E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}

/* 处理按键事件 */
void editor_process_keypress()
{
    static int quit_times = MVIM_QUIT_TIMES;

    /* 获取按键 */
    int c = editor_read_key();

    switch (c)
    {
    case '\r':
        editor_insert_newline();
        break;

        /* ctrl + q 退出 */
    case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0)
        {
            editor_set_status_message("WARNING!!! File has unsaved changes. "
                                      "Press Ctrl-Q %d more times to quit.",
                                      quit_times);
            quit_times--;
            return;
        }
        write(STDOUT_FILENO, "\x1b[2J", 4); // 清空屏幕
        write(STDOUT_FILENO, "\x1b[H", 3);  // 设置光标到左上角
        exit(0);
        break;

    case CTRL_KEY('s'):
        editor_save();
        break;

    case HOME_KEY:
        E.cx = 0; // 光标设置到第一列
        break;

    case END_KEY:
        if (E.cy < E.num_rows)
            E.cx = E.row[E.cy].size;
        break;

    case CTRL_KEY('f'):
        editor_find();
        break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if (c == DEL_KEY)
            editor_move_cursor(ARROW_RIGHT);
        editor_del_char();
        break;

    case PAGE_UP:
    case PAGE_DOWN: {
        /* 往上翻页 */
        if (c == PAGE_UP)
        {
            E.cy = E.rowoff;
        }
        /* 往下翻页 */
        else if (c == PAGE_DOWN)
        {
            E.cy = E.rowoff + E.screen_rows - 1;
            /* 光标位置设置为最后一行 */
            if (E.cy > E.num_rows)
                E.cy = E.num_rows;
        }

        /* 翻页时设置光标位置为第一行或者最后一行 */
        int count = E.screen_rows;
        while (count--)
        {
            editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
    }
    break;

        /* 处理方向键 */
    case ARROW_LEFT:  // D
    case ARROW_RIGHT: // C
    case ARROW_UP:    // A
    case ARROW_DOWN:  // B
        editor_move_cursor(c);
        break;

    case CTRL_KEY('l'):
    case '\x1b':
        break;

    default:
        editor_insert_char(c);
        break;
    }
    quit_times = MVIM_QUIT_TIMES;
}

void editor_update_syntax(EditorRow *row)
{
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if (E.syntax == NULL)
        return;

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment)
        {
            if (!strncmp(&row->render[i], scs, scs_len))
            {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string)
        {
            if (in_comment)
            {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len))
                {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else
                {
                    i++;
                    continue;
                }
            }
            else if (!strncmp(&row->render[i], mcs, mcs_len))
            {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS)
        {
            if (in_string)
            {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize)
                {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS)
        {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || ((c == '.') && (prev_hl == HL_NUMBER)))
            {

                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep)
        {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2)
                    klen--;
                if (!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen]))
                {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.num_rows)
        editor_update_syntax(&E.row[row->idx + 1]);
}

int editor_syntax_to_color(int hl)
{
    switch (hl)
    {
    case HL_COMMENT:
    case HL_MLCOMMENT:
        return 36;
    case HL_KEYWORD1:
        return 33;
    case HL_KEYWORD2:
        return 32;
    case HL_STRING:
        return 35;
    case HL_NUMBER:
        return 31;
    case HL_MATCH:
        return 34;
    default:
        return 37;
    }
}

void editor_select_syntax_highlight()
{
    E.syntax = NULL;
    if (E.filename == NULL)
        return;

    char *ext = strrchr(E.filename, '.'); // 获取文件扩展名

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++)
    {
        struct EditorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i])
        {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i])))
            {
                E.syntax = s;
                int filerow;
                for (filerow = 0; filerow < E.num_rows; filerow++)
                {
                    editor_update_syntax(&E.row[filerow]);
                }

                return;
            }
            i++;
        }
    }
}

int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

/* 初始化 */
void init_editor()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.syntax = NULL;

    if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
        die("get_window_size");

    E.screen_rows -= 2; // 预留状态栏合信息栏
}

int main(int argc, char *argv[])
{
    enable_raw_mode(); // 开启原始输入模式
    init_editor();
    if (argc >= 2)
    {
        editor_open(argv[1]);
    }

    editor_set_status_message("帮助: Ctrl-S = 保存 | Ctrl-Q = 退出 | Ctrl-F = 搜索");

    /* 循环地接收按键并处理，然后刷新内容 */
    while (1)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }
    return 0;
}
