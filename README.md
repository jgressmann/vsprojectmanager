Simplistic Visual Studio project support for Qt Creator
=======================================================

This plugin allows you to open, build, and run Microsoft Visual Studio C++ projects from within Qt Creator using the native file.

Requirements
------------
To build the project you need to have the corresponding Visual Studio version. For debugging you need to have
the [Debugging Tools for Windows](https://msdn.microsoft.com/en-us/library/windows/hardware/ff551063(v=vs.85).aspx) installed.

Supported Visual Studio versions
--------------------------------
* VS2005
* VS2010
* VS2013
* VS2015

Author
-------
* Jean Gressmann (jean@0x42.de)

Build
-----
* Get the source for [Qt Creator](https://github.com/qtproject/qt-creator)
* Get this plugin's sources and put them into Qt Creator's plugin directory.
* Edit Qt Creator's plugins.pro to include this plugin's project file.
* Build

Download
--------

* binary for [Qt Creator 3.6.1](https://www.dropbox.com/s/7dkpx47bx5kmul1/VsProjectManager.dll?dl=1)
* binary for [Qt Creator 4.0.0](https://www.dropbox.com/s/7hx171zhimgz0mw/VsProjectManager.dll?dl=1)

Installation
------------
Copy the plugin binary to Qt Creator's plugin directory `<Qt Creator dir>\lib\qtcreator\plugins` and restart Qt Creator.


Caveats
-------
If you plan to open VS2010 or later project files you want to consider changing the file extension used by Qt Creator to store its
project settings which defaults to .user as that particular extension conflicts with Visual Studio's user settings.

To change the extension export an environment variable with the name QTC_EXTENSION and set it to something else, e.g. `set QTC_EXTENSION=.qtc`.
Restart Qt Creator for the change to take effect.


TODO
----
* Add support to load & build solution files
