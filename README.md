# Sensor Stream Recorder

The purpose of this recorder is to synchronize sensor stream with an annotated descripton of the current action (gesture name), and save them in a filing structure consisting of a video file, audio file, and a subtilte file (`.srt`). 

* Video file which is a `.avi` format file that contains the images from the camera (10fps) of the current scene
* Audio file which is a `.wav` format file that contains the sensor stream (synchronized with the images) written in little-endian order
* Subtitile file which is a `.srt` format file that has annotations of the actions (gestures) corresponding to the various times of the recording 

[![Status](https://travis-ci.org/HassanAmr/Sensor-Stream-Recorder.svg?branch=master)](https://travis-ci.org/HassanAmr/Sensor-Stream-Recorder)  

Installation
------------
    
    $ cmake .
    $ make

In case you want the recorder with the plotter, you will need to modify the file called `CMakeLists.txt` to use `Recorder_w_Plot.cpp`

Usage:
  
    $ ./Recorder

      This program helps you train your sensor data by piping the sensor stream directly into this program

      Please make sure that the Recorder is active before starting the sensor stream feed, otherwise the feed will not be synchronized.
      
      Usage:
          ./Recorder [recorder_name] [expected_#_inputs] [sampling_frequency]

      Control keys (Recorder window must be active for these controls to work):
          ESC - quit the program
          r - start, or stop a recording
          n - get ready for the next item in the list found in the file called `gesture`
          s - start/end annotating the recording with the current gesture pointed to by using the key `n`

      Example:
          unbuffer python SerialPort.py | ./Recorder Hassan 9 20
 

Please note:
  In case the stream was interrupted during the recording session, please restart the recorder along with the stream as explained above.

