# BLE-Interact

<div align="center">

![BLE-Interact Logo](https://img.shields.io/badge/BLE-Interact-blue?style=for-the-badge&logo=bluetooth)

**A native DLL bridge for Java applications to interact with Bluetooth Low Energy devices**

[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg)]()
[![JDK](https://img.shields.io/badge/JDK-8+-yellow.svg)]()

</div>

## 📋 Overview

BLE-Interact is a powerful native DLL library that provides seamless integration between Java applications and low-level Bluetooth Low Energy (BLE) functionality on Windows systems. This library leverages Windows Runtime (WinRT) APIs to interact with BLE devices and exposes this functionality to Java through JNI (Java Native Interface).

## ✨ Features

- 🔍 Scan for nearby BLE devices
- 🔌 Connect to specific BLE devices by address
- 🚀 Initialize UART service for communication
- 📤 Send data to BLE devices
- 📥 Receive notifications from BLE devices
- 🔄 Handle automatic device connection status changes
- 🛑 Manage BLE device connections

## 🔧 Requirements

- Windows operating system
- JDK 8 or higher
- BLE-capable hardware

## 📚 JNI Functions

The library exposes the following JNI functions that can be called from Java:

### `initialize()`

Initializes the WinRT apartment for BLE operations.

```java
public native void initialize();
```

### `searchBLEDevices()`

Starts a scan for nearby BLE devices. Returns an ArrayList of BLEDevice objects.

```java
public native ArrayList<BLEDevice> searchBLEDevices();
```

### `connectDevice(String deviceAddress)`

Connects to a BLE device using its address. Returns true if connection is successful.

```java
public native boolean connectDevice(String deviceAddress);
```

### `initializeUARTCharacteristics(String uartServiceUuid, String rxUuid, String txUuid)`

Initializes UART service with the specified UUIDs for communication. Returns true if successful.

```java
public native boolean initializeUARTCharacteristics(String uartServiceUuid, String rxUuid, String txUuid);
```

### `writeToRX(String data)`

Writes data to the RX characteristic of the connected device. Returns true if successful.

```java
public native boolean writeToRX(String data);
```

### `disconnectDevice()`

Disconnects from the currently connected device. Returns true if successful.

```java
public native boolean disconnectDevice();
```

### `cleanup()`

Releases all resources used by the library.

```java
public native void cleanup();
```

## 🎮 Java Side Integration

Your Java class should load the DLL and declare the native methods:

```java
public class BluetoothBLE {
    static {
        System.loadLibrary("BleInteract");
    }

    // Declare native methods here
    public native void initialize();
    public native ArrayList<BLEDevice> searchBLEDevices();
    public native boolean connectDevice(String deviceAddress);
    public native boolean initializeUARTCharacteristics(String uartServiceUuid, String rxUuid, String txUuid);
    public native boolean writeToRX(String data);
    public native boolean disconnectDevice();
    public native void cleanup();

    // Callback methods that will be called from C++
    public void onDeviceConnected(String message) {
        // Handle connection event
    }

    public void onDeviceDisconnected(String message) {
        // Handle disconnection event
    }

    public void onDeviceNotificationReceived(String data) {
        // Handle received data
    }
}
```

## 🚀 Example Usage

```java
BluetoothBLE ble = new BluetoothBLE();

// Initialize the BLE service
ble.initialize();

// Search for devices
ArrayList<BLEDevice> devices = ble.searchBLEDevices();

// Connect to a device
if (devices != null && !devices.isEmpty()) {
    boolean connected = ble.connectDevice(devices.get(0).getAddress());

    if (connected) {
        // Initialize UART service
        boolean initialized = ble.initializeUARTCharacteristics(
            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E", // UART service UUID
            "6E400002-B5A3-F393-E0A9-E50E24DCCA9E", // RX UUID
            "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // TX UUID
        );

        if (initialized) {
            // Send data to device
            ble.writeToRX("Hello from Java!");
        }
    }
}

// Clean up when done
ble.disconnectDevice();
ble.cleanup();
```

## 📂 Examples

Check out the `examples` folder in the repository to see working implementations of BLE-Interact. These examples demonstrate how to integrate the library into your Java applications and provide practical use cases for common BLE operations.

## 🛠️ Build Instructions

1. Ensure you have the Windows SDK installed
2. Build the DLL using Visual Studio or your preferred C++ compiler
3. Place the compiled DLL in your Java project's native library path

## 📝 Notes

- The library handles BLE connection status changes and notifies the Java application through callback methods
- The search function scans for devices for 6 seconds before returning results
- The library properly cleans up resources when disconnected or when the JVM is unloaded
- Error handling is implemented throughout the library for robustness

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

---

Made with ❤️ by imroodydev

```

```
