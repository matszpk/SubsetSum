SubsetSum
=========

Sample OpenCL application that solves SubsetSum problem by using brute force method.

This is small application that can uses OpenCL to solve SubsetSum problem.
Can be used for educational purpose to show methods to design efficient heterogenous applications.
This application uses MemQueue system to efficiently broadcasts small tasks between
threads (workers). 

Application can use SSE2, SSE4.1 Intel extensions and OpenCL. It is especially designed
for Radeon GPU's (likes Radeon HD7X00).

Usage is simple:
../build/src/SubsetSum -G sample.txt

where sample.txt is file with sequence of the numbers. Application accepts 128-bit numbers
and can solve efficiently N=50-70 problems.

Application can use two method to solve problem:
- naive
- hash (can handle hashes greater than GPU memory).

Software requirements:
- glibmm
- cppunit for tests
- popt
- cl.hpp header for OpenCL
