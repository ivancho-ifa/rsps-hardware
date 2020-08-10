#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "gps/gps.hpp"

const char* ssid = "our-router";
const char* password = "qKMPM93636";

const char* host = "http://secure-falls-78908.herokuapp.com";

bool isLocked = false;

TinyGPSPlus gps;
SoftwareSerial gps_serial(D2, D1);

ESP8266WebServer server(80);

void setup() {
	Serial.begin(115200);
	Serial.println();

	gps_serial.begin(9600);

	Serial.printf("Connecting to %s ", ssid);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println(" connected");

	{
		const auto uri = String() + host + "/report-ip";
		WiFiClient client;
		HTTPClient http;

		http.begin(client, uri);
		http.addHeader("Content-Type", "application/json");

		const auto ipMessage = String() + "{\"ip\":\"" + WiFi.localIP().toString() + "\"}";
		Serial.println(ipMessage);
		const auto status = http.POST(ipMessage);

		Serial.println("[Response:]");
		if (status > 0)
			Serial.println(String() + "HTTP status code: " + status);
		else
			Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(status).c_str());

		http.end();
	}

	server.on("/is-locked", [isLocked]() {
		server.send(200, "application/json", String() + "{\"isLocked\":" + isLocked + "}");
	});

	server.on("/lock", [&isLocked]() {
		isLocked = true;
		server.send(200, "application/json", String() + "{\"isLocked\":" + isLocked + "}");
	});

	server.on("/unlock", [&isLocked]() {
		isLocked = false;
		server.send(200, "application/json", String() + "{\"isLocked\":" + isLocked + "}");
	});

	server.on("/get-location", []() {
		TinyGPSLocation& location = rsps::gps::location();
		if (location.isValid()) {
			const auto locationMessage =
				String() + "{\"latitude\":" + location.lat() + ",\"longitude\":" + location.lng() + "}";
			server.send(200, "application/json", locationMessage);
		}
		else
			server.send(500);
	});

	server.begin();

	Serial.print(String() + "Listening on ");
	Serial.print(WiFi.localIP());
	Serial.println(":80");
}

void loop() {
	server.handleClient();

	while (gps_serial.available() > 0)
		if (gps.encode(gps_serial.read()))
			displayInfo();

	if (millis() > 5000 && gps.charsProcessed() < 10) {
		Serial.println("No GPS detected");
		while (true)
			;
	}

	if (gps.location.isValid())
		sendLocation(gps.location);
}

void sendLocation(TinyGPSLocation& location) {
	if (WiFi.status() == WL_CONNECTED) {
		Serial.println("[Connected]");

		const auto uri = String() + host + "/report-location";
		Serial.println(String() + "[Sending a request to " + uri + "]");

		WiFiClient client;
		HTTPClient http;

		http.begin(client, uri);
		http.addHeader("Content-Type", "application/json");

		const auto locationMessage =
			String() + "{\"latitude\":" + location.lat() + ",\"longitude\":" + location.lng() + "}";
		Serial.println(locationMessage);
		const auto status = http.POST(locationMessage);

		Serial.println("[Response:]");
		if (status > 0)
			Serial.println(String() + "HTTP status code: " + status);
		else
			Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(status).c_str());

		http.end();
		Serial.println("\n[Disconnected]");
	}
	else
		Serial.println("connection failed!]");

	delay(5000);
}

void displayInfo() {
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
		Serial.print(time.minute());

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

	Serial.println();
	Serial.println();
	delay(1000);
}
