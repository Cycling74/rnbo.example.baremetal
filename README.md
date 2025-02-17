# RNBO Minimal Export for Bare Metal Targets

A simple example of the RNBO minimal export for bare metal targets

## Author

Stefan Brunner - stb@cycling74.com

## Description

This should hopefully get you started with integrating the Minimal Export of your RNBO patcher in your C++ project.

As so often: this is under heavy development, so use this at your own risk, it might suddenly crash, burn, glitch, annoy your cat - anything can happen

Make sure you have the latest RNBO beta installed. Otherwise this might not be working.

More information can be found here: https://rnbo.cycling74.com/learn/minimal-export

## Export

RNBO C++ Source Code Export generated files need to go into the _/export_ directory. Be aware that you need to use the "Minimal Export" feature to generate code that works with this example.

Consider using a fixed vector size that matches the one set in the example (currently 64 samples, but feel free to change it to your needs) for your export.

## Usage

A simple CMakeLists.txt is provided. So you could for example:

mkdir build
cd build/
cmake ..
cmake --build .

or:

mkdir build
cd build/
cmake .. -G Xcode

and build with Xcode, or whatever cmake flavour suits you best.

## API

There is some comments about the usage of the API in the file _main.cpp_ and more detailed information is
available on: https://rnbo.cycling74.com/learn/minimal-export
