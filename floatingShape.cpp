#include <iostream>
#include <vector>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <X11/keysym.h>
using namespace std;

// One point in 3D space.
struct Point3D {
    double x;
    double y;
    double z;
};

struct Point2D {
    int x;
    int y;
};

// One line connecting two points.
// Color: 0 = red, 1 = green, 2 = blue.
struct Edge {
    int start;
    int end;
    int color;
};

// A shape contains points and the edges connecting them.
struct Shape {
    vector<Point3D> points;
    vector<Edge> edges;
};

Shape createSquarePyramid() {
    Shape pyramid;

    // Base corners: indices 0, 1, 2, 3.
    // Tip: index 4.
    pyramid.points = {
        {-1, -1, -1},
        { 1, -1, -1},
        { 1, -1,  1},
        {-1, -1,  1},
        { 0,  1,  0}
    };

    // Move the average vertex position to (0, 0, 0).
    // This will be the center used for rotation.
    Point3D center = {0, 0, 0};

    for (const Point3D& point : pyramid.points) {
        center.x += point.x;
        center.y += point.y;
        center.z += point.z;
    }

    center.x /= pyramid.points.size();
    center.y /= pyramid.points.size();
    center.z /= pyramid.points.size();

    for (Point3D& point : pyramid.points) {
        point.x -= center.x;
        point.y -= center.y;
        point.z -= center.z;
    }

    pyramid.edges = {
        {0, 1, 0},  // Base edges
        {1, 2, 1},
        {2, 3, 2},
        {3, 0, 0},
        {0, 4, 1},  // Edges connecting the base to the tip
        {1, 4, 2},
        {2, 4, 0},
        {3, 4, 1}
    };

    return pyramid;
}

// Return a rotated copy of a point.
// angle is measured in radians.
Point3D rotateX(Point3D point, double angle) {
    Point3D rotated;

    rotated.x = point.x;
    rotated.y = point.y * cos(angle) - point.z * sin(angle);
    rotated.z = point.y * sin(angle) + point.z * cos(angle);

    return rotated;
}

Point3D rotateY(Point3D point, double angle) {
    Point3D rotated;

    rotated.x = point.x * cos(angle) + point.z * sin(angle);
    rotated.y = point.y;
    rotated.z = -point.x * sin(angle) + point.z * cos(angle);

    return rotated;
}

Point3D rotateZ(Point3D point, double angle) {
    Point3D rotated;

    rotated.x = point.x * cos(angle) - point.y * sin(angle);
    rotated.y = point.x * sin(angle) + point.y * cos(angle);
    rotated.z = point.z;

    return rotated;
}

// A rigid camera transform preserves lengths and angles in 3D.
// Match the reference camera: position (5, 7, 70), yaw initially -0.1.
Point3D viewingTransform(Point3D point, double cameraYaw) {
    point.x -= 5.0;
    point.y -= 7.0;
    point.z -= 70.0;
    return rotateY(point, -cameraYaw);
}

Point2D projectPoint(Point3D point) {
    const double FOCAL_LENGTH = 600.0;
    double scale = FOCAL_LENGTH / -point.z;
    return {
        static_cast<int>(round(400.0 + point.x * scale)),
        static_cast<int>(round(300.0 - point.y * scale))
    };
}

void drawLine(XImage* image, Point2D start, Point2D end,
              unsigned long color) {
    int x = start.x;
    int y = start.y;

    int dx = abs(end.x - start.x);
    int dy = -abs(end.y - start.y);

    int stepX = (start.x < end.x) ? 1 : -1;
    int stepY = (start.y < end.y) ? 1 : -1;

    int error = dx + dy;

    while (true) {
        // Only write pixels inside the image.
        if (x >= 0 && x < image->width &&
            y >= 0 && y < image->height) {
            XPutPixel(image, x, y, color);
        }

        if (x == end.x && y == end.y) {
            break;
        }

        int twiceError = 2 * error;

        if (twiceError >= dy) {
            error += dy;
            x += stepX;
        }

        if (twiceError <= dx) {
            error += dx;
            y += stepY;
        }
    }
}

