#include <windows.h>
#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <algorithm>

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

struct Vec2 {
    double x, y;
    Vec2(double x=0, double y=0) : x(x), y(y) {}
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator*(double s) const { return Vec2(x*s, y*s); }
    Vec2 operator/(double s) const { return s != 0 ? Vec2(x/s, y/s) : Vec2(); }
    Vec2 operator-() const { return Vec2(-x, -y); }
    double dot(const Vec2& other) const { return x*other.x + y*other.y; }
    double cross(const Vec2& other) const { return x*other.y - y*other.x; }
    double length() const { return std::sqrt(x*x + y*y); }
    Vec2 normalized() const { double len = length(); return len > 0 ? *this * (1/len) : Vec2(); }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
};

struct Contact {
    Vec2 force;
    Vec2 point;
    std::string label;
};

struct Box {
    int id;
    double mass;
    double width, height;
    double x, y;
    double vx, vy;
    std::string label;
    std::vector<Contact> debugContacts;
};

std::vector<Box> boxes;
double gravity = 9.81;
double floorY = 550.0;
double restitution = 0.6;
double mu_dynamic = 0.3;
double mu_static = 0.3;
double friction_threshold = 0.1;
double spring_k = 5000.0;
double damper_d = 0.0; // Will be set based on restitution
bool paused = true;
bool addBoxModeEnabled = false;
int addingBox = 0;
double tempX = 0, tempY = 0, tempW = 50, tempH = 50;
int nextId = 1;
bool showVectors = false;

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 600;
const int BUTTON_HEIGHT = 50;

HWND hwndPlayPause, hwndAdd, hwndClear, hwndDebug;
HWND hwndGravityLabel, hwndGravityEdit, hwndFrictionLabel, hwndFrictionEdit, hwndElasticityLabel, hwndElasticityEdit, hwndApply;

void addBox(double x, double y, double w, double h, double mass) {
    Box b{};
    b.id = nextId;
    int idx = nextId - 1;
    char letter = 'A' + (idx % 26);
    std::string suffix = (idx / 26 > 0) ? std::to_string(idx / 26) : "";
    b.label = std::string(1, letter) + suffix;
    b.x = x;
    b.y = y;
    b.width = w;
    b.height = h;
    b.mass = mass;
    b.vx = 0;
    b.vy = 0;
    boxes.push_back(b);
    nextId++;
}

