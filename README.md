<div align="center">

# 🌿 Greenhouse — IoT Sensor Network

** · Mobile and Embedded Computing · UCLouvain 2023–2024**

![Contiki](https://img.shields.io/badge/Contiki--NG-IoT-4CAF50?style=flat)
![Cooja](https://img.shields.io/badge/Simulator-Cooja-0077FF?style=flat)
![Language](https://img.shields.io/badge/Language-C%20%2B%20Python-FA7343?style=flat)
![Solo](https://img.shields.io/badge/Improved%20version-Solo%20redo-lightgrey?style=flat)

*A wireless sensor network simulating a smart greenhouse — individually reimplemented and improved from an initial group submission.*

</div>

---

## 📋 Overview

This project simulates a multi-greenhouse IoT environment using **Contiki-NG** and the **Cooja** network simulator. A hierarchy of motes (gateway, subgateways, sensors) communicate wirelessly to automate irrigation and lighting based on real-time sensor data, managed by an external Python server.

The full design report is available here: [`report.pdf`](./report.pdf)

---

## 🏗️ Architecture

```
[Python Server]
      |  TCP / Serial Socket
[Gateway]  ←──────────────────────────────┐
      |  Broadcast / Multicast             │
[Subgateway 1]         [Subgateway 2]      │
   ├── Light Sensor       ├── Light Sensor │
   ├── Bulb               ├── Bulb         │
   ├── Bulb               ├── Bulb         │
   └── Irrigation         └── Irrigation   │
         └── [Mobile Terminal] ────────────┘
```

---

## ✨ Key Features

### 🔗 Network & Routing
- Multi-greenhouse support via subgateway hierarchy
- **Parent selection** using RSSI signal strength + device type priority
- **Backoff timer** to avoid message collisions during discovery
- Dynamic **routing tables** per subgateway with device type awareness
- Multicast messaging from gateway to multiple subgateways simultaneously

### 💓 Heartbeat Mechanism
- All sensor nodes periodically send heartbeats to their subgateway
- Devices that go out of range are automatically removed from the routing table
- Reconnection supported within a configurable `CHECK_INTERVAL`

### 📱 Mobile Terminal
- Connects exclusively to a light sensor parent
- Performs RSSI-based parent selection across greenhouses
- Exchanges Z maintenance messages then shuts down gracefully

### 🌱 Automation Logic (Server-side)
- Light sensor value > 50 → server triggers **bulbs ON** for a random duration
- Server sends periodic **irrigation start** commands to all greenhouses
- Acknowledgments received and logged per greenhouse

---

## 🗂️ Code Structure

```
├── gateway.c          # Central gateway — bridges server and subgateways
├── subgateway.c       # Per-greenhouse coordinator with routing table
├── light.c            # Light sensor — sends luminosity values (0–100)
├── bulb.c             # Bulb — turns on for 5s on server command
├── irrigation.c       # Irrigation — activates for 2s, sends ACK
├── server.py          # Python server — TCP connection to gateway
├── helper.h / .c      # Shared packet structure & utility functions
└── Makefile
```

---

## 🚀 Running the Simulation

**Prerequisites:** Contiki-NG, Cooja simulator, Docker, Python 3

**1. Get the Docker container IP**
```bash
docker ps
docker inspect <container_name>   # look for "IPAddress", e.g. 172.17.0.2
```

**2. Open Cooja and set up the simulation**
- Create the **Gateway mote first** (it gets the root address `0100.0000.0000.0000`)
- Enable **Serial Socket Server** on the gateway (e.g. port `60001`)
- Add remaining motes (subgateways, sensors, etc.)
- Set simulation speed to `x1`

**3. Launch the Python server**
```bash
python3 server.py --ip 172.17.0.2 --port 60001
```

**4. Start the simulation** in Cooja

---

## ⚠️ Known Limitations

- **Depth-1 routing only** — all sensor nodes must be directly connected to a subgateway; no multi-hop forwarding
- Fixed message structure (type/node/id codes) not suited for multi-hop extension
- Max 4 devices per subgateway (1 light, 2 bulbs, 1 irrigation)

---

## 📄 Report

The full written analysis — covering design decisions, protocol details, simulation results, and proposed improvements — is available in [`report.pdf`](./report.pdf).

---

<div align="center">

UCLouvain · Academic year 2023–2024 · Pierre B.*

</div>
