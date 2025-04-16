const char* controllerInfo = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Estado del cultivo</title>
  <style>
    body {
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      background-color: #e8f5e9;
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 1rem;
    }

    .container {
      width: 100%;
      max-width: 400px;
      padding: 24px;
      box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
      background-color: white;
      border-radius: 16px;
      text-align: center;
    }

    h1 {
      font-size: 1.7rem;
      font-weight: bold;
      color: #2e7d32;
      margin-bottom: 1.5rem;
    }

    .form-group {
      background-color: #f0f8ff;
      padding: 10px 16px;
      border-radius: 12px;
      margin-bottom: 1rem;
      color: #333;
      font-weight: bold;
      text-align: center;
    }

    span {
      display: block;
    }

    button {
      width: 100%;
      padding: 12px;
      margin-top: 10px;
      border-radius: 8px;
      font-weight: bold;
      font-size: 1rem;
      cursor: pointer;
      border: none;
    }

    .update {
      background-color: #388e3c;
      color: white;
    }

    .exit {
      background-color: transparent;
      color: #388e3c;
      border: 2px solid #388e3c;
    }
    .color-tag {
      display: inline-flex;
      align-items: center;
      background-color: #f0f8ff;
      border-radius: 12px;
      padding: 6px 10px;
      margin: 4px;
      font-weight: bold;
      color: #333;
      font-size: 0.95rem;
    }

    .color-tag::before {
      content: "";
      width: 14px;
      height: 14px;
      background-color: var(--color);
      border-radius: 3px;
      margin-right: 8px;
      border: 1px solid #999;
    }
  </style>
</head>
<body>
  <div class="container">
    <form class="config-form">

      <h1>Estado del cultivo</h1>

      <div class="form-group">     
        <span id="dispositivoActivo">Cargando...</span>
      </div>

      <div class="form-group">       
        <span id="fotoperiodo">Cargando...</span>
      </div>

      <div class="form-group"> 
        <div class="color-tag" style="--color:#0000ff" id="luzAzul">20%</div>     
        <div class="color-tag" style="--color:#ff0000" id="luzRoja">30%</div>
        <div class="color-tag" style="--color:#00000" id="luzBlanca">50%</div>   
      </div>

      <div class="form-group">        
        <span id="irrigacion">Cargando...</span>
      </div>

      <div class="form-group">       
        <span id="ventilacion">Cargando...</span>
      </div>

      <div class="form-group">      
        <span id="edad">Cargando...</span>
      </div>
      <button class="update" formaction="/update" formmethod="POST" type="submit">Actualizar</button>
      <button class="exit" formaction="/exit" type="submit">Salir</button> 
    </form>
  </div>

  <script>
        var valoresESP32 = 
)rawliteral";

const char* controllerInfo2 = R"rawliteral(
    document.getElementById("dispositivoActivo").innerText = `Cultivo: a85665458-${valoresESP32.dispositivoActivo}`;
    document.getElementById("fotoperiodo").innerText = `Fotoperiodo: ${valoresESP32.fotoperiodo} horas`;
    document.getElementById("luzAzul").innerText = `${valoresESP32.luzAzul}%`;
    document.getElementById("luzRoja").innerText = `${valoresESP32.luzRoja}%`;
    document.getElementById("luzBlanca").innerText = `${valoresESP32.luzBlanca}%`;
    document.getElementById("irrigacion").innerText = `Irrigación: ${valoresESP32.horasIrrigacion} veces al día x ${valoresESP32.minutosIrrigacion} minutos`;
    document.getElementById("ventilacion").innerText = `Ventilación: ${valoresESP32.horasVentilador} veces al día x ${valoresESP32.minutosVentilador} minutos`;
    document.getElementById("edad").innerText = `Tiempo: ${valoresESP32.semana} semanas / ${valoresESP32.dias} días`;
  </script>
</body>
</html>
)rawliteral";


