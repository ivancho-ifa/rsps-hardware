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
 *     @todo Print data on display
 *
 *   To be done:
 *     @todo Send data to an MQTT broker
 *     @todo Parse data from MQTT broker (server side)
 *     @todo Display data in website
 *     @todo Speed meter with a magnet detector
 */


// ESP8266 libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// GPS libraries
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// OLED display libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 ///< OLED display width, in pixels
#define SCREEN_HEIGHT 64 ///< OLED display height, in pixels


using on_parsed_gps = void (*)();


TinyGPSPlus gps;
const int8_t gps_rx{D3}; ///< The pin connected to the TX pin of the GPS receiver.
const int8_t gps_tx{D4}; ///< The pin connected to the RX pin of the GPS receiver.
const uint32_t gps_baud{9600}; ///< The default baud rate of the GPS receiver.
SoftwareSerial gps_serial{gps_rx, gps_tx};

String own_ssid;
const String own_ssid_prefix{"rsps-tracker-"}; ///< This device's network SSID prefix when in soft AP mode.
String id{"000"}; ///< This device's ID.
ESP8266WebServer configuration_server; ///< The configuration configuration_server of this device.

const uint8_t button_0{D0}; ///< Button "0" of the device.
bool is_in_configuration_mode{false}; ///< The state of button "0".

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); ///< Display connected to the I2C pins


void setup() {
	Serial.begin(115200);
	Serial.println();

	display.begin(SSD1306_SWITCHCAPVCC, 0x3C); ///< I2C address 0x3C. Change if collision with another sensor
	delay(2000); ///< Leaving time for the display to initialize
	display.clearDisplay();

	display.setTextSize(1);
	display.setTextColor(WHITE);

	println("Ride safe, park safe!", 0, 0);
	display.display();
	delay(5000);

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
	if (millis() > 5000 && gps.charsProcessed() < 10) {
		display.clearDisplay();
		println("No GPS data received... check wiring or find a place with better signal", 0, 0);
		display.display();
	}
}


void print(const char* message) {
	Serial.print(message);
	display.print(message);
}

void println(const char* message) {
	Serial.println(message);
	display.println(message);
}

void println(const char* message, int16_t x, int16_t y) {
	display.setCursor(x, y);
	print(message);
}

void println(const String& message) {
	println(message.c_str());
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
	own_ssid = own_ssid_prefix + id;
	if (WiFi.softAP(own_ssid, "")) {
		display.clearDisplay();
		println("Entered \"Client setup mode\"...", 0, 0);
		display.display();
		delay(5000); ///< Leave time for the user to read the message

		print_configuration_mode_info();

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

						const String confirmation_message = "Successfully set tracker's ID to " + id;
						print_message_in_configuration_mode(confirmation_message);

						configuration_server.send(200, "text/plain", confirmation_message);

						return; ///< Stop execution on success
					}
				}

				/// Handle parsing error
				const char* error_message = "Failed to parse tracker-id field";
				print_message_in_configuration_mode(error_message);
				configuration_server.send(400, "text/plain", error_message);
			}
			else {
				/// @todo Print name of method.
				const char* errorMessage = "Method Not Allowed";
				print_message_in_configuration_mode(errorMessage);
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

		display.clearDisplay();
		println("Exiting \"Client setup mode\"...", 0, 0);
		display.display();
		delay(5000);

		configuration_server.stop();
	}
}


void print_configuration_mode_info() {
	display.clearDisplay();
	display.setCursor(0, 0);
	println("To configure the tracker:");
	println("- Connect to Wi-Fi network "+ own_ssid + " (no password)");
	println("- Connect to IP " + WiFi.softAPIP().toString());
	display.display();
}

void print_message_in_configuration_mode(const char* message) {
	display.clearDisplay();
	println(message, 0, 0);
	display.display();
	delay(5000);
	print_configuration_mode_info();
}

void print_message_in_configuration_mode(const String& message) {
	print_message_in_configuration_mode(message.c_str());
}


void display_info() {
	display.clearDisplay();
	display.setCursor(0,0);

	if (gps.location.isValid()) {
		println("Latitude: " + String(gps.location.lat(), 6));
		println("Longitude: " + String(gps.location.lng(), 6));

		if (gps.altitude.isValid())
			println("Altitude: " + String(gps.altitude.meters()));
		else
			println("Altitude: Not Available");
	}
	else
		println("Location: Not Available");

	print("Date: ");
	if (gps.date.isValid())
		println(String{} + gps.date.month() + "/" + gps.date.day() + "/" + gps.date.year());
	else
		println("Not Available");

	print("Time: ");
	/// Display time in HH:MM:SS.CC
	if (gps.time.isValid()) {
		String time_format;

		if (gps.time.hour() < 10)
			time_format += "0";
		time_format += gps.time.hour();

		time_format += ":";

		if (gps.time.minute() < 10)
			time_format += "0";
		time_format += gps.time.minute();

		time_format += ":";

		if (gps.time.second() < 10)
			time_format += "0";
		time_format += gps.time.second();

		time_format += ".";

		if (gps.time.centisecond() < 10)
			time_format += "0";
		time_format += gps.time.centisecond();

		println(time_format);
	}
	else
		println("Not Available");

	print("Speed: ");
	if (gps.speed.isValid())
		println(String(gps.speed.kmph(), 6));
	else
		println("Not Available");

	Serial.println();
	display.display();
}
