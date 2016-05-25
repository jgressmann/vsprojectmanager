Simplistic Visual Studio project support for Qt Creator
=======================================================

This plugin allows you to open, build, and run Microsoft Visual Studio C++ projects from within Qt Creator using the native file.

Requirements
------------
For each project file you are trying to open you need to have the corresponding Visual Studio installed.

Features
--------
* Supported Visual Studio versions:
  - VS2005
  - VS2010

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

* binary for [Qt Creator 3.6](https://www.dropbox.com/s/7dkpx47bx5kmul1/VsProjectManager.dll?dl=1) 

Installation
------------
Copy the plugin binary to Qt Creator's plugin directory <Qt Creator dir>lib\qtcreator\plugins


Caveats
-------
If you plan to open VS2010 project files you want to consider changing the file extension used by Qt Creator to store its 
project settings which defaults to .user as VS2010 also uses that extension to store user related project settings.

To change the extension export an environment variable with the name QTC_EXTENSION and set it to something else, e.g. `set QTC_EXTENSION=.qtc`.
Restart Qt Creator for the change to take effect.


TODO
----
* Support more Visual Studio versions
* Add branch for QTC 4.0



