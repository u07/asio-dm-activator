# ASIO-DM-ACTIVATOR

ASIO-DM-Activator is a plugin for **Cubase** and **Studio One** that enables the **ASIO Direct Monitoring** feature (commonly known as "Blue Z") for audio interfaces that lack native support for it.

This is a beta release, currently supporting only one device class: the **Audient iD series** (iD14 and above). This limitation is due to testing constraints, as I own an **iD14 MKII**. However, the plugin can theoretically be extended to support any device with a dedicated software mixer.

## Why is this needed?  
The iD series lacks a physical **Direct Monitoring** button, making it inconvenient to access the software mixer every time via mouse. Additionally, DAWs can automatically toggle monitoring during recording and turn it off when unnecessary — something a native DM implementation simplifies.

---

## How It Works

As a DAW plugin, ASIO-DM-Activator intercepts the audio subsystem initialization, modifies the driver's capability response, and adds **Direct Monitoring** to the list of supported features. When successful, your DAW will display DM as available. 



Each time you enable or disable monitoring on a track, the plugin translates the activation request into a format understood by the driver. To the driver, it looks like you’re manually adjusting the mixer slider with your mouse.

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

Homepage: [https://PetelinSasha.ru](https://PetelinSasha.ru)

--- 

I’d love to hear from you if you can help expand support for additional hardware.
