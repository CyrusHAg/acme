#define ACME_FIRMWARE_VERSION "1.0.0"
#include <AcmeOTA.h>

AcmeOTAClass ota;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }
  if (!ota.begin("acme-esp32s3")) {
    Serial.println("mDNS initialization failed");
    return;
  }
  Serial.print("HTTP OTA ready: ");
  Serial.println(ota.hostname() + ".local");
  Serial.println(WiFi.localIP());
}

void loop() {
  ota.handle();
}