Shape createTriangularPyramid() {
    Shape pyramid;

    // Equilateral triangular base and a tip above it.
    const double root3 = sqrt(3.0);

    pyramid.points = {
        {-1, -0.5, -root3 / 3},
        { 1, -0.5, -root3 / 3},
        { 0, -0.5,  2 * root3 / 3},
        { 0,  1.5,  0}
    };

    // The average vertex position is already (0, 0, 0).
    pyramid.edges = {
        {0, 1, 0},
        {1, 2, 1},
        {2, 0, 2},
        {0, 3, 1},
        {1, 3, 2},
        {2, 3, 0}
    };

    return pyramid;
}

Shape createCrossedBasePyramid() {
    Shape pyramid = createSquarePyramid();

    // Add both diagonals of the square base.
    pyramid.edges.push_back({0, 2, 1});
    pyramid.edges.push_back({1, 3, 2});

    return pyramid;
}

// Clip at the camera's near plane before dividing by depth.
void drawSegment(XImage* image, Point3D start, Point3D end,
                 double cameraYaw, unsigned long color) {
    start = viewingTransform(start, cameraYaw);
    end = viewingTransform(end, cameraYaw);
    const double NEAR_PLANE = 1.0;
    if (start.z > -NEAR_PLANE && end.z > -NEAR_PLANE) {
        return;
    }
    if (start.z > -NEAR_PLANE || end.z > -NEAR_PLANE) {
        double t = (-NEAR_PLANE - start.z) / (end.z - start.z);
        Point3D intersection = {
            start.x + t * (end.x - start.x),
            start.y + t * (end.y - start.y),
            -NEAR_PLANE
        };
        if (start.z > -NEAR_PLANE) start = intersection;
        else end = intersection;
    }
    drawLine(image, projectPoint(start), projectPoint(end), color);
}

