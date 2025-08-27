# Remote-internship Lab assignments
Lab assignments in System Security Lab


This repository contains lab assignments completed as part of system/software security training.  
Each lab focuses on practical skills in robotics control, dynamic analysis, and compiler infrastructure.  

## Lab 1: ArduRover Control & Sensor Fault Injection
### Lab 1-1: Barometer Sensor Fault Injection
- Implemented functionality to **inject errors** into the **barometer sensor readings**.
- Verified how the rover responds to abnormal sensor data and fault conditions.
- 2 Files (AP_Baro/AP_Baro.cpp and AP_Baro/AP_Baro.h) are modified.


### Lab 1-2: Basic Rover Control
- Implemented Python code to control an **ArduRover** robot.
- Features: **forward**, **backward**, **stop**, and **rotation**.

## Lab 2: Dynamic Analysis with Valgrind
- Implemented **data-dependency tracking** on top of **Valgrind**.
- Captures dependencies via runtime memory/register access instrumentation.

## Lab 3: Static Call Graph Analysis with LLVM
- Built a static **function call graph** using the **LLVM API**.
- Handles **direct** and **indirect** (function pointer) calls in C/C++.
