package main

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"os"
	"time"

	"tinygo.org/x/bluetooth"
)

type GattServer struct {
	name    string
	adapter *bluetooth.Adapter
	advert  *bluetooth.Advertisement

	SyncCount      uint32
	TimestampsPath string
}

func NewGattServer(name string, timestampsPath string) *GattServer {
	return &GattServer{
		name:           name,
		adapter:        bluetooth.DefaultAdapter,
		advert:         bluetooth.DefaultAdapter.DefaultAdvertisement(),
		SyncCount:      0,
		TimestampsPath: timestampsPath,
	}
}

func (s *GattServer) Start() {
	must("enable BLE stack", s.adapter.Enable())
	s.adapter.SetConnectHandler(connectHandler)
	s.ConfigureAdvertSyncStamp()
	s.StartAdvert()

	must("add handshake service", s.adapter.AddService(&bluetooth.Service{
		UUID: parseUuid(HandshakeServiceUuid),
		Characteristics: []bluetooth.CharacteristicConfig{
			{
				UUID:  parseUuid(HandshakeCharacUuid),
				Flags: bluetooth.CharacteristicWritePermission,
				WriteEvent: func(client bluetooth.Connection, offset int, value []byte) {
					fmt.Printf("Received configure write, size: %d\n", len(value))
				},
			},
		},
	}))
	var timestampResponseCharac bluetooth.Characteristic
	must("add timestamp response service", s.adapter.AddService(&bluetooth.Service{
		UUID: parseUuid(TimestampResponseServiceUuid),
		Characteristics: []bluetooth.CharacteristicConfig{
			{
				UUID:   parseUuid(TimestampResponseCharacUuid),
				Value:  []byte{},
				Flags:  bluetooth.CharacteristicNotifyPermission,
				Handle: &timestampResponseCharac,
			},
		},
	}))
	must("add timestamp request service", s.adapter.AddService(&bluetooth.Service{
		UUID: parseUuid(TimestampRequestServiceUuid),
		Characteristics: []bluetooth.CharacteristicConfig{
			{
				UUID:  parseUuid(TimestampRequestCharacUuid),
				Flags: bluetooth.CharacteristicWritePermission,
				WriteEvent: func(client bluetooth.Connection, offset int, value []byte) {
					fromStampIndex := binary.BigEndian.Uint32(value)
					fmt.Printf("Received timestamp request, size: %d, from_stamp_index: %d\n", len(value), fromStampIndex)
					go s.writeTimestampsToClient(fromStampIndex, timestampResponseCharac)
				},
			},
		},
	}))
	must("add update service", s.adapter.AddService(&bluetooth.Service{
		UUID: parseUuid(UpdateServiceUuid),
		Characteristics: []bluetooth.CharacteristicConfig{
			{
				UUID:  parseUuid(UpdateCharacUuid),
				Flags: bluetooth.CharacteristicWritePermission,
				WriteEvent: func(client bluetooth.Connection, offset int, value []byte) {
					clockTimeSeconds := binary.BigEndian.Uint32(value)
					fmt.Printf("Received clock time in seconds: %d", clockTimeSeconds)
				},
			},
		},
	}))

	address, err := s.adapter.Address()
	if err != nil {
		panic(err)
	}
	fmt.Printf("Starting Gatt Server (%s)...\n", address.String())
}

func (s *GattServer) writeTimestampsToClient(fromStampIndex uint32, responseCharac bluetooth.Characteristic) {
	timestamps := s.readTimestampsFromFile()
	totalTimestamps := uint32(len(timestamps))

	// Send the total number of timestamps first (4 bytes)
	sizeBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(sizeBuf, totalTimestamps-fromStampIndex)
	_, err := responseCharac.Write(sizeBuf)
	if err != nil {
		fmt.Println("Error writing total size:", err)
		return
	}
	const chunkSize = 5
	for i := fromStampIndex; i < totalTimestamps; i += chunkSize {
		time.Sleep(1 * time.Second)
		end := i + chunkSize
		if end > totalTimestamps {
			end = totalTimestamps
		}

		// take 5 timestamps at once, so max packet size is 20 bytes (MTU)
		packet := make([]byte, 4*(end-i))
		for j, tsIndex := 0, i; tsIndex < end; j, tsIndex = j+1, tsIndex+1 {
			fmt.Println("Writing timestamp:", timestamps[tsIndex])
			binary.BigEndian.PutUint32(packet[j*4:(j+1)*4], timestamps[tsIndex])
		}

		fmt.Println("Packet size:", len(packet))
		if _, err := responseCharac.Write(packet); err != nil {
			panic(err)
			return
		}
	}
}

func (s *GattServer) readTimestampsFromFile() []uint32 {
	file, err := os.Open(s.TimestampsPath)
	if err != nil {
		panic(err)
	}
	defer file.Close()

	layout := "02/01/2006 15:04"
	scanner := bufio.NewScanner(file)
	loc, _ := time.LoadLocation("Local")

	var unixTimestamps []uint32
	for scanner.Scan() {
		line := scanner.Text()
		t, err := time.ParseInLocation(layout, line, loc)
		if err != nil {
			fmt.Printf("Skipping invalid timestamp %q: %v\n", line, err)
			continue
		}
		unixTs := t.Unix()
		unixTimestamps = append(unixTimestamps, uint32(unixTs))
	}
	return unixTimestamps
}

func (s *GattServer) StartAdvert() {
	must("start advert", s.advert.Start())
}

func (s *GattServer) StopAdvert() {
	_ = s.advert.Stop()
}

func (s *GattServer) ConfigureAdvertSyncStamp() {
	syncCountBuff := make([]byte, 4)
	binary.BigEndian.PutUint32(syncCountBuff, s.SyncCount)

	must("configure advert", s.advert.Configure(bluetooth.AdvertisementOptions{
		LocalName: s.name,
		ManufacturerData: []bluetooth.ManufacturerDataElement{
			{
				CompanyID: 0,
				Data:      syncCountBuff,
			},
		},
		Interval: 8000,
	}))
}

func connectHandler(device bluetooth.Device, connected bool) {
	if connected {
		fmt.Println("Client connected:", device.Address.String())
		// TODO: simulate requesting clock time from Android
	} else {
		fmt.Println("Client disconnected:", device.Address.String())
	}
}

func parseUuid(uuid string) bluetooth.UUID {
	parsedUuid, err := bluetooth.ParseUUID(uuid)
	if err != nil {
		panic(err)
	}
	return parsedUuid
}

func must(action string, err error) {
	if err != nil {
		println("crash: failed to " + action)
		panic(err)
	}
}
