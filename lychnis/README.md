# lychnis

Lychnis is a fiber-tracing tool for [IMS](http://open.bitplane.com/Default.aspx?tabid=268) format.

## Building the code

Currently this program can only be compiled and run on Windows 64-bit systems.

Before building the code, the following prerequisites be must be compiled and installed.

* [Qt](https://www.qt.io/) 5.9 with compiler msvc2015_64 (v140)
* [VTK](https://vtk.org/) 8.2 with VTK_Group_Qt and VTK_ENABLE_KITS enabled
* [OpenCV](https://opencv.org/)

This repository uses a *WordPress-like* configuration strategy instead of *git submodules*. Copy file *dependencies.pri.tmpl* to *dependencies.pri* and modify *dependencies.pri* according to the actual  locations of the dependencies, and then the code can be compiled in Qt Creator under release mode.

The icon of lychnis comes from [pixabay](https://pixabay.com/illustrations/ancient-antique-asteraceae-3543927/).
