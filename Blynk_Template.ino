/*
 Name:		Blynk_Template.ino
 Created:	9/19/2019 9:04:46 AM
 Author:	Andriy Honcharenko
*/

/* CODE BEGIN Includes */
#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
/* CODE END Includes */

/* CODE BEGIN UD */
/* User defines ---------------------------------------------------------*/
#define BLYNK_PRINT Serial

#define NAME_DEVICE						"MyHomeIoT-ESP8266"

#define BUTTON_SYS0_PIN					0
#define LED_SYS_PIN						13

#define BUTTON_SYS_B0_VPIN				V20
#define WIFI_SIGNAL_VPIN				V80

#define INTERVAL_PRESSED_RESET_ESP		3000L
#define INTERVAL_PRESSED_RESET_SETTINGS 5000L
#define INTERVAL_PRESSED_SHORT			50
#define INTERVAL_SEND_DATA				30033L
#define INTERVAL_RECONNECT				60407L
#define INTERVAL_REFRESH_DATA			4065L
#define WIFI_MANAGER_TIMEOUT			180

#define EEPROM_SETTINGS_SIZE			512
#define EEPROM_START_SETTING_WM			0
#define EEPROM_SALT_WM					12661

#define LED_SYS_TOGGLE()				digitalWrite(LED_SYS_PIN, !digitalRead(LED_SYS_PIN))
#define LED_SYS_ON()					digitalWrite(LED_SYS_PIN, LOW)
#define LED_SYS_OFF()					digitalWrite(LED_SYS_PIN, HIGH)
/* CODE END UD */

/* CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
bool shouldSaveConfigWM		= false; //flag for saving data
bool btnSystemState0		= false;
bool triggerBlynkConnect	= false;
bool isFirstConnect			= true; // Keep this flag not to re-sync on every reconnection

int startPressBtn = 0;

//structure for initial settings. It now takes 116 bytes
typedef struct {
	char  host[33] = NAME_DEVICE;				// 33 + '\0' = 34 bytes
	char  blynkToken[33] = "";					// 33 + '\0' = 34 bytes
	char  blynkServer[33] = "blynk-cloud.com";	// 33 + '\0' = 34 bytes
	char  blynkPort[6] = "8442";				// 04 + '\0' = 05 bytes
	int   salt = EEPROM_SALT_WM;				// 04		 = 04 bytes
} WMSettings;									// 111 + 1	 = 112 bytes (112 this is a score of 0)
//-----------------------------------------------------------------------------------------

WMSettings wmSettings;

BlynkTimer timer;

Ticker tickerESP8266;

//Declaration OTA WebUpdater
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
/* CODE END PV */

/* CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
static void configModeCallback(WiFiManager* myWiFiManager);
static void saveConfigCallback(void);
static void tick(void);
static void untick(void);
static void readSystemKey(void);
static void timerRefreshData(void);
static void timerSendServer(void);
static void timerReconnect(void);
/* CODE END PFP */

// the setup function runs once when you press reset or power the board

