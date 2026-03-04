#include <windows.h>
#include <vector>
#include <cmath>
#include <sstream>

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

struct Box {
    int id;
    double mass;
    double width, height;
    double x, y;
    double vx, vy;
};

std::vector<Box> boxes;
double gravity = 9.81;
double floorY = 550.0;
double restitution = 0.6;
double friction = 2.0;
bool paused = false;
int addingBox = 0;
double tempX = 0, tempY = 0, tempW = 50, tempH = 50;
int nextId = 1;

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 600;
const int BUTTON_HEIGHT = 50;

HWND hwndPlayPause, hwndAdd, hwndClear;

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
    boxes.push_back(b);
}

void simulate(double dt) {
    for (auto &b : boxes) {
        b.vy += gravity * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;

        double bottom = b.y + b.height * 0.5;
        if (bottom >= floorY) {
            b.y = floorY - b.height * 0.5;
            if (std::abs(b.vy) > 0.1) {
                b.vy = -b.vy * restitution;
            } else {
                b.vy = 0;
            }
            double frictionFactor = 1.0 - std::min(1.0, friction * dt);
            b.vx *= frictionFactor;
        }

        if (b.x < b.width * 0.5) {
            b.x = b.width * 0.5;
            b.vx = std::abs(b.vx) * 0.8;
        }
        if (b.x > WINDOW_WIDTH - b.width * 0.5) {
            b.x = WINDOW_WIDTH - b.width * 0.5;
            b.vx = -std::abs(b.vx) * 0.8;
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

        hwndPlayPause = CreateWindow("BUTTON", paused ? "RESUME" : "PAUSE", WS_CHILD | WS_VISIBLE,
                                     10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        hwndAdd = CreateWindow("BUTTON", "ADD BOX", WS_CHILD | WS_VISIBLE,
                              120, 10, 100, 30, hwnd, (HMENU)2, NULL, NULL);
        hwndClear = CreateWindow("BUTTON", "CLEAR", WS_CHILD | WS_VISIBLE,
                                230, 10, 100, 30, hwnd, (HMENU)3, NULL, NULL);

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
        RECT floorRect = {0, (int)floorY, width, height - (int)floorY};
        FillRect(hdcMem, &floorRect, (HBRUSH)GetStockObject(GRAY_BRUSH));

        // Draw boxes
        for (auto &b : boxes) {
            int x1 = (int)(b.x - b.width * 0.5);
            int y1 = (int)(b.y - b.height * 0.5);
            int x2 = (int)(b.x + b.width * 0.5);
            int y2 = (int)(b.y + b.height * 0.5);
            
            HBRUSH brush = CreateSolidBrush(RGB(0, 200, 255));
            RECT boxRect = {x1, y1, x2, y2};
            FillRect(hdcMem, &boxRect, brush);
            DeleteObject(brush);
            
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 100, 200));
            HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
            Rectangle(hdcMem, x1, y1, x2, y2);
            SelectObject(hdcMem, oldPen);
            DeleteObject(pen);
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

        // Draw info text
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(0, 0, 0));
        TextOut(hdcMem, 350, 20, "Click & drag to add box | Space: pause | C: clear", 48);

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
                    addingBox = 1;
                } else if (LOWORD(wParam) == 3) {
                    boxes.clear();
                }
                return 0;

            case WM_LBUTTONDOWN: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                if (my > BUTTON_HEIGHT) {
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
