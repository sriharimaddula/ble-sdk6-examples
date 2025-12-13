package `in`.alfaleus.droptorcore.sync

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.util.isNotEmpty
import `in`.alfaleus.droptorcore.BleConstants
import `in`.alfaleus.droptorcore.LEPrefs
import inx.alfaleus.droptor.IAppCallback
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

class DroptorSync(private val context: Context, var uxCallback: IAppCallback?) :
  BluetoothGattCallback() {

  private val bluetoothManager = context.getSystemService(BluetoothManager::class.java)

  private val lePrefs = LEPrefs()
  private val macGattServer = lePrefs.macAddress

  private var currConnState: Int = BluetoothProfile.STATE_DISCONNECTED
  private var currGatt: BluetoothGatt? = null

  private var servicesDiscovered = false

  private var pendingWrites: Iterator<ByteArray>? = null

  enum class PendingWorkType {
    TIMESTAMP_SYNC,
    UPDATE_DROPTOR,
    NONE,
  }

  private var pendingWorkType = PendingWorkType.NONE

  private var unconfirmedSyncStamp: String? = null
  private var tsResReceivePacketIndex = 0

  private var stateChangeReceiverRegistered = false

  private val lastPerformedActionTime = AtomicLong(0L)
  private val alive = AtomicBoolean(true)

  private val handler = Handler(Looper.getMainLooper())

  private val idleConnectionChecker = object : Runnable {
    override fun run() {
      if (!alive.get()) {
        handler.removeCallbacks(this)
      }
      if (currGatt != null && currConnState == BluetoothProfile.STATE_CONNECTED) {
        val idleFor = System.currentTimeMillis() - lastPerformedActionTime.get()
        if (idleFor >= TimeUnit.SECONDS.toMillis(20)) {
          Log.d(TAG, "Idle connection! Closing any active connection")
          currGatt?.disconnect()
          currGatt = null
        }
      }
      handler.postDelayed(this, TimeUnit.SECONDS.toMillis(20))
    }
  }

  init {
    handler.postDelayed(idleConnectionChecker, TimeUnit.SECONDS.toMillis(25))
  }

  private fun logActivity() {
    lastPerformedActionTime.set(System.currentTimeMillis())
  }

  @OptIn(ExperimentalStdlibApi::class)
  private val scanCallback = object : ScanCallback() {
    override fun onScanResult(callbackType: Int, result: ScanResult) {
      val mfData = result.scanRecord?.manufacturerSpecificData
      if (mfData != null && mfData.isNotEmpty()) {
        val syncStamp = mfData.valueAt(0).toHexString()
        val ourSyncStamp = lePrefs.lastSyncStamp

        if (syncStamp > ourSyncStamp) {
          unconfirmedSyncStamp = syncStamp
          Log.d(TAG, "Timebook out of date, requesting update, new($syncStamp), old($ourSyncStamp)")

          pendingWorkType = PendingWorkType.TIMESTAMP_SYNC
          connect()
        } else if (syncStamp < ourSyncStamp) {
          Log.d(TAG, "Eye Droptor out of time sync! ours($ourSyncStamp) theirs($syncStamp)")
          pendingWorkType = PendingWorkType.UPDATE_DROPTOR
          connect()
        } else {
          Log.d(TAG, "Up to date ours($ourSyncStamp) theirs($syncStamp)...")
        }
        uxCallback?.syncRequired(ourSyncStamp, syncStamp)
      }
    }

    override fun onScanFailed(errorCode: Int) {
      // TODO: test when bluetooth is turned off
      Log.d(TAG, "onScanFailed with error code $errorCode")
    }
  }

  private val bluetoothStateChangeReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
      when (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
        BluetoothAdapter.STATE_OFF -> {
          // TODO: we may alert the user to turn it back on
          Log.d(TAG, "Bluetooth turned off, closing gatt")
          currGatt?.disconnect()
          currGatt = null
        }

        BluetoothAdapter.STATE_ON -> {
          Log.d(TAG, "Bluetooth turned on, starting scan")
          startWatch()
        }
      }
    }
  }

  fun startWatch() {
    // TODO: beware sometimes the scan may fail
    bluetoothManager.adapter.bluetoothLeScanner.startScan(
      listOf(ScanFilter.Builder().setDeviceAddress(macGattServer).build()),
      ScanSettings.Builder()
        .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
        .build(),
      scanCallback
    )
    if (!stateChangeReceiverRegistered) {
      context.registerReceiver(
        bluetoothStateChangeReceiver,
        IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
      )
      stateChangeReceiverRegistered = true
    }
    Log.d(TAG, "Starting BLE Gatt scan")
  }

  @OptIn(ExperimentalStdlibApi::class)
  private fun startTimebookSync() {
    currGatt?.apply {
      // now lets test it again
      val timestampResponseService = getService(BleConstants.TS_RES_SERVICE_UUID)
      if (timestampResponseService == null) {
        Log.d(TAG, "Timestamp response service is null!")
        return
      }
      enableNotifications(this, timestampResponseService.characteristics[0])
      uxCallback?.syncStarted()
    }
  }

  private fun connect() {
    when (currConnState) {
      BluetoothProfile.STATE_DISCONNECTED -> {
        currGatt = bluetoothManager.adapter.getRemoteDevice(macGattServer)
          .connectGatt(context, false, this)
      }

      BluetoothProfile.STATE_CONNECTED -> {
        Log.d(TAG, "connect() Gatt Server already connected")
      }

      else -> {
        Log.d(TAG, "connect() Bad currConnState $currConnState, not connecting")
      }
    }
  }

  override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
    currConnState = newState
    servicesDiscovered = false

    when (newState) {
      BluetoothProfile.STATE_CONNECTED -> {
        Log.d(TAG, "Connected to Gatt Server")
        gatt.discoverServices()
        uxCallback?.connected()
        logActivity()
      }

      BluetoothProfile.STATE_DISCONNECTED -> {
        Log.d(TAG, "Disconnected from Gatt Server")
        gatt.close()
        currGatt = null
        uxCallback?.disconnected()
      }
    }
  }

  override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
    if (status == BluetoothGatt.GATT_SUCCESS) {
      Log.d(TAG, "Services were discovered, work_mode $pendingWorkType")
      servicesDiscovered = true

      when (pendingWorkType) {
        PendingWorkType.TIMESTAMP_SYNC -> {
          pendingWorkType = PendingWorkType.NONE
          startTimebookSync()
        }

        PendingWorkType.UPDATE_DROPTOR -> {
          pendingWorkType = PendingWorkType.NONE
          updateDroptor(gatt)
        }

        else -> {
          // write the next pending packet
          pendingWrites?.let {
            if (it.hasNext()) {
              startSequentialWrite()
            }
          }
        }
      }

      uxCallback?.servicesDiscovered()
    } else {
      Log.d(TAG, "Failed to discover Gatt Services...")
    }
  }

  @OptIn(ExperimentalStdlibApi::class)
  override fun onDescriptorWrite(
    gatt: BluetoothGatt,
    descriptor: BluetoothGattDescriptor,
    status: Int
  ) {
    logActivity()
    if (status == BluetoothGatt.GATT_SUCCESS) {
      if (descriptor.uuid == BleConstants.CCC_UUID) {
        writeTimebookRequest(gatt)
      }
    } else {
      Log.e(
        TAG,
        "Failed to write descriptor for ${descriptor.characteristic.uuid}, status: $status"
      )
    }
  }

  private fun updateDroptor(gatt: BluetoothGatt) {
    val clockResponseService = gatt.getService(BleConstants.UPDATE_SERVICE)
    if (clockResponseService == null) {
      Log.e(TAG, "Clock response service is null! Aborting")
      return
    }
    val clockResponseCharac = clockResponseService.characteristics[0]
    if (clockResponseCharac == null) {
      Log.d(TAG, "Clock response characteristic is null! Aborting")
      return
    }
    write(
      gatt,
      clockResponseCharac,
      ByteBuffer.allocate(4)
        .putInt(System.currentTimeMillis().toUInt().toInt())
        .array()
    )
  }

  @OptIn(ExperimentalStdlibApi::class)
  private fun writeTimebookRequest(gatt: BluetoothGatt) {
    val timestampRequestService = gatt.getService(BleConstants.TS_REQ_SERVICE_UUID)
    if (timestampRequestService == null) {
      Log.d(TAG, "Timestamp request service is null!")
      return
    }
    tsResReceivePacketIndex = 0
    write(
      gatt,
      timestampRequestService.characteristics[0],
      lePrefs.lastSyncStamp.hexToByteArray()
    )
  }


  override fun onCharacteristicWrite(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic,
    status: Int
  ) {
    logActivity()
    val serviceUuid = characteristic.service.uuid
    if (serviceUuid == BleConstants.CONFIGURE_SERVICE_UUID) {
      pendingWrites?.let {
        if (it.hasNext()) {
          write(gatt, characteristic, it.next())
        } else {
          Log.d(TAG, "All configure packets written to Gatt!")
          uxCallback?.wroteConfig()
        }
      }
    } else if (serviceUuid == BleConstants.TS_REQ_SERVICE_UUID) {
      Log.d(TAG, "Update request was sent to Gatt Server")
      uxCallback?.syncStarted()
    }
  }

  private var totalTsResTimestamps: Int = 0
  private var receivedTsResTimestamps = mutableListOf<Long>()

  override fun onCharacteristicChanged(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic,
    value: ByteArray
  ) {
    logActivity()
    val serviceUuid = characteristic.service.uuid
    if (serviceUuid == BleConstants.TS_RES_SERVICE_UUID) {
      val buffer = ByteBuffer.wrap(value)
      buffer.order(ByteOrder.BIG_ENDIAN)
      if (tsResReceivePacketIndex == 0) {
        // it's a header packet, contains number of the total timestamps
        totalTsResTimestamps = buffer.int
        receivedTsResTimestamps.clear()
        Log.d(TAG, "Received response header packet, total stamps $totalTsResTimestamps")
      } else {
        while (buffer.hasRemaining() && receivedTsResTimestamps.size < totalTsResTimestamps) {
          val stamp = buffer.int.toLong() and 0xFFFFFFFFL
          receivedTsResTimestamps += stamp
          Log.d(TAG, "Received a stamp $stamp")
        }
        if (receivedTsResTimestamps.size == totalTsResTimestamps) {
          updateTimestampsToLocalDb()
        }
      }
      tsResReceivePacketIndex++
    }
  }

  private fun updateTimestampsToLocalDb() {
    lePrefs.lastSyncStamp = unconfirmedSyncStamp!!
    unconfirmedSyncStamp = null
    Log.d(TAG, "Timebook sync complete, received $totalTsResTimestamps timestamps")

    val longEntries = receivedTsResTimestamps.map { TimeUnit.SECONDS.toMillis(it) }.toLongArray()
    context.sendBroadcast(
      Intent("in.alfaleus.droptorcore.action_receive_remote_entry")
        .putExtra("remote_timestamps", longEntries)
        .also {
          it.component = ComponentName(
            "in.alfaleus.droptor",
            "in.alfaleus.droptor.receivers.RemoteEntryReceiver"
          )
        }
    )
    uxCallback?.syncComplete(longEntries)
  }

  fun configure(minutesOfDay: IntArray) {
    val packets = mutableListOf<ByteArray>()

    // packet containing length of reminders, and current time in seconds
    packets += ByteBuffer.allocate(8)
      .putInt(minutesOfDay.size)
      .putInt((System.currentTimeMillis() / 1000).toUInt().toInt())
      .array()

    // take 5 reminders at once to fit packet size of 20 bytes
    minutesOfDay.asList().chunked(5).forEach { batch ->
      val packet = ByteBuffer.allocate(batch.size * 4)
      batch.forEach { minuteOfDay -> packet.putInt(minuteOfDay) }
      packets += packet.array()
    }
    pendingWrites = packets.iterator()
    if (currConnState == BluetoothProfile.STATE_CONNECTED) {
      startSequentialWrite()
    } else {
      connect()
    }
  }

  /**
   * Writes the first packet, others are written sequentially
   * in the onCharacteristicWrite() block
   */
  private fun startSequentialWrite() {
    currGatt?.apply {
      val configService = getService(BleConstants.CONFIGURE_SERVICE_UUID)
      if (configService != null) {
        write(this, configService.characteristics[0], pendingWrites?.next()!!)
      } else {
        Log.e(TAG, "Cannot write configure packets, config service not found!")
      }
    }
  }

  fun close() {
    bluetoothManager.adapter.bluetoothLeScanner.stopScan(scanCallback)
    currGatt?.disconnect()
    currGatt?.close()
    context.unregisterReceiver(bluetoothStateChangeReceiver)
    alive.set(false)
    handler.removeCallbacks(idleConnectionChecker)
  }

  private fun write(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic,
    value: ByteArray
  ) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
      gatt.writeCharacteristic(
        characteristic,
        value,
        BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
      )
    } else {
      characteristic.value = value
      gatt.writeCharacteristic(characteristic)
    }
  }

  private fun enableNotifications(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic
  ) {
    if (gatt.setCharacteristicNotification(characteristic, true)) {
      val descriptor = characteristic.getDescriptor(BleConstants.CCC_UUID)
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
      } else {
        descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        gatt.writeDescriptor(descriptor)
      }
    }
  }

  companion object {
    private const val TAG = "DroptorSync"

  }
}