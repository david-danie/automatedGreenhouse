# Componente principal. Por ahora solo el punto de entrada (hito 1: esqueleto que
# arranca). A medida que avance la migración se irán añadiendo fuentes (Plant.cpp,
# utils.cpp, etc.) a SRCS y los componentes de IDF a REQUIRES (nvs_flash, driver,
# esp_wifi, esp_http_server, esp_netif, ...).
idf_component_register(
    SRCS
        "main.cpp"
        "utils.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        mbedtls
)
