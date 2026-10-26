#include <iostream>
#include <vector>
#include <string>
#include <conio.h>
#include <unordered_set>
#include <utility>
#include <queue>
using std::vector;
using std::cout;
using std::endl;
using std::string;
using std::unordered_set;
using std::pair;
using std::to_string;
// class GUI {
//     void start() { // 启动

//     }
   

// }

struct Config {
    int screen_width = 80;
    int screen_height = 20;
    int board_width = 10;
    int board_height = 10;
    int snake_init_length = 3;
    int food_init_count = 4;
};

inline Config config;

enum { // key codes
    KEY_UP = 72,
    KEY_DOWN = 80,
    KEY_LEFT = 75,
    KEY_RIGHT = 77,
    KEY_ENTER = 13,
    KEY_ESC = 27,
    KEY_CTRL_C = 3
};


enum class NEXT_ACTION {
    TITLE_UI,
    SNAKE_UI,
    EXIT
};

enum { // directions
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

const vector<char> DIRECTION_SYMBOL_MAP = {
    '^',
    'v',
    '<',
    '>',
};

// basic

struct Point {
    int y;
    int x;
    bool operator==(const Point& other) const {
        return y == other.y && x == other.x;
    }
};

struct PointHash {
    size_t operator()(const Point& point) const {
        return std::hash<int>()(point.y) ^ std::hash<int>()(point.x);
    }
};

int flatten_coordinate(int y, int x, int width) {
    return y * width + x;
}

pair<int, int> expand_coordinate(int flat_coordinate, int width) {
    return {flat_coordinate / width, flat_coordinate % width};
}

Point random_pick_from_set(unordered_set<Point, PointHash>& set) {
    auto it = set.begin();
    std::advance(it, std::rand() % set.size());
    return *it;
}

Point point_move_one_step_mod(Point point, int direction, int mod_y, int mod_x) { // 坐标朝direction移动一格
    /*
        逻辑：
            如果移动后超出边界，则返回边界值
            否则返回移动后的坐标
    */
    switch (direction) {
        case UP:
            return {(point.y - 1 + mod_y) % mod_y, point.x};
        case DOWN:
            return {(point.y + 1) % mod_y, point.x};
        case LEFT:
            return {point.y, (point.x - 1 + mod_x) % mod_x};
        case RIGHT:
            return {point.y, (point.x + 1) % mod_x};
        default:
            throw std::invalid_argument("Invalid direction");
    }
}

int get_middle_idx(int length) { // 获取中间索引 (偶数时取靠前面的)
    return (length - 1) / 2;
}

int get_head_idx_slice_inside_slice(int small_length, int big_length) { // 小区间放在大区间中间，返回小区间相对于大区间的偏移量
    /* 如果无法正好放在中间，则取靠前面的
        示例：
            1 : 3 -> 1
            1 : 2 -> 0
            2 : 3 -> 0
            2 : 4 -> 1
    */

    
    return (big_length - small_length) / 2;
}


//

class Cell {
    public:
    char symbol;
    string format; // 空字符串代表无特殊格式
    Cell(char symbol = ' ', string format = "") {
        this->symbol = symbol;
        this->format = format;
    }
};

class Screen {
    vector<vector<Cell>> data;
    int width;
    int height;

    public:
    Screen(int width, int height) {
        this->width = width;
        this->height = height;
        this->data = vector<vector<Cell>>(height, vector<Cell>(width, Cell()));
    }

    public:
    void render() {
        // agent生成
        string frame = "\033[H";
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                const Cell& cell = data[i][j];
                if (cell.format.empty()) {
                    frame.push_back(cell.symbol);
                } else {
                    frame.append("\033[" + cell.format + "m" + cell.symbol + "\033[0m");
                }
            }
            frame += "\033[K\n";
        }
        frame += "\033[J";
        cout << frame << std::flush;
    }

    void write_point(Point point, char symbol, string format = "") {
        this->data[point.y][point.x] = Cell(symbol, format);
    }

    void write_line_center(int line_idx, const string& text) {
        int text_start_idx = get_middle_idx(width) - get_middle_idx(text.size());
        for (int i = 0; i < text.size(); i++) {
            this->data[line_idx][text_start_idx + i] = Cell(text[i]);
        }
    }   
    void write_lines_leftdown(const vector<string>& texts) { // 写在左下角
        for (int i = 0; i < texts.size(); i++) {
            this->write_line_left(height - 1 - i, texts[i]);
        }
    }
    void write_line_left(int line_idx, const string& text) {
        int text_start_idx = 0;
        for (int i = 0; i < text.size(); i++) {
            this->data[line_idx][text_start_idx + i] = Cell(text[i]);
        }
    }
    void clear() {
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                this->data[i][j] = Cell();
            }
        }
    }
};

