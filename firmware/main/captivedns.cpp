// ===========================================================================
//  captivedns.cpp — Servidor DNS captivo mínimo. Ver captivedns.h.
//  Responde apGatewayIp a toda consulta DNS tipo A. Patrón estándar de portal
//  captivo: reutiliza la pregunta y le añade una respuesta A apuntando al AP.
// ===========================================================================
#include "captivedns.h"
#include <cstring>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "cdns";

// IP del SoftAP (debe coincidir con apGatewayIp de Constants.h: 192.168.4.1).
static const uint8_t AP_IP[4] = { 192, 168, 4, 1 };

// Cabecera DNS (12 bytes). Solo se manipulan los flags y los contadores.
#pragma pack(push, 1)
struct DnsHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};
#pragma pack(pop)

static void dnsTask(void*) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { ESP_LOGE(TAG, "socket falló"); vTaskDelete(nullptr); return; }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind :53 falló");
        close(sock);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "DNS captivo escuchando en :53");
    uint8_t buf[512];
    while (true) {
        sockaddr_in src = {};
        socklen_t sl = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
        // Mínimo: cabecera (12) + al menos 1 byte de pregunta.
        if (n < (int)sizeof(DnsHeader) + 5) continue;

        DnsHeader* h = (DnsHeader*)buf;
        // Solo respondemos consultas estándar (QR=0). Marca respuesta (QR=1,
        // AA=1), 1 pregunta (la original) y 1 respuesta.
        h->flags  = htons(0x8180);   // response, recursion available
        h->ancount = htons(1);
        h->nscount = 0;
        h->arcount = 0;

        // La pregunta ocupa desde el byte 12 hasta el QNAME(0) + QTYPE(2) + QCLASS(2).
        int qlen = 0;
        int p = sizeof(DnsHeader);
        while (p < n && buf[p] != 0) p += buf[p] + 1;  // salta labels
        p += 1 + 4;                                     // nul + QTYPE + QCLASS
        if (p > n) continue;                            // pregunta malformada
        qlen = p - sizeof(DnsHeader);

        // Respuesta: puntero al nombre (0xC00C), TYPE A, CLASS IN, TTL, RDLENGTH=4, IP.
        int o = sizeof(DnsHeader) + qlen;
        if (o + 16 > (int)sizeof(buf)) continue;
        buf[o++] = 0xC0; buf[o++] = 0x0C;               // puntero al QNAME (offset 12)
        buf[o++] = 0x00; buf[o++] = 0x01;               // TYPE A
        buf[o++] = 0x00; buf[o++] = 0x01;               // CLASS IN
        buf[o++] = 0x00; buf[o++] = 0x00; buf[o++] = 0x00; buf[o++] = 0x3C; // TTL 60s
        buf[o++] = 0x00; buf[o++] = 0x04;               // RDLENGTH 4
        buf[o++] = AP_IP[0]; buf[o++] = AP_IP[1]; buf[o++] = AP_IP[2]; buf[o++] = AP_IP[3];

        sendto(sock, buf, o, 0, (sockaddr*)&src, sl);
    }
}

namespace captivedns {

bool begin() {
    BaseType_t r = xTaskCreate(dnsTask, "captivedns", 3072, nullptr, 5, nullptr);
    return r == pdPASS;
}

} // namespace captivedns
