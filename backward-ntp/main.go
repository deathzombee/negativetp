package main

import (
	"encoding/binary"
	"log"
	"net"
	"time"
)

const (
	NTPPort        = ":123" // Use ":123" and run with sudo for real NTP
	NTPPacketSize  = 48
	NTPEpochOffset = 2208988800 // Seconds between 1900 and 1970
)

var (
	startWallTime = time.Now()
	startNTPTime  = float64(time.Now().Unix()) + NTP_EPOCH_OFFSET_FLOAT
)

const NTP_EPOCH_OFFSET_FLOAT = 2208988800.0

func main() {
	addr, err := net.ResolveUDPAddr("udp", NTPPort)
	if err != nil {
		log.Fatalf("resolve: %v", err)
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	defer conn.Close()

	log.Println("🌀 Reverse NTP server running on", NTPPort)

	buf := make([]byte, NTPPacketSize)
	for {
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err != nil || n != NTPPacketSize {
			continue
		}
		go handle(conn, clientAddr, buf)
	}
}

func handle(conn *net.UDPConn, addr *net.UDPAddr, req []byte) {
	resp := make([]byte, NTPPacketSize)
	resp[0] = 0x1C // LI = 0, Version = 3, Mode = 4 (server)
	resp[1] = 1    // stratum
	resp[2] = 4    // poll

	var precision int8 = -20
	resp[3] = byte(precision) // NTP precision
	// Root delay and dispersion set to small values
	binary.BigEndian.PutUint32(resp[4:8], 0x00004000)  // 16 ms root delay
	binary.BigEndian.PutUint32(resp[8:12], 0x00004000) // 16 ms root dispersion
	copy(resp[12:16], []byte("GO  "))                  // Reference ID

	elapsed := time.Since(startWallTime).Seconds()
	reversedTime := startNTPTime - elapsed
	seconds := uint32(reversedTime)
	fraction := uint32((reversedTime - float64(seconds)) * (1 << 32))

	putTimestamp(resp[16:], seconds, fraction) // reference
	copy(resp[24:], req[40:48])                // originate
	putTimestamp(resp[32:], seconds, fraction) // receive
	putTimestamp(resp[40:], seconds, fraction) // transmit

	_, _ = conn.WriteToUDP(resp, addr)
}

func putTimestamp(b []byte, seconds, fraction uint32) {
	b[0] = byte(seconds >> 24)
	b[1] = byte(seconds >> 16)
	b[2] = byte(seconds >> 8)
	b[3] = byte(seconds)
	b[4] = byte(fraction >> 24)
	b[5] = byte(fraction >> 16)
	b[6] = byte(fraction >> 8)
	b[7] = byte(fraction)
}