class UI {
    protected:
    Screen* screen;

    public:

    UI() {
        this->screen = new Screen(config.screen_width, config.screen_height);
    }

    void render() {
        this->screen->render();

    }
};


// title ui

const vector<string> TITLE_OPTIONS = {
    "start",
    "exit"
};

enum ENUM_TITLE_OPTION_IDX {
    TITLE_IDX_START = 0,
    TITLE_IDX_EXIT = 1
};

class TitleUI: public UI {
    /* 
        界面示例
            > start <
            exit
        
        布局规则：
            每行左右居中
            整体上下居中
            每个选项之间间隔一行

    */
    int title_idx;
    public:
    TitleUI() : UI() {
        update(TITLE_IDX_START);
    }

    void update(int idx) { // 更新screen
        // 清空screen后重新写
        this->title_idx = idx;
        this->screen->clear();

        // 选项总行数
        int total_options_lines = TITLE_OPTIONS.size() * 2 - 1;

        int first_option_line_idx = get_head_idx_slice_inside_slice(total_options_lines, config.screen_height);

        // 绘制选项
        for (int i = 0; i < TITLE_OPTIONS.size(); i++) {
            int option_line_idx = first_option_line_idx + i * 2;
            string text;
            if (i == this->title_idx) {
                text = ">" + TITLE_OPTIONS[i] + "<";
            } else {
                text = TITLE_OPTIONS[i];
            }
            this->screen->write_line_center(option_line_idx, text);
        }

        // 绘制按键提示
        this->screen->write_lines_leftdown({"up,down : select", "enter : confirm"});

    }

    NEXT_ACTION run() {
        /*
            接受up, down, enter
        */

        render();

        while (true) {
            int key = _getch();
            if (key == KEY_CTRL_C) {
                return NEXT_ACTION::EXIT;
            } else if (key == KEY_UP) {
                title_idx = (title_idx - 1 + TITLE_OPTIONS.size()) % TITLE_OPTIONS.size();
                update(title_idx);
                render();
            } else if (key == KEY_DOWN) {
                title_idx = (title_idx + 1) % TITLE_OPTIONS.size();
                update(title_idx);
                render();
            } else if (key == KEY_ENTER) {
                if (this->title_idx == TITLE_IDX_START) { // 开始游戏
                    return NEXT_ACTION::SNAKE_UI;
                } else if (this->title_idx == TITLE_IDX_EXIT) { // 退出游戏
                    return NEXT_ACTION::EXIT;
                }
            } else {
                continue;
            }
        }
    }
};

// snake ui

class SnakeUI: public UI {
    /*
        符号:
            # : 墙
            < > ^ v : 蛇头, 蛇身
            o : 食物

        信号:
            up, down, left, right : 移动
            ctrl + c : 退出程序

        蛇示例:
            <<<
              ^
            >>^

        数据结构 - 蛇
            一维数组，每个元素代表一节(坐标，方向)
            蛇头在数组末尾
            方向指向下一节

        数据结构 - 食物
            哈希集合，每个元素代表一个食物(坐标)

        数据结构 - 空白格集合
            哈希集合，每个元素代表一个空白格(坐标)
        杂：
            初始位置：朝下，棋盘中居中

            从蛇尾开始，每一节指向下一节

            蛇头不允许后退

            撞到蛇身 (即整条蛇前进后，蛇头与蛇身重合) 游戏结束 (会改变蛇头方向但不会前进)

        两种模式：
            运行模式
            游戏结束模式

    */
    struct SnakeNode {
        Point point;
        int direction;
    };

