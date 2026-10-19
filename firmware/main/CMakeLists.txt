# Componente principal. Por ahora solo el punto de entrada (hito 1: esqueleto que
# arranca). A medida que avance la migración se irán añadiendo fuentes (Plant.cpp,
# utils.cpp, etc.) a SRCS y los componentes de IDF a REQUIRES (nvs_flash, driver,
# esp_wifi, esp_http_server, esp_netif, ...).
idf_component_register(
    SRCS
        "main.cpp"
        "utils.cpp"
        "storage.cpp"
        "rtc.cpp"
        "actuators.cpp"
        "control.cpp"
        "wifi.cpp"
        "httpserver.cpp"
        "captivedns.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        mbedtls
        nvs_flash
        driver
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        lwip
)
