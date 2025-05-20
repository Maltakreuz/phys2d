#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <vector>
#include <algorithm>

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 1340;
int BALLS_COUNT = 2000;
int MIN_SIZE = 5;
int MAX_SIZE = 10;
float GRAVITY = 500;
int RESOLVE_STEPS = 64;
float EXPLOSION_STRENGTH = 5;

struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }

    Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }

    Vec2 operator*(float scalar) const {
        return Vec2(x * scalar, y * scalar);
    }

    Vec2& operator+=(const Vec2& other) {
        x += other.x; y += other.y;
        return *this;
    }

    Vec2& operator-=(const Vec2& other) {
        x -= other.x; y -= other.y;
        return *this;
    }

    float dot(const Vec2& other) const {
        return x * other.x + y * other.y;
    }

    float length_squared() const {
        return x * x + y * y;
    }

    float length() const {
        return sqrt(length_squared());
    }
    
    Vec2 operator/(float scalar) const {
    return Vec2(x / scalar, y / scalar);
}

    Vec2 normalized() const {
        float len = length();
        return (len > 0) ? Vec2(x / len, y / len) : Vec2(0, 0);
    }
};

struct Ball {
    Vec2 pos;
    Vec2 prev_pos;
    Vec2 vel;
    SDL_Color color;
    float radius;
    bool colliding = false;
};

struct BallPair {
    int a, b;
    float penetration = 0.0f; // чем больше — тем важнее обрабатывать раньше
};

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

float dt = 0.0f;
Uint32 last_frame_time = 0;
float fps = 0.0f;
int fps_frames = 0;
Uint32 fps_start_time = 0;

void init();
void cleanup();
void draw_circle(int cx, int cy, int radius, int segments);
void draw_border();
void draw_text(const char* text, int x, int y);
void draw_text(const char* text, int x, int y, SDL_Color color);
void update_fps();
void draw();
void draw_texts();
void init_balls(int);
void draw_circle(int cx, int cy, int radius, SDL_Color color);
void draw_ball(const Ball& b);
void update_ball(Ball& ball);
void update();
std::vector<BallPair> broad_phase();
bool test_circle_collision(const Ball& a, const Ball& b);
std::vector<BallPair> detect_collisions();
void resolve_collisions_naive_iterative(const std::vector<BallPair>& pairs, int iterations);
void resolve_collisions_impulse(const std::vector<BallPair>& pairs, int iterations);
void resolve_collisions_impulse_baumgarte(const std::vector<BallPair>& pairs, int iterations);
void resolve_collisions_pbd(const std::vector<BallPair>& pairs, int iterations);
void explode_nearby_balls(Vec2 center, float radius, float strength, std::vector<Ball>& balls);

std::vector<Ball> balls;

int main(int argc, char* argv[]) {
    init();

    int running = 1;
    last_frame_time = SDL_GetTicks();
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            if (event.type == SDL_FINGERDOWN) {
                float fx = event.tfinger.x * SCREEN_WIDTH;
                float fy = event.tfinger.y * SCREEN_HEIGHT;
                Vec2 center = {fx, fy};
                explode_nearby_balls(center, 900.0f, EXPLOSION_STRENGTH, balls);
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                Vec2 center = {(float)mx, (float)my};
                explode_nearby_balls(center, 900.0f, EXPLOSION_STRENGTH, balls);
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false; // или любой твой флаг для выхода из главного цикла
                }
            }
        }

        update_fps();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        update();
        draw();
        draw_texts();

        SDL_RenderPresent(renderer);
        //SDL_Delay(16); // ~60 FPS
    }

    cleanup();
    return 0;
}

void init() {
    SDL_Init(SDL_INIT_VIDEO);
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        exit(1);
    }

    font = TTF_OpenFont("./FreeSans.ttf", 24);
    if (!font) {
        font = TTF_OpenFont("/storage/emulated/0/Download/freesans/FreeSans.ttf", 24);
        if (!font) {
            SDL_Log("Failed to load font: %s", TTF_GetError());
            exit(1);
        }
    }



    window = SDL_CreateWindow("Lionessy 2D",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH, SCREEN_HEIGHT,
                              SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(window, -1, 0);

fps_start_time = SDL_GetTicks();
    fps_frames = 0;
    init_balls(BALLS_COUNT);
}

