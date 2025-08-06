#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <vector>
#include "tlv.h"

const char* ssid     = "POCO_X3_Pro";
const char* password = "sina13821818";
const int   udpPort  = 5540;
const int   ledPin   = 2;

const int pwmChannel = 0;
const int pwmFreq    = 500;
const int pwmResBits = 8;

uint8_t currentLevel = 0;

uint32_t timerEndMs    = 0;
uint16_t delayedOffSec = 0;
TaskHandle_t timerTaskHandle = nullptr;

bool lampState = false;
WiFiUDP udp;

void scheduleDelayedOff(uint16_t sec)
{
    if (timerTaskHandle) {
        vTaskDelete(timerTaskHandle);
        timerTaskHandle = nullptr;
    }

    delayedOffSec = sec;
    timerEndMs    = (sec == 0) ? 0 : (millis() + sec * 1000UL);

    if (sec == 0) return;

    xTaskCreate(
        [](void* p){
            uint16_t s = *(uint16_t*)p;
            delete (uint16_t*)p;

            vTaskDelay(pdMS_TO_TICKS(s * 1000));

            ledcWrite(pwmChannel, 0);
            currentLevel   = 0;
            lampState      = false;
            delayedOffSec  = 0;
            timerEndMs     = 0;
            timerTaskHandle = nullptr;
            Serial.println("Timer elapsed → OFF");
            vTaskDelete(nullptr);
        },
        "OffTimer", 2048, new uint16_t(sec), 1, &timerTaskHandle
    );
}

void startIdentifyBlink(uint16_t sec)
{
    if (sec == 0) {
        ledcWrite(pwmChannel, lampState ? currentLevel : 0);
        return;
    }

    xTaskCreate(
        [](void* p){
            uint16_t t = *(uint16_t*)p; delete (uint16_t*)p;

            uint8_t blinkLevel = currentLevel ? currentLevel : 127;

            uint32_t cycles = t * 6;
            bool on = false;
            for (uint32_t i = 0; i < cycles; ++i) {
                on = !on;
                ledcWrite(pwmChannel, on ? blinkLevel : 0);
                vTaskDelay(pdMS_TO_TICKS(83));
            }

            ledcWrite(pwmChannel, lampState ? currentLevel : 0);
            vTaskDelete(nullptr);
        },
        "IdentifyBlink", 1024, new uint16_t(sec), 1, nullptr
    );
}

void setupWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    WiFi.enableIpV6();
    
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(350);
    }
    Serial.print("\nConnected! IP=");
    Serial.println(WiFi.localIP());

    Serial.print("IPv6 (link-local)=");
    Serial.println(WiFi.localIPv6()); 
}

void publishMDNS()
{
    if (!MDNS.begin("smartlamp")) {
        Serial.println("mDNS error!");
        return;
    }

    MDNS.addService("_matter", "_udp", udpPort);

    MDNS.addServiceTxt("_matter", "_udp", "VP",  "0xFFF1+0x8000");
    MDNS.addServiceTxt("_matter", "_udp", "DT",  "0x0101");
    MDNS.addServiceTxt("_matter", "_udp", "CM",  "2");
    MDNS.addServiceTxt("_matter", "_udp", "DN",  "SmartLamp");
    MDNS.addServiceTxt("_matter", "_udp", "SII", "4520");
    MDNS.addServiceTxt("_matter", "_udp", "CRI", "300");
    MDNS.addServiceTxt("_matter","_udp","PI","20202021");

    Serial.println("mDNS _matter published.");
}

void sendAck() {
    const char* ack = "ACK";
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write((const uint8_t*)ack, strlen(ack));
    udp.endPacket();
}

