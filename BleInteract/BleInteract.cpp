#pragma once
#include "pch.h"

#include <jni.h>
#include <Windows.h>
#include <BluetoothApis.h> 

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <iomanip>  
#include <sstream> 
#include <iostream>
#include <unordered_set>
#include <thread>          
#include <vector>
#include <optional>
#include <string>
#include <locale>
#include <windows.storage.streams.h>
#include <mutex>

using namespace winrt;
using namespace winrt::Windows::Storage::Streams;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
 

// To track if the search is ongoing
std::atomic<bool> isSearching{ false };

// Global variables to store BLE device and UART characteristics // using option will create copy //
std::shared_ptr<BluetoothLEDevice> connectedDevice = nullptr;
std::shared_ptr<GattCharacteristic> rxCharacteristic = nullptr;
std::shared_ptr<GattCharacteristic> txCharacteristic = nullptr;

// Global variables to store UART UUIDs
std::optional<winrt::guid> uartServiceGuid = std::nullopt;
std::optional<winrt::guid> rxUuid = std::nullopt;
std::optional<winrt::guid> txUuid = std::nullopt;

// Global variable to store the global reference to the Java object
jobject globalObj = nullptr;

// Struct to hold device information
struct DeviceInfo {
    std::wstring name;
    uint64_t address;
};

// Function to convert GUID to string (for UUIDs)
std::wstring GuidToString(const winrt::guid& guid)
{
    std::wstringstream ws;
    ws << std::hex << std::setfill(L'0') << std::setw(8) << guid.Data1
        << L"-" << std::setw(4) << guid.Data2
        << L"-" << std::setw(4) << guid.Data3
        << L"-" << std::setw(2) << (int)guid.Data4[0]
        << L"" << std::setw(2) << (int)guid.Data4[1]
        << L"-" << std::setw(2) << (int)guid.Data4[2]
        << L"" << std::setw(2) << (int)guid.Data4[3]
        << L"" << std::setw(2) << (int)guid.Data4[4]
        << L"" << std::setw(2) << (int)guid.Data4[5]
        << L"" << std::setw(2) << (int)guid.Data4[6]
        << L"" << std::setw(2) << (int)guid.Data4[7];
    return ws.str();
}

// Function to convert wstring (UTF-16) to string (UTF-8)
std::vector<uint8_t> WStringToUTF8Bytes(const std::wstring& wstr) {
    std::string utf8Str;
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed > 0) {
        utf8Str.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &utf8Str[0], size_needed, NULL, NULL);
    }
    return std::vector<uint8_t>(utf8Str.begin(), utf8Str.end());
}

void init() {
    std::locale::global(std::locale(""));

    std::wcout.imbue(std::locale());
    std::wcout << L"BluetoothBLE constructed" << std::endl;
}

// Function to save the global reference
void saveGlobalReference(JNIEnv* env, jobject javaObject) {
    if (globalObj == nullptr) {  // Only save if it's not already saved
        globalObj = env->NewGlobalRef(javaObject);
    }
}

// Function to get the saved global reference
jobject getGlobalReference() {
    return globalObj;  // Return the saved global reference
}

// Function to clean up the global reference
void cleanup(JNIEnv* env) {
    if (env == nullptr) {
        return; // Fail-safe: No valid environment
    }

    // Stop notification
    if (txCharacteristic)
    {
        try {
            auto status = txCharacteristic->WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::None
            ).get();

            if (status == GattCommunicationStatus::Success) {
                std::cout << "Notifications stopped successfully." << std::endl;
            }
            else {
                std::cerr << "Failed to stop notifications." << std::endl;
            }

            txCharacteristic.reset();
        }
        catch (const winrt::hresult_error& ex) {
            std::cerr << "WinRT Exception: " << winrt::to_string(ex.message()) << std::endl;
        }
        catch (const std::exception& ex) {
            std::cerr << "Standard Exception: " << ex.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception occurred while stopping notifications." << std::endl;
        }
    }

    // Stop receiving
    if (rxCharacteristic)
    {
        try {
            rxCharacteristic.reset();
        }
        catch (const winrt::hresult_error& ex) {
            std::cerr << "WinRT Exception: " << winrt::to_string(ex.message()) << std::endl;
        }
        catch (const std::exception& ex) {
            std::cerr << "Standard Exception: " << ex.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception occurred while stopping write operation." << std::endl;
        }
    }
    
    // Stop connection 
    if (connectedDevice) {
        connectedDevice->Close();
        connectedDevice.reset();
        connectedDevice = nullptr;
    }

    uartServiceGuid.reset();
    rxUuid.reset();
    txUuid.reset();
   
    if (globalObj != nullptr) {
        env->DeleteGlobalRef(globalObj);  // Delete the global reference when done
        globalObj = nullptr;  // Set to nullptr to avoid dangling reference
    }
}

