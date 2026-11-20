// Independent SSH implementation for BREAK acknowledgements. It does not link
// to DeuceSSH or include its headers. Build separately for optional audit checks.
package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"sync/atomic"
	"time"

	"golang.org/x/crypto/ssh"
)

type patternWriter struct {
	position int
	done     chan error
	finished bool
	failure  error
}

func (writer *patternWriter) Write(data []byte) (int, error) {
	const digits = "0123456789abcdef"
	if len(data) == 0 {
		return 0, nil
	}
	if len(data) > 262144-writer.position {
		writer.finish(fmt.Errorf("excess stdout"))
		return 0, writer.failure
	}
	for index, value := range data {
		if value != digits[(writer.position+index)%16] {
			writer.finish(fmt.Errorf("stdout mismatch"))
			return 0, writer.failure
		}
	}
	writer.position += len(data)
	if writer.position == 262144 {
		writer.finish(nil)
	}
	return len(data), nil
}

func (writer *patternWriter) finish(err error) {
	if err != nil {
		writer.failure = err
	}
	if !writer.finished {
		writer.finished = true
		writer.done <- err
	}
}

func run() error {
	if len(os.Args) != 4 {
		return fmt.Errorf("expected socket descriptor, private key and host blob")
	}
	descriptor, err := strconv.Atoi(os.Args[1])
	if err != nil || descriptor < 3 {
		return fmt.Errorf("invalid socket descriptor")
	}
	private, err := os.ReadFile(os.Args[2])
	if err != nil {
		return err
	}
	signer, err := ssh.ParsePrivateKey(private)
	clear(private)
	if err != nil {
		return err
	}
	algorithmSigner, ok := signer.(ssh.AlgorithmSigner)
	if !ok {
		return fmt.Errorf("fixture signer lacks algorithm selection")
	}
	signer, err = ssh.NewSignerWithAlgorithms(algorithmSigner, []string{ssh.KeyAlgoRSASHA256})
	if err != nil {
		return err
	}
	blob, err := os.ReadFile(os.Args[3])
	if err != nil {
		return err
	}
	file := os.NewFile(uintptr(descriptor), "break-peer")
	if file == nil {
		return fmt.Errorf("invalid socket file")
	}
	connection, err := net.FileConn(file)
	closed := file.Close()
	if err != nil {
		return err
	}
	defer connection.Close()
	if closed != nil {
		return closed
	}
	if err = connection.SetDeadline(time.Now().Add(45 * time.Second)); err != nil {
		return err
	}
	var hostChecks atomic.Uint32
	config := &ssh.ClientConfig{
		User: "audit", Auth: []ssh.AuthMethod{ssh.PublicKeys(signer)},
		ClientVersion: "SSH-2.0-GoSSH_audit", HostKeyAlgorithms: []string{ssh.KeyAlgoRSASHA256},
		HostKeyCallback: func(_ string, _ net.Addr, key ssh.PublicKey) error {
			if !bytes.Equal(key.Marshal(), blob) {
				return fmt.Errorf("host pin mismatch")
			}
			hostChecks.Add(1)
			return nil
		},
		Config: ssh.Config{
			KeyExchanges: []string{"curve25519-sha256"}, Ciphers: []string{"aes256-ctr"},
			MACs: []string{"hmac-sha2-256"}, RekeyThreshold: 65536,
		},
	}
	transport, channels, requests, err := ssh.NewClientConn(connection, "break.audit", config)
	if err != nil {
		return err
	}
	client := ssh.NewClient(transport, channels, requests)
	defer client.Close()
	session, err := client.NewSession()
	if err != nil {
		return err
	}
	defer session.Close()
	output := &patternWriter{done: make(chan error, 1)}
	var errorOutput bytes.Buffer
	session.Stdout, session.Stderr = output, &errorOutput
	input, err := session.StdinPipe()
	if err != nil {
		return err
	}
	terminalMode := os.Getenv("DSSH_TEST_TERMINAL_MODE")
	if terminalMode != "" {
		modes := ssh.TerminalModes{ssh.ECHO: 0}
		switch terminalMode {
		case "none":
		case "0":
			modes[42] = 0
		case "1":
			modes[42] = 1
		default:
			return fmt.Errorf("invalid terminal-mode fixture")
		}
		if err = session.RequestPty("dssh_iutf8", 29, 87, modes); err != nil {
			return err
		}
	}
	if err = session.Start("dssh-interop"); err != nil {
		return err
	}
	notificationResults := make([]map[string]interface{}, 0, 8)
	if os.Getenv("DSSH_TEST_NOTIFICATION_FLAGS") != "" {
		requests := []struct {
			name string
			payload []byte
		}{
			{"signal", ssh.Marshal(struct{ Signal string }{"TERM"})},
			{"window-change", ssh.Marshal(struct{ Columns, Rows, Width, Height uint32 }{87, 29, 696, 232})},
			{"exit-status", ssh.Marshal(struct{ Status uint32 }{0x10203040})},
			{"exit-signal", ssh.Marshal(struct {
				Signal string
				Core bool
				Error, Language string
			}{"TERM", true, "notification text", "en"})},
		}
		for _, request := range requests {
			for _, reply := range []bool{true, false} {
				accepted, requestError := session.SendRequest(request.name, reply, request.payload)
				if requestError != nil || accepted {
					return fmt.Errorf("notification %s reply=%t accepted=%t error=%v", request.name, reply, accepted, requestError)
				}
				notificationResults = append(notificationResults, map[string]interface{}{
					"name": request.name, "reply": reply, "accepted": accepted})
			}
		}
	}
	results := make([]map[string]interface{}, 0, 14)
	for _, duration := range []uint32{0, 1, 499, 500, 3000, 3001, ^uint32(0)} {
		for _, reply := range []bool{false, true} {
			var payload [4]byte
			binary.BigEndian.PutUint32(payload[:], duration)
			accepted, requestError := session.SendRequest("break", reply, payload[:])
			expected := reply && duration >= 500 && duration <= 3000
			if requestError != nil || accepted != expected {
				return fmt.Errorf("BREAK duration=%d reply=%t accepted=%t error=%v", duration, reply, accepted, requestError)
			}
			results = append(results, map[string]interface{}{"duration": duration, "reply": reply, "accepted": accepted})
		}
	}
	accepted, err := session.SendRequest("break-order-marker", true, nil)
	if err != nil || accepted {
		return fmt.Errorf("ordering marker: accepted=%t error=%v", accepted, err)
	}
	select {
	case completion := <-output.done:
		if completion != nil {
			return completion
		}
	case <-time.After(40 * time.Second):
		return fmt.Errorf("stdout did not complete")
	}
	if _, err = io.WriteString(input, "dssh-ack\n"); err != nil {
		return err
	}
	if err = input.Close(); err != nil {
		return err
	}
	err = session.Wait()
	exit, ok := err.(*ssh.ExitError)
	if !ok || exit.ExitStatus() != 23 {
		return fmt.Errorf("exit result: %v", err)
	}
	if output.failure != nil || output.position != 262144 || errorOutput.String() != "interop-stderr\n" || hostChecks.Load() < 2 {
		return fmt.Errorf("transfer/stderr/rekey verification failed")
	}
	return json.NewEncoder(os.Stdout).Encode(map[string]interface{}{
		"implementation": "golang.org/x/crypto/ssh", "cases": results, "terminal_mode": terminalMode,
		"notifications": notificationResults, "stdout_bytes": output.position,
		"remote_exit_status": exit.ExitStatus(), "host_checks": hostChecks.Load(), "valid": true,
	})
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
