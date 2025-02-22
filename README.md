# ASIO-DM-ACTIVATOR

ASIO-DM-Activator is a plugin for **Cubase** and **Studio One** that enables the **ASIO Direct Monitoring** feature (also known as "**Blue Z**") for audio interfaces that lack native support for it.

This is a beta release, currently supporting only one device class: the **Audient iD series** (iD14 and above). This limitation is due to testing constraints, as I own an **iD14 MKII**. However, the plugin can theoretically be extended to support any device with a dedicated software mixer. I’d love to hear from you if you can help expand support for additional hardware.

---

## Why is this needed?  
The iD series lacks a physical Direct Monitoring button, meaning you have to grab the mouse and open the mixer app every time you need to toggle it. Now, the button will be right inside the DAW, and the DAW can even switch it on automatically during recording. A significant time saver.

---


## How It Works

As a DAW plugin, ASIO-DM-Activator intercepts the audio subsystem initialization, modifies the driver's capability response, and adds **Direct Monitoring** to the list of supported features. When successful, your DAW will display DM as available. From this point on, the familiar monitoring button changes its behavior, turning into a zero-latency Direct Monitoring switch.

![cube12](https://github.com/user-attachments/assets/45ad1bfd-a411-416d-b43e-9a988ce446e4)
![studio1_](https://github.com/user-attachments/assets/2451e06a-1c8e-47c7-b8a4-eefb6d15302c)

Each time you enable or disable monitoring on a track, the plugin translates the activation request into a format understood by the driver. To the driver, it looks like you’re manually adjusting the mixer slider with your mouse.

- Only two channel states are supported: **ON and OFF**. This ensures the volume balance you've set in the mixer app remains unchanged. 
- Only the **Main mix** is affected. Cue A/B remains untouched.

---

## Installation

Place the `asio-dm-activator.dll` file in the appropriate folder:  

- **Cubase**:  
  `C:\Program Files\Steinberg\Cubase 14\Components`  
- **Studio One**:  
  `C:\Program Files\PreSonus\Studio One 7\Plugins`  

---

## Requirements and Limitations  

1. A 64-bit OS and 64-bit DAW are required.  
2. Compatible with Windows 7 and later.  
3. If multiple devices from the same manufacturer are connected, the plugin will use the first one.  
4. **Beta version**: Use at your own risk.

## Debug

If the plugin misbehaves — wrong channels, no monitoring on your device — run [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) to check the logs. The real-time output provides insight into plugin's operation and may help identify issues. If you decide to open an issue, include these logs to expedite troubleshooting.

![image](https://github.com/user-attachments/assets/f3ae433c-a667-40cf-8ca2-77e3bb9a9c69)


Homepage: [https://PetelinSasha.ru/notes/asio-dm-activator](https://petelinsasha.ru/notes/asio-dm-activator)