    enum class MODE {
        RUNNING,
        GAME_OVER
    };

    int board_width;
    int board_height;
    int bias_y; // 棋盘相对于屏幕的偏移量
    int bias_x; // 棋盘相对于屏幕的偏移量
    MODE mode;

    std::deque<SnakeNode> snake;
    unordered_set<Point, PointHash> foods;
    unordered_set<Point, PointHash> empty_cells;
    unordered_set<Point, PointHash> snake_cells; // 蛇占据的格

    Point snakePoint_to_screenPoint(Point point) {
        // 每两列空一格
        return {point.y + bias_y, point.x * 2 + bias_x};
    }

    int snakey_to_screeny(int y) {
        return y + bias_y;
    }

    int snakex_to_screenx(int x) {
        return x * 2 + bias_x;
    }

    public:
    SnakeUI() : UI() {
        this->board_width = config.board_width;
        this->board_height = config.board_height;
        this->mode = MODE::RUNNING;
        //- 初始化空白格集合
        for (int y = 0; y < board_height; y++) {
            for (int x = 0; x < board_width; x++) {
                empty_cells.insert({y, x});
            }
        }

        // 初始化蛇
        /// 蛇中心节点
        int middle_node = get_middle_idx(config.snake_init_length);
        /// 棋盘中间行idx
        int middle_line_idx = get_middle_idx(board_height);
        /// 蛇尾行idx
        int head_line_idx = middle_line_idx - middle_node;
        /// 棋盘中心列
        int center_col_idx = get_middle_idx(board_width);
        /// 蛇占据的格
        for (int i = 0; i < config.snake_init_length; i++) {
            snake.push_back({{head_line_idx + i, center_col_idx}, DOWN});
            snake_cells.insert({head_line_idx + i, center_col_idx});
        }
        /// 去掉蛇占据的空白格
        for (const SnakeNode& node : snake) {
            empty_cells.erase(node.point);
        }

        // 初始化食物
        for (int i = 0; i < config.food_init_count; i++) {
            add_random_food();
        }
        //、 去掉食物占据的空白格
        for (const Point& food : foods) {
            empty_cells.erase(food);
        }

        // 计算偏移量
        this->bias_y = get_head_idx_slice_inside_slice(board_height, config.screen_height);
        this->bias_x = get_head_idx_slice_inside_slice(board_width * 2 - 1, config.screen_width);

        update();
    }

    void update() {
        this->screen->clear();

        // 绘制蛇
        for (int i = 0; i < snake.size() - 1; i++) { // 除了头，其他节点
            const SnakeNode& node = snake[i];
            this->screen->write_point(
                snakePoint_to_screenPoint(node.point),
                DIRECTION_SYMBOL_MAP[node.direction]);
        }
        // 绘制蛇头，带颜色
        const SnakeNode& head = snake.back();
        this->screen->write_point(snakePoint_to_screenPoint(head.point), DIRECTION_SYMBOL_MAP[head.direction], "31");

        // 绘制食物
        for (const Point& food : foods) {
            this->screen->write_point(snakePoint_to_screenPoint(food), 'o');
        }

        // 绘制墙
        /// 上面一行
        for (int i = 0; i < board_width; i++) {
            this->screen->write_point(snakePoint_to_screenPoint({-1, i}), '#');
        }
        /// 下面
        for (int i = 0; i < board_width; i++) {
            this->screen->write_point(snakePoint_to_screenPoint({board_height, i}), '#');
        }
        /// 左边
        for (int i = 0; i < board_height; i++) {
            this->screen->write_point(snakePoint_to_screenPoint({i, -1}), '#');
        }
        /// 右边
        for (int i = 0; i < board_height; i++) {
            this->screen->write_point(snakePoint_to_screenPoint({i, board_width}), '#');
        }

        // 绘制信息
        int start_y = snakey_to_screeny(board_height + 2);

        // 绘制长度信息
        this->screen->write_line_center(start_y, "length : " + to_string(snake.size()));

        // 绘制游戏结束 (在最下方的墙的下面空一行)
        if (mode == MODE::GAME_OVER) {
            this->screen->write_line_center(start_y + 1, "game over");
        }

        // 左下角按键提示
        this->screen->write_lines_leftdown({"up,down,left,right : move", "r : restart", "esc : exit"});
    }

