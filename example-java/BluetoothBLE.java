
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

/**
 * A class to manage Bluetooth Low Energy (BLE) device operations.
 */
public class BluetoothBLE {

	// Load the native library containing JNI methods.
	static {
		System.loadLibrary("BleInteract");
	}

	// Class-level scheduler instance
	private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

	// Event listener for custom callbacks.
	private BluetoothEventListener eventListener;

	// UUIDs for UART service and characteristics.
	private String uartService = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
	private String txCharacteristic = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
	private String rxCharacteristic = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

	// Connected device
	private String connectedDevice;

	/**
	 * Object constructor
	 */
	public BluetoothBLE() {
		new Thread(() -> {
			try {
				this.initialize();
				System.out.println("Apartment initialized successfully");
			} catch (Exception e) {
				e.printStackTrace();
			}
		}).start();
	}

	/** Get connected device */
	public String getConnectedDevice() {
		return connectedDevice;
	}

	/**
	 * Initialize object
	 */
	private native boolean initialize();

	/**
	 * Searches for available BLE devices.
	 */
	private native List<BLEDevice> searchBLEDevices();

	/**
	 * Connects to a BLE device by its address.
	 */
	private native boolean connectDevice(String deviceAddress);

	/**
	 * Disconnects from the currently connected BLE device.
	 */
	public native boolean disconnectDevice();

	/**
	 * Initializes UART characteristics for communication.
	 */
	private native boolean initializeUARTCharacteristics(String uartServiceId, String rxUUID, String txUUID);

	/**
	 * Writes a message to the RX characteristic.
	 */
	private native boolean writeToRX(String message);

	/**
	 * Cleans up native resources.
	 */
	public native void cleanup();

	/**
	 * Handles device disconnection event.
	 */
	private void onDeviceDisconnected(String message) {
		connectedDevice = null;
		System.out.println("Notification: " + message);
		if (this.eventListener != null) {
			this.eventListener.onDeviceDisconnected();
		}
	}

	/**
	 * Handles data notification event.
	 */
	private void onDeviceNotificationReceived(String message) {
		System.out.println("Data received: " + message);
		if (this.eventListener != null) {
			this.eventListener.onSerialReceived(message);
		}
	}

	/**
	 * Handles device connection event.
	 */
	private void onDeviceConnected(String message) {
		//
		System.out.println("Notification: " + message);
	}

	/**
	 * Registers an event listener for custom event handling.
	 */
	public void setEventListener(BluetoothEventListener listener) {
		this.eventListener = listener;
	}

	/**
	 * Initializes UART services and characteristics for communication.
	 */
	public void initializeServices(String service, String transferId, String receiveId) {
		this.uartService = service;
		this.txCharacteristic = transferId;
		this.rxCharacteristic = receiveId;
	}

	/**
	 * Sends a message using the RX characteristic.
	 */
	public boolean sendSerialMessage(String message) {
		if (connectedDevice == null) {
			if (this.eventListener != null) {
				this.eventListener.onDeviceDisconnected();
			}
			return false;
		}

		if (rxCharacteristic != null) {
			return writeToRX(message + "\n"); // Sends message
		} else {
			return false; // Returns false if RX characteristic is null
		}
	}

	/**
	 * Discovers available BLE devices.
	 */
	public void discoverBleDevices() {
		Thread task = new Thread(() -> {
			List<BLEDevice> devices = this.searchBLEDevices();

			// Filter devices where the name includes "BBC"
			List<BLEDevice> filtered = devices.stream()
					.filter(device -> device.getName() != null && device.getName().contains("micro")).toList();

			if (filtered != null) {
				if (eventListener != null) {
					scheduler.schedule(() -> eventListener.onDevicesDiscovered(filtered), 1000, TimeUnit.MILLISECONDS);
				}
			} else {
				if (eventListener != null) {
					eventListener.onDiscoveryFailed(); // Notify failure
				}
			}
		});

		// task.setDaemon(true);
		task.start();
	}

	/**
	 * Connect to device by id
	 */
	public void connectToDevice(String deviceId) {
		Thread task = new Thread(() -> {
			boolean connected = this.connectDevice(deviceId);
			boolean servicesConnected = this.initializeUARTCharacteristics(this.uartService, this.rxCharacteristic,
					this.txCharacteristic);

			scheduler.schedule(() -> {
				if (connected && servicesConnected) {
					this.connectedDevice = deviceId;
					this.eventListener.onDeviceConnected(deviceId);
				} else {
					this.eventListener.onDeviceFailConnect();
				}
			}, 1000, TimeUnit.MILLISECONDS);
		});

		// task.setDaemon(true);
		task.start();
	}

	/**
	 * Clean the BLEService threads avoiding it hanging in the background when not
	 * needed
	 */
	public void clear() {
		this.disconnectDevice();
		this.cleanup();
	}

	// Main method for testing the BluetoothBLE functionality.
	public static void main(String[] args) {
		try {
			BluetoothBLE ble = new BluetoothBLE();

			// Phase 1: Discover devices
			System.out.println("Phase 1: Discovering devices...");
			List<BLEDevice> devices = ble.searchBLEDevices();
			if (devices != null) {
				for (BLEDevice device : devices) {
					System.out.println("Device: " + device.getName() + ", ID: " + device.getId());
				}
			} else {
				System.out.println("No devices found.");
			}

			// Phase 2: Connect to a device
			System.out.println("\nPhase 2: Connecting to device...");
			boolean connected = ble.connectDevice("c3d2aefc2745");
			System.out.println("Connected: " + connected);
			Thread.sleep(2000);

			// Phase 3: Initialize UART characteristics
			System.out.println("\nPhase 3: Initializing UART...");
			boolean initialized = ble.initializeUARTCharacteristics(
					ble.uartService, ble.rxCharacteristic, ble.txCharacteristic);
			System.out.println("Initialized: " + initialized);
			Thread.sleep(2000);

			// Phase 4: Send messages
			System.out.println("\nPhase 4: Sending messages...");
			for (int i = 0; i < 5; i++) {
				Thread.sleep(2000);
				ble.sendSerialMessage("INIT_GET\n");
			}
			Thread.sleep(2000);

			// Phase 5: Disconnect and cleanup
			System.out.println("\nPhase 5: Disconnecting and cleaning up...");
			ble.disconnectDevice();
			ble.cleanup();

		} catch (Exception e) {
			e.printStackTrace(); // Print exception details
		}
	}

	private List<BLEDevice> test() {
		List<BLEDevice> testate = new ArrayList<>();
		for (int i = 0; i < 50; i++) {
			BLEDevice device = new BLEDevice("Test", "" + i);
			testate.add(device);
		}
		return testate;
	}
}
