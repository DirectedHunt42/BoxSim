#include <windows.h>
#include <vector>
#include <cmath>
#include <sstream>

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

struct Vec2 {
    double x, y;
    Vec2(double x=0, double y=0) : x(x), y(y) {}
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator*(double s) const { return Vec2(x*s, y*s); }
    Vec2 operator-() const { return Vec2(-x, -y); }
    double dot(const Vec2& other) const { return x*other.x + y*other.y; }
    double cross(const Vec2& other) const { return x*other.y - y*other.x; }
    double length() const { return std::sqrt(x*x + y*y); }
    Vec2 normalized() const { double len = length(); return len > 0 ? *this * (1/len) : Vec2(); }
};

struct Box {
    int id;
    double mass;
    double width, height;
    double x, y;
    double vx, vy;
    double angle;
    double angularVelocity;
    double inertia;
};

std::vector<Box> boxes;
double gravity = 9.81;
double floorY = 550.0;
double restitution = 0.6;
double friction = 2.0;
bool paused = true;
bool addBoxModeEnabled = false;
int addingBox = 0;
double tempX = 0, tempY = 0, tempW = 50, tempH = 50;
int nextId = 1;
bool showDebug = false;

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 600;
const int BUTTON_HEIGHT = 50;

HWND hwndPlayPause, hwndAdd, hwndClear, hwndDebug;

void addBox(double x, double y, double w, double h, double mass) {
    Box b{};
    b.id = nextId++;
    b.x = x;
    b.y = y;
    b.width = w;
    b.height = h;
    b.mass = mass;
    b.vx = 0;
    b.vy = 0;
    b.angle = 0;
    b.angularVelocity = 0;
    b.inertia = b.mass * (b.width * b.width + b.height * b.height) / 12.0;
    boxes.push_back(b);
}

bool checkCollision(const Box& b1, const Box& b2, Vec2& normal, double& depth, Vec2& contactPoint) {
    // Get corners
    Vec2 corners1[4], corners2[4];
    double cos1 = std::cos(b1.angle), sin1 = std::sin(b1.angle);
    double hw1 = b1.width * 0.5, hh1 = b1.height * 0.5;
    double dxs[4] = {-hw1, hw1, hw1, -hw1};
    double dys[4] = {-hh1, -hh1, hh1, hh1};
    for (int i = 0; i < 4; ++i) {
        double rx = dxs[i] * cos1 - dys[i] * sin1;
        double ry = dxs[i] * sin1 + dys[i] * cos1;
        corners1[i] = Vec2(b1.x + rx, b1.y + ry);
    }
    double cos2 = std::cos(b2.angle), sin2 = std::sin(b2.angle);
    double hw2 = b2.width * 0.5, hh2 = b2.height * 0.5;
    dxs[0] = -hw2; dxs[1] = hw2; dxs[2] = hw2; dxs[3] = -hw2;
    dys[0] = -hh2; dys[1] = -hh2; dys[2] = hh2; dys[3] = hh2;
    for (int i = 0; i < 4; ++i) {
        double rx = dxs[i] * cos2 - dys[i] * sin2;
        double ry = dxs[i] * sin2 + dys[i] * cos2;
        corners2[i] = Vec2(b2.x + rx, b2.y + ry);
    }

    // Axes (normals)
    Vec2 axes[4];
    axes[0] = Vec2(-sin1, cos1);
    axes[1] = Vec2(cos1, sin1);
    axes[2] = Vec2(-sin2, cos2);
    axes[3] = Vec2(cos2, sin2);

    double minOverlap = 1e9;
    Vec2 minAxis;
    for (int i = 0; i < 4; ++i) {
        Vec2 axis = axes[i].normalized(); // Already unit, but ensure
        double min1 = 1e9, max1 = -1e9;
        for (auto& c : corners1) {
            double proj = c.dot(axis);
            min1 = std::min(min1, proj);
            max1 = std::max(max1, proj);
        }
        double min2 = 1e9, max2 = -1e9;
        for (auto& c : corners2) {
            double proj = c.dot(axis);
            min2 = std::min(min2, proj);
            max2 = std::max(max2, proj);
        }
        if (max1 < min2 || max2 < min1) return false;
        double overlap = std::min(max1, max2) - std::max(min1, min2);
        if (overlap < minOverlap) {
            minOverlap = overlap;
            minAxis = axis;
        }
    }

    // Determine normal direction
    Vec2 centerDiff = Vec2(b2.x - b1.x, b2.y - b1.y);
    if (minAxis.dot(centerDiff) < 0) minAxis = -minAxis;
    normal = minAxis;
    depth = minOverlap;

    // Approximate contact point using support points
    Vec2 support1;
    double maxProj1 = -1e9;
    for (auto& c : corners1) {
        double proj = c.dot(-normal);
        if (proj > maxProj1) {
            maxProj1 = proj;
            support1 = c;
        }
    }
    Vec2 support2;
    double maxProj2 = -1e9;
    for (auto& c : corners2) {
        double proj = c.dot(normal);
        if (proj > maxProj2) {
            maxProj2 = proj;
            support2 = c;
        }
    }
    contactPoint = (support1 + support2) * 0.5;
    return true;
}