// Calling java method from the class
void CallJavaMethod(JNIEnv* env, jobject javaObject, const char* methodName, const char* methodSig, const char* message) {
    try {
        if (env == nullptr) {
            std::cout << "env is null" << std::endl;
            return;
        }

        if (javaObject == nullptr) {
            std::cout << "javaObject is null" << std::endl;
            return;
        }


        jclass javaClass = env->GetObjectClass(javaObject);
        if (!javaClass) {
            std::cout << "Failed to find Java class." << std::endl;
            return;
        }

        jmethodID methodId = env->GetMethodID(javaClass, methodName, methodSig);
        if (!methodId) {
            std::cout << "Failed to find Java method: " << methodName << std::endl;
            env->DeleteLocalRef(javaClass);  // Release the local reference
            return;
        }

        // Convert the message to a Java string
        jstring javaMessage = env->NewStringUTF(message);
        if (!javaMessage) {
            std::cout << "Failed to create Java string from message." << std::endl;
            env->DeleteLocalRef(javaClass);  // Release the local reference
            return;
        }

        // Call the Java method
        env->CallVoidMethod(javaObject, methodId, javaMessage);

        // Delete local references
        env->DeleteLocalRef(javaMessage);
        env->DeleteLocalRef(javaClass);  // Don't forget to delete the class reference
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in CallJavaMethod: " << e.what() << std::endl;
    }
}

// Function to handle connection status change
void OnConnectionStatusChanged(JNIEnv* env, jobject javaObject, BluetoothLEDevice const& sender, winrt::Windows::Foundation::IInspectable const& args) {
    try {
        // Get connection status of the device
        auto status = sender.ConnectionStatus();

        if (status == BluetoothConnectionStatus::Disconnected) {
            connectedDevice.reset();
            rxCharacteristic.reset();
            txCharacteristic.reset();
            uartServiceGuid.reset();
            rxUuid.reset();
            txUuid.reset();

            std::wcout << L"Device disconnected. Checking last GATT error..." << std::endl;
            // auto error = sender.DeviceInformation().Pairing().ProtectionLevel();  // Example for pairing error
            // std::wcout << L"Last known error: " << static_cast<int>(error) << std::endl;
        }

        // Check connection status and print appropriate message
        std::wcout << L"Connection Status Changed: " << (status == BluetoothConnectionStatus::Connected ? L"Connected" : L"Disconnected") << std::endl;

        // Call Java `onDeviceDisconnected`
        if (status == BluetoothConnectionStatus::Connected) {
            CallJavaMethod(env, javaObject, "onDeviceConnected", "(Ljava/lang/String;)V", "Device Connected");
        }
        else {
            CallJavaMethod(env, javaObject, "onDeviceDisconnected", "(Ljava/lang/String;)V", "Device Disconnected");
        }

        // Get JavaVM from env
        JavaVM* jvm;
        env->GetJavaVM(&jvm);

        // Detach from the thread after use
        jvm->DetachCurrentThread();
    }
    catch (const winrt::hresult_error& e) {
        std::cerr << "Exception while handling connection status: " << e.message().c_str() << std::endl;
    }
}

