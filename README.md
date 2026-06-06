# Ephemeris Live

<img src="https://cdn.discordapp.com/attachments/1416155869686661234/1505585440281989230/icon.png" alt="Icon" width="400" height="400"> 

A lightweight, real-time astronomical tracking dashboard. Designed as a responsive "mission control" station for amateur astronomers, visual observers, and DIY telescope builders (specifically optimized for the Hadley 3D-printed telescope).

**Now powered by Native C++ and Raylib.**

Ephemeris Live has been completely rebuilt from the ground up to provide a high-performance experience. By migrating from the LÖVE (Lua) framework to a native C++ implementation, we have achieved lower CPU overhead, better memory management, and a more stable, responsive interface.

https://ephemeris-live.netlify.app/

---

## 🚀 What's New in the C++ Version
* **Native Performance:** Fully rewritten in C++ using the **Raylib** library for smooth, hardware-accelerated rendering.
* **Modern GUI System:** Utilizing `raygui-cpp` for a clean, modular, and responsive interface.
* **Optimized Data Pipeline:** Native data handling with `nlohmann/json` for lightning-fast parsing of telemetry from the **NASA/JPL Horizons API**.
* **Threaded Architecture:** Asynchronous data fetching ensures the UI remains buttery smooth while fetching celestial updates.

---

## Core Features
* **Asynchronous NASA/JPL Data Fetching:** Dedicated background threads fetch real-time celestial data without frame drops.
* **Custom Locations:** Enter your address in the specs tab for precision local tracking.
* **Live Planisphere Radar:** Real-time computation of celestial coordinates into an intuitive top-down sky map projection.
* **Tactical Night Mode:** One-click red-scale filter to preserve your dark adaptation.

---

## Building from Source

This is a native C++ project (C++17/20).

1. **Requirements:** - Visual Studio 2022+ (for Windows)
   - `vcpkg` (for dependency management)
2. **Setup:**
   - Clone the repository.
   - The project uses `vcpkg` to manage `raylib` and `nlohmann-json`.
3. **Compilation:**
   - Open the `/Ephemeris_Live.slnx` (or `.vcxproj`) file in Visual Studio.
   - Build the solution.

---

## ⚠️ Prototype Disclaimer ⚠️
This app is a prototype for a **Push-TO project** for manual telescopes (specifically the Hadley 3D-Printed Telescope). As a native C++ application, it is under active development. Expect improvements to tracking precision and UI fluidity in upcoming commits.

---

*Contributions, bug reports, and telescope-specific feature requests are highly encouraged!*
