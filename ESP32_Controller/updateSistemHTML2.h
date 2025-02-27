const char* updateForm2 = R"rawliteral(


        function actualizarFormulario() {
            document.getElementById('activarSistema').checked = valoresESP32.dispositivoActivo === "Activo";
            document.getElementById('fotoperiodo').value = valoresESP32.fotoperiodo;
            document.getElementById('luzAzul').value = valoresESP32.luzAzul;
            document.getElementById('luzAzulValue').innerText = `${valoresESP32.luzAzul}%`;
            document.getElementById('luzRoja').value = valoresESP32.luzRoja;
            document.getElementById('luzRojaValue').innerText = `${valoresESP32.luzRoja}%`;
            document.getElementById('luzBlanca').value = valoresESP32.luzBlanca;
            document.getElementById('luzBlancaValue').innerText = `${valoresESP32.luzBlanca}%`;
            document.getElementById('horasIrrigacion').value = valoresESP32.horasIrrigacion;
            document.getElementById('minutosIrrigacion').value = valoresESP32.minutosIrrigacion;
            document.getElementById('horasVentilador').value = valoresESP32.horasVentilador;
            document.getElementById('minutosVentilador').value = valoresESP32.minutosVentilador;
        }

        document.addEventListener('DOMContentLoaded', actualizarFormulario);
        document.getElementById('miFormulario').addEventListener('submit', function (event) {

            event.preventDefault();
            const formData = new FormData(this);
            const data = {};
            let errores = [];

            formData.forEach((value, key) => {
                if (key === 'pass' && (value.length < 8 || value.length > 32)) {
                    errores.push("La contraseña debe tener al menos 8 caracteres.");
                }
                if (key === 'fp' || key === 'irrH' || key === 'irrM' || key === 'ventH' || key === 'ventM') {
                    if (value === "" || isNaN(value)) {
                        errores.push(`${key} debe ser un número.`);
                    } else {
                        const num = parseInt(value);
                        if ((key === 'fp' && (num < 0 || num > 23)) ||
                            (key === 'irrH' && (num < 0 || num > 23)) ||
                            (key === 'irrM' && (num < 0 || num > 59)) ||
                            (key === 'ventH' && (num < 0 || num > 23)) ||
                            (key === 'ventM' && (num < 0 || num > 59))) {
                            errores.push(`${key} está fuera del rango permitido.`);
                        }
                    }
                }

                data[key] = value;

            });

            const checkbox = document.getElementById('activarSistema');
            data['enable'] = checkbox.checked;

            if (errores.length > 0) {
                alert("Errores en el formulario:\n" + errores.join("\n"));
                return;  
            }

            const fechaHoraActual = new Date();
            data['dia'] = fechaHoraActual.getDate();
            data['mes'] = fechaHoraActual.getMonth() + 1;
            data['anio'] = fechaHoraActual.getFullYear().toString().slice(-2);
            data['diaSem'] = fechaHoraActual.getDay();
            data['hr'] = fechaHoraActual.getHours();
            data['min'] = fechaHoraActual.getMinutes();
            data['seg'] = fechaHoraActual.getSeconds();

            fetch('http://192.168.4.1/data', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            })
                .then(response => response.json())
                .then(jsonData => {
                    document.body.innerHTML = '';
                    const respuestaDiv = document.createElement('div');
                    respuestaDiv.innerHTML = `<h1>Respuesta del servidor</h1>
                                          <p>Status: ${jsonData.status}</p>
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