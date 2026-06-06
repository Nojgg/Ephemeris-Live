// app.js
const PLANET_API = "http://localhost:8080/api";
const canvas = document.getElementById('skyCanvas');
const ctx = canvas.getContext('2d');
const statusDisplay = document.getElementById('status');

async function fetchWithTimeout(url, timeout = 3000) {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeout);
    const response = await fetch(url, { signal: controller.signal });
    clearTimeout(id);
    return response;
}

async function updateSky() {
    const selectedPlanet = document.getElementById('planetSelect').value;
    console.log("Starting fetch for:", selectedPlanet); // Check if this logs

    try {
        const response = await fetchWithTimeout(`${PLANET_API}?planet=${selectedPlanet}`);
        console.log("Response status:", response.status); // Check if this logs

        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);

        const data = await response.json();
        console.log("Data received:", data); // Check if this logs

        renderPlanet(data);
        statusDisplay.innerText = `Success: ${data.planet}`;
        statusDisplay.style.color = "green";
    } catch (error) {
        console.error("DEBUG ERROR:", error); // Check console for this
        statusDisplay.innerText = "Error: " + error.message; // See the error on screen
        statusDisplay.style.color = "red";
    }
}
function renderPlanet(data) {
    // Force clear and draw a test square
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 50, 50); // Does a red box appear in the top-left?

    console.log(`Drawing at X: ${x}, Y: ${y}`); // Check if these numbers are valid (0-800, 0-400)

    // ... rest of your code
}

// Start the loop
setInterval(updateSky, 5000);
updateSky(); // Initial call