void simulate(double dt) {
    if (dt == 0) return;

    for (auto &b : boxes) {
        b.debugContacts.clear();
    }

    std::vector<Vec2> forces(boxes.size(), Vec2(0, 0));

    // Gravity
    for (size_t i = 0; i < boxes.size(); ++i) {
        Box &b = boxes[i];
        forces[i].y += b.mass * gravity;
        b.debugContacts.push_back({Vec2(0, b.mass * gravity), Vec2(b.x, b.y), "Gravity"});
    }

    // Floor and walls
    for (size_t i = 0; i < boxes.size(); ++i) {
        Box &b = boxes[i];
        // Floor
        double bottom = b.y + b.height / 2.0;
        if (bottom > floorY) {
            double depth = bottom - floorY;
            double vrn = b.vy;
            double n_mag = spring_k * depth + damper_d * vrn;
            if (n_mag < 0) n_mag = 0;
            forces[i].y -= n_mag;
            Vec2 contact(b.x, floorY);
            b.debugContacts.push_back({Vec2(0, -n_mag), contact, "Normal from Floor"});

            // Friction
            if (n_mag > 0) {
                double mu = (std::abs(b.vx) < friction_threshold) ? mu_static : mu_dynamic;
                double f_max = mu * n_mag;
                double accel_x = forces[i].x / b.mass; // tangential accel from other forces
                double f_needed = -b.vx / dt - accel_x; // to keep vx=0 next step
                double f_fric = 0;
                if (std::abs(b.vx) < friction_threshold && std::abs(f_needed) < f_max) {
                    f_fric = f_needed * b.mass;
                    b.debugContacts.push_back({Vec2(f_fric, 0), contact, "Static Friction"});
                } else {
                    f_fric = - (b.vx > 0 ? 1 : (b.vx < 0 ? -1 : 0)) * mu_dynamic * n_mag;
                    b.debugContacts.push_back({Vec2(f_fric, 0), contact, "Kinetic Friction"});
                }
                forces[i].x += f_fric;
            }
        }
        // Left wall
        double left = b.x - b.width / 2.0;
        if (left < 0) {
            double depth = -left;
            double vrn = -b.vx;
            double n_mag = spring_k * depth + damper_d * vrn;
            if (n_mag < 0) n_mag = 0;
            forces[i].x += n_mag;
            Vec2 contact(0, b.y);
            b.debugContacts.push_back({Vec2(n_mag, 0), contact, "Normal from Left Wall"});
        }
        // Right wall
        double right = b.x + b.width / 2.0;
        if (right > WINDOW_WIDTH) {
            double depth = right - WINDOW_WIDTH;
            double vrn = b.vx;
            double n_mag = spring_k * depth + damper_d * vrn;
            if (n_mag < 0) n_mag = 0;
            forces[i].x -= n_mag;
            Vec2 contact(WINDOW_WIDTH, b.y);
            b.debugContacts.push_back({Vec2(-n_mag, 0), contact, "Normal from Right Wall"});
        }
    }

    // Box-to-box contacts
    for (size_t i = 0; i < boxes.size(); ++i) {
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            Box &b1 = boxes[i];
            Box &b2 = boxes[j];

            double left1 = b1.x - b1.width / 2.0;
            double right1 = b1.x + b1.width / 2.0;
            double top1 = b1.y - b1.height / 2.0;
            double bottom1 = b1.y + b1.height / 2.0;

            double left2 = b2.x - b2.width / 2.0;
            double right2 = b2.x + b2.width / 2.0;
            double top2 = b2.y - b2.height / 2.0;
            double bottom2 = b2.y + b2.height / 2.0;

            if (right1 < left2 || left1 > right2 || bottom1 < top2 || top1 > bottom2) continue;

            double overlap_x = std::min(right1 - left2, right2 - left1);
            double overlap_y = std::min(bottom1 - top2, bottom2 - top1);

            Vec2 normal(0, 0);
            double depth = 0;
            if (overlap_x < overlap_y) {
                depth = overlap_x;
                if (b1.x < b2.x) {
                    normal = Vec2(-1, 0);
                } else {
                    normal = Vec2(1, 0);
                }
            } else {
                depth = overlap_y;
                if (b1.y < b2.y) {
                    normal = Vec2(0, -1);
                } else {
                    normal = Vec2(0, 1);
                }
            }

            Vec2 rel_v = Vec2(b1.vx - b2.vx, b1.vy - b2.vy);
            double vrn = rel_v.dot(normal);
            double n_mag = spring_k * depth - damper_d * vrn;
            if (n_mag < 0) n_mag = 0;

            Vec2 contact((b1.x + b2.x) / 2.0, (b1.y + b2.y) / 2.0);

            forces[i] += normal * n_mag;
            forces[j] += normal * -n_mag;
            b1.debugContacts.push_back({normal * n_mag, contact, "Normal from " + b2.label});
            b2.debugContacts.push_back({normal * -n_mag, contact, "Normal from " + b1.label});

            // Friction
            if (n_mag > 0) {
                Vec2 tangent(-normal.y, normal.x);
                double vrt = rel_v.dot(tangent);
                double mu = (std::abs(vrt) < friction_threshold) ? mu_static : mu_dynamic;
                double f_max = mu * n_mag;
                double f_t = -vrt * (damper_d / 10.0); // Approximate tangential damping
                if (std::abs(f_t) > f_max) f_t = f_max * (f_t < 0 ? -1 : 1);
                Vec2 fric = tangent * f_t;
                forces[i] += fric;
                forces[j] += -fric;
                b1.debugContacts.push_back({fric, contact, "Friction from " + b2.label});
                b2.debugContacts.push_back({-fric, contact, "Friction from " + b1.label});
            }
        }
    }

    // Integrate
    for (size_t i = 0; i < boxes.size(); ++i) {
        Box &b = boxes[i];
        Vec2 a = forces[i] / b.mass;
        b.vx += a.x * dt;
        b.vy += a.y * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
    }
}

class Window {
public:
    HWND hwnd;
    HDC hdc, hdcMem;
    HBITMAP hBitmap;
    int width, height;
    LARGE_INTEGER perfCounterFrequency, lastTime;