void cleanup() {
    if (font) TTF_CloseFont(font);
    TTF_Quit();

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    SDL_Quit();
}

void draw_circle(int cx, int cy, int radius, SDL_Color color) {
    int segments = std::max(6, radius);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float angle_step = 2.0f * M_PI / segments;

    for (int i = 0; i < segments; ++i) {
        float a0 = i * angle_step;
        float a1 = (i + 1) * angle_step;

        int x0 = cx + (int)(cosf(a0) * radius);
        int y0 = cy + (int)(sinf(a0) * radius);
        int x1 = cx + (int)(cosf(a1) * radius);
        int y1 = cy + (int)(sinf(a1) * radius);

        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

void draw_ball(const Ball& b) {
    //SDL_Color color = b.colliding ? SDL_Color{255,255,255,255} : SDL_Color{200,200,200,255};
    draw_circle((int)b.pos.x, (int)b.pos.y, b.radius,b.color);
}

void draw_border() {
    int w = SCREEN_WIDTH - 1;
    int h = SCREEN_HEIGHT - 1;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, 0, 0, w - 1, 0);
    SDL_RenderDrawLine(renderer, 0, 0, 0, h - 1);
    SDL_RenderDrawLine(renderer, 0, h, w, h);
    SDL_RenderDrawLine(renderer, w, 0, w, h);
}

void draw_text(const char* text, int x, int y) {
    SDL_Color color = {255, 255, 255, 255};
    draw_text(text, x, y, color);
}

void draw_text(const char* text, int x, int y, SDL_Color color) {
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface)
        return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void update_fps() {
    fps_frames++;
    Uint32 now = SDL_GetTicks();

    if (now - fps_start_time >= 1000) {
        fps = fps_frames * 1000.0f / (now - fps_start_time);
        fps_frames = 0;
        fps_start_time = now;
    }
dt = (now - last_frame_time) / 1000.0f; // в секундах
last_frame_time = now;
}

void update() {
    for (auto& ball : balls)
        update_ball(ball);
        
    auto collision_pairs = detect_collisions();

    // сортируем по убыванию глубины проникновения
    std::sort(collision_pairs.begin(), collision_pairs.end(), [](const BallPair& a, const BallPair& b) { return a.penetration > b.penetration; });

    //resolve_collisions_naive_iterative(collision_pairs, RESOLVE_STEPS);
    //resolve_collisions_impulse_baumgarte(collision_pairs, RESOLVE_STEPS);
    resolve_collisions_pbd(collision_pairs, RESOLVE_STEPS);
    
}

void draw() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    draw_border();
    for (const Ball& ball : balls)
    draw_ball(ball);
}

void draw_texts() {
    int sw, sh;
    SDL_GetRendererOutputSize(renderer, &sw, &sh);

    char buf[64];
    sprintf(buf, "Screen size: %dx%d", sw, sh);
    draw_text(buf, 20, 20);

    char fps_buf[64];
    sprintf(fps_buf, "FPS: %.2f", fps);
    draw_text(fps_buf, 20, 50);
    
    SDL_version compiled;
    SDL_VERSION(&compiled);
    
    SDL_version linked;
    SDL_GetVersion(&linked);
    
    char ver_buf[128];
sprintf(ver_buf, "SDL compiled: %d.%d.%d  linked: %d.%d.%d", 
        compiled.major, compiled.minor, compiled.patch,
        linked.major, linked.minor, linked.patch);
//draw_text(ver_buf, 20, 70);

    char balls_no_buf[64];
    sprintf(balls_no_buf, "Balls count: %d", BALLS_COUNT);
    draw_text(balls_no_buf, 400, 20);
}