void processPacket(const uint8_t* buf, int len) {
    int endpoint=-1, cluster=-1, command=-1;
    size_t off=0;
    while (off < (size_t)len) {
        uint8_t tag; const uint8_t* val; size_t vlen, used;
        if (!tlvDecodeNext(buf + off, len - off, tag, val, vlen, used))
            break;
        if (tag==0 && vlen==1)  endpoint = val[0];
        else if (tag==1 && vlen==2) cluster = val[0] | (val[1] << 8);
        else if (tag==2 && vlen==1) command = val[0];
        off += used;
    }
    Serial.printf("TLV parsed → ep=%d, cl=0x%04X, cmd=0x%02X\n",
                  endpoint, cluster, command);

    if (cluster == 0x0006 && endpoint == 0)
    {

        // ON
        if (command == 0x01) {
            if (currentLevel == 0) {
                currentLevel = 127;
            }
            ledcWrite(pwmChannel, currentLevel);
            lampState = true;
            Serial.printf("LED ON  (level=%u)\n", currentLevel);
            sendAck();
            return;
        }

        // OFF
        else if (command == 0x00) {
            currentLevel = 0;
            ledcWrite(pwmChannel, 0);
            lampState = false;
            Serial.println("LED OFF");
            sendAck();
            return;
        }

        // Setting timer or Toggle
        else if (command == 0x02) {
            uint16_t attrId = 0, value = 0;
            size_t   o = 0;
            while (o < len) {
                uint8_t tg; const uint8_t* v; size_t vl, u;
                if (!tlvDecodeNext(buf + o, len - o, tg, v, vl, u)) break;
                if (tg == 4 && vl == 2) attrId = v[0] | (v[1] << 8);
                if (tg == 5 && vl == 2) value  = v[0] | (v[1] << 8);
                o += u;
            }

            // Setting OFF timer
            if (attrId == 0x4001) {
                scheduleDelayedOff(value);
                Serial.printf("DelayedOff = %u s\n", value);
                sendAck();
                return;
            }

            // TOGGLE
            if (lampState) {
                currentLevel = 0;
                ledcWrite(pwmChannel, 0);
                lampState = false;
                Serial.println("LED TOGGLE → OFF");
            } else {
                if (currentLevel == 0) currentLevel = 127;
                ledcWrite(pwmChannel, currentLevel);
                lampState = true;
                Serial.printf("LED TOGGLE → ON  (level=%u)\n", currentLevel);
            }
            sendAck();
            return;
        }

        // OFF timer
        else if (command == 0x03) {
            uint16_t attrId = 0;
            size_t o = 0;
            while (o < len) {
                uint8_t tg; const uint8_t* v; size_t vl, used;
                if (!tlvDecodeNext(buf + o, len - o, tg, v, vl, used)) break;
                if (tg == 4 && vl == 2)
                    attrId = v[0] | (v[1] << 8);
                o += used;
            }

            // Reading OFF timer
            if (attrId == 0x4001) {
                uint16_t remaining = 0;
                if (timerEndMs && millis() < timerEndMs) {
                    remaining = (timerEndMs - millis()) / 1000;
                }

                std::vector<uint8_t> out;
                tlvEncodeUInt16(out, 4, attrId);
                tlvEncodeUInt16(out, 5, remaining);
                udp.beginPacket(udp.remoteIP(), udp.remotePort());
                udp.write(out.data(), out.size());
                udp.endPacket();
                return;
            }

            sendAck();
            return;
        }
    }

    // Identify
    if (cluster == 0x0003 && command == 0x00) {
        uint16_t identifyTime = 0;

        size_t o = 0;
        while (o < len) {
            uint8_t tg; const uint8_t* v; size_t vl, used;
            if (!tlvDecodeNext(buf + o, len - o, tg, v, vl, used)) break;
            if (tg == 0 && vl == 2)
                identifyTime = v[0] | (v[1] << 8);
            o += used;
        }

        Serial.printf("Identify for %u s\n", identifyTime);
        startIdentifyBlink(identifyTime);
        sendAck();
        return;
    }

    // Setting Level
    if (cluster == 0x0008 && command == 0x04) {
        uint8_t level = 0;
        size_t off2 = 0;
        while (off2 < len) {
            uint8_t tag; const uint8_t* v; size_t vl, used;
            if (!tlvDecodeNext(buf + off2, len - off2, tag, v, vl, used)) break;
            if (tag == 3 && vl == 1) level = v[0];
            off2 += used;
        }
        currentLevel = level;
        ledcWrite(pwmChannel, currentLevel);
        lampState = currentLevel > 0;
        Serial.printf("Set level to %u\n", currentLevel);
        sendAck();
        return;
    }

    // Reading Level
    if (cluster == 0x0008 && command == 0x03) {
        uint16_t attrId = 0;
        size_t o = 0;
        while (o < len) {
            uint8_t tg; const uint8_t* v; size_t vl, used;
            if (!tlvDecodeNext(buf + o, len - o, tg, v, vl, used)) break;
            if (tg == 4 && vl == 2)
                attrId = v[0] | (v[1] << 8);
            o += used;
        }

        if (attrId == 0x0000) {
            std::vector<uint8_t> out;
            tlvEncodeUInt16(out, 4, attrId);
            tlvEncodeUInt8 (out, 5, currentLevel);
            udp.beginPacket(udp.remoteIP(), udp.remotePort());
            udp.write(out.data(), out.size());
            udp.endPacket();
        } else {
            sendAck();
        }
        return;
    }

    // Descriptor
    if (cluster == 0x001D && command == 0x01)  {
        std::vector<uint8_t> out;
        tlvEncodeUInt8 (out, 0, 0);
        tlvEncodeUInt16(out, 1, 0x0101);
        uint8_t listBytes[] = { 0x06,0x00, 0x08,0x00, 0x1D,0x00, 0x28,0x00 };
        tlvEncodeBytes (out, 2, listBytes, sizeof(listBytes));

        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(out.data(), out.size());
        udp.endPacket();
        return;
    }

    // Info
    if (cluster == 0x0028 && command == 0x01) {
        std::vector<uint8_t> out;
        tlvEncodeUInt8 (out, 0, 0);
        tlvEncodeUInt16(out, 1, 0xFFF1);
        tlvEncodeUInt16(out, 2, 0x8000);
        tlvEncodeUInt8 (out, 3, 0);
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(out.data(), out.size());
        udp.endPacket();
        return;
    }

    // PBKDFParamRequest
    if (cluster == 0x0000 && command == kPBKDFParamRequest) {
        Serial.println("PBKDFParamRequest");

        pairing.sessionID = 0x1234;
        uint32_t ip32 = udp.remoteIP();
        memcpy(pairing.peerAddr, &ip32, 4);
        pairing.active = true;
        pairing.step   = 0;

        std::vector<uint8_t> resp;
        tlvEncodeUInt16(resp, 0, pairing.sessionID);
        tlvEncodeUInt8 (resp, 1, 0x20);
        tlvEncodeUInt8 (resp, 2, 0x03);
        uint8_t fakeSalt[16] = {0};
        tlvEncodeBytes(resp, 3, fakeSalt, 16);

        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(resp.data(), resp.size());
        udp.endPacket();
        Serial.println("→ PBKDFParamResponse sent");
        return;
    }

    // PASE PAKE1
    if (cluster == 0x0000 && command == kPASEPake1 && pairing.active) {
        Serial.println("PASE Pake1");

        std::vector<uint8_t> resp;
        resp.push_back(kPASEPake2);
        resp.resize(1 + 65, 0);

        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(resp.data(), resp.size());
        udp.endPacket();

        pairing.step = 1;
        Serial.println("→ Pake2 sent");
        return;
    }

    // PASE PAKE3
    if (cluster == 0x0000 && command == kPASEPake3 && pairing.active
            && pairing.step == 1) {
        Serial.println("PASE Pake3");

        sendAck();
        pairing.active = false;
        pairing.step   = 2;

        Serial.println("Pairing complete (DEV MODE)");
        return;
    }

}

void handleUdp() {
    int pkSize = udp.parsePacket();
    if (pkSize <= 0) return;

    std::vector<uint8_t> buf(pkSize);
    int len = udp.read(buf.data(), pkSize);
    if (len <= 0) return;

    Serial.print("RX ");
    for (int i=0;i<len;i++) Serial.printf("%02X ", buf[i]);
    Serial.println();
    processPacket(buf.data(), len);
}

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    ledcSetup(pwmChannel, pwmFreq, pwmResBits);
    ledcAttachPin(ledPin, pwmChannel);
    ledcWrite(pwmChannel, 0);
    setupWiFi();
    udp.begin(udpPort);
    Serial.printf("UDP listening on %d\n", udpPort);

    publishMDNS();
}

void loop() {
    handleUdp();
}