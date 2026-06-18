# Head-Tracking Kit (HTK)

A lightweight, open-source, webcam-based head-tracking system for simulation games, written in C++.
Supports FreeTrack and TrackIR-compatible input and output protocols.

> This project is being actively developed and still heavily work in progress, but is functional overall.

## Screenshots
<img width="1212" height="844" alt="htk" src="https://github.com/user-attachments/assets/a78718d1-ccd5-4518-ae93-4c3a375a9803" />



## Features
- 6DOF head tracking using a standard webcam
- Compatible with major simulators  
  *(tested mainly with DCS World and Microsoft Flight Simulator)*
- Low-latency tracking
- Qt6–based user interface
- FreeTrack and TrackIR protocol support

## Roadmap
- Improve head-pose estimation robustness at extreme angles  
  *(target: reliable tracking up to ~90° head rotation)*
- Reduce landmark loss beyond ~45° head rotation
- Further latency and stability optimizations
