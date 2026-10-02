# 3D Floating Shapes

This C++ program displays three rotating wireframe shapes: a square pyramid, a triangular pyramid, and a square pyramid with crossed diagonals at its base. The lines use red, green, and blue.

The program uses Xlib and a pixel buffer to draw in an 800 × 600 area. Each shape rotates around its own center, cycling through the x-, y-, and z-axes. It targets 60 FPS and prints the measured FPS in the terminal.

## Controls

- Left-click: change the shape.
- Enter: hide/show the axes.
- Left/right arrows: move the shape and axes together, including off-screen.
- Close button: exit.

## Run

Use Linux or WSL with an X11 display, g++, and libx11-dev installed. In the project folder, run:

```bash
make
./shapes
```

To remove the compiled program:

```bash
make clean
```
