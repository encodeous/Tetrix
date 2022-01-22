#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW
#define MAX_DEVICES 2
#define CLK_PIN   13  // or SCK
#define DATA_PIN  11  // or MOSI
#define CS_PIN    10  // or SS
#define LEFT_PIN 6
#define RIGHT_PIN 4
#define ROTATE_PIN 2
#define DROP_PIN 8

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

enum pixel{
    OFF,
    RED,
    GREEN,
    ORANGE,
    YELLOW
};

struct piece{
    byte offsx[4];
    byte offsy[4];
    int sz;
    piece& withsz(int csz){
        sz = csz;
        return *this;
    }
    piece& set(int i, int y, int x){
        offsx[i] = x;
        offsy[i] = y;
        return *this;
    }
    void copyTo(piece& v){
        memcpy(v.offsx, offsx, sizeof offsx);
        memcpy(v.offsy, offsy, sizeof offsy);
        v.sz = sz;
    }
};

byte buffer[8][8];
bool changed = true;

void set_pixel(int x, int y, pixel color){
    if(buffer[x][y] != color){
        changed = true;
        buffer[x][y] = color;
    }
}

void clear_pixels() {
    memset(buffer, 0, sizeof(buffer));
    changed = true;
}

long int ticks = 0;

void renderPixels(){
    if(!changed) return;
    changed = false;
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            if(buffer[i][j] == ORANGE){
                mx.setPoint(i, j, true);
                mx.setPoint(i, j + 8, true);
                continue;
            }
            mx.setPoint(i, j, false);
            mx.setPoint(i, j + 8, false);
            if(buffer[i][j] == RED){
                mx.setPoint(i, j, true);
            }else if(buffer[i][j] == GREEN){
                mx.setPoint(i, j + 8, true);
            }else if(buffer[i][j] == YELLOW){
                if(ticks % 4 == 0){
                    mx.setPoint(i, j, true);
                }else{
                    mx.setPoint(i, j, false);
                }
                mx.setPoint(i, j + 8, true);
                changed = true;
            }
        }
    }
    clear_pixels();
    mx.update();
}

piece pieces[7];


void make_pieces() {
    // ####
    pieces[0]
            .withsz(4)
            .set(0, 2, 0)
            .set(1, 2, 1)
            .set(2, 2, 2)
            .set(3, 2, 3);

    // #
    // ###
    pieces[1]
            .withsz(3)
            .set(0, 2, 0)
            .set(1, 1, 0)
            .set(2, 1, 1)
            .set(3, 1, 2);

    //   #
    // ###
    pieces[2]
            .withsz(3)
            .set(0, 2, 2)
            .set(1, 1, 0)
            .set(2, 1, 1)
            .set(3, 1, 2);

    // ##
    // ##
    pieces[3]
            .withsz(2)
            .set(0, 0, 0)
            .set(1, 1, 1)
            .set(2, 1, 0)
            .set(3, 0, 1);

    //  ##
    // ##
    pieces[4]
            .withsz(3)
            .set(0, 1, 0)
            .set(1, 1, 1)
            .set(2, 2, 1)
            .set(3, 2, 2);

    //  #
    // ###
    pieces[5]
            .withsz(3)
            .set(0, 1, 0)
            .set(1, 1, 1)
            .set(2, 2, 1)
            .set(3, 1, 2);

    // ##
    //  ##

    pieces[6]
            .withsz(3)
            .set(0, 2, 0)
            .set(1, 2, 1)
            .set(2, 1, 1)
            .set(3, 1, 2);
}

void rotate(piece& rot){
    int sz = rot.sz;
    for(int i = 0; i < 4; i++){
        byte ty = sz - rot.offsx[i] - 1;
        byte tx = rot.offsy[i];
        rot.offsx[i] = tx;
        rot.offsy[i] = ty;
    }
}

bool isStarting = true;
bool hasStarted = false;
int stage = 0;

