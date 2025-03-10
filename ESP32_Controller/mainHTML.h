const char* controllerInfo = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Variables</title>
    <style>

        body {
            font-family: 'Arial', sans-serif;
            margin: 0;
            padding: 0;
            background-color: #ffffff;
            color: #000000;
            font-size: 16px; 
        }

        .container {
            max-width: 80vw; 
            margin: 50px auto;
            padding: 2rem; 
            background-color: #9ce9de;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            border-radius: 1rem;
        }

        h1 {
            text-align: center;
            color: #003366;
            font-size: 2.3rem; 
        }

        .config-form {
            margin-top: 2rem;
            padding: 1rem;
            background-color: #f0f8ff;
            border-radius: 0.5rem;
        }

        .form-group {
            display: flex;
            flex-direction: column;
            margin-bottom: 1.5rem;
        }

        .config-form .button {
            width: 100%;
            padding: 1rem;
            background-color: #003366;
            color: white;
            border: none;
            border-radius: 0.5rem;
            font-size: 1rem;
            cursor: pointer;
            transition: background-color 0.3s;
        }

        .config-form .button:hover {
            background-color: #57bd9e;
        }

        .row {
            display: flex;
            justify-content: center;
            align-items: center;
            margin-bottom: 10px;
        }


        @media (min-width: 1200px) {
            body {
                font-size: 18px; 
            }

            .container {
                max-width: 60vw; 
            }
        }

        @media (max-width: 600px) {

            .config-form .button {
                width: 100%;
                margin-bottom: 10px;
            }
        }
    </style>
</head>
<body>

<div class="container">

    <form class="config-form">

        <h1>Estado del cultivo</h1>

        <div class="row">
            <div class="form-group">
                <span id="dispositivoActivo">Cargando...</span>
            </div>
        </div>

        <div class="row">
            <div class="form-group">
                <span id="fotoperiodo">Cargando...</span>
            </div>
        </div>

        <div class="row">
            <div class="form-group">
                <span id="luces">Cargando...</span>
            </div>
        </div>

        <div class="row">
            <div class="form-group">
                <span id="irrigacion">Cargando...</span>
            </div>
        </div>

        <div class="row">
            <div class="form-group">
                <span id="ventilacion">Cargando...</span>
            </div>
        </div>
    
        <div class="row">
            <div class="form-group">
                <span id="edad">Cargando...</span>
            </div>
        </div>

        <div class="row">
            <button class="button" formaction="http://192.168.4.1/update" formmethod="POST" type="submit">Modificar</button>
        </div>

        <div class="row">
            <button class="button" formaction="http://192.168.4.1/exit" formmethod="GET" type="submit">Salir</button>
        </div>
        
    </form>

</div>

<script>
    var valoresESP32 = 
)rawliteral";

const char* controllerInfo2 = R"rawliteral(
  
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

const char* registerUserForm = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bienvenido a SmartPlant</title>
    <style>
        body {
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
            background-color: #e8f5e9;
            font-family: Arial, sans-serif;
            margin: 0;
        }
        .container {
            padding: 24px;
            max-width: 400px;
            box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
            background-color: white;
            border-radius: 16px;
            text-align: center;
        }
        h2 {
            font-size: 24px;
            font-weight: bold;
            color: #2e7d32;
        }
        p {
            font-size: 14px;
            color: #757575;
            margin-bottom: 16px;
        }
        input, button {
            width: calc(100% - 20px);
            padding: 10px;
            margin-bottom: 10px;
            border-radius: 8px;
            box-sizing: border-box;
        }
        input {
            border: 1px solid #81c784;
        }
        button {
            font-weight: bold;
            cursor: pointer;
            border: none;
        }
        .register {
            background-color: #388e3c;
            color: white;
        }
        .disconnect {
            background-color: transparent;
            color: #388e3c;
            border: 1px solid #388e3c;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Bienvenido a SmartPlant</h2>
        <p>Gestiona tu cultivo de manera inteligente</p>
        <input type="text" id="username" placeholder="Usuario">
        <input type="password" id="password" placeholder="Contraseña">
        <button class="register" onclick="registerUser()">Registrarse</button>
        <button class="disconnect" onclick="disconnect()">Desconectarse</button>
    </div>
    <script>
        function registerUser() {
            let username = document.getElementById("username").value;
            let password = document.getElementById("password").value;
            alert("Usuario: " + username + " registrado!");
        }
        function disconnect() {
            alert("Desconectado");
        }
    </script>
</body>
</html>
)rawliteral";
