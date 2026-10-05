#include "WebServer.h"
#include "Controller.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

namespace
{
    constexpr char AP_SSID[] = "Bullitt-Light";
    constexpr char AP_PASSWORD[] = "bullitt123";

    ESP8266WebServer server(80);

    void handleRoot()
    {
        File file = LittleFS.open("/index.html", "r");

        if (!file)
        {
            server.send(
                500,
                "text/plain",
                "index.html not found"
            );
            return;
        }

        server.streamFile(
            file,
            "text/html; charset=utf-8"
        );

        file.close();
    }

    void handleEffect()
    {
        if (!server.hasArg("effect"))
        {
            server.send(
                400,
                "text/plain",
                "Missing effect"
            );
            return;
        }

        int effect = server.arg("effect").toInt();

        if (effect < 0 || effect >= 8)
        {
            server.send(
                400,
                "text/plain",
                "Invalid effect"
            );
            return;
        }

        setCurrentEffect(
            static_cast<Effect>(effect)
        );

        server.send(
            200,
            "text/plain",
            "OK"
        );
    }

    void handleStatus()
    {
        String json = "{";

        json += "\"effect\":";
        json += String(
            static_cast<int>(getCurrentEffect())
        );

        json += ",\"effectName\":\"";
        json += getEffectName(getCurrentEffect());
        json += "\"";

        json += ",\"state\":\"";
        json += getWheelStateName(getWheelState());
        json += "\"";

        json += ",\"speed\":";
        json += String(getSpeedKmh(), 1);

        json += ",\"smoothSpeed\":";
        json += String(getSmoothSpeedKmh(), 1);

        json += ",\"animationSpeed\":";
        json += String(getAnimationSpeedKmh(), 1);

        json += "}";

        server.send(
            200,
            "application/json",
            json
        );
    }

    void handleNotFound()
    {
        server.send(
            404,
            "text/plain",
            "Not found"
        );
    }
}

void webServerSetup()
{
    if (!LittleFS.begin())
    {
        Serial.println(
            "LittleFS mount failed"
        );
    }

    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        AP_SSID,
        AP_PASSWORD
    );

    Serial.println();
    Serial.print("WiFi AP: ");
    Serial.println(AP_SSID);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/api/effect",
        HTTP_GET,
        handleEffect
    );

    server.on(
        "/api/status",
        HTTP_GET,
        handleStatus
    );

    server.onNotFound(
        handleNotFound
    );

    server.begin();

    Serial.println(
        "Webserver started"
    );
}

void webServerLoop()
{
    server.handleClient();
}