struct button{
    button(int i) {
        pin = i;
    }
    int pin;
    bool is_down = false;
    unsigned long last_up = millis();
    unsigned long down_time = -1;
    void* callback;
    void on_tick() {
        int sensorVal = digitalRead(pin);
        if (sensorVal == HIGH) {
            is_down = false;
            last_up = millis();
            down_time = -1;
            if(up_func != nullptr) up_func();
        } else {
            if(down_time != -1){
                if(millis() - down_time > 300){
                    if(millis() - last_up < 70) return;
                    last_up = millis();
                    // pushed
                    if(push_func != nullptr) push_func();
                }
            }else{
                if(millis() - down_time < 50) return;
                down_time = millis();
                // pushed
                if(!is_down){
                    if(push_func != nullptr) push_func();
                }
                is_down = true;
            }
        }
    }
    typedef void (*callback_function)();
    callback_function push_func = nullptr;
    callback_function up_func = nullptr;
    void set_pushed(callback_function function){
        push_func = function;
    }
    void set_up(callback_function function){
        up_func = function;
    }
};

button btn_left{LEFT_PIN}, btn_right{RIGHT_PIN}, btn_rotate{ROTATE_PIN}, btn_down{DROP_PIN};

struct board{
    bool filled[8][8];
    int cx = 2, cy = 10;
    int vy = 0;
    int cleared = 0;
    piece cur;

    void render_board(){
        trace();
        for(int i = 0; i < 8; i++){
            for(int j = 0; j < 8; j++){
                if(filled[i][j]){
                    set_pixel(i, j, RED);
                }
            }
        }
        for(int i = 0; i < 4; i++){
            int x = cx + cur.offsx[i];
            int y = vy + cur.offsy[i];
            if(x >= 8 || y >= 8 || x < 0 || y < 0) continue;
            set_pixel(x, y, ORANGE);
        }
        for(int i = 0; i < 4; i++){
            int x = cx + cur.offsx[i];
            int y = cy + cur.offsy[i];
            if(x >= 8 || y >= 8 || x < 0 || y < 0) continue;
            set_pixel(x, y, GREEN);
        }
    }

    void attempt_rotate(){
        for(int i = 0; i < 3; i++){
            for(int j = 0; j <= i; j++){
                rotate(cur);
            }
            if(check_collision()){
                // undo rotation
                for(int k = 0; k < 3 - i; k++){
                    rotate(cur);
                }
            }else return;
        }
    }

    void rand_rot(){
        int k = random(0, 3);
        for(int j = 0; j < k; j++){
            rotate(cur);
        }
    }

    unsigned long last_tick = millis();
    unsigned long last_fast_tick = millis();

    int slow_tick_cnt = 0;
    int fast_tick_cnt = 0;

    bool accelerated = false;

    void tick(){
        int lms = 10000 / (cleared + 10);
        if(accelerated){
            lms *= 0.18;
        }
        if(millis() - last_tick > lms){
            last_tick = millis();
            slow_tick();
        }

        if(millis() - last_fast_tick > 100){
            last_fast_tick = millis();
            fast_tick();
        }

        if(lost){
            if(slow_tick_cnt % 2 == 0){
                render_board();
            }
            return;
        }

        // --- Read Input ---

        btn_down.on_tick();
        btn_rotate.on_tick();
        btn_right.on_tick();
        btn_left.on_tick();

        // --- Render Game ---
        render_board();
    }

    int gcd(int a, int b){
        if(a == 0) return b;
        return gcd(b % a, a);
    }

    void fast_tick(){
        fast_tick_cnt++;
    }

    void slow_tick(){
        if(lost){
            slow_tick_cnt++;
            return;
        }
        cy--;
        if(check_collision()){
            cy++;
            stick();
        }
    }

    bool check_collision(){
        for(int i = 0; i < 4; i++){
            int x = cx + cur.offsx[i];
            int y = cy + cur.offsy[i];
            if(x >= 8 || x < 0 || y < 0) return true;
            if(y < 8 && filled[x][y]) return true;
        }
        return false;
    }

