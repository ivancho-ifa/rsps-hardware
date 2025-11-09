# RSPS Mock Tracker

A Node.js-based GPS tracker simulator for testing the RSPS system without physical hardware.

## Features

- Publishes GPS data to the same MQTT broker as real hardware trackers
- Simulates realistic GPS movement at fixed speed (15 km/h)
- Interactive control via keyboard commands
- Supports lock/unlock states and movement detection
- Configurable tracker ID, update interval, and starting position

## Installation

```bash
npm install
```

## Usage

### Basic Usage

```bash
npm start
```

This starts a mock tracker with:
- Tracker ID: `001`
- Update interval: 2000ms (2 seconds)
- Starting position: Sofia, Bulgaria (42.6977, 23.3219)

### Custom Configuration

```bash
npm start <tracker-id> <update-interval-ms> <start-lat> <start-lng>
```

**Examples:**

```bash
# Tracker 002, update every 1 second, starting in New York
npm start 002 1000 40.7128 -74.0060

# Tracker 003, update every 5 seconds, starting in London
npm start 003 5000 51.5074 -0.1278

# Tracker 999, update every 500ms, starting in Tokyo
npm start 999 500 35.6762 139.6503
```

## Interactive Commands

While the mock tracker is running, use these keyboard commands:

- **`l`** - Toggle lock/unlock state
- **`m`** - Toggle movement (auto-sets speed to 15 km/h when starting)
- **`p`** - Set position as lat,lng (prompts for input)
- **`q`** - Quit the simulator

## Data Format

The mock tracker publishes JSON messages matching the hardware format:

```json
{
  "latitude": 42.697700,
  "longitude": 23.321900,
  "altitude": 550.0,
  "speed": 15.5,
  "locked": false
}
```

Published to topic: `rsps/trackers/rsps-tracker-<id>`

## Testing Scenarios

### Test Locked Bike Alarm

1. Start the mock tracker: `npm start`
2. Press `l` to lock the bike
3. Press `m` to start movement
4. The tracker will publish data showing `locked: true` and `speed > 0`
5. The backend should detect this and relay to the frontend

### Test Multiple Trackers

Run multiple instances in separate terminals:

```bash
# Terminal 1
cd mock-tracker && npm start 001

# Terminal 2
cd mock-tracker && npm start 002

# Terminal 3
cd mock-tracker && npm start 003
```

## How It Works

- **Position Updates**: When moving, calculates new GPS coordinates based on fixed speed (15 km/h) and heading (default: North)
- **Realistic Variation**: Adds small random variations to simulate GPS drift
- **MQTT Publishing**: Connects to `test.mosquitto.org:1883` (same as hardware)
- **Real-time Display**: Shows current status in the terminal with live updates