void setup() 
{
	Serial.begin(115200);

	pinMode(BUTTON_SYS0_PIN, INPUT_PULLUP);
	pinMode(LED_SYS_PIN, OUTPUT);

	// Read the WM settings data from EEPROM to RAM
	EEPROM.begin(EEPROM_SETTINGS_SIZE);
	EEPROM.get(EEPROM_START_SETTING_WM, wmSettings);
	EEPROM.end();

	if (wmSettings.salt != EEPROM_SALT_WM) 
	{
		Serial.println(F("Invalid wmSettings in EEPROM, trying with defaults"));
		WMSettings defaults;
		wmSettings = defaults;
	}

	// Print old values to the terminal
	Serial.println(wmSettings.host);
	Serial.println(wmSettings.blynkToken);
	Serial.println(wmSettings.blynkServer);
	Serial.println(wmSettings.blynkPort);

	tickerESP8266.attach(0.5, tick);   // start ticker with 0.5 because we start in AP mode and try to connect

	//Local intialization. Once its business is done, there is no need to keep it around
	WiFiManager wifiManager;

	//reset saved wmSettings
	//wifiManager.resetSettings();

	//set minimu quality of signal so it ignores AP's under that quality
	//defaults to 8%
	//wifiManager.setMinimumSignalQuality();

	//sets timeout before webserver loop ends and exits even if there has been no setup.
	//useful for devices that failed to connect at some point and got stuck in a webserver loop
	//in seconds setConfigPortalTimeout is a new name for setTimeout
	wifiManager.setConfigPortalTimeout(WIFI_MANAGER_TIMEOUT);

	// The extra parameters to be configured (can be either global or just in the setup)
	// After connecting, parameter.getValue() will get you the configured value
	// id/name placeholder/prompt default length
	WiFiManagerParameter custom_device_name_text("<br/>Enter name of the device<br/>or leave it as it is<br/>");
	wifiManager.addParameter(&custom_device_name_text);

	WiFiManagerParameter custom_device_name("device-name", "device name", wmSettings.host, 33);
	wifiManager.addParameter(&custom_device_name);

	WiFiManagerParameter custom_blynk_text("<br/>Blynk config.<br/>");
	wifiManager.addParameter(&custom_blynk_text);

	WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", wmSettings.blynkToken, 33);
	wifiManager.addParameter(&custom_blynk_token);

	WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", wmSettings.blynkServer, 33);
	wifiManager.addParameter(&custom_blynk_server);

	WiFiManagerParameter custom_blynk_port("blynk-port", "port", wmSettings.blynkPort, 6);
	wifiManager.addParameter(&custom_blynk_port);

	//set config save notify callback
	wifiManager.setSaveConfigCallback(saveConfigCallback);

	//set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
	wifiManager.setAPCallback(configModeCallback);

	//set custom ip for portal
	//wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

	//fetches ssid and pass from eeprom and tries to connect
	//if it does not connect it starts an access point with the specified name
	//here  "AutoConnectAP"
	//and goes into a blocking loop awaiting configuration
	//wifiManager.autoConnect();
	//or use this for auto generated name ESP + ChipID

	if (wifiManager.autoConnect(wmSettings.host))
	{
		//if you get here you have connected to the WiFi
		Serial.println(F("Connected WiFi!"));
	}
	else
	{
		Serial.println(F("failed to connect and hit timeout"));
	}

	untick();	// cancel the flashing LED

	// Copy the entered values to the structure
	strcpy(wmSettings.host, custom_device_name.getValue());
	strcpy(wmSettings.blynkToken, custom_blynk_token.getValue());
	strcpy(wmSettings.blynkServer, custom_blynk_server.getValue());
	strcpy(wmSettings.blynkPort, custom_blynk_port.getValue());

	// Print new values to the terminal
	Serial.println(wmSettings.host);
	Serial.println(wmSettings.blynkToken);
	Serial.println(wmSettings.blynkServer);
	Serial.println(wmSettings.blynkPort);

	if (shouldSaveConfigWM)
	{
		LED_SYS_ON();
		// Write the input to the EEPROM
		EEPROM.begin(EEPROM_SETTINGS_SIZE);
		EEPROM.put(EEPROM_START_SETTING_WM, wmSettings);
		EEPROM.end();
		//---------------------------------
		LED_SYS_OFF();
	}

	// Run OTA WebUpdater
	MDNS.begin(wmSettings.host);
	httpUpdater.setup(&httpServer);
	httpServer.begin();
	MDNS.addService("http", "tcp", 80);
	Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", wmSettings.host);

	// Configure connection to blynk server
	Blynk.config(wmSettings.blynkToken, wmSettings.blynkServer, atoi(wmSettings.blynkPort));

	if (Blynk.connect())
	{
		//TODO: something to do if connected
	}
	else
	{
		//TODO: something to do if you failed to connect
	}

	timer.setInterval(INTERVAL_REFRESH_DATA, timerRefreshData);
	timer.setInterval(INTERVAL_SEND_DATA, timerSendServer);
	timer.setInterval(INTERVAL_RECONNECT, timerReconnect);	
}

// the loop function runs over and over again until power down or reset
void loop() 
{
	if (Blynk.connected())
	{
		Blynk.run(); // Initiates Blynk Server  
	}
	else
	{
		if (!tickerESP8266.active())
		{
			tickerESP8266.attach(2, tick);
		}
	}

	timer.run(); // Initiates BlynkTimer
	
	httpServer.handleClient(); // Initiates OTA WebUpdater  

	readSystemKey();
}

/* BLYNK CODE BEGIN */
BLYNK_CONNECTED()
{
	untick();	

	Serial.println(F("Blynk Connected!"));
	Serial.println(F("local ip"));
	Serial.println(WiFi.localIP());		

	char str[32];
	sprintf_P(str, PSTR("%s Online!"), wmSettings.host);	
	Blynk.notify(str);

	if (isFirstConnect)
	{
		Blynk.syncAll();
		isFirstConnect = false;
	}
}

