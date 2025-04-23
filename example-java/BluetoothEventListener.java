
import java.util.List;

public interface BluetoothEventListener {
	void onDeviceDisconnected();

	void onDeviceConnected(String deviceId);

	void onSerialReceived(String message);

	void onDevicesDiscovered(List<BLEDevice> devices);

	void onDiscoveryFailed();

	void onDeviceFailConnect();
}