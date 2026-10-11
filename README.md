# CS0045_MIDTERM_RUIZ
Project: StarPoint
A 2D space-shooter game made in C++ using OpenGL and FreeGLUT/GLUT. Move the player ship, shoot the boss, avoid enemy bullets, and defeat the boss before losing all three lives.

Developed by: Joshua Danrei C. Ruiz

Requirements
A C++ compiler, such as g++ or MinGW-w64
FreeGLUT, including its development headers and library
OpenGL development libraries
Windows is the intended platform for the current source because it uses GetAsyncKeyState() from windows.h to read movement and Shift input.

Controls:
W/A/S/D - Movement
Left Mouse Button - Shoot
Shift (Hold) - Precision Mode 
P - Pauses/Resumes the game
R - Resets the game
Esc - Exit Program

Notes
- The game uses an 800 × 600 window.
- The timer updates at approximately 16 ms intervals (about 60 updates per second).
- The current source initializes the random-number generator with a fixed seed (srand(1)), so the starfield's random sequence is repeatable.