// Function to initialize UART characteristics (RX, TX, etc.)
jboolean InitializeUARTCharacteristics(JNIEnv* env, jobject javaObject, winrt::guid uartServiceGuid, winrt::guid rxId, winrt::guid txId) {
    try {
        // Check if UART service and characteristics are already initialized
        if (connectedDevice == nullptr && rxCharacteristic == nullptr && txCharacteristic == nullptr) {
            std::wcout << "UART characteristics already initialized, reusing existing values." << std::endl;
            return JNI_TRUE;
        }

        // Store global variable
        txUuid = txId;
        rxUuid = rxId;

        // Connect to the GATT service asynchronously
        auto asyncOperation = connectedDevice->GetGattServicesForUuidAsync(uartServiceGuid);
        auto gattServiceResult = asyncOperation.get();  // Use .get() to block and get the result.
        auto services = gattServiceResult.Services();  // Save the services to a variable for readability.

        if (services.Size() == 0) {
            std::cerr << "UART service not found!" << std::endl;
            return JNI_FALSE;
        }

        // Get the characteristics for RX and TX UUIDs
        auto rxCharOperation = services.GetAt(0).GetCharacteristicsForUuidAsync(rxId);
        auto txCharOperation = services.GetAt(0).GetCharacteristicsForUuidAsync(txId);
        auto rxCharResult = rxCharOperation.get();
        auto txCharResult = txCharOperation.get();

        // Block and get RX characteristics & TX characteristics
        auto txChar = txCharResult.Characteristics();
        auto rxChar = rxCharResult.Characteristics();

        // Print all 
        for (uint32_t i = 0; i < txChar.Size(); ++i) {
            auto charUuid = txChar.GetAt(i).Uuid();
            std::wcout << L"Retrieved TX Characteristic UUID: " << GuidToString(charUuid) << std::endl;

            auto characteristic = txChar.GetAt(i);
            auto properties = characteristic.CharacteristicProperties();
            // std::wcout << L"TX Characteristic Properties: " << static_cast<int>(properties) << std::endl;

            if ((properties & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None) {
                std::wcout << L"TX Characteristic supports Notify!" << std::endl;
            }
        }

        if (rxChar.Size() == 0 || txChar.Size() == 0) {
            std::cerr << "Unable to find RX or TX characteristics!" << std::endl;
            return JNI_FALSE;
        }

        // Initialize the UART (enable notifications, etc.)
        rxCharacteristic = std::make_shared<GattCharacteristic>(rxChar.GetAt(0)); // This is where you'll write data to the micro:bit (RX)
        txCharacteristic = std::make_shared<GattCharacteristic>(txChar.GetAt(0)); // This is where you'll read data from the micro:bit (TX)


        if (globalObj == nullptr) {
            saveGlobalReference(env, javaObject);
        }

        // Get JavaVM from env
        JavaVM* jvm;
        env->GetJavaVM(&jvm);

        // Enable notifications for the TX characteristic (Micro:bit sending data)
        txCharacteristic->ValueChanged([env, jvm](GattCharacteristic sender, GattValueChangedEventArgs args) {
            try {
                // Log that the callback was triggered
                std::wcout << L"Indication received from TX characteristic!" << std::endl;

                // Read incoming data
                IBuffer dataBuffer = args.CharacteristicValue();
                uint32_t length = dataBuffer.Length();

                if (length == 0) {
                    std::wcout << L"Data buffer is empty." << std::endl;
                    return;
                }

                std::vector<uint8_t> dataBytes(length);
                DataReader::FromBuffer(dataBuffer).ReadBytes(dataBytes);

                // Convert bytes to a readable format (e.g., UTF-8 string)
                std::string receivedData(dataBytes.begin(), dataBytes.end());
                // std::cout << L"Data received from TX : " << receivedData << std::endl;


                // Attach the current thread to the JVM if needed
                JNIEnv* attachedEnv = nullptr;
                // Correct the type here by passing (void**)&attachedEnv
                if (jvm->AttachCurrentThread((void**)&attachedEnv, nullptr) != JNI_OK) {
                    std::cerr << "Failed to attach current thread to JVM" << std::endl;
                    return;
                }

                // Call Java `onDeviceNotificationReceived`
                CallJavaMethod(attachedEnv, globalObj, "onDeviceNotificationReceived", "(Ljava/lang/String;)V", receivedData.c_str());

                // Detach from the thread after use
                jvm->DetachCurrentThread();
            }
            catch (const winrt::hresult_error& e) {
                std::cerr << "Exception in indication callback: " << e.message().c_str() << std::endl;
            }
            });

        // Check if support notification 
        auto properties = txCharacteristic->CharacteristicProperties();
        if ((properties & GattCharacteristicProperties::Indicate) == GattCharacteristicProperties::None) {
            std::cerr << "TX characteristic does not support notifications!" << std::endl;
            return JNI_FALSE;
        }

        // Enable notifications on the TX characteristic
        GattClientCharacteristicConfigurationDescriptorValue config = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
        auto result = txCharacteristic->WriteClientCharacteristicConfigurationDescriptorAsync(config).get();

        if (SUCCEEDED(result)) {
            std::cout << "Notifications successfully enabled for TX characteristic!" << std::endl;
        }
        else {
            std::cerr << "Failed to enable notifications for TX characteristic!" << std::endl;
            txCharacteristic.reset();
            rxCharacteristic.reset();
            return JNI_FALSE;
        }

        std::cout << "UART characteristics initialized successfully!" << std::endl;
        return JNI_TRUE;
    }
    catch (const winrt::hresult_error& e) {
        std::cerr << "Exception initializing UART characteristics: " << e.message().c_str() << std::endl;
        txCharacteristic.reset();
        rxCharacteristic.reset();
        return JNI_FALSE;
    }
}

// Initialize the class obj
extern "C" __declspec(dllexport) void JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_initialize(JNIEnv* env, jobject obj) {

    // Initialize the WinRT apartment for BLE operations
    init_apartment();

    // UTF8 for debugger
    // init();
}

// JNI function to initialize UART characteristics
extern "C" __declspec(dllexport) jboolean JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_initializeUARTCharacteristics(JNIEnv* env, jobject obj, jstring uartServiceUuidStr, jstring rxUuidStr, jstring txUuidStr) {
    try {
        std::wcout << L"JNI Init UART service." << std::endl;

        // Check for null inputs
        if (uartServiceUuidStr == nullptr || rxUuidStr == nullptr || txUuidStr == nullptr) {
            std::cerr << "Error: One or more UUID parameters are null." << std::endl;
            return JNI_FALSE; // Invalid input parameters
        }

        if (!connectedDevice) {
            std::wcout << "No device connected to " << std::endl;
            return JNI_FALSE;
        }

        // Initialize the WinRT apartment for BLE operations
        init_apartment();

        const char* uartServiceUuidChar = env->GetStringUTFChars(uartServiceUuidStr, nullptr);
        std::wstring uartServiceUuid(uartServiceUuidChar, uartServiceUuidChar + strlen(uartServiceUuidChar));
        env->ReleaseStringUTFChars(uartServiceUuidStr, uartServiceUuidChar);

        const char* rxUuidChar = env->GetStringUTFChars(rxUuidStr, nullptr);
        std::wstring rxUuid(rxUuidChar, rxUuidChar + strlen(rxUuidChar));
        env->ReleaseStringUTFChars(rxUuidStr, rxUuidChar);

        const char* txUuidChar = env->GetStringUTFChars(txUuidStr, nullptr);
        std::wstring txUuid(txUuidChar, txUuidChar + strlen(txUuidChar));
        env->ReleaseStringUTFChars(txUuidStr, txUuidChar);

        // Convert string UUIDs to GUIDs
        winrt::guid uartServiceGuid(uartServiceUuid);
        winrt::guid rxGuid(rxUuid);
        winrt::guid txGuid(txUuid);

        // Check if a device is already connected
        if (connectedDevice) {
            std::cout << "Initializing UART Service" << std::endl;

            // Initialize UART Characteristics on the connected device
            return InitializeUARTCharacteristics(env, obj, uartServiceGuid, rxGuid, txGuid);
        }
        else {
            std::cerr << "No device connected!" << std::endl;
            return JNI_FALSE;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while initializing UART characteristics: " << e.what() << std::endl;
        return JNI_FALSE;
    }
}

// Function to search all BLE Devices its a 15 seconds search
extern "C" __declspec(dllexport) jobject JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_searchBLEDevices(JNIEnv* env, jobject obj) {
    try { 
        if (env == nullptr) {
            return nullptr; // Fail-safe: No valid environment
        }

        // Check if a search is already in progress , and automatically set to true
        if (isSearching.exchange(true)) {
            // Atomically set the flag to true if it was false
            std::wcout << L"Search already in progress. Please wait." << std::endl;
            return nullptr; // Exit if another search is in progress
        }

        // Check if a device is connect
        if (connectedDevice) {
            connectedDevice->Close();
            connectedDevice.reset();
            std::cout << "Device disconnected cause of searching." << std::endl;
        }

        // BLE watcher setup
        BluetoothLEAdvertisementWatcher watcher;

        // BLE scanning mode
        watcher.ScanningMode(BluetoothLEScanningMode::Active);
 
        // Set to store Bluetooth addresses of devices we have already found
        std::unordered_set<uint64_t> seenAddresses;
        // Store device Info
        std::vector<DeviceInfo> devices;

        // Event on bluetooth ble device found
        watcher.Received([&seenAddresses, &devices](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args) {
            try {
                // Check if devices is not null
                if (&devices == nullptr) {
                    return;
                }

                // Bluetooth address
                uint64_t deviceAddress = args.BluetoothAddress();

                if (!seenAddresses.insert(deviceAddress).second) {
                    return; // Skip duplicate address
                }

                // Get the device name
                std::wstring deviceName = args.Advertisement().LocalName().empty() ? L"Unknown Device" : args.Advertisement().LocalName().c_str();

                // Add the device to the list
                devices.push_back({ deviceName, deviceAddress });
            }
            catch (const std::exception& e) {
                std::cerr << "Exception occurred: " << e.what() << std::endl;
            }
        });

        // Start scanning for BLE devices
        watcher.Start();

        // Searching bt
        std::wcout << "Searching BLE devcies..." << std::endl;

        // Wait for 15 seconds before automatically stopping the scan
        std::this_thread::sleep_for(std::chrono::seconds(6));

        std::wcout << "Terminating search after 6 seconds..." << std::endl;

        // Stop searching
        watcher.Stop();
       
        // Allow time for pending events to complete before processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        // Reset is searching
        isSearching.store(false);

        std::wcout << "Search terminated !!" << std::endl;

        // Check if devices list is empty, return null if it is
        if (devices.empty()) {
            return nullptr;  // Return null if no devices are found
        }
        else {
            // Get the BLEDevice class
            jclass deviceClass = env->FindClass("com/bitbybit/services/bluetooth/BLEDevice");
            if (!deviceClass) {
                std::cerr << "BLEDevice class not found!" << std::endl;
                return nullptr;
            }

            // Get BLEDevice Constructor
            jmethodID deviceConstructor = env->GetMethodID(deviceClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
            if (!deviceConstructor) {
                std::cerr << "BLEDevice constructor not found!" << std::endl;
                return nullptr;
            }

            // Create a Java ArrayList to hold the devices
            jclass arrayListClass = env->FindClass("java/util/ArrayList");
            jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
            jobject arrayList = env->NewObject(arrayListClass, arrayListConstructor);

            jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");


            // Update the loop where devices are added to the ArrayList
            for (const auto& device : devices) {
                // Convert the address to a string
                std::wstringstream addressStream;
                addressStream << std::hex << device.address; // Convert to hexadecimal format
                std::wstring addressWString = addressStream.str();

                // Convert the wide string to a Java string
                jstring deviceAddressStr = env->NewString((const jchar*)addressWString.c_str(), (jsize)addressWString.size());
                if (env->ExceptionCheck()) {
                    std::cerr << "JNI Exception while creating device address string" << std::endl;
                    return nullptr;
                }

                jstring deviceNameStr = env->NewString((const jchar*)device.name.c_str(), (jsize)device.name.size());
                if (env->ExceptionCheck()) {
                    env->DeleteLocalRef(deviceAddressStr);
                    std::cerr << "JNI Exception while creating device name string" << std::endl;
                    return nullptr;
                }


                // Create a new Device object
                jobject deviceObject = env->NewObject(deviceClass, deviceConstructor, deviceNameStr, deviceAddressStr);

                // Add the Device object to the ArrayList
                env->CallBooleanMethod(arrayList, env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z"), deviceObject);

                // Clean up local references
                env->DeleteLocalRef(deviceNameStr);
                env->DeleteLocalRef(deviceAddressStr);
                env->DeleteLocalRef(deviceObject);
            }

            // Return the ArrayList to Java
            return arrayList;
        }

        return nullptr;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;

        // Reset is searching
        isSearching.store(false);
        return nullptr;
    }
    catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;

        // Reset is searching
        isSearching.store(false);
        return nullptr;
    }
}

// Function to connect to the Bluetooth device by address
extern "C" __declspec(dllexport) jboolean JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_connectDevice(JNIEnv* env, jobject obj, jstring deviceAddressStr) {
    try {
        // Check for null inputs
        if (deviceAddressStr == nullptr) {
            std::cerr << "Error: One or more UUID parameters are null." << std::endl;
            return JNI_FALSE; // Invalid input parameters
        }

        // Convert Java String (jstring) to std::wstring
        const jchar* rawString = env->GetStringChars(deviceAddressStr, nullptr);
        if (rawString == nullptr) {
            std::cerr << "Error: Failed to retrieve string chars from device address." << std::endl;
            return JNI_FALSE; // Handle failed string conversion
        }

        std::wstring deviceAddressWString(rawString, rawString + env->GetStringLength(deviceAddressStr)); // Correctly construct wstring
        env->ReleaseStringChars(deviceAddressStr, rawString);

        // Convert std::wstring to uint64_t for the Bluetooth address
        uint64_t deviceAddress = std::stoull(deviceAddressWString, nullptr, 16); // Hexadecimal address

        // Check if the device is already connected and reset if needed
        if (connectedDevice != nullptr) {
            std::wcout << L"Disconnecting from the current device before trying to connect to a new one." << std::endl;
            connectedDevice->Close();  // Close any previous connection
            connectedDevice.reset();   // Reset the connection state
        }

        // Retrieve the BLE device using its Bluetooth address
        auto asyncOperation = BluetoothLEDevice::FromBluetoothAddressAsync(deviceAddress);
        auto bleDevice = asyncOperation.get(); // Wait for the async operation to complete
        
        // Print the device connection status
        // std::wcout << "Device status:" << static_cast<int>(bleDevice.ConnectionStatus()) << std::endl;

        // Check if the device is found
        if (bleDevice != nullptr) {
            // Set Connected device
            connectedDevice = std::make_shared<BluetoothLEDevice>(bleDevice); // Use shared_ptr to avoid copying

            if (globalObj == nullptr) {
                saveGlobalReference(env, obj);
            }

            // Get JavaVM from env
            JavaVM* jvm;
            env->GetJavaVM(&jvm);
          
            // Subscribe to connection status change
            connectedDevice->ConnectionStatusChanged([env, jvm](auto&& sender, auto&& args) {
                // Attach the current thread to the JVM if needed
                JNIEnv* attachedEnv = nullptr;
                // Correct the type here by passing (void**)&attachedEnv
                if (jvm->AttachCurrentThread((void**)&attachedEnv, nullptr) != JNI_OK) {
                    std::cerr << "Failed to attach current thread to JVM" << std::endl;
                    return;
                }

                // Call the event handler safely
                OnConnectionStatusChanged(attachedEnv, globalObj, sender, args);

                // Detach from the thread after use
                jvm->DetachCurrentThread();
            });        

            // Check connection status
            if (connectedDevice->ConnectionStatus() == BluetoothConnectionStatus::Connected) {
                std::wcout << L"Device is already connected." << std::endl;
                return JNI_TRUE;
            }
            else {
                std::wcout << L"Attempting to connect..." << std::endl;

                // Attempt to discover GATT services
                auto gattServices = connectedDevice->GetGattServicesAsync().get(); // Wait for GATT services discovery

                // Check if the device is connected or reachable
                if (bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
                    std::cerr << "Device is off or unreachable!" << std::endl;
                    return JNI_FALSE; // Handle device being off or unreachable
                }

                // Check if device is already connected
                if (gattServices.Status() == GattCommunicationStatus::Success) {
                    auto services = gattServices.Services();
                    std::wcout << L"Device connected successfully. Services available: " << services.Size() << std::endl;
                    return JNI_TRUE;
                }
                else {
                    std::wcerr << L"Failed to discover GATT services. Status: " << static_cast<int>(gattServices.Status()) << std::endl;
                    connectedDevice->Close();
                    connectedDevice.reset();
                    return JNI_FALSE;
                }
            }
        }
        else {
            std::cerr << "Failed to connect to device!" << std::endl;
            connectedDevice.reset();
            return JNI_FALSE;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while connecting to device: " << e.what() << std::endl;
        // This could involve resetting the event handler or other cleanup, depending on your API
        connectedDevice.reset();
        return JNI_FALSE;
    }
}

// Function to disconnect the device
extern "C" __declspec(dllexport) jboolean JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_disconnectDevice(JNIEnv* env, jobject obj) {
    try {
        if (connectedDevice != nullptr) {
            connectedDevice->Close();
            connectedDevice.reset();
 
            cleanup(env);

            std::cout << "Device disconnected successfully." << std::endl;
            return JNI_TRUE;
        }
        else {
            std::cerr << "No device connected to disconnect!" << std::endl;
            return JNI_FALSE;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while disconnecting the device: " << e.what() << std::endl;
        return JNI_FALSE;
    }
}

// Function to write data to the RX characteristic
extern "C" __declspec(dllexport) jboolean JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_writeToRX(JNIEnv* env, jobject obj, jstring dataStr) {
    try {
        if (!connectedDevice) {
            std::cerr << "No device connected!" << std::endl;
            return JNI_FALSE;
        }
        else if (!rxCharacteristic) {
            std::cerr << "No rxCharacteristic connected!" << std::endl;
            return JNI_FALSE;
        }

        // Convert the input jstring to std::wstring
        const char* dataChar = env->GetStringUTFChars(dataStr, nullptr);
        if (!dataChar) {
            std::cerr << "Failed to retrieve string characters!" << std::endl;
            return JNI_FALSE;
        }
        std::wstring data(dataChar, dataChar + strlen(dataChar));
        env->ReleaseStringUTFChars(dataStr, dataChar);

        // Convert std::wstring to UTF-8 encoded std::vector<uint8_t>
        std::vector<uint8_t> messageBytes = WStringToUTF8Bytes(data);  // Convert to bytes

        // Convert the data to bytes using DataWriter
        DataWriter writer;
        writer.WriteBytes(messageBytes);
        IBuffer buffer = writer.DetachBuffer();

        // Check if the buffer is empty
        if (buffer.Length() == 0) {
            std::cerr << "Error: IBuffer is empty. No data to send to RX." << std::endl;
            return JNI_FALSE;
        }

        // Write the data to the RX characteristic (micro:bit receives here)
        // auto writeResult = rxCharacteristic.value().WriteValueAsync(buffer).get();
        auto writeResult = rxCharacteristic->WriteValueAsync(buffer, GattWriteOption::WriteWithResponse).get();

        if (SUCCEEDED(writeResult)) {
            std::wcout << "Data written to RX: " << data << std::endl;
        }
        else {
            std::cerr << "Failed to write data to RX!" << std::endl;
            return JNI_FALSE;
        }

        return JNI_TRUE;
    }
    catch (const winrt::hresult_error& e) {
        std::cerr << "Exception while writing to RX: " << e.message().c_str() << std::endl;
        return JNI_FALSE;
    }
}

// Cleanup to release all threaths
extern "C" __declspec(dllexport) void JNICALL Java_com_bitbybit_services_bluetooth_BluetoothBLE_cleanup(JNIEnv* env, jobject obj) {
    cleanup(env);
    std::wcout << L"Bluetooth searvice cleaned up successfully." << std::endl;
}

// Automatically calling unload when the class is unloaded 
extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::wcout << L"Unloading BTE-Intercat service." << std::endl;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return; // Exit if unable to get JNI environment
    }

    // Perform cleanup
    cleanup(env);
}