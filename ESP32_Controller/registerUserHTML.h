const char* wellcomeForm = R"rawliteral(
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
        button {
            width: calc(100% - 20px);
            padding: 10px;
            margin-bottom: 10px;
            border-radius: 8px;
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
        <button class="register" onclick="registerUser()">Registrarse</button>
        <button class="disconnect" onclick="disconnect()">Desconectarse</button>
    </div>
    <script>
        function registerUser() {
            fetch('http://192.168.4.1/registeru', { method: 'GET' })
              .then(response => {
                  console.log(response.headers.get("Content-Type")); // Verifica si es "text/html"
                  return response.text();
              })
              .then(data => {
                  console.log("Contenido recibido:", data); // Depura el HTML recibido
                  document.body.innerHTML = data;
              })
              .catch(error => console.error("Error en fetch:", error));
        }
        function disconnect() { 
            fetch('http://192.168.4.1/exit', { method: 'GET' })
                .then(response => response.text())
                .then(data => alert("Desconectado: " + data))
                .catch(error => alert("Error al desconectar: " + error));
        }
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