    bool check_virtual_collision(){
        for(int i = 0; i < 4; i++){
            int x = cx + cur.offsx[i];
            int y = vy + cur.offsy[i];
            if(x >= 8 || x < 0 || y < 0) return true;
            if(y < 8 && filled[x][y]) return true;
        }
        return false;
    }

    void new_piece(){
        pieces[random(0, 6)].copyTo(cur);
        rand_rot();
    }

    void stick(){
        for(int i = 0; i < 4; i++){
            int y = cy + cur.offsy[i];
            if(y >= 8){
                end_game();
                return;
            }
        }
        for(int i = 0; i < 4; i++){
            int x = cx + cur.offsx[i];
            int y = cy + cur.offsy[i];
            filled[x][y] = true;
        }
        cy = 8;
        cx = 2;
        check_lines();
        new_piece();
    }

    bool lost = false;

    void end_game(){
        lost = true;
        accelerated = false;
    }

    void row_cpy(int r, int d){
        for(int i = 0; i < 8; i++){
            filled[i][d] = filled[i][r];
        }
    }

    void fill_row(int r){
        for(int i = 0; i < 8; i++){
            filled[i][r] = false;
        }
    }

    void check_lines(){
        int lidx = 0;
        for(int i = 0; i < 8; i++){
            bool all_clear = true;
            for(int j = 0; j < 8; j++){
                if(!filled[j][i]){
                    all_clear = false;
                }
            }
            row_cpy(i, lidx);
            if(!all_clear){
                lidx++;
            }else cleared++;
        }
        for(int i = lidx + 1; i < 8; i++){
            fill_row(i);
        }
    }

    void trace(){
        vy = cy;
        while(true){
            vy--;
            if(check_virtual_collision()){
                vy++;
                return;
            }
        }
    }

    void left(){
        cx--;
        if(check_collision()) cx++;
    }

    void right(){
        cx++;
        if(check_collision()) cx--;
    }
}game;

void btn_down_press(){
    game.accelerated = true;
}
void btn_down_release(){
    game.accelerated = false;
}
void lft(){
    game.left();
}
void rit(){
    game.right();
}
void rot(){
    game.attempt_rotate();
}
void setup() {
    Serial.begin(9600);
    pinMode(LEFT_PIN, INPUT_PULLUP);
    pinMode(RIGHT_PIN, INPUT_PULLUP);
    pinMode(ROTATE_PIN, INPUT_PULLUP);
    pinMode(DROP_PIN, INPUT_PULLUP);
    btn_down.set_pushed(btn_down_press);
    btn_down.set_up(btn_down_release);
    btn_left.set_pushed(lft);
    btn_right.set_pushed(rit);
    btn_rotate.set_pushed(rot);
    mx.begin();
    mx.control(MD_MAX72XX::INTENSITY, 1);
    mx.control(MD_MAX72XX::UPDATE, 0);
    make_pieces();
    randomSeed(analogRead(0));
    game.new_piece();
}

void loop() {
    ticks++;
    if(isStarting || stage != 0){
        if(stage < 8 && isStarting){
            for(int i = 0; i <= stage; i++){
                for(int j = 0; j <= stage; j++){
                    buffer[i][j] ^= RED;
                    buffer[7-i][7-j] ^= GREEN;
                }
            }
            stage++;
            delay(100);
        }else{
            stage--;
            for(int i = 0; i <= stage; i++){
                for(int j = 0; j <= stage; j++){
                    buffer[i][j] ^= RED;
                    buffer[7-i][7-j] ^= GREEN;
                }
            }
            isStarting = false;
            delay(100);
        }
        changed = true;
    }else{
        hasStarted = true;
    }
    if(hasStarted){
        game.tick();
    }
    // --- Output Result ---
    renderPixels();
}