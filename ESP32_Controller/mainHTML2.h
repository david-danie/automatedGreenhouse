const char* controllerInfo2 = R"rawliteral(
;
    document.getElementById("dispositivoActivo").innerText = `Cultivo: a85665458-${valoresESP32.dispositivoActivo}`;
    document.getElementById("fotoperiodo").innerText = `Fotoperiodo: ${valoresESP32.fotoperiodo} horas`;
    document.getElementById("luces").innerText = `A:${valoresESP32.luzAzul}% - R:${valoresESP32.luzRoja}% - B:${valoresESP32.luzBlanca}%`;
    document.getElementById("irrigacion").innerText = `Irrigación: ${valoresESP32.horasIrrigacion} veces al día x ${valoresESP32.minutosIrrigacion} minutos`;
    document.getElementById("ventilacion").innerText = `Ventilación: ${valoresESP32.horasVentilador} veces al día x ${valoresESP32.minutosVentilador} minutos`;
    document.getElementById("edad").innerText = `Tiempo: ${valoresESP32.semana} semanas / ${valoresESP32.dias} días`;
    
</script>
</body>
</html>
)rawliteral";