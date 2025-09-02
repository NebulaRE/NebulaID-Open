# NebulaID-Open

**NebulaID-Open** is the open-source, educational version of the **NebulaID** project.
It demonstrates how to collect stable hardware identifiers in **Windows user-mode** and generate a deterministic **SHA-256 hash** that can be used as a Hardware ID (HWID).

This repository focuses on **transparency**: showing *what data is collected* and *how a basic HWID can be generated*.
The proprietary version used inside Nebula’s licensed software includes additional security layers and closed-source logic.

## 🔍 Features (Open)

* Collects hardware identifiers from:
  * Motherboard (serial, system UUID)
  * CPU (CPUID, brand string)
  * Disk (physical drive serial)
  * BIOS (serial, SMBIOS UUID)
  * Network adapters (physical MACs, filtered and ordered)
* Filters out:
  * Generic values (`00000000`, `123456789`, `To be filled by O.E.M.`)
  * Loopback and virtual adapters
* Detects common virtualization platforms (VMware, Hyper-V, VirtualBox, etc.)
* Generates a **deterministic SHA-256 hash** from valid sources
  * Optional HMAC-SHA256 support
* No persistence: HWID is generated **on-demand**, never stored locally

## 🛠️ Building

Requirements:

* Windows 10/11 SDK
* Visual Studio 2019/2022
* C++17 or later

Link against:

```
wbemuuid.lib
ole32.lib
oleaut32.lib
iphlpapi.lib
bcrypt.lib
advapi32.lib
```

Build & run the example app (`NebulaID.cpp`) to see collected values and the generated HWID.

## ⚠️ Notice

This project is **educational** and intended for **transparency**.
It demonstrates how hardware identifiers can be collected and combined into a deterministic hash in Windows user-mode.

The following components are **not included** in this repository:

* **Proprietary HMAC salts or secret keys** – used in Nebula’s internal license system.
* **Anti-fraud mechanisms** – such as anti-debugging, anti-hooking, DLL injection detection, and tamper checks.
* **Server-side validation logic** – including challenge/response, license revocation, anomaly detection, and secure communication protocols.
* **Obfuscation and integrity protection** – applied to the proprietary builds to resist reverse engineering.
* **Per-product variations** – different rules and redundancy strategies applied to each licensed Nebula software.

The **commercial version of NebulaID**, used internally in Nebula’s licensed products, extends this open version with multiple closed-source layers of protection to ensure **resilience, tamper resistance, and secure server-side validation**.

This repository exists solely to provide **educational value** and **transparency** about what types of hardware data are collected, and does not represent the full security architecture of Nebula’s licensing system.

## 📜 License

Distributed under the **MIT License**.
See [LICENSE](LICENSE.txt) for details.
