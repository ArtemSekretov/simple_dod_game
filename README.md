# simple_dod_game

Build And Run Instructions

Dependencies:

* Node.js
* MSVC (Visual Studio Build Tools)

To ensure a clean setup, I recommend configuring dependencies to work only within the current shell (see the example in shell.bat). 
After setting up the dependency paths, run build.bat to create the executables and generate all necessary files.

To run the application, use:

* run_dm.bat for the debug build
* run_rm.bat for the release build

The game uses DirectX 11 for rendering.

In debug builds, the game uses the DirectX Debug Layer.
For the debug layer to work, it is necessary to install Graphics Tools from the Optional Features section in Windows Settings.
