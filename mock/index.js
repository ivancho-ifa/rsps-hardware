import mqtt from 'mqtt'
import readline from 'readline'

// Configuration from command-line args or defaults
const config = {
	trackerId: process.argv[2] || '001',
	updateInterval: parseInt(process.argv[3]) || 2000, // milliseconds
	startLat: parseFloat(process.argv[4]) || 42.6977, // Sofia, Bulgaria default
	startLng: parseFloat(process.argv[5]) || 23.3219,
}

// State
let state = {
	latitude: config.startLat,
	longitude: config.startLng,
	altitude: 550.0, // meters
	speed: 0.0, // km/h
	locked: false,
	moving: false,
	heading: 0, // degrees (0=North, 90=East, 180=South, 270=West)
}

// MQTT setup
const mqttTopic = `rsps/trackers/rsps-tracker-${config.trackerId}`
const client = mqtt.connect({
	protocol: 'mqtt',
	hostname: 'test.mosquitto.org',
	port: 1883,
})

client.on('connect', () => {
	console.log(`✓ Connected to MQTT broker`)
	console.log(`✓ Publishing to topic: ${mqttTopic}`)
	console.log(`✓ Tracker ID: ${config.trackerId}`)
	console.log(`✓ Update interval: ${config.updateInterval}ms`)
	console.log(`✓ Starting position: ${state.latitude}, ${state.longitude}`)
	console.log('\n' + '='.repeat(60))
	console.log('Commands:')
	console.log('  l - Toggle lock/unlock')
	console.log('  m - Toggle movement (simulates riding)')
	console.log('  p - Set position (lat,lng)')
	console.log('  q - Quit')
	console.log('='.repeat(60) + '\n')

	startPublishing()
	startInteractiveMode()
})

client.on('error', (err) => {
	console.error('MQTT Error:', err)
	process.exit(1)
})

// Publish GPS data periodically
function startPublishing() {
	setInterval(() => {
		if (state.moving) {
			updatePosition()
		}

		const message = {
			latitude: parseFloat(state.latitude.toFixed(6)),
			longitude: parseFloat(state.longitude.toFixed(6)),
			altitude: parseFloat(state.altitude.toFixed(1)),
			speed: parseFloat(state.speed.toFixed(1)),
			locked: state.locked,
		}

		client.publish(mqttTopic, JSON.stringify(message))

		const status = [
			`Status: ${state.locked ? '🔒 LOCKED' : '🔓 UNLOCKED'}`,
			state.moving ? '🚴 MOVING' : '⏸️  STOPPED',
			`Speed: ${state.speed.toFixed(1)} km/h`,
			`Pos: ${state.latitude.toFixed(6)}, ${state.longitude.toFixed(6)}`,
		].join(' | ')

		// Overwrite same line
		process.stdout.write('\r' + status)
	}, config.updateInterval)
}

// Update position based on speed and heading
function updatePosition() {
	if (state.speed === 0) return

	// Calculate distance traveled in this interval (km)
	const timeHours = config.updateInterval / (1000 * 60 * 60)
	const distanceKm = state.speed * timeHours

	// Convert to degrees (approximately)
	// 1 degree latitude ≈ 111 km
	// 1 degree longitude ≈ 111 km * cos(latitude)
	const latChange = (distanceKm / 111) * Math.cos(state.heading * Math.PI / 180)
	const lngChange = (distanceKm / (111 * Math.cos(state.latitude * Math.PI / 180))) *
	                  Math.sin(state.heading * Math.PI / 180)

	state.latitude += latChange
	state.longitude += lngChange

	// Add small random variations to make it more realistic
	state.latitude += (Math.random() - 0.5) * 0.00001
	state.longitude += (Math.random() - 0.5) * 0.00001
	state.altitude += (Math.random() - 0.5) * 0.5
	state.speed += (Math.random() - 0.5) * 0.5
	state.speed = Math.max(0, state.speed) // Don't go negative
}

// Interactive command-line interface
function startInteractiveMode() {
	const rl = readline.createInterface({
		input: process.stdin,
		output: process.stdout,
		prompt: ''
	})

	// Set up raw mode for single key presses
	readline.emitKeypressEvents(process.stdin)
	if (process.stdin.isTTY) {
		process.stdin.setRawMode(true)
	}

	process.stdin.on('keypress', (str, key) => {
		if (key.ctrl && key.name === 'c') {
			process.exit(0)
		}

		switch (key.name) {
			case 'l':
				state.locked = !state.locked
				console.log(`\n${state.locked ? '🔒 LOCKED' : '🔓 UNLOCKED'}`)
				break

			case 'm':
				state.moving = !state.moving
				if (state.moving) {
					state.speed = 15.0 // Default riding speed
					console.log('\n🚴 Started moving at 15 km/h')
				} else {
					state.speed = 0.0
					console.log('\n⏸️  Stopped')
				}
				break

			case 'p':
				process.stdin.setRawMode(false)
				rl.question('\nEnter position (lat,lng): ', (answer) => {
					const parts = answer.split(',')
					if (parts.length === 2) {
						const lat = parseFloat(parts[0].trim())
						const lng = parseFloat(parts[1].trim())
						if (!isNaN(lat) && !isNaN(lng)) {
							state.latitude = lat
							state.longitude = lng
							console.log(`✓ Position set to ${lat}, ${lng}`)
						}
					}
					process.stdin.setRawMode(true)
				})
				break

			case 'q':
				console.log('\n\nShutting down...')
				client.end()
				process.exit(0)
				break
		}
	})
}

// Graceful shutdown
process.on('SIGINT', () => {
	console.log('\n\nShutting down...')
	client.end()
	process.exit(0)
})