    Window(int w, int h, const char* title) : width(w), height(h) {
        QueryPerformanceFrequency(&perfCounterFrequency);
        QueryPerformanceCounter(&lastTime);

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.lpszClassName = "BoxSimWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);

        hwnd = CreateWindowEx(0, "BoxSimWindow", title,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             w + 16, h + 39, NULL, NULL, NULL, this);

        hdc = GetDC(hwnd);
        hdcMem = CreateCompatibleDC(hdc);
        hBitmap = CreateCompatibleBitmap(hdc, w, h);
        SelectObject(hdcMem, hBitmap);

        hwndPlayPause = CreateWindow("BUTTON", "RESUME", WS_CHILD | WS_VISIBLE,
                                     10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        hwndAdd = CreateWindow("BUTTON", "ADD BOX MODE", WS_CHILD | WS_VISIBLE,
                              120, 10, 120, 30, hwnd, (HMENU)2, NULL, NULL);
        hwndClear = CreateWindow("BUTTON", "CLEAR", WS_CHILD | WS_VISIBLE,
                                250, 10, 100, 30, hwnd, (HMENU)3, NULL, NULL);
        hwndDebug = CreateWindow("BUTTON", "SHOW VECTORS", WS_CHILD | WS_VISIBLE,
                                360, 10, 120, 30, hwnd, (HMENU)4, NULL, NULL);

        hwndGravityLabel = CreateWindow("STATIC", "Gravity:", WS_CHILD | WS_VISIBLE,
                                        490, 10, 60, 30, hwnd, NULL, NULL, NULL);
        hwndGravityEdit = CreateWindow("EDIT", "9.81", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                       550, 10, 50, 30, hwnd, (HMENU)6, NULL, NULL);
        hwndFrictionLabel = CreateWindow("STATIC", "Friction:", WS_CHILD | WS_VISIBLE,
                                         610, 10, 60, 30, hwnd, NULL, NULL, NULL);
        hwndFrictionEdit = CreateWindow("EDIT", "0.3", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                        670, 10, 50, 30, hwnd, (HMENU)7, NULL, NULL);
        hwndElasticityLabel = CreateWindow("STATIC", "Elasticity:", WS_CHILD | WS_VISIBLE,
                                         730, 10, 70, 30, hwnd, NULL, NULL, NULL);
        hwndElasticityEdit = CreateWindow("EDIT", "0.6", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                        800, 10, 50, 30, hwnd, (HMENU)8, NULL, NULL);
        hwndApply = CreateWindow("BUTTON", "Apply", WS_CHILD | WS_VISIBLE,
                                 860, 10, 60, 30, hwnd, (HMENU)5, NULL, NULL);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    }

    ~Window() {
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdc);
    }

    double getDeltaTime() {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        double dt = (double)(currentTime.QuadPart - lastTime.QuadPart) / perfCounterFrequency.QuadPart;
        lastTime = currentTime;
        return dt > 0.05 ? 0.05 : dt;
    }