void simulate(double dt) {
    for (auto &b : boxes) {
        b.vy += gravity * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.angle += b.angularVelocity * dt;
    }

    // Handle wall and floor collisions for each box
    for (auto &b : boxes) {
        // Compute corners
        Vec2 corners[4];
        double cos_a = std::cos(b.angle), sin_a = std::sin(b.angle);
        double hw = b.width * 0.5, hh = b.height * 0.5;
        double dxs[4] = {-hw, hw, hw, -hw};
        double dys[4] = {-hh, -hh, hh, hh};
        for (int i = 0; i < 4; ++i) {
            double rx = dxs[i] * cos_a - dys[i] * sin_a;
            double ry = dxs[i] * sin_a + dys[i] * cos_a;
            corners[i] = Vec2(b.x + rx, b.y + ry);
        }

        // Floor
        bool floorCollided = false;
        double maxY = -1e9;
        Vec2 deepest;
        for (auto& c : corners) {
            if (c.y > maxY) {
                maxY = c.y;
                deepest = c;
            }
        }
        if (maxY > floorY) {
            floorCollided = true;
            double depth = maxY - floorY;
            Vec2 normal(0, -1);
            Vec2 r = deepest - Vec2(b.x, b.y);
            // Position correction
            Vec2 sep = normal * depth;
            b.x += sep.x;
            b.y += sep.y;
            // Velocity impulse
            Vec2 perp_r(-r.y, r.x);
            Vec2 v_at = Vec2(b.vx, b.vy) + perp_r * b.angularVelocity;
            double vrn = v_at.dot(normal);
            if (vrn < 0) {
                double cross_rn = r.cross(normal);
                double denom = 1.0 / b.mass + (cross_rn * cross_rn) / b.inertia;
                if (denom > 0) {
                    double j = - (1 + restitution) * vrn / denom;
                    Vec2 impulse = normal * j;
                    b.vx += impulse.x / b.mass;
                    b.vy += impulse.y / b.mass;
                    b.angularVelocity += r.cross(impulse) / b.inertia;
                }
            }
        }

        // Left wall
        double minX = 1e9;
        Vec2 deepest_left;
        for (auto& c : corners) {
            if (c.x < minX) {
                minX = c.x;
                deepest_left = c;
            }
        }
        if (minX < 0) {
            double depth = 0 - minX;
            Vec2 normal(1, 0);
            Vec2 r = deepest_left - Vec2(b.x, b.y);
            Vec2 sep = normal * depth;
            b.x += sep.x;
            b.y += sep.y;
            Vec2 perp_r(-r.y, r.x);
            Vec2 v_at = Vec2(b.vx, b.vy) + perp_r * b.angularVelocity;
            double vrn = v_at.dot(normal);
            if (vrn < 0) {
                double cross_rn = r.cross(normal);
                double denom = 1.0 / b.mass + (cross_rn * cross_rn) / b.inertia;
                if (denom > 0) {
                    double j = - (1 + restitution) * vrn / denom;
                    Vec2 impulse = normal * j;
                    b.vx += impulse.x / b.mass;
                    b.vy += impulse.y / b.mass;
                    b.angularVelocity += r.cross(impulse) / b.inertia;
                }
            }
        }

        // Right wall
        double maxX = -1e9;
        Vec2 deepest_right;
        for (auto& c : corners) {
            if (c.x > maxX) {
                maxX = c.x;
                deepest_right = c;
            }
        }
        if (maxX > WINDOW_WIDTH) {
            double depth = maxX - WINDOW_WIDTH;
            Vec2 normal(-1, 0);
            Vec2 r = deepest_right - Vec2(b.x, b.y);
            Vec2 sep = normal * depth;
            b.x += sep.x;
            b.y += sep.y;
            Vec2 perp_r(-r.y, r.x);
            Vec2 v_at = Vec2(b.vx, b.vy) + perp_r * b.angularVelocity;
            double vrn = v_at.dot(normal);
            if (vrn < 0) {
                double cross_rn = r.cross(normal);
                double denom = 1.0 / b.mass + (cross_rn * cross_rn) / b.inertia;
                if (denom > 0) {
                    double j = - (1 + restitution) * vrn / denom;
                    Vec2 impulse = normal * j;
                    b.vx += impulse.x / b.mass;
                    b.vy += impulse.y / b.mass;
                    b.angularVelocity += r.cross(impulse) / b.inertia;
                }
            }
        }

        // Apply friction if on floor
        if (floorCollided) {
            if (std::abs(b.vy) > 0.1) {
                // Already handled bounce in impulse
            } else {
                b.vy = 0;
            }
            double frictionFactor = 1.0 - std::min(1.0, friction * dt);
            if (std::abs(b.vx) > 0.1) {
                b.angularVelocity = -b.vx * 0.05;
            }
            b.vx *= frictionFactor;
            b.angularVelocity *= 0.98;
        }
    }

    // Box-to-box collisions
    for (size_t i = 0; i < boxes.size(); ++i) {
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            Box &b1 = boxes[i];
            Box &b2 = boxes[j];
            Vec2 normal;
            double depth;
            Vec2 contact;
            if (checkCollision(b1, b2, normal, depth, contact)) {
                // Position correction
                double totalMass = b1.mass + b2.mass;
                double frac1 = (totalMass > 0) ? b2.mass / totalMass : 0.5;
                double frac2 = (totalMass > 0) ? b1.mass / totalMass : 0.5;
                Vec2 sep = normal * (depth + 0.1);
                b1.x -= sep.x * frac1;
                b1.y -= sep.y * frac1;
                b2.x += sep.x * frac2;
                b2.y += sep.y * frac2;

                // Impulse
                Vec2 r1 = contact - Vec2(b1.x, b1.y);
                Vec2 r2 = contact - Vec2(b2.x, b2.y);
                Vec2 perp_r1(-r1.y, r1.x);
                Vec2 perp_r2(-r2.y, r2.x);
                Vec2 v1_at = Vec2(b1.vx, b1.vy) + perp_r1 * b1.angularVelocity;
                Vec2 v2_at = Vec2(b2.vx, b2.vy) + perp_r2 * b2.angularVelocity;
                Vec2 rel_vel = v1_at - v2_at;
                double vrn = rel_vel.dot(normal);
                if (vrn >= 0) continue;
                double cross_r1n = r1.cross(normal);
                double cross_r2n = r2.cross(normal);
                double denom = 1.0 / b1.mass + 1.0 / b2.mass + (cross_r1n * cross_r1n) / b1.inertia + (cross_r2n * cross_r2n) / b2.inertia;
                if (denom == 0) continue;
                double j = - (1 + restitution) * vrn / denom;
                if (j > 50) j = 50;
                Vec2 impulse = normal * j;
                b1.vx += impulse.x / b1.mass;
                b1.vy += impulse.y / b1.mass;
                b1.angularVelocity += r1.cross(impulse) / b1.inertia;
                b2.vx -= impulse.x / b2.mass;
                b2.vy -= impulse.y / b2.mass;
                b2.angularVelocity += r2.cross(-impulse) / b2.inertia;
            }
        }
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
        hwndDebug = CreateWindow("BUTTON", "SHOW DEBUG", WS_CHILD | WS_VISIBLE,
                                360, 10, 120, 30, hwnd, (HMENU)4, NULL, NULL);

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

        // Draw floor
        RECT floorRect = {0, (int)floorY, width, height};
        FillRect(hdcMem, &floorRect, (HBRUSH)GetStockObject(GRAY_BRUSH));

        // Draw boxes
        for (auto &b : boxes) {
            // Rotate around center
            double cos_a = std::cos(b.angle);
            double sin_a = std::sin(b.angle);
            
            double w = b.width * 0.5;
            double h = b.height * 0.5;
            
            // Rotate corners
            int corners[4][2];
            double dxs[] = {-w, w, w, -w};
            double dys[] = {-h, -h, h, h};
            
            for (int c = 0; c < 4; ++c) {
                double rx = dxs[c] * cos_a - dys[c] * sin_a;
                double ry = dxs[c] * sin_a + dys[c] * cos_a;
                corners[c][0] = (int)(b.x + rx);
                corners[c][1] = (int)(b.y + ry);
            }
            
            HBRUSH brush = CreateSolidBrush(RGB(0, 200, 255));
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 100, 200));
            HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdcMem, brush);
            
            POINT pts[4] = {{corners[0][0], corners[0][1]}, {corners[1][0], corners[1][1]}, 
                           {corners[2][0], corners[2][1]}, {corners[3][0], corners[3][1]}};
            Polygon(hdcMem, pts, 4);
            
            SelectObject(hdcMem, oldPen);
            SelectObject(hdcMem, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
        }

        // Draw preview box
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

        // Draw debug info if enabled
        if (showDebug) {
            for (auto &b : boxes) {
                // Center of mass (black dot)
                HBRUSH comBrush = CreateSolidBrush(RGB(0, 0, 0));
                SelectObject(hdcMem, comBrush);
                Ellipse(hdcMem, (int)b.x - 3, (int)b.y - 3, (int)b.x + 3, (int)b.y + 3);
                DeleteObject(comBrush);

                // Velocity vector (red arrow)
                HPEN velPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                SelectObject(hdcMem, velPen);
                MoveToEx(hdcMem, (int)b.x, (int)b.y, NULL);
                double vel_len = std::sqrt(b.vx * b.vx + b.vy * b.vy);
                double arrow_len = std::min(50.0, vel_len * 2.0);
                if (vel_len > 0) {
                    double scale = arrow_len / vel_len;
                    LineTo(hdcMem, (int)(b.x + b.vx * scale), (int)(b.y + b.vy * scale));
                }
                DeleteObject(velPen);

                // Gravity force vector (blue downward arrow)
                HPEN gravPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
                SelectObject(hdcMem, gravPen);
                MoveToEx(hdcMem, (int)b.x, (int)b.y, NULL);
                double g_len = 20.0 * b.mass; // Proportional to mass, but since mass=1, fixed
                LineTo(hdcMem, (int)b.x, (int)(b.y + g_len));
                DeleteObject(gravPen);
            }
        }

        // Draw info text
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(0, 0, 0));
        if (addBoxModeEnabled) {
            TextOut(hdcMem, 490, 20, "BOX MODE: Click & drag to add | Space: pause | C: clear", 54);
        } else {
            TextOut(hdcMem, 490, 20, "Click ADD BOX MODE to add boxes | Space: pause | C: clear", 57);
        }

        // Blit to screen
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
                    showDebug = !showDebug;
                    SetWindowText(hwndDebug, showDebug ? "HIDE DEBUG" : "SHOW DEBUG");
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
    Window window(WINDOW_WIDTH, WINDOW_HEIGHT, "Box Physics Simulator");

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            double dt = window.getDeltaTime();
            if (!paused) {
                simulate(dt);
            }
            window.render();
            Sleep(16);
        }
    }

    return 0;
}