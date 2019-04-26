# GigaLoop-Viewer
The source code of panoramic video viewer implemented in the paper "Gigapixel Panorama Video Loops".

This is the implementation of a local panoramic video viewer proposed in the paper [**Gigapixel Panorama Video Loops**](https://dl.acm.org/citation.cfm?id=3144455) in ACM Transactions on Graphics (TOG).


## Introduction

**GigaLoop-Viewer** is a viewer to interactive with a large-scale panoramic video. The large-scale video should be diced into a set of square video tiles in a 2D grid structure at multiple levels of detail. This viewer loads the video tiles into memory based on a priority schedule. The interface is shown below.

![image](https://github.com/hmmlillian/GigaLoop-Viewer/blob/master/demo/UI.PNG)


## Getting Started

### Prerequisites
- Windows (64bit)
- Visual Studio 2013
- Third-party libraries:
  - OpenCV 2.4.10
  - Qt 5.5
  - [FFmpeg](http://ffmpeg.zeranoe.com/builds/)
  
### Build
The viewer is implemented in C++ and requires compiling ```code\VideoViewer.sln``` in Visual Studio. Before compiling it, please add enviroment variables OPENCVDIR, QTDIR and FFMPEGDIR corresponding to the paths of the three libraries.

### Demo & Run
We prepare a pre-built executable program under the folder ```demo\exe```. To try it, please

(1) Put the video tiles under the folder ```demo\data```. We provide an [example](https://drive.google.com/file/d/1bqa3BEZwWITIsNASyO42oOLDA5cKuMEM/view?usp=sharing) for reference.

(2) Executable script ```demo\run.bat``` including the following commands:
  ```
  cd exe
  VideoViewer.exe -d ..\data\
  ```  
  
  
## Citation
If you find **GigaLoop-Viewer** helpful for your research, please consider citing:
```
@article{he2017gigapixel,
  title={Gigapixel Panorama Video Loops},
  author={He, Mingming and Liao, Jing and Sander, Pedro V and Hoppe, Hugues},
  journal={ACM Transactions on Graphics (TOG)},
  volume={37},
  number={1},
  pages={3},
  year={2017},
  publisher={ACM}
}
```
