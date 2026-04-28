
# Remote Access Server

<p align="center">
	<img src="https://img.shields.io/badge/status-active-brightgreen" alt="Status">
	<img src="https://img.shields.io/badge/platform-linux%20%7C%20windows-blue" alt="Platform">
</p>

---


## Overview

This project provides a **Remote Access Server** with a web-based control panel and secure SSH tunneling capabilities. The idea is to make it easy to manage locally deployed lightweight virtual machines (VMs) that do not have a graphical user interface (GUI). Users can queue, monitor, and manage tunnel commands for remote clients via a user-friendly web interface.

---

## 🌐 Web Server Features

- **Clients Page:** View all clients that have communicated with the server, including hostname and internal IP.
- **Client Commands:** Queue and release commands for each client. Commands must be released before execution. Status updates after execution.
- **Web Interface:** Access via [https://localhost](https://localhost)

---

## 🔐 Tunnel Commands

### 1. Open SSH Tunnel
**Purpose:**
> Opens an SSH tunnel from the remote client to a specified remote host and port, forwarding a local port on the client to a remote port on the server. An SSH server is started on the client for secure connections.

**Parameters:**
- `L` (Local Port): Port on the client for the SSH server.
- `R` (Remote Port): Port on the remote server to forward to.

**Usage Example:**
```sh
ssh -i local_server_key client_user@127.0.0.1 -p R
```

---

### 2. Close SSH Tunnel
**Purpose:**
> Closes the active SSH tunnel previously opened for the client.

**Parameters:** None

---

### 3. Open TCP Tunnel
**Purpose:**
> Opens a TCP tunnel from the remote client, forwarding a specified local port to a remote port on the server for secure TCP connections.

**Parameters:**
- `L` (Local Port): Port on the client to forward.
- `R` (Remote Port): Port on the remote server to forward to.

---

### 4. Close TCP Tunnel
**Purpose:**
> Closes the active TCP tunnel previously opened for the client.

**Parameters:** None

---

### 5. Open Dynamic Tunnel
*To be implemented.*

### 6. Close Dynamic Tunnel
*To be implemented.*

---


## ⚠️ Disclaimer

> **Warning:** This project is under ongoing development and is intended for research and educational purposes only. It should **only be used locally**, as several important security measures are not implemented yet. **Do not use in production environments or for any malicious or unauthorized activities.** The authors take no responsibility for misuse.

---

## 🚀 Usage


### 1. Install Web Server (Linux)

```sh
sudo chmod +x webserver/setup.sh
sudo webserver/setup.sh
```

After installation, access the web server at: [https://127.0.0.1](https://127.0.0.1)

**Note:** The required SSH keys for running `client.exe` will be automatically copied to the `ssh_keys` folder during web server installation.

---

### 2. Compile and Run Client (Windows)

TODO

**SSH Key Requirements:**
- `host_key`: ssh private key acting as the host key of the local SSH server. 
- `key`: SSH private key for connecting to the remote SSH server.
- `local_server_key.pub`: Public key of the local SSH server.

**Run the client:**

```sh
client.exe [IP]
```

Where `[IP]` is the address of the remote web server.