int main() {
    Display* display = XOpenDisplay(nullptr);

    if (display == nullptr) {
        cerr << "Could not open the X11 display.\n";
        return 1;
    }

    int screen = DefaultScreen(display);

    const int WIDTH = 800;
    const int HEIGHT = 600;

    Visual* visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);

    XImage* image = XCreateImage(
        display, visual, depth, ZPixmap,
        0, nullptr, WIDTH, HEIGHT, 32, 0
    );

    if (image == nullptr) {
        cerr << "Could not create the image.\n";
        XCloseDisplay(display);
        return 1;
    }

    if (depth != 24 || image->bits_per_pixel != 32) {
        cerr << "This program requires 24-bit color in 32-bit pixels.\n";
        XDestroyImage(image);
        XCloseDisplay(display);
        return 1;
    }

    image->data = static_cast<char*>(
        calloc(HEIGHT, image->bytes_per_line)
    );

    if (image->data == nullptr) {
        cerr << "Could not allocate the pixel buffer.\n";
        XDestroyImage(image);
        XCloseDisplay(display);
        return 1;
    }

    unsigned long colors[3] = {
        visual->red_mask,
        visual->green_mask,
        visual->blue_mask
    };

    unsigned long axisColor = BlackPixel(display, screen);

    vector<Shape> shapes = {
        createSquarePyramid(),
        createTriangularPyramid(),
        createCrossedBasePyramid()
    };

    const char* titles[3] = {
        "Square Pyramid",
        "Triangular Pyramid",
        "Square Pyramid with Crossed Base"
    };

    int currentShape = 0;
    bool showAxes = true;
    double cameraYaw = -0.1;

    const double PI = acos(-1.0);
    double angleY = 0.0;
    double angleZ = 0.0;

    // Both rotations advance every frame, at the reference speeds.
    const double Y_SPEED = PI / 2.0;
    const double Z_SPEED = PI / 10.0;

    vector<Point3D> axisPoints = {
        {-50, 0, 0}, {50, 0, 0},
        {0, -50, 0}, {0, 50, 0},
        {0, 0, -30}, {0, 0, 30}
    };

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0, 0,
        WIDTH, HEIGHT,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    XStoreName(display, window, titles[currentShape]);

    XSelectInput(
        display, window,
        ExposureMask | KeyPressMask | ButtonPressMask
    );

    Atom closeMessage = XInternAtom(
        display, "WM_DELETE_WINDOW", False
    );
    XSetWMProtocols(display, window, &closeMessage, 1);

    // The software framebuffer is fixed at 800 by 600.
    XSizeHints sizeHints = {};
    sizeHints.flags = PMinSize | PMaxSize;
    sizeHints.min_width = sizeHints.max_width = WIDTH;
    sizeHints.min_height = sizeHints.max_height = HEIGHT;
    XSetWMNormalHints(display, window, &sizeHints);
    XMapWindow(display, window);
    GC gc = XCreateGC(display, window, 0, nullptr);

    bool running = true;

    using Clock = chrono::steady_clock;
    auto previousTime = Clock::now();

    // Derive every deadline from one epoch: late frames do not add drift.
    const auto scheduleStart = previousTime;
    long long scheduledFrame = 0;
    auto deadlineFor = [&](long long frame) {
        return scheduleStart + chrono::duration_cast<Clock::duration>(
            chrono::duration<double>(static_cast<double>(frame) / 60.0));
    };

    auto fpsStart = Clock::now();
    int frameCount = 0;

    while (running) {
        auto frameStart = Clock::now();

        // Process mouse, keyboard, and close events.
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == ClientMessage &&
                static_cast<Atom>(event.xclient.data.l[0])
                    == closeMessage) {
                running = false;
            }

            if (event.type == ButtonPress &&
                event.xbutton.button == Button1) {
                currentShape = (currentShape + 1) % 3;
                XStoreName(display, window, titles[currentShape]);
            }

            if (event.type == ButtonPress &&
                event.xbutton.button == Button3) {
                showAxes = !showAxes;
            }

            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);

                if (key == XK_Return || key == XK_KP_Enter ||
                    key == XK_x || key == XK_X) {
                    showAxes = !showAxes;
                } else if (key == XK_Left) {
                    cameraYaw = remainder(cameraYaw - 0.05, 2 * PI);
                } else if (key == XK_Right) {
                    cameraYaw = remainder(cameraYaw + 0.05, 2 * PI);
                } else if (key == XK_Escape) {
                    running = false;
                }
            }
        }

        if (!running) {
            break;
        }

        // Keep rotation speed independent of frame rate.
        double elapsed =
            chrono::duration<double>(
                frameStart - previousTime
            ).count();

        previousTime = frameStart;
        angleY = fmod(angleY + Y_SPEED * elapsed, 2 * PI);
        angleZ = fmod(angleZ + Z_SPEED * elapsed, 2 * PI);

        // White is all bits set on the supported TrueColor visual.
        // Clear whole rows instead of calling XPutPixel 480,000 times.
        memset(image->data, 0xff, image->bytes_per_line * HEIGHT);

        if (showAxes) {
            for (int i = 0; i < 6; i += 2) {
                drawSegment(image, axisPoints[i], axisPoints[i + 1],
                            cameraYaw, axisColor);
            }
        }

        const Shape& shape = shapes[currentShape];
        vector<Point3D> rotatedPoints;
        for (const Point3D& point : shape.points) {
            // Compose Y and Z rotations on the original geometry each frame.
            Point3D rotated = rotateZ(rotateY(point, angleY), angleZ);
            // Reference shapes use approximately six world units per side.
            rotatedPoints.push_back({6 * rotated.x, 6 * rotated.y,
                                     6 * rotated.z});
        }

        // Draw the selected shape.
        for (const Edge& edge : shape.edges) {
            drawSegment(image, rotatedPoints[edge.start],
                        rotatedPoints[edge.end], cameraYaw, colors[edge.color]);
        }

        // Send the completed image to the window.
        XPutImage(
            display, window, gc, image,
            0, 0, 0, 0, WIDTH, HEIGHT
        );

        XFlush(display);

        ++scheduledFrame;
        auto deadline = deadlineFor(scheduledFrame);
        auto finished = Clock::now();
        if (finished > deadline) {
            // Skip missed deadlines instead of rendering a burst of frames.
            scheduledFrame = static_cast<long long>(
                chrono::duration<double>(finished - scheduleStart).count()
                * 60.0) + 1;
            deadline = deadlineFor(scheduledFrame);
        }
        // Standard C++ has no blocking timed wait without thread facilities.
        // Poll the monotonic clock on this single thread until the deadline.
        while (Clock::now() < deadline) {
        }

        // Report the actual loop frame rate once per second.
        frameCount++;

        auto now = Clock::now();
        double measurementTime =
            chrono::duration<double>(now - fpsStart).count();

        if (measurementTime >= 1.0) {
            double actualFPS = frameCount / measurementTime;

            cout << "FPS: " << actualFPS << endl;

            frameCount = 0;
            fpsStart = now;
        }
    }

    XFreeGC(display, gc);
    XDestroyImage(image);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
