package main

import (
	"fmt"
)

func main() {
	timestampsPath := "/home/ekina/GolandProjects/DroptorGatt/timestamps.txt"
	gatt := NewGattServer("Go Gatt", timestampsPath)
	gatt.Start()
	defer gatt.StopAdvert()

	fmt.Println()
	for {
		fmt.Println("Select an operation (1-2)")
		fmt.Printf("\t1. Increment sync count, currently %d\n", gatt.SyncCount)
		fmt.Println("\t2. Decrement sync count to request time")

		var selectOption int
		_, err := fmt.Scan(&selectOption)
		if err != nil {
			fmt.Println("Bad option selection!")
		}

		switch selectOption {
		case 1:
			gatt.SyncCount++
			gatt.StopAdvert()
			gatt.ConfigureAdvertSyncStamp()
			gatt.StartAdvert()
			break
		case 2:
			gatt.SyncCount--
			gatt.StopAdvert()
			gatt.ConfigureAdvertSyncStamp()
			gatt.StartAdvert()
			break
		}
	}
}
