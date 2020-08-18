/**
 * @file RSPS-hardware.ino
 *
 * The "Ride safe, park safe" firmware's main file.
 *
 * Development plan for the "Ride safe, park safe" project:
 *
 *   Done:
 *     @todo Implement configuration mode
 *     @todo Collect data from GPS sensor
 *     @todo Display data in website
 *
 *   To be done:
 *     @todo Print data on display
 *     @todo Send data to an MQTT broker
 *     @todo Parse data from MQTT broker (server side)
 *     @todo Speed meter with a magnet detector
 */


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <SoftwareSerial.h>
#include <TinyGPS++.h>


using on_parsed_gps = void (*)();


TinyGPSPlus gps;
const int8_t gps_rx{D3}; ///< The pin connected to the TX pin of the GPS receiver.
const int8_t gps_tx{D4}; ///< The pin connected to the RX pin of the GPS receiver.
const uint32_t gps_baud{9600}; ///< The default baud rate of the GPS receiver.
SoftwareSerial gps_serial{gps_rx, gps_tx};

const String own_ssid_prefix{"rsps-tracker-"}; ///< This device's network SSID prefix when in soft AP mode.
String id{"000"}; ///< This device's ID.
ESP8266WebServer configuration_server; ///< The configuration configuration_server of this device.

const uint8_t button_0{D0}; ///< Button "0" of the device.
bool is_in_configuration_mode{false}; ///< The state of button "0".


void setup() {
	Serial.begin(115200);
	Serial.println();

	gps_serial.begin(gps_baud);

	pinMode(button_0, INPUT);
}


void loop() {
	if (button_pressed(button_0)) {
		is_in_configuration_mode = !is_in_configuration_mode;
		configuration_mode();
	}

	gps_parse(display_info);

	/// @todo Test how this is behaving on disconnecting the GPS receiver...
	if (millis() > 5000 && gps.charsProcessed() < 10)
		Serial.println("No GPS data received... check wiring or find a place with better signal");
}


bool button_pressed(uint8_t button_pin) {
	int button_state = digitalRead(button_pin);

	if (button_state == HIGH) {
		delay(200); ///< Avoid bouncing.

		return true;
	}
	else
		return false;
}


/**
 * Parses the data read from the GPS receiver and executes @p cb
 *
 * @param cb Executed when data is parsed.
 */

void gps_parse(const on_parsed_gps& cb) {
	while (gps_serial.available() > 0)
		if (gps.encode(gps_serial.read()))
			cb();
}


/**
 * Starts soft AP mode and runs a simple server which allows clients to
 * customize the ID of the tracker.
 */

void configuration_mode() {
	const String own_ssid{own_ssid_prefix + id};
	if (WiFi.softAP(own_ssid, "")) {
		Serial.println("Entered \"Client setup mode\"...");

		const IPAddress own_ip{WiFi.softAPIP()};
		/// @todo Display this on the device screen
		Serial.println("To configure the tracker:");
		Serial.printf("1. Connect to Wi-Fi network %s (no password)\n", own_ssid.c_str());
		Serial.print("2. Connect to IP ");
		Serial.println(own_ip);

		configuration_server.on("/", []() {
			const char* page =
				"<!DOCTYPE html>"
				"<html lang=\"en\">"
				"<head>"
					"<meta charset=\"UTF-8\">"
					"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
					"<title>Ride safe, park safe!</title>"
				"</head>"
				"<body>"
					"<form action=\"id\" enctype=\"application/x-www-form-urlencoded\" method=\"post\">"
						"<input type=\"text\" name=\"tracker-id\" id=\"tracker-id\">"
						"<input type=\"submit\" value=\"Set tracker ID\">"
					"</form>"
				"</body>"
				"</html>";
			configuration_server.send(200, "text/html", page);
		});
		configuration_server.on("/id", []() {
			if (configuration_server.method() == HTTP_POST) {
				if (configuration_server.hasArg("plain")) {
					const String& plain = configuration_server.arg("plain");
					const String tracker_id_prefix = "tracker-id=";
					/// Make sure that "plain" contains only one field - tracker-id.
					if (plain.startsWith(tracker_id_prefix)
						&& plain.lastIndexOf('=') == tracker_id_prefix.length() - 1) {
						id = plain.substring(tracker_id_prefix.length());

						const String confirmationMessage = "Successfully set tracker's ID to " + id;
						Serial.println(confirmationMessage);
						configuration_server.send(200, "text/plain", confirmationMessage);

						return; ///< Stop execution on success
					}
				}

				/// Handle parsing error
				const String errorMessage = "Failed to parse tracker-id field";
				Serial.println(errorMessage);
				configuration_server.send(400, "text/plain", errorMessage);
			}
			else {
				/// @todo Print name of method.
				const char* errorMessage = "Method Not Allowed";
				Serial.println(errorMessage);
				configuration_server.send(405, "text/plain", errorMessage);
			}
		});
		configuration_server.onNotFound([]() {
			configuration_server.send(404, "text/plain", "Not found");
		});

		configuration_server.begin();

		while (is_in_configuration_mode) {
			configuration_server.handleClient();

			if (button_pressed(button_0))
				is_in_configuration_mode = !is_in_configuration_mode;
		}

		Serial.println("Exiting \"Client setup mode\"...");
		configuration_server.stop();
	}
}


void display_info() {
	if (gps.location.isValid()) {
		Serial.print("Latitude: ");
		Serial.println(gps.location.lat(), 6);
		Serial.print("Longitude: ");
		Serial.println(gps.location.lng(), 6);

		if (gps.altitude.isValid()) {
			Serial.print("Altitude: ");
			Serial.println(gps.altitude.meters());
		}
		else
			Serial.println("Altitude: Not Available");
	}
	else
		Serial.println("Location: Not Available");

	Serial.print("Date: ");
	if (gps.date.isValid()) {
		Serial.print(gps.date.month());
		Serial.print("/");
		Serial.print(gps.date.day());
		Serial.print("/");
		Serial.println(gps.date.year());
	}
	else
		Serial.println("Not Available");

	Serial.print("Time: ");
	// Display time in HH:MM:SS.CC
	if (gps.time.isValid()) {
		if (gps.time.hour() < 10)
			Serial.print(F("0"));
		Serial.print(gps.time.hour());

		Serial.print(":");

		if (gps.time.minute() < 10)
			Serial.print(F("0"));
		Serial.print(gps.time.minute());

		Serial.print(":");

		if (gps.time.second() < 10)
			Serial.print(F("0"));
		Serial.print(gps.time.second());

		Serial.print(".");

		if (gps.time.centisecond() < 10)
			Serial.print(F("0"));
		Serial.println(gps.time.centisecond());
	}
	else
		Serial.println("Not Available");

	Serial.print("Speed: ");
	if (gps.speed.isValid())
		Serial.println(gps.speed.kmph());
	else
		Serial.println("Not Available");

	Serial.println();
	Serial.println();
	delay(1000);
}
