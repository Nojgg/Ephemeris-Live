# Ephemeris Live

<img src="https://cdn.discordapp.com/attachments/1416155869686661234/1505585440281989230/icon.png?ex=6a0b2935&is=6a09d7b5&hm=606c12e58ffa1f0025441f1169a52b77770f034c8cd455aa5e3face3c7a1251d&" alt="Icon" width="300" height="200">   

A lightweight, real-time astronomical tracking dashboard built using the LÖVE framework (Lua). It’s designed to act as a responsive "mission control" station for amateur astronomers, visual observers, or anyone using a manual setup like the Hadley 3D-printed telescope.

Instead of heavy background assets or massive local databases, the app queries the **NASA/JPL Horizons API** asynchronously over lightweight HTTP threads to get mathematically precise coordinate trends, tracking profiles, and visual data for targets in our solar system.

![Dashboard Preview](https://cdn.discordapp.com/attachments/1416155869686661234/1505567083667132527/image.png?ex=6a0b181c&is=6a09c69c&hm=2310635e6e56564d0069b748b390d72033eeea1bca7d187b2d8ddf78658b78d2&) 

---

## Features

* **Asynchronous NASA/JPL Data Fetching:** Spawns dedicated background threads to fetch real-time celestial data from the Horizons database without stalling or causing frame drops in the main rendering thread.
* **Auto-Location Detection:** Uses a fast, non-blocking IP lookup to grab your approximate latitude and longitude on startup so your local Alt/Az coordinates, rise times, and transit schedules are accurate immediately.
* **Custom Locations:** Enter your own adress in the spec tab.
* **Live Planisphere Radar View:** Computes and translates raw celestial coordinates into an intuitive top-down local sky map projection.
* **Simulated Newtonian Eyepiece Reticle:** Features a custom stencil-masked eyepiece simulator. It scales magnification based on your current scope and eyepiece focal lengths, simulating inverted optics and moon/planet configurations.
* **Tactical Night Mode:** Toggles a red-scale color profile across all drawing matrices to preserve your eyes' dark adaptation when using a laptop or screen out in the field.

---

## Keyboard Controls & Shortcuts

* **`N`** - Toggle Red-scale filter
* **`R`** - Reset time offset back to current real-world UTC time
* **`Left Arrow` / `Right Arrow`** - Shift time tracking backwards or forwards by 1 hour increments to predict upcoming transits
* **`+` / `-`** - Increase or decrease the simulated eyepiece focal length to test out different magnifications fluidly

---

## Running Locally

To run the project, you simply need to install the exe file in the release page 

### Cloned Repository Execution
1. Clone this repository down to your machine:
   ```bash
   git clone [https://github.com/Nojgg/Ephemeris-Live.git](https://github.com/Nojgg/Ephemeris-Live.git)
   cd ephemeris-live
   ```
