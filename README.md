# simple_x11_tiling_wm

Just barely functional, still lots of missing features. ***The end product will not be a practical or useful WM.***
<br>
This is a continuation of my previous project [learning_x11_wm](https://github.com/RockRottenSalad/learning_x11_wm)
where I had made a very simple X11 window manager in an attempt to learn how to work with X11. This is a retake
on that simple project with an attempt to make yet another simple window manager, using the things I learned
last time.
<br>
## Current features
- Spawning windows
- Closing windows(Not reliable, buggy, tends to crash)
- Dynamically tile master window on left and slave windows on right
- Incrementing the master stack
- Resizing master stack
- Cycle forwards/backwards through the stack
<br>

## Missing features
- Workspaces
- Status bar(if I feel like it)
- Actually making this thing stable and not crash 
<br>

## Running
Needs Xephyer to run in a nested X server. Uses clang to build by default, but can be changed by editing the macro inside wiz_build.c.
Obviously also depends on X11 libs.

```
mkdir bin
clang ./wiz_build.c -o wiz_build
./wiz_build run

```


