# ArduinoHRVAnalysis
Development of an embedded system based on Arduino microcontroller to analyse Heart Rate Variability and compute its Fourier transform. The FFT results are sent through the serial port and then read by Matlab, which estimates and shows the Local Power Spectral Density.

Authors: 
Andrea Arcangeli
Martina De Marinis

## Hardware 
This device uses the following components:
- Arduino MKR1000 (MEGA/Uno/MKR1010 are equivalent)
- AD8232: single lead heart rate monitor

The 3 electrodes connected to the AD8232 are placed on the right arm, the left arm and the left leg.
![ad8232](https://user-images.githubusercontent.com/63754081/223095052-1d2bc996-6273-4a90-8e77-8634393d53e7.jpg)

## AD8232 -> Arduino pin mapping
- AD8232 3.3V -> Arduino 3.3V
- AD8232 OUTPUT -> Arduino A0
- AD8232 GND -> Arduino GND

## To read and show data on Matlab
To display the results on Matlab, the following steps must be followed:
1. Connect the Arduino to the laptop
2. Run Arduino IDE and open ad8232.ino
3. Run Matlab and open ArduinoSerialConnector.mat script and edit this line:
```C 
arduinoObj = serialport("XXX", 115200); 
```
with the correct COM port ('if you are not using Windows operating system, enter the correct port name'), eg:
With Arduino connected on COM10 port:
```C 
arduinoObj = serialport("COM10", 115200); 
```

So, in Arduino IDE:
1. In the script ad8232.ino change (on line 87) ``` bool enable_FFT = false; ``` to ``` bool enable_FFT = true; ```
2. Upload the firmware to the Arduino
3. Quickly go to Matlab and run arduinoSerialConnector.mat script

Wait a few tens of seconds to see the results...

## Examples of displayed results

Subject status: rest
![fft_hrv_relax](https://user-images.githubusercontent.com/63754081/223096113-7e334973-ace8-43c8-be1e-789d6869a6e9.png)

Subject status: alert
![fft_hrv_alert](https://user-images.githubusercontent.com/63754081/223096142-a70c44ef-7357-40d7-aa0c-178d64a7a0ff.png)
