# MediBox
A Medibox is a tool that is used to remind users to take their medications at the appointed times and to store medications appropriately according to the rules.

## First stage

we developed a menu which is able to
* Set time zone by taking the offset from UTC as input
* ability to set 3 alarms
* ability to disable all three alarms at once

three buttons were used for navigate in the display and to set the time  and the time zone . An OLED display is used to display the current time and to display the menu option when the push button is clicked.

Because the chosen time zone is fetched from the NTP server over WI-FI, the ESP32 micro controller, which has an integrated Bluetooth and WI-FI module, is used for this device.

We need to constantly monitor the temperature and the humidity inside the device to store the medical drugs under the given conditions. For this a DHT22 temperature and humidity sensor is used.

A buzzer and an LED are utilized to notify the user of the medical intake time and to warn them of any unfavorable conditions inside the device.

![Screenshot 2023-08-28 235516](https://github.com/leonfdo/MediBox/assets/78163260/8c82318a-00b0-439d-9e79-2e013e65db75)

## Second Stage

This stage attempts to improve the Medibox's fundamental functions and add new features to increase the device's utility.

- It is essential to monitor light intensity when storing certain medicines as they may be sensitive to sunlight. For this a LDR sensor is used.
-  A shaded sliding window has been installed to prevent excessive light from entering the Medibox. The shaded sliding window is connected to a servo motor responsible for adjusting the light intensity entering the Medibox.
	- The following equation represents the relationship between the motor angle and the intensity of light entering the Medibox:
	-  θ = θoffset + (180 − θoffset) × I × γ 
	- where, 
	- θ is the motor angle 
	-  θoffset is the minimum angle (default value of 30 degrees) 
	- I is the intensity of light, ranging from 0 to 1 
	- γ is the controlling factor (default value of 0.75)
	
- Different medicines may have different requirements for the minimum angle and the controlling factor used to adjust the position of the shaded sliding window.
- To enable the user to adjust the minimum angle and controlling factor, Node-RED dashboard is created .
- In additionally I have implemented a dashboard to display the temperature and the Humidity and another dashboard to change the alarm time schedules.

![Screenshot 2023-08-29 004903](https://github.com/leonfdo/MediBox/assets/78163260/8de8b88f-35e4-4b7b-b596-c155749a0397)