BLYNK_WRITE(BUTTON_SYS_B0_VPIN) // Example
{	
	//TODO: something to do when a button is clicked in the Blynk app
	
	Serial.println(F("System_0 button pressed is App!"));	
}
/* BLYNK CODE END */

/* CODE BEGIN USER FUNCTION */
static void timerRefreshData(void)
{
	//TODO: here are functions for updating data from sensors, ADC, etc ...
}

static void timerSendServer(void)
{
	if (Blynk.connected())
	{
		//TODO: here are the functions that send data to the Blynk server
		Blynk.virtualWrite(WIFI_SIGNAL_VPIN, map(WiFi.RSSI(), -105, -40, 0, 100)); //Example send level WiFi signal
	}
	else
	{
		//TODO:
	}
}

static void timerReconnect(void)
{
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.println(F("WiFi not connected"));

		if (WiFi.begin() == WL_CONNECTED)
		{
			Serial.println(F("WiFi reconnected"));
		}
		else
		{
			Serial.println(F("WiFi not reconnected"));
		}
	}
	else// if (WiFi.status() == WL_CONNECTED)
	{
		Serial.println(F("WiFi in connected"));

		if (!Blynk.connected())
		{
			if (Blynk.connect())
			{
				Serial.println(F("Blynk reconnected"));
			}
			else
			{
				Serial.println(F("Blynk not reconnected"));
			}
		}
		else
		{
			Serial.println(F("Blynk in connected"));
		}
	}
}

static void configModeCallback(WiFiManager* myWiFiManager)
{
	Serial.println(F("Entered config mode"));
	Serial.println(WiFi.softAPIP());
	//if you used auto generated SSID, print it
	Serial.println(myWiFiManager->getConfigPortalSSID());
	//entered config mode, make led toggle faster
	tickerESP8266.attach(0.2, tick);
}

//callback notifying us of the need to save config
static void saveConfigCallback()
{
	Serial.println(F("Should save config"));
	shouldSaveConfigWM = true;
}

static void tick(void)
{
	//toggle state  
	LED_SYS_TOGGLE();     // set pin to the opposite state
}

static void untick(void)
{
	tickerESP8266.detach();
	LED_SYS_OFF(); //keep LED off	
	
}

static void readSystemKey(void)
{
	if (!digitalRead(BUTTON_SYS0_PIN) && !btnSystemState0)
	{
		btnSystemState0 = true;
		startPressBtn = millis();
	}
	else if (digitalRead(BUTTON_SYS0_PIN) && btnSystemState0)
	{
		btnSystemState0 = false;
		int pressTime = millis() - startPressBtn;

		if (pressTime > INTERVAL_PRESSED_RESET_ESP && pressTime < INTERVAL_PRESSED_RESET_SETTINGS)
		{
			if (Blynk.connected())
			{				
				Blynk.notify(String(wmSettings.host) + F(" reboot!"));
			}
			
			Blynk.disconnect();		
			tickerESP8266.attach(0.1, tick);			
			delay(2000);
			ESP.restart();
		}
		else if (pressTime > INTERVAL_PRESSED_RESET_SETTINGS)
		{
			if (Blynk.connected())
			{								
				Blynk.notify(String(wmSettings.host) + F(" setting reset! Connected WiFi AP this device!"));
			}			

			WMSettings defaults;
			wmSettings = defaults;

			LED_SYS_ON();
			// We write the default data to EEPROM
			EEPROM.begin(EEPROM_SETTINGS_SIZE);
			EEPROM.put(EEPROM_START_SETTING_WM, wmSettings);
			EEPROM.end();
			//------------------------------------------
			LED_SYS_OFF();

			delay(1000);
			WiFi.disconnect();
			delay(1000);
			ESP.restart();
		}
		else if (pressTime < INTERVAL_PRESSED_RESET_ESP && pressTime > INTERVAL_PRESSED_SHORT)
		{			
			Serial.println(F("System button_0 pressed is Device!"));
			// TODO: insert here what will happen when you press the ON / OFF button			
		}
		else if (pressTime < INTERVAL_PRESSED_SHORT)
		{			
			Serial.printf("Fixed false triggering %ims", pressTime);
			Serial.println();
		}		
	}
}
/* CODE END USER FUNCTION */