void init_ball(int x, int y) {
    Ball b;
    b.pos.x = x;
    b.pos.y = y;
    b.prev_pos = b.pos;
    b.vel.x = ((rand() % 200) - 100) / 50.0f;
    b.vel.y = ((rand() % 200) - 100) / 50.0f;
    b.radius = MIN_SIZE + rand() % MAX_SIZE;

    Uint8 r = static_cast<Uint8>(rand() % 30);               // почти без красного (0–29)
    Uint8 g = static_cast<Uint8>(100 + rand() % 80);         // мягкий зелёный (100–179)
    Uint8 bl = static_cast<Uint8>(160 + rand() % 96);        // доминирующий синий (160–255)

    b.color = { r, g, bl, 255 };

    balls.push_back(b);
}


void init_balls(int count) {
    balls.clear();
    balls.reserve(count);

    int step = MIN_SIZE + MAX_SIZE + 20;
    int max_cols = SCREEN_WIDTH / step;

    for (int i = 0; i < count; ++i) {
        int col = i % max_cols;
        int row = i / max_cols;

        int x = col * step;
        int y = SCREEN_HEIGHT - step * (row + 1);  // снизу вверх, начиная от пола

        init_ball(x, y);
    }
}

void update_ball_by_velocity(Ball& b) {
    b.vel.y += GRAVITY * dt;
    b.pos.x += b.vel.x * dt;
    b.pos.y += b.vel.y * dt;
}

void update_ball_verlet_by_pos(Ball& b) {
    float max_displacement = 5.0f;  // настраиваемое ограничение
    
    Vec2 temp = b.pos;
    Vec2 acceleration = {0.0f, GRAVITY};
    b.pos += (b.pos - b.prev_pos) + acceleration * (dt * dt);

    // Ограничение максимального смещения за кадр (скорости)
    Vec2 velocity = b.pos - b.prev_pos;
    
    float len = velocity.length();
    if (len > max_displacement) {
        velocity = velocity * (max_displacement / len);
        b.pos = b.prev_pos + velocity;
    }

    b.prev_pos = temp;
}


void update_ball_walls_and_floor(Ball& b) {

    float CEILING_OUT_OF_SCREEN = 1080;
    // Границы
    float floor_y = (float)(SCREEN_HEIGHT - b.radius);
    float ceiling_y = -CEILING_OUT_OF_SCREEN + b.radius; // потолок выше экрана
    float left_x = (float)b.radius;
    float right_x = (float)(SCREEN_WIDTH - b.radius);

    // Отскок от пола
    if (b.pos.y > floor_y) {
        b.pos.y = floor_y;
        b.vel.y = -b.vel.y * 0.7f;
        //b.prev_pos = b.pos;
    }

    // Отскок от потолка
    if (b.pos.y < ceiling_y) {
        b.pos.y = ceiling_y;
        b.vel.y = -b.vel.y * 0.7f;
        //b.prev_pos = b.pos;
    }

    // Отскок от стен
    if (b.pos.x < left_x) {
        b.pos.x = left_x;
        b.vel.x = -b.vel.x * 0.7f;
        //b.prev_pos = b.pos;
    } else if (b.pos.x > right_x) {
        b.pos.x = right_x;
        b.vel.x = -b.vel.x * 0.7f;
        //b.prev_pos = b.pos;
    }
    
}


void update_ball(Ball& b) {
    bool use_verlet = true;

    if (!use_verlet) {
        update_ball_by_velocity(b);
    } else {
        update_ball_verlet_by_pos(b);
    }

    update_ball_walls_and_floor(b);
}

std::vector<BallPair> broad_phase() {
    std::vector<BallPair> candidates;

    std::vector<int> indices(balls.size());
    for (int i = 0; i < balls.size(); ++i)
        indices[i] = i;

    std::sort(indices.begin(), indices.end(), [](int i, int j) {
        return balls[i].pos.x - balls[i].radius < balls[j].pos.x - balls[j].radius;
    });

    for (int i = 0; i < indices.size(); ++i) {
        const Ball& a = balls[indices[i]];
        float ax_max = a.pos.x + a.radius;

        for (int j = i + 1; j < indices.size(); ++j) {
            const Ball& b = balls[indices[j]];
            float bx_min = b.pos.x - b.radius;

            if (bx_min > ax_max)
                break;

            candidates.push_back({indices[i], indices[j]});
        }
    }

    return candidates;
}