    void render() {
        RECT rect = {0, 0, width, height};
        FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

        RECT floorRect = {0, (int)floorY, width, height};
        FillRect(hdcMem, &floorRect, (HBRUSH)GetStockObject(GRAY_BRUSH));

        for (auto &b : boxes) {
            int left = (int)(b.x - b.width / 2.0);
            int top = (int)(b.y - b.height / 2.0);
            int right = (int)(b.x + b.width / 2.0);
            int bottom = (int)(b.y + b.height / 2.0);
            HBRUSH brush = CreateSolidBrush(RGB(0, 200, 255));
            RECT boxRect = {left, top, right, bottom};
            FillRect(hdcMem, &boxRect, brush);
            FrameRect(hdcMem, &boxRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            DeleteObject(brush);

            TextOut(hdcMem, (int)b.x - 5, top - 15, b.label.c_str(), b.label.length());
        }

        if (addingBox == 1) {
            int x1 = (int)std::min(tempX, tempX + tempW);
            int y1 = (int)std::min(tempY, tempY + tempH);
            int x2 = (int)std::max(tempX, tempX + tempW);
            int y2 = (int)std::max(tempY, tempY + tempH);
            
            HBRUSH brush = CreateSolidBrush(RGB(100, 200, 100));
            RECT boxRect = {x1, y1, x2, y2};
            FillRect(hdcMem, &boxRect, brush);
            DeleteObject(brush);
        }

        if (showVectors) {
            for (auto &b : boxes) {
                HBRUSH comBrush = CreateSolidBrush(RGB(0, 0, 0));
                SelectObject(hdcMem, comBrush);
                Ellipse(hdcMem, (int)b.x - 3, (int)b.y - 3, (int)b.x + 3, (int)b.y + 3);
                DeleteObject(comBrush);
                TextOut(hdcMem, (int)b.x + 5, (int)b.y - 10, "COM", 3);

                double vel_len = std::sqrt(b.vx * b.vx + b.vy * b.vy);
                if (vel_len > 0.1) {
                    double arrow_len = std::min(50.0, vel_len * 2.0);
                    double scale = arrow_len / vel_len;
                    double end_x = b.x + b.vx * scale;
                    double end_y = b.y + b.vy * scale;
                    HPEN velPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                    SelectObject(hdcMem, velPen);
                    MoveToEx(hdcMem, (int)b.x, (int)b.y, NULL);
                    LineTo(hdcMem, (int)end_x, (int)end_y);
                    TextOut(hdcMem, (int)end_x, (int)end_y, "Velocity", 8);
                    DeleteObject(velPen);
                }

                double g_len = 20.0;
                double end_x = b.x;
                double end_y = b.y + g_len;
                HPEN gravPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
                SelectObject(hdcMem, gravPen);
                MoveToEx(hdcMem, (int)b.x, (int)b.y, NULL);
                LineTo(hdcMem, (int)end_x, (int)end_y);
                TextOut(hdcMem, (int)end_x, (int)end_y, "Gravity", 7);
                DeleteObject(gravPen);

                for (auto& c : b.debugContacts) {
                    double f_len = c.force.length();
                    if (f_len > 0.1) {
                        Vec2 dir = c.force.normalized();
                        double draw_len = std::min(50.0, f_len / 100.0); // Scale down since forces can be large
                        Vec2 end = c.point + dir * draw_len;
                        HPEN contactPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
                        SelectObject(hdcMem, contactPen);
                        MoveToEx(hdcMem, (int)c.point.x, (int)c.point.y, NULL);
                        LineTo(hdcMem, (int)end.x, (int)end.y);
                        TextOut(hdcMem, (int)end.x, (int)end.y, c.label.c_str(), c.label.length());
                        DeleteObject(contactPen);
                    }
                }
            }
        }

        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(0, 0, 0));
        if (addBoxModeEnabled) {
            TextOut(hdcMem, 930, 20, "BOX MODE: Click & drag to add | Space: pause | C: clear", 54);
        } else {
            TextOut(hdcMem, 930, 20, "Click ADD BOX MODE to add boxes | Space: pause | C: clear", 57);
        }

        BitBlt(hdc, 0, BUTTON_HEIGHT, width, height - BUTTON_HEIGHT,
               hdcMem, 0, BUTTON_HEIGHT, SRCCOPY);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Window* pThis = nullptr;
        if (msg == WM_CREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (!pThis) return DefWindowProc(hwnd, msg, wParam, lParam);

        switch (msg) {
            case WM_CLOSE:
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            
            case WM_COMMAND:
                if (LOWORD(wParam) == 1) {
                    paused = !paused;
                    SetWindowText(hwndPlayPause, paused ? "RESUME" : "PAUSE");
                } else if (LOWORD(wParam) == 2) {
                    addBoxModeEnabled = !addBoxModeEnabled;
                    SetWindowText(hwndAdd, addBoxModeEnabled ? "EXIT BOX MODE" : "ADD BOX MODE");
                } else if (LOWORD(wParam) == 3) {
                    boxes.clear();
                } else if (LOWORD(wParam) == 4) {
                    showVectors = !showVectors;
                    SetWindowText(hwndDebug, showVectors ? "HIDE VECTORS" : "SHOW VECTORS");
                } else if (LOWORD(wParam) == 5) {
                    char buf[32];
                    GetWindowText(hwndGravityEdit, buf, 32);
                    gravity = atof(buf);
                    GetWindowText(hwndFrictionEdit, buf, 32);
                    mu_static = mu_dynamic = atof(buf);
                    GetWindowText(hwndElasticityEdit, buf, 32);
                    restitution = atof(buf);
                    damper_d = 300.0 * (1.0 - restitution);
                }
                return 0;

            case WM_LBUTTONDOWN: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                if (addBoxModeEnabled && my > BUTTON_HEIGHT) {
                    tempX = mx;
                    tempY = my;
                    tempW = 0;
                    tempH = 0;
                    addingBox = 1;
                }
                return 0;
            }

            case WM_LBUTTONUP:
                if (addingBox == 1 && tempW > 10 && tempH > 10) {
                    addBox(tempX + tempW * 0.5, tempY + tempH * 0.5, tempW, tempH, 1.0);
                }
                addingBox = 0;
                return 0;

            case WM_MOUSEMOVE: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                if (addingBox == 1) {
                    tempW = std::abs(mx - (int)tempX);
                    tempH = std::abs(my - (int)tempY);
                    if (tempW < 1) tempW = 1;
                    if (tempH < 1) tempH = 1;
                }
                return 0;
            }

            case WM_KEYDOWN:
                if (wParam == VK_SPACE) {
                    paused = !paused;
                    SetWindowText(hwndPlayPause, paused ? "RESUME" : "PAUSE");
                } else if (wParam == 'C') {
                    boxes.clear();
                }
                return 0;

            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Window window(WINDOW_WIDTH, WINDOW_HEIGHT, "Free Body Diagram Simulator");

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            double dt = window.getDeltaTime();
            if (!paused) {
                // Substeps for stability
                int substeps = 4;
                double sub_dt = dt / substeps;
                for (int s = 0; s < substeps; ++s) {
                    simulate(sub_dt);
                }
            }
            window.render();
            Sleep(16);
        }
    }

    return 0;
}