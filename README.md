
The executables and demo data can be downloaded from **[releases](https://github.com/SMART-pipeline/Lychnis-tracing/releases/download/1.2.5/Lychnis-1.2.5.zip)**.
Full version including the source codes will be released later.

# Lychnis-tracer

## System requirements
* Software dependencies: ImarisFileConverter (freely downloadable in https://imaris.oxinst.com/big-data), for converting a variety of image formats to .ims format, which is supported by Lychnis.
* Hardware dependencies: NVIDIA graphics card
* Operating systems (including version numbers): Windows 10 64-bit
* Versions the software has been tested on: 1.2.5
* Any required non-standard hardware: None

## Installation guide
Instructions: Unzip “Lychnis.zip” and run “Software/lychnis-1.2.5.exe” directly.

## Demo
Instructions to run on data:
1. Click menu “File -> Load project” and select “Demo-data/Lychnis/ROI.json”
2. Drag slider at the top to adjust contrast.

## Instructions for use
How to run the software on your data
1. Click menu “File -> Open IMS file” to import IMS image instead of existing project
2. Hold down “Z” and left-click to pick a point in volume which will be connected to currently selected point.
3. Hold down “X” and left-click to select a previously inserted point.
4. Hold down “X” and right-click to connect currently selected point to the next selected point.
5. Press “ESC” to cancel selection, “Delete” to delete selected point.
6. Drag slider at the bottom to change resolution.
7. Press Space to reload a data block centered at selected point.
8. Press F to trace axons automatically when at least two points have been inserted.


# Lychnis-stitcher

## System requirements
* Software dependencies: None
* Hardware dependencies: NVIDIA graphics card
* Operating systems (including version numbers): Windows 10 64-bit
* Versions the software has been tested on: 1.1.0
* Any required non-standard hardware: None

## Installation guide
Instructions: Unzip “Lychnis.zip” and run “Software/Lychnis-stitcher-1.1.0.exe” directly.

## Demo
Instructions to run on data:
1. Click menu “File -> Load project” and select “Demo-data/Lychnis-stitcher/91-92.json”
2. Press C
3. Press Ctrl+B 

## Instructions for use
How to run the software on your data
1. Hold down “Z” and left-click to pick a point in volume which will be connected to currently selected point.
2. Hold down “X” and left-click to select a previously inserted point.
3. Press “ESC” to cancel selection, “Delete” to delete selected point.
4. Drag slider at the top to adjust contrast.
5.Click menu “Edit -> Build deformation from markers” to deform images based on the label points.

