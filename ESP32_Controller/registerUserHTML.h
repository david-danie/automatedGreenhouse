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
            color: #126b17;
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
            box-sizing: border-box;
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
        <p>Gestiona tu cultivo de manera inteligente.</p>
        <p>¿Deseas registrar un usuario?</p>
        <form id="wellcomeForm">
            <button class="register" type="submit" value="registeru">Registrar</button>
            <button class="disconnect" type="submit" value="exit">Salir</button>
        </form>

    </div>
    <script>
        document.getElementById('wellcomeForm').addEventListener('submit', function (event) {
            event.preventDefault();
        
            const form = event.target;
            const submitter = event.submitter;
        
            let url = '';
            let method = 'POST';
        
            if (submitter.value === 'registeru') {
                url = '/registeru';
            } else if (submitter.value === 'exit') {
                url = '/exit';
                method = 'GET';
            } else {
                return;
            }
        
            fetch(url, {
                method: method
            })
            .then(response => {
                const contentType = response.headers.get("content-type") || "";
                if (contentType.includes("application/json")) {
                    return response.json().then(data => {
                        document.body.innerHTML = '';
                        const respuestaDiv = document.createElement('div');
                        respuestaDiv.innerHTML = `
                            <h1>Respuesta del dispositivo</h1>
                            <p>Status: ${data.status}</p>
                            <p>Mensaje: ${data.msg}</p>`;
                        document.body.appendChild(respuestaDiv);
                    });
                } else {
                    return response.text().then(html => {
                        document.body.innerHTML = html;
                    });
                }
            })
            .catch((error) => {
                console.error('Error:', error);
                document.body.innerHTML = '<p style="color:red;">Error de conexión con el dispositivo.</p>';
            });
        });
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
        <p>Registra un usuario y contraseña</p>
        <form id="registerForm">
            <input type="text" id="username" placeholder="Usuario">
            <input type="password" id="password" placeholder="Contraseña">
            <button class="register" type="submit" name="action" value="userData">Registrar</button>
            <button class="disconnect" type="submit" name="action" value="exit">Desconectarse</button>
        </form>
    </div>
    <script>
        document.getElementById('registerForm').addEventListener('submit', function (event) {
            event.preventDefault();
        
            const form = event.target;
            const submitter = event.submitter;
        
            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value.trim();
        
            let errores = [];
            const data = { username, password };
        
            let url = '';
            let method = 'POST';
            let useBody = true;
        
            if (submitter.value === 'userData') {
                url = 'http://192.168.4.1/userData';
        
                // Validación de longitud
                if (username.length < 8 || username.length > 22) {
                    errores.push("El usuario debe tener entre 8 y 22 caracteres.");
                }
                if (password.length < 8 || password.length > 22) {
                    errores.push("La contraseña debe tener entre 8 y 22 caracteres.");
                }
        
                // No más de 3 caracteres repetidos seguidos
                const repetidosRegex = /(.)\1{3,}/;
                if (repetidosRegex.test(username)) {
                    errores.push("El usuario no puede contener más de 3 caracteres repetidos seguidos.");
                }
                if (repetidosRegex.test(password)) {
                    errores.push("La contraseña no puede contener más de 3 caracteres repetidos seguidos.");
                }
        
                if (errores.length > 0) {
                    alert("Errores en el formulario:\n" + errores.join("\n"));
                    return;
                }
        
            } else if (submitter.value === 'exit') {
                url = 'http://192.168.4.1/exit';
                method = 'GET';
                useBody = false;
            } else {
                return;
            }
        
            fetch(url, {
                method: method,
                headers: {
                    'Content-Type': 'application/json'
                },
                ...(useBody ? { body: JSON.stringify(data) } : {})
            })
            .then(response => response.json())
            .then(jsonData => {
                document.body.innerHTML = '';
                const respuestaDiv = document.createElement('div');
                respuestaDiv.innerHTML = `<h1>Respuesta del dispositivo.</h1>
                                          <p>Status: ${jsonData.status}.</p>
                                          <p>Mensaje: ${jsonData.msg}</p>`;
                document.body.appendChild(respuestaDiv);
            })
            .catch((error) => {
                console.error('Error:', error);
            });
        });
        </script>      
</body>
</html>
)rawliteral";