    NEXT_ACTION run() {
        render();
        while (true) {
            int key = _getch();
            if (mode == MODE::RUNNING) {
                if (key == KEY_CTRL_C) {
                    return NEXT_ACTION::EXIT;
                } else if (key == KEY_ESC) {
                    return NEXT_ACTION::TITLE_UI;
                } else if (key == 'r') {
                    return NEXT_ACTION::SNAKE_UI;
                } else if (key == KEY_UP) {
                    try_move_forward(UP);
                    update();
                    render();
                } else if (key == KEY_DOWN) {
                    try_move_forward(DOWN);
                    update();
                    render();
                } else if (key == KEY_LEFT) {
                    try_move_forward(LEFT);
                    update();
                    render();
                } else if (key == KEY_RIGHT) {
                    try_move_forward(RIGHT);
                    update();
                    render();
                } else {
                    continue;
                }
            } else if (mode == MODE::GAME_OVER) {
                if (key == KEY_ESC || key == KEY_CTRL_C) {
                    return NEXT_ACTION::EXIT;
                }
            }
        }
    }

    void try_move_forward(int direction) {
        /*
            逻辑
                若和前方是蛇头后方的一节，不做响应
                如果前方没有食物：
                    改变当前蛇头朝向
                    添加新蛇头
                    删除最后一节蛇尾
                如果前方有食物：
                    删除食物
                    改变当前蛇头朝向
                    添加新蛇头
                    添加新食物
        */
        // 蛇头坐标
        SnakeNode& head = snake.back();
        int y = head.point.y;
        int x = head.point.x;

        // 下一格的坐标 = 蛇头坐标朝dir前进一格
        Point new_point = point_move_one_step_mod(head.point, direction, board_height, board_width);
        
        // 蛇头后方一节蛇身的坐标
        SnakeNode& back_node = snake[snake.size() - 2];

        // 如果前方是蛇头后方的一节，不做响应
        if (new_point == back_node.point) { 
            return;
        }

        // 如果撞到蛇身，游戏结束
        if (snake_cells.count(new_point)) {
            // 改变当前蛇头朝向
            head.direction = direction;
            switch_to_game_over();
        // 否则正常前进
        } else {
        

            // 响应
            if (foods.count(new_point)) { // 如果前方有食物
                // 删除食物
                foods.erase(new_point); 
                empty_cells.insert(new_point);
                // 改变当前蛇头朝向
                head.direction = direction;
                // 添加新蛇头
                snake.push_back({new_point, direction});
                snake_cells.insert(new_point);
                empty_cells.erase(new_point);
                // 添加新食物
                add_random_food();
            } else { // 如果前方没有食物
                // 删除最后一节蛇尾
                Point tail_point = snake.front().point;
                snake.pop_front();
                snake_cells.erase(tail_point);
                empty_cells.insert(tail_point);
                // 改变当前蛇头朝向
                head.direction = direction;
                // 添加新蛇头
                snake.push_back({new_point, direction});
                snake_cells.insert(new_point);
                empty_cells.erase(new_point);
            }
        }
    }

    void add_random_food() {
        /*
            在空白格集合中随机选择一个坐标，添加到食物集合中
        */
        Point point = random_pick_from_set(empty_cells);
        empty_cells.erase(point);
        foods.insert(point);
    }

    void switch_to_game_over() {
        // 游戏结束
        this->mode = MODE::GAME_OVER;
    }

};

// main loop

int main() {
    /*逻辑层
        进入标题界面
    */
    NEXT_ACTION next_action = NEXT_ACTION::TITLE_UI;
    while (true) {
        switch (next_action) {
            case NEXT_ACTION::TITLE_UI:
                next_action = TitleUI().run();
                break;
            case NEXT_ACTION::SNAKE_UI:
                next_action = SnakeUI().run();
                break;
            case NEXT_ACTION::EXIT:
                return 0;
        }
    }
    return 0;
}