bool test_circle_collision(const Ball& a, const Ball& b) {
    float dx = a.pos.x - b.pos.x;
    float dy = a.pos.y - b.pos.y;
    float r = a.radius + b.radius;
    return dx * dx + dy * dy < r * r;
}

std::vector<BallPair> detect_collisions() {
    
    for (auto& ball : balls)
        ball.colliding = false;
    
    std::vector<BallPair> result;
    std::vector<BallPair> candidates = broad_phase();

    for (const BallPair& pair : candidates) {
        const Ball& a = balls[pair.a];
        const Ball& b = balls[pair.b];

        if (test_circle_collision(a, b)) {
            float dx = a.pos.x - b.pos.x;
            float dy = a.pos.y - b.pos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            float penetration = (a.radius + b.radius) - dist;
            
            BallPair p = pair;
            p.penetration = penetration;
            result.push_back(p);
            balls[pair.a].colliding = true;
            balls[pair.b].colliding = true;
        }
    }

    return result;
}

void resolve_collisions_naive_iterative(const std::vector<BallPair>& pairs, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        for (const BallPair& pair : pairs) {
            Ball& a = balls[pair.a];
            Ball& b = balls[pair.b];

            SDL_FPoint diff = {b.pos.x - a.pos.x, b.pos.y - a.pos.y};
            float dist_sq = diff.x * diff.x + diff.y * diff.y;
            float min_dist = a.radius + b.radius;

            if (dist_sq == 0.0f) {
                diff = {1.0f, 0.0f};
                dist_sq = 1.0f;
            }

            float dist = sqrtf(dist_sq);
            float overlap = min_dist - dist;

            if (overlap > 0.0f) {
                SDL_FPoint n = {diff.x / dist, diff.y / dist};
                float move = overlap * 0.5f;

                a.pos.x -= n.x * move;
                a.pos.y -= n.y * move;
                b.pos.x += n.x * move;
                b.pos.y += n.y * move;

                float v_rel = (b.vel.x - a.vel.x) * n.x + (b.vel.y - a.vel.y) * n.y;
                if (v_rel < 0.0f) {
                    float impulse = -v_rel;
                    a.vel.x -= n.x * impulse;
                    a.vel.y -= n.y * impulse;
                    b.vel.x += n.x * impulse;
                    b.vel.y += n.y * impulse;
                }
            }
        }
    }
}

void resolve_collisions_impulse(const std::vector<BallPair>& pairs, int iterations) {
    for (int iter = 0; iter < iterations; ++iter) {
        for (const BallPair& pair : pairs) {
            Ball& a = balls[pair.a];
            Ball& b = balls[pair.b];

Vec2 pos_a(a.pos.x, a.pos.y);
            Vec2 pos_b(b.pos.x, b.pos.y);

            Vec2 delta = pos_b - pos_a;
            float dist_sq = delta.length_squared();
            float radius_sum = a.radius + b.radius;

            if (dist_sq == 0.0f) {
                delta = Vec2(1.0f, 0.0f);
                dist_sq = 1.0f;
            }

            float dist = sqrtf(dist_sq);
            float penetration = radius_sum - dist;

            if (penetration > 0.0f) {
                Vec2 normal = delta * (1.0f / dist); // нормаль столкновения

                // Раздвигаем шары поровну
                Vec2 correction = normal * (penetration * 0.5f);
                a.pos.x -= correction.x;
                a.pos.y -= correction.y;
                b.pos.x += correction.x;
                b.pos.y += correction.y;

                // Скорости
                Vec2 vel_a(a.vel.x, a.vel.y);
                Vec2 vel_b(b.vel.x, b.vel.y);

                // Относительная скорость вдоль нормали
                float rel_vel = (vel_b - vel_a).dot(normal);

                if (rel_vel < 0.0f) {
                    float impulse = -rel_vel;

                    Vec2 impulse_vec = normal * impulse;

                    a.vel.x -= impulse_vec.x;
                    a.vel.y -= impulse_vec.y;

                    b.vel.x += impulse_vec.x;
                    b.vel.y += impulse_vec.y;
                }
            }
        }
    }
}

