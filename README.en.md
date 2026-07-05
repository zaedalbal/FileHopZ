# FileHopZ

[Русский](README.md) | [English](README.en.md)

FileHopZ is a lightweight tool for direct file transfer over a local network.
It is built around a simple workflow: run the receiver on one machine, run the
sender on another, and transfer files, directories, and symlinks directly
without an intermediate server.

The project uses its own transfer protocol on top of UDP. FileHopZ handles data
delivery itself: it confirms received chunks, resends lost packets, and restores
the correct file order on the receiving side.

## Transfer And Security

Transferred data is encrypted after the connection is established. FileHopZ uses
X25519 for key exchange and ChaCha20-Poly1305 for encryption and integrity
checking.

Important: FileHopZ is intended for trusted local networks. It encrypts transfer
contents, but it does not currently verify the identity of the other side. This
means it should not be used as a replacement for SSH/SFTP or other tools with
full authentication, especially on untrusted networks.

## Requirements

* A compiler with **C++23** support
* **CMake 3.25+**
* **Git**
* **Boost 1.82+**
* **OpenSSL 3.0+**
* **spdlog 1.10+**
* **CLI11 2.3+**

### Installing Dependencies

**Ubuntu/Debian:**

```bash
sudo apt install git cmake g++ libboost-all-dev libssl-dev libspdlog-dev libcli11-dev
```

**Fedora:**

```bash
sudo dnf install git cmake gcc-c++ boost-devel openssl-devel spdlog-devel cli11-devel
```

**Arch Linux / Manjaro:**

```bash
sudo pacman -S git cmake base-devel boost openssl spdlog cli11
```

If your CMake version is older than 3.25, you can try changing
`CMakeLists.txt`.

## Build

```bash
mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

cmake --build .
```

After a successful build, the executable will be created in the `build`
directory.

## Installation

```bash
./install.sh
```

The script builds the Release version and installs `filehopz` to
`/usr/local/bin/filehopz`. If writing to `/usr/local/bin` requires administrator
permissions, the script uses `sudo`.

To remove the installed binary:

```bash
./uninstall.sh
```

## Usage

```bash
Sender:   filehopz send <address> <port> <file/directory>
Receiver: filehopz recv [--auto-accept] <port> <output_directory>
```

Detailed help is available with `filehopz --help`.

After startup, the receiver prints local IPv4 addresses with the selected port
in the `address:port` format. You can pass one of them to the sender. Before
receiving data, the receiver shows the transfer size and asks for confirmation:
`y` to accept, `n` to refuse. Use `--auto-accept` to accept automatically.

Local run example:

```bash
filehopz recv --auto-accept 9000 ./received
filehopz send 192.168.0.101 9000 ./my-directory
```

## Limitations

FileHopZ is currently designed for direct transfer inside a trusted local
network. It is not a P2P system with NAT traversal, discovery, or relay servers.

The project is not positioned as a production-ready security tool: it has
transport encryption, but it does not provide peer authentication, PKI, key
pinning, or protection against active handshake substitution.