void resolve_collisions_impulse_baumgarte(const std::vector<BallPair>& pairs, int iterations) {
    constexpr float baumgarte_base = 0.2f;  // базовый коэффициент Baumgarte
    constexpr float penetration_slop = 0.05f; // минимальный порог проникновения, ниже которого не исправляем

    for (int i = 0; i < iterations; ++i) {
        // Релаксация: коэффициент с каждой итерацией уменьшается (линейно)
        float baumgarte_coef = baumgarte_base * (1.0f - float(i) / float(iterations));

        for (const BallPair& pair : pairs) {
            Ball& a = balls[pair.a];
            Ball& b = balls[pair.b];

            SDL_FPoint diff = {b.pos.x - a.pos.x, b.pos.y - a.pos.y};
            float dist_sq = diff.x * diff.x + diff.y * diff.y;
            float min_dist = a.radius + b.radius;

            if (dist_sq == 0.0f) {
                diff = {1.0f, 0.0f};
                dist_sq = 1.0f;
            }

            float dist = sqrtf(dist_sq);
            float penetration = min_dist - dist;

            if (penetration > penetration_slop) {
                SDL_FPoint n = {diff.x / dist, diff.y / dist};

                // Baumgarte positional correction (мягко подгоняем позиции)
                float baumgarte_correction = baumgarte_coef * penetration;

                a.pos.x -= n.x * baumgarte_correction * 0.5f;
                a.pos.y -= n.y * baumgarte_correction * 0.5f;
                b.pos.x += n.x * baumgarte_correction * 0.5f;
                b.pos.y += n.y * baumgarte_correction * 0.5f;

                // Импульсная коррекция скорости
                float v_rel = (b.vel.x - a.vel.x) * n.x + (b.vel.y - a.vel.y) * n.y;
                if (v_rel < 0.0f) {
                    float impulse = -v_rel;
                    a.vel.x -= n.x * impulse;
                    a.vel.y -= n.y * impulse;
                    b.vel.x += n.x * impulse;
                    b.vel.y += n.y * impulse;
                }
            }
        }
    }
}

void resolve_collisions_pbd(const std::vector<BallPair>& pairs, int iterations) {
    for (int step = 0; step < iterations; ++step) {
        for (const BallPair& pair : pairs) {
            Ball& a = balls[pair.a];
            Ball& b = balls[pair.b];

            Vec2 delta = {b.pos.x - a.pos.x, b.pos.y - a.pos.y};
            float dist2 = delta.length_squared();
            float r = a.radius + b.radius;

            if (dist2 < r * r && dist2 > 0.0001f) {
                float dist = sqrt(dist2);
                float penetration = r - dist;
                Vec2 correction = delta * (0.5f * penetration / dist); // поровну

                a.pos.x -= correction.x;
                a.pos.y -= correction.y;
                b.pos.x += correction.x;
                b.pos.y += correction.y;

                a.colliding = b.colliding = true;
            }
        }
    }
}

void explode_nearby_balls_velocity_based(Vec2 center, float radius, float strength, std::vector<Ball>& balls) {
    for (auto& ball : balls) {
        Vec2 pos(ball.pos.x, ball.pos.y);
        Vec2 dir = pos - center;

        float dist2 = dir.length_squared();
        if (dist2 < radius * radius && dist2 > 1e-4f) {
            float dist = sqrtf(dist2);
            Vec2 norm_dir = dir / dist;
            float force = strength * (1.0f - dist / radius);

            ball.vel.x += norm_dir.x * force;
            ball.vel.y += norm_dir.y * force;
        }
    }
}

void explode_nearby_balls(Vec2 center, float radius, float strength, std::vector<Ball>& balls) {
    for (auto& ball : balls) {
        Vec2 pos = ball.pos;
        Vec2 dir = pos - center;

        float dist2 = dir.length_squared();
        if (dist2 < radius * radius && dist2 > 1e-4f) {
            float dist = sqrtf(dist2);
            Vec2 norm_dir = dir / dist;
            float force = strength * (1.0f - dist / radius);

            // Напрямую сдвигаем prev_pos в противоположную сторону
            // чтобы при следующем шаге Verlet получился "пинок"
            ball.prev_pos.x -= norm_dir.x * force;
            ball.prev_pos.y -= norm_dir.y * force;
        }
    }
}
