static const char dashboardForm[] = R"===(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="theme-color" content="#4caf50">
    <title>Actualiza parámetros</title>
    <style>
        /* ========== RESET ========== */
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        /* ========== BASE ========== */
        body {
            font-family: system-ui, -apple-system, sans-serif;
            background: #c6e6c6;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 16px;
        }

        .form-card {
            background: white;
            border-radius: 16px;
            box-shadow: 0 8px 32px rgba(76, 175, 80, 0.15);
            padding: 32px;
            width: 100%;
            max-width: 400px;
            position: relative;
            overflow: hidden;
        }

        .form-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 4px;
            background: linear-gradient(90deg, #4caf50, #8bc34a, #cddc39);
        }

        /* ========== TRANSICIÓN ENTRE PASOS ========== */
        /* El alto del form se anima desde JS (transition: height) para que la
           tarjeta crezca/encoja suave en vez de pegar un brinco al cambiar de paso. */
        #mainForm {
            transition: height 0.35s ease;
        }

        /* Mientras animamos el alto, recortamos el desborde del paso entrante. */
        #mainForm.is-animating {
            overflow: hidden;
        }

        /* Clases que JS aplica al paso que entra, según la dirección. */
        .step-enter-forward {
            animation: stepFromRight 0.35s ease both;
        }

        .step-enter-back {
            animation: stepFromLeft 0.35s ease both;
        }

        @keyframes stepFromRight {
            from {
                opacity: 0;
                transform: translateX(28px);
            }

            to {
                opacity: 1;
                transform: translateX(0);
            }
        }

        @keyframes stepFromLeft {
            from {
                opacity: 0;
                transform: translateX(-28px);
            }

            to {
                opacity: 1;
                transform: translateX(0);
            }
        }

        /* Stepper "1 · 2" */
        .stepper {
            display: flex;
            justify-content: center;
            gap: 8px;
            margin-bottom: 12px;
        }

        .step-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: #c8e6c9;
            transition: background 0.3s ease, transform 0.3s ease;
        }

        .step-dot.active {
            background: #4caf50;
            transform: scale(1.25);
        }

        /* Respeta a quienes prefieren menos movimiento */
        @media (prefers-reduced-motion: reduce) {
            #mainForm {
                transition: none;
            }

            .step-enter-forward,
            .step-enter-back {
                animation: none;
            }

            .step-dot {
                transition: none;
            }
        }

        /* ========== HEADER ========== */
        .form-header {
            text-align: center;
        }

        .form-header h1 {
            color: #2e7d32;
            font-size: 24px;
            font-weight: 500;
            margin-bottom: 8px;
        }

        /* ========== ENCABEZADOS DE SECCIÓN ========== */
        /* Resaltan a qué se refiere cada grupo de campos (General, Iluminación, etc.). */
        .section-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 8px;
            margin: 22px 0 12px;
            color: #2e7d32;
            font-size: 15px;
            font-weight: 600;
        }

        .section-header:first-child {
            margin-top: 0;
        }

        /* Toggle "Sistema activo" alineado a la derecha del encabezado (como params2). */
        .section-toggle {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 13px;
            font-weight: 400;
            color: #2e7d32;
            cursor: pointer;
        }

        /* ========== FORM ========== */
        .password-wrapper,
        .form-group {
            position: relative;
            display: flex;
            flex-direction: column;
            gap: 6px;
            margin-bottom: 16px;
        }

        .form-group2 {
            position: relative;
            display: flex;
            justify-content: space-between;
            flex-direction: row;
            gap: 6px;
            margin-bottom: 16px;
        }

        /* Los campos dentro de una fila doble no llevan su propio margen inferior:
           el espaciado lo aporta .form-group2, para que todas las filas (simples
           o dobles) queden separadas de forma uniforme. */
        .form-group2 .form-group {
            margin-bottom: 0;
        }

        .form-group label {
            font-size: 13px;
            color: #2e7d32;
        }

        .select-container {
            position: relative;
        }

        .select-container::after {
            content: '▾';
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            color: #4caf50;
            pointer-events: none;
            font-size: 14px;
        }

        select {
            padding-right: 30px;
        }

        select {
            width: 100%;
            padding: 10px 12px;
            border: 1px solid #c8e6c9;
            border-radius: 6px;
            font-size: 14px;
            color: #383838;
            background: white;
            cursor: pointer;
            transition: all 0.3s ease;
            appearance: none;
            -webkit-appearance: none;
            -moz-appearance: none;
        }

        .form-group input[type="text"],
        .form-group input[type="number"],
        .form-group input[type="password"] {
            padding: 10px 12px;
            font-size: 14px;
            border-radius: 6px;
            border: 1px solid #c8e6c9;
            color: #383838;
            background: white;
            transition: all 0.3s ease;
        }

        /* ========== SLIDERS UNIVERSALES ========== */
        /* Contenedor del slider con progreso visual */
        .slider-container {
            position: relative;
            height: 8px;
            background: #e0e0e0;
            border-radius: 4px;
            margin-top: 4px;
        }

        .slider-progress {
            position: absolute;
            top: 0;
            left: 0;
            height: 8px;
            border-radius: 4px;
            pointer-events: none;
            width: 100%;
        }

        /* Slider base - posicionado absolutamente sobre la barra */
        input[type="range"] {
            -webkit-appearance: none;
            appearance: none;
            width: 100%;
            background: transparent;
            outline: none;
            position: absolute;
            top: 50%;
            left: 0;
            transform: translateY(-50%);
            z-index: 2;
            cursor: pointer;
            height: 20px;
            margin: 0;
        }

        /* Track (rail) - hacerlo invisible */
        input[type="range"]::-webkit-slider-runnable-track {
            width: 100%;
            height: 8px;
            background: transparent;
            border: none;
        }

        input[type="range"]::-moz-range-track {
            width: 100%;
            height: 8px;
            background: transparent;
            border: none;
        }

        /* Thumbs base para todos */
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            cursor: pointer;
            margin-top: -6px;
        }

        input[type="range"]::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            cursor: pointer;
            border: none;
        }

        /* Slider Azul */
        #ledA::-webkit-slider-thumb {
            background: #2196F3;
            box-shadow: 0 2px 8px rgba(33, 150, 243, 0.5);
        }

        #ledA::-moz-range-thumb {
            background: #2196F3;
            box-shadow: 0 2px 8px rgba(33, 150, 243, 0.5);
        }

        /* Slider Rojo */
        #ledR::-webkit-slider-thumb {
            background: #f44336;
            box-shadow: 0 2px 8px rgba(244, 67, 54, 0.5);
        }

        #ledR::-moz-range-thumb {
            background: #f44336;
            box-shadow: 0 2px 8px rgba(244, 67, 54, 0.5);
        }

        /* Slider Blanco */
        #ledB::-webkit-slider-thumb {
            background: #ffffff;
            border: 2px solid #9e9e9e;
            box-shadow: 0 2px 8px rgba(158, 158, 158, 0.5);
        }

        #ledB::-moz-range-thumb {
            background: #ffffff;
            border: 2px solid #9e9e9e;
            box-shadow: 0 2px 8px rgba(158, 158, 158, 0.5);
        }

        .toggle-password {
            position: absolute;
            right: 3%;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            color: #757575;
            cursor: pointer;
            font-size: 18px;
            padding: 8px;
            line-height: 1;
        }

        .toggle-password:hover {
            color: #4caf50;
        }

        /* ========== BUTTONS ========== */
        .buttons {
            display: flex;
            gap: 12px;
            margin-top: 22px;
        }

        button {
            flex: 1;
            padding: 16px 24px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }

        .btn-primary {
            background: linear-gradient(135deg, #4caf50, #66bb6a);
            color: white;
            box-shadow: 0 4px 16px rgba(76, 175, 80, 0.3);
        }

        .btn-secondary {
            background: linear-gradient(135deg, #ff7043, #ff8a65);
            color: white;
            box-shadow: 0 4px 16px rgba(255, 112, 67, 0.3);
        }

        .footer {
            padding-top: 32px;
            text-align: center;
        }

        .footer p {
            color: #2e7d32;
            font-size: 14px;
        }

        /* ========== RESPONSE ========== */
        .mensaje-ok,
        .mensaje-error {
            padding: 20px;
            border-radius: 8px;
            font-size: 1.1rem;
            text-align: center;
        }

        .mensaje-ok {
            background: #e8f5e9;
            border: 2px solid #4caf50;
            color: #2e7d32;
        }

        .mensaje-error {
            background: #ffebee;
            border: 2px solid #f44336;
            color: #c62828;
        }

        /* ========== DASHBOARD (modo vista) ========== */
        /* Presenta los datos del dispositivo como tarjetas en vez de un form
           deshabilitado: escala mejor conforme se agreguen más parámetros. */
        .dash-hero {
            background: linear-gradient(135deg, #4caf50, #66bb6a);
            border-radius: 12px;
            padding: 18px 20px;
            color: white;
            margin-bottom: 20px;
        }

        .dash-hero-top {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
            margin-bottom: 16px;
        }

        .dash-plant {
            font-size: 20px;
            font-weight: 600;
        }

        /* Píldora de estado (Activo / Inactivo) con punto indicador. */
        .dash-status {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            font-size: 12px;
            font-weight: 600;
            padding: 4px 10px;
            border-radius: 999px;
            background: rgba(255, 255, 255, 0.22);
            white-space: nowrap;
        }

        .dash-status::before {
            content: '';
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: #fff;
        }

        .dash-status.is-off {
            opacity: 0.85;
        }

        .dash-status.is-off::before {
            background: #ffcdd2;
        }

        /* Semana / Día del ciclo del cultivo. */
        .dash-cycle {
            display: flex;
            gap: 12px;
        }

        .dash-cycle-item {
            flex: 1;
            background: rgba(255, 255, 255, 0.15);
            border-radius: 8px;
            padding: 10px;
            text-align: center;
        }

        .dash-cycle-num {
            display: block;
            font-size: 24px;
            font-weight: 700;
            line-height: 1;
        }

        .dash-cycle-lbl {
            display: block;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-top: 4px;
            opacity: 0.9;
        }

        .dash-section {
            margin-bottom: 18px;
        }

        .dash-section-title {
            color: #2e7d32;
            font-size: 13px;
            font-weight: 600;
            margin-bottom: 8px;
        }

        .dash-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
        }

        /* Las dos tarjetas del fotoperiodo (horas + espectros) se apilan. */
        .dash-stack {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }

        /* Tarjeta de horas: Prendido | Apagado dentro de una sola card. */
        .dash-duo {
            display: flex;
            align-items: center;
        }

        .dash-duo-item {
            flex: 1;
        }

        .dash-duo-sep {
            align-self: stretch;
            width: 1px;
            background: #c8e6c9;
            margin: 0 12px;
        }

        /* Tarjeta de espectros: 3 columnas (azul, rojo, blanco), cada una con
           su punto de color, porcentaje y etiqueta. */
        .dash-spectrum-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            text-align: center;
        }

        .dash-spectrum-col {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 4px;
        }

        /* Punto de color a la izquierda del porcentaje, en la misma línea. */
        .dash-spectrum-top {
            display: inline-flex;
            align-items: center;
            gap: 6px;
        }

        .dash-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
        }

        /* El punto blanco necesita borde para verse sobre el fondo claro. */
        .dash-dot.is-white {
            border: 1px solid #9e9e9e;
        }

        .dash-spectrum-val {
            font-size: 16px;
            font-weight: 600;
            color: #2e3a2e;
        }

        .dash-spectrum-col .dash-stat-label {
            margin-bottom: 0;
        }

        /* Mismos border-radius/borde que los inputs del form para mantener
           la coherencia visual entre el modo vista y el modo edición. */
        .dash-stat {
            background: #f1f8e9;
            border: 1px solid #c8e6c9;
            border-radius: 6px;
            padding: 12px;
        }

        .dash-stat-label {
            font-size: 11px;
            color: #689f38;
            margin-bottom: 4px;
        }

        .dash-stat-value {
            font-size: 18px;
            font-weight: 600;
            color: #2e3a2e;
        }

        .dash-stat-value small {
            font-size: 12px;
            font-weight: 500;
            color: #7a8a7a;
        }

        /* ========== RESPONSIVE ========== */
        @media (max-width: 480px) {
            .form-card {
                padding: 24px;
                margin: 10px;
            }

            .form-header h1 {
                font-size: 20px;
            }

            .buttons {
                flex-direction: column;
            }

            button {
                padding: 14px 20px;
                font-size: 14px;
            }

            .toggle-password {
                font-size: 18px;
            }
        }

        @media (max-width: 320px) {
            .form-card {
                padding: 20px;
            }

            .form-header h1 {
                font-size: 18px;
            }
        }
    </style>
</head>

<body>
    <div class="form-card" id="form-card">
        <div class="form-header">
            <h1 id="cardTitle">Parámetros del dispositivo</h1>
            <div class="stepper" id="stepper" aria-hidden="true" style="display:none;">
                <span class="step-dot active" data-step="1"></span>
                <span class="step-dot" data-step="2"></span>
            </div>
        </div>

        <form id="mainForm">
            <div id="dashboard">
                <div class="dash-hero">
                    <div class="dash-hero-top">
                        <span class="dash-plant" id="dashPlanta">—</span>
                        <span class="dash-status" id="dashStatus">—</span>
                    </div>
                    <div class="dash-cycle">
                        <div class="dash-cycle-item">
                            <span class="dash-cycle-num" id="dashSemana">—</span>
                            <span class="dash-cycle-lbl">Semana</span>
                        </div>
                        <div class="dash-cycle-item">
                            <span class="dash-cycle-num" id="dashDia">—</span>
                            <span class="dash-cycle-lbl">Día</span>
                        </div>
                    </div>
                </div>

                <div class="dash-section">
                    <div class="dash-section-title">Fotoperiodo</div>
                    <div class="dash-stack">
                        <div class="dash-stat">
                            <div class="dash-duo">
                                <div class="dash-duo-item">
                                    <div class="dash-stat-label">Prendido</div>
                                    <div class="dash-stat-value"><span id="dashFpOn">—</span> <small>h</small></div>
                                </div>
                                <div class="dash-duo-sep"></div>
                                <div class="dash-duo-item">
                                    <div class="dash-stat-label">Apagado</div>
                                    <div class="dash-stat-value"><span id="dashFpOff">—</span> <small>h</small></div>
                                </div>
                            </div>
                        </div>
                        <div class="dash-stat">
                            <div class="dash-spectrum-grid">
                                <div class="dash-spectrum-col">
                                    <div class="dash-spectrum-top">
                                        <span class="dash-dot" style="background:#2196F3"></span>
                                        <span class="dash-spectrum-val" id="dashLedA">—</span>
                                    </div>
                                    <span class="dash-stat-label">Espectro Azul</span>
                                </div>
                                <div class="dash-spectrum-col">
                                    <div class="dash-spectrum-top">
                                        <span class="dash-dot" style="background:#f44336"></span>
                                        <span class="dash-spectrum-val" id="dashLedR">—</span>
                                    </div>
                                    <span class="dash-stat-label">Espectro Rojo</span>
                                </div>
                                <div class="dash-spectrum-col">
                                    <div class="dash-spectrum-top">
                                        <span class="dash-dot is-white" style="background:#ffffff"></span>
                                        <span class="dash-spectrum-val" id="dashLedB">—</span>
                                    </div>
                                    <span class="dash-stat-label">Espectro Blanco</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <div class="dash-section">
                    <div class="dash-section-title">Riego</div>
                    <div class="dash-grid">
                        <div class="dash-stat">
                            <div class="dash-stat-label">Frecuencia</div>
                            <div class="dash-stat-value" id="dashIrrH">—</div>
                        </div>
                        <div class="dash-stat">
                            <div class="dash-stat-label">Duración</div>
                            <div class="dash-stat-value"><span id="dashIrrM">—</span> <small>min</small></div>
                        </div>
                    </div>
                </div>

                <div class="dash-section">
                    <div class="dash-section-title">Ventilación</div>
                    <div class="dash-grid">
                        <div class="dash-stat">
                            <div class="dash-stat-label">Frecuencia</div>
                            <div class="dash-stat-value" id="dashVentH">—</div>
                        </div>
                        <div class="dash-stat">
                            <div class="dash-stat-label">Duración</div>
                            <div class="dash-stat-value"><span id="dashVentM">—</span> <small>min</small></div>
                        </div>
                    </div>
                </div>

                <div class="buttons">
                    <button type="button" class="btn-primary" id="btnEdit">Editar parámetros</button>
                    <button type="button" class="btn-secondary" id="btnExit">Salir</button>
                </div>
            </div>

            <div id="stepParams" style="display:none;">
                <div class="section-header">
                    <span>General</span>
                    <label class="section-toggle" for="enable">
                        Sistema activo
                        <input type="checkbox" id="enable" name="enable">
                    </label>
                </div>

                <div class="form-group">
                    <label for="planta">Nombre de la planta</label>
                    <input type="text" name="planta" autocomplete="off" id="planta" maxlength="20" required>
                </div>

                <div class="section-header">
                    <span>Fotoperiodo</span>
                </div>

                <div class="form-group2">
                    <div class="form-group">
                        <label for="fpOn">Prendido (h)</label>
                        <input type="number" id="fpOn" name="fpOn" min="0" max="23" step="1" required>
                    </div>

                    <div class="form-group">
                        <label for="fpOff">Apagado (h)</label>
                        <input type="number" id="fpOff" name="fpOff" min="0" max="23" step="1" required>
                    </div>
                </div>

                <div class="form-group">
                    <label for="ledA">Espectro Azul: <span id="luzAzulValue">50%</span></label>
                    <div class="slider-container">
                        <input type="range" id="ledA" name="ledA" min="0" max="100" step="5" value="50">
                        <div class="slider-progress" id="progressA"></div>
                    </div>
                </div>

                <div class="form-group">
                    <label for="ledR">Espectro Rojo: <span id="luzRojaValue">50%</span></label>
                    <div class="slider-container">
                        <input type="range" id="ledR" name="ledR" min="0" max="100" step="5" value="50">
                        <div class="slider-progress" id="progressR"></div>
                    </div>
                </div>

                <div class="form-group">
                    <label for="ledB">Espectro Blanco: <span id="luzBlancaValue">50%</span></label>
                    <div class="slider-container">
                        <input type="range" id="ledB" name="ledB" min="0" max="100" step="5" value="50">
                        <div class="slider-progress" id="progressB"></div>
                    </div>
                </div>

                <div class="section-header">
                    <span>Riego</span>
                </div>

                <div class="form-group2">
                    <div class="form-group">
                        <label for="irrH">Frecuencia</label>
                        <div class="select-container">
                            <select id="irrH" name="irrH" required>
                                <option value="0">0 veces al día</option>
                                <option value="1">1 vez al día</option>
                                <option value="2">2 veces al día</option>
                                <option value="3">4 veces al día</option>
                                <option value="4">8 veces al día</option>
                                <option value="5">12 veces al día</option>
                                <option value="6">24 veces al día</option>
                            </select>
                        </div>
                    </div>

                    <div class="form-group">
                        <label for="irrM">Minutos</label>
                        <input type="number" name="irrM" required id="irrM" min="0" max="59" step="1">
                    </div>
                </div>

                <div class="section-header">
                    <span>Ventilación</span>
                </div>

                <div class="form-group2">
                    <div class="form-group">
                        <label for="ventH">Frecuencia</label>
                        <div class="select-container">
                            <select id="ventH" name="ventH" required>
                                <option value="0">0 veces al día</option>
                                <option value="1">1 vez al día</option>
                                <option value="2">2 veces al día</option>
                                <option value="3">4 veces al día</option>
                                <option value="4">8 veces al día</option>
                                <option value="5">12 veces al día</option>
                                <option value="6">24 veces al día</option>
                            </select>
                        </div>
                    </div>

                    <div class="form-group">
                        <label for="ventM">Minutos</label>
                        <input type="number" name="ventM" required id="ventM" min="0" max="59" step="1">
                    </div>
                </div>

                <div class="buttons">
                    <button type="submit" class="btn-primary">Actualizar</button>
                    <button type="button" class="btn-secondary" id="btnCancel">Cancelar</button>
                </div>
            </div>

            <div id="stepAuth" style="display:none;">
                <div class="form-group">
                    <label for="user">Usuario</label>
                    <input type="text" name="user" id="user" autocomplete="off" maxlength="32" required>
                </div>

                <div class="form-group">
                    <label for="password">Contraseña</label>
                    <div class="password-wrapper">
                        <input type="password" id="password" name="pass" autocomplete="new-password" maxlength="64" required>
                        <button type="button" class="toggle-password" onclick="togglePassword()" id="toggleBtn"
                            aria-label="Mostrar contraseña">👁</button>
                    </div>
                </div>

                <div class="buttons">
                    <button type="button" class="btn-secondary" id="btnBack">Volver</button>
                    <button type="button" class="btn-primary" id="btnAuthContinue">Continuar</button>
                </div>
            </div>
        </form>

        <div class="footer">
            <p>© <span id="year">2026</span> Smartplant • Versión 1.0</p>
        </div>
    </div>

    <script>
        // Endpoint del ESP32 que devuelve los parámetros actuales (GET → JSON).
        // Es la contraparte de lectura de /newparams (escritura). Si el firmware
        // expone otra ruta, cámbiala aquí.
        const PARAMS_ENDPOINT = "/getparams";

        // Valores que reporta el dispositivo. semana/dia = avance del ciclo del
        // cultivo (no es la fecha del reloj que se envía en /newparams).
        // Estos valores son solo un respaldo: si /getparams no responde, el
        // dashboard se sigue pudiendo visualizar sin backend.
        var valoresESP32 = {
            nombrePlanta: "Planta",
            dispositivoActivo: false,
            fotoperiodo: 0,
            fotoperiodoApagado: 0,
            luzAzul: 0,
            luzRoja: 0,
            luzBlanca: 0,
            horasIrrigacion: 0,
            minutosIrrigacion: 0,
            horasVentilador: 0,
            minutosVentilador: 0,
            semana: 0,
            dia: 0,
        }

        // Etiquetas legibles para los selects de frecuencia (valor → texto).
        const FREQ_LABELS = {
            0: "0 × día", 1: "1 × día", 2: "2 × día", 3: "4 × día",
            4: "8 × día", 5: "12 × día", 6: "24 × día"
        };

        // Mapa de las claves internas de valoresESP32 a los alias que puede
        // mandar el dispositivo. La primera coincidencia presente en el JSON
        // gana. Se aceptan tanto las claves de /newparams (planta, fpOn, ledA…)
        // como los nombres internos, por si el firmware cambia de esquema.
        const PARAM_ALIASES = {
            nombrePlanta: ["planta", "nombrePlanta"],
            dispositivoActivo: ["enable", "dispositivoActivo"],
            fotoperiodo: ["fpOn", "fotoperiodo"],
            fotoperiodoApagado: ["fpOff", "fotoperiodoApagado"],
            luzAzul: ["ledA", "luzAzul"],
            luzRoja: ["ledR", "luzRoja"],
            luzBlanca: ["ledB", "luzBlanca"],
            horasIrrigacion: ["irrH", "horasIrrigacion"],
            minutosIrrigacion: ["irrM", "minutosIrrigacion"],
            horasVentilador: ["ventH", "horasVentilador"],
            minutosVentilador: ["ventM", "minutosVentilador"],
            semana: ["semana"],
            dia: ["dia"],
        };

        // Vuelca un JSON crudo del dispositivo sobre valoresESP32, tomando solo
        // las claves presentes (las ausentes conservan su valor de respaldo).
        function aplicarValoresDispositivo(json) {
            if (!json || typeof json !== "object") return;
            for (const [interna, alias] of Object.entries(PARAM_ALIASES)) {
                const clave = alias.find((k) => json[k] !== undefined && json[k] !== null);
                if (clave === undefined) continue;
                valoresESP32[interna] = json[clave];
            }
        }

        // Lee los parámetros actuales del ESP32 y los vuelca en valoresESP32.
        // Devuelve true si la lectura fue exitosa; si falla, conserva los valores
        // previos (respaldo) y avisa en consola, sin romper la pantalla.
        async function obtenerValoresDispositivo() {
            try {
                const res = await fetch(PARAMS_ENDPOINT, {
                    headers: { "Accept": "application/json" }
                });
                if (!res.ok) throw new Error("HTTP " + res.status);
                aplicarValoresDispositivo(await res.json());
                return true;
            } catch (e) {
                console.warn(
                    "No se pudieron cargar los parámetros del dispositivo; " +
                    "se usan los valores de respaldo.", e
                );
                return false;
            }
        }

        // Carga inicial: pide los datos al ESP32 y pinta el dashboard + el form.
        async function cargarParametros() {
            await obtenerValoresDispositivo();
            actualizarFormulario();
            actualizarDashboard();
            setEstado("view");
        }

        function actualizarFormulario() {
            document.getElementById('planta').value = valoresESP32.nombrePlanta || "";
            document.getElementById('enable').checked = valoresESP32.dispositivoActivo === true || valoresESP32.dispositivoActivo === "true";
            document.getElementById('fpOn').value = valoresESP32.fotoperiodo;
            document.getElementById('fpOff').value = valoresESP32.fotoperiodoApagado;
            document.getElementById('ledA').value = valoresESP32.luzAzul;
            document.getElementById('luzAzulValue').innerText = `${valoresESP32.luzAzul}%`;
            document.getElementById('ledR').value = valoresESP32.luzRoja;
            document.getElementById('luzRojaValue').innerText = `${valoresESP32.luzRoja}%`;
            document.getElementById('ledB').value = valoresESP32.luzBlanca;
            document.getElementById('luzBlancaValue').innerText = `${valoresESP32.luzBlanca}%`;
            document.getElementById('irrH').value = valoresESP32.horasIrrigacion;
            document.getElementById('irrM').value = valoresESP32.minutosIrrigacion;
            document.getElementById('ventH').value = valoresESP32.horasVentilador;
            document.getElementById('ventM').value = valoresESP32.minutosVentilador;

            // Actualizar barras de progreso
            updateSliderProgress('ledA', 'progressA');
            updateSliderProgress('ledR', 'progressR');
            updateSliderProgress('ledB', 'progressB');
        }

        // Vuelca los valores del dispositivo en el dashboard (modo vista).
        function actualizarDashboard() {
            const v = valoresESP32;
            const activo = v.dispositivoActivo === true || v.dispositivoActivo === "true";

            document.getElementById('dashPlanta').textContent = v.nombrePlanta || "Planta";

            const status = document.getElementById('dashStatus');
            status.textContent = activo ? "Activo" : "Inactivo";
            status.classList.toggle('is-off', !activo);

            document.getElementById('dashSemana').textContent = v.semana ?? "—";
            document.getElementById('dashDia').textContent = v.dia ?? "—";

            document.getElementById('dashFpOn').textContent = v.fotoperiodo;
            document.getElementById('dashFpOff').textContent = v.fotoperiodoApagado;

            setDashLed('dashLedA', v.luzAzul);
            setDashLed('dashLedR', v.luzRoja);
            setDashLed('dashLedB', v.luzBlanca);

            document.getElementById('dashIrrH').textContent = FREQ_LABELS[v.horasIrrigacion] ?? "—";
            document.getElementById('dashIrrM').textContent = v.minutosIrrigacion;
            document.getElementById('dashVentH').textContent = FREQ_LABELS[v.horasVentilador] ?? "—";
            document.getElementById('dashVentM').textContent = v.minutosVentilador;
        }

        function setDashLed(valueId, val) {
            const n = Number(val) || 0;
            document.getElementById(valueId).textContent = `${n}%`;
        }

        document.addEventListener('DOMContentLoaded', () => {
            // Pide los datos al ESP32 y, con la respuesta (o el respaldo si
            // falla), pinta el dashboard y prepara el form de edición.
            cargarParametros();
        });

        const form = document.getElementById("mainForm");
        const dashboard = document.getElementById("dashboard");
        const stepParams = document.getElementById("stepParams");
        const stepAuth = document.getElementById("stepAuth");
        const btnEdit = document.getElementById("btnEdit");
        const btnExit = document.getElementById("btnExit");
        const btnCancel = document.getElementById("btnCancel");
        const btnBack = document.getElementById("btnBack");
        const btnAuthContinue = document.getElementById("btnAuthContinue");
        const stepper = document.getElementById("stepper");
        const cardTitle = document.getElementById("cardTitle");

        // Estado de la pantalla: "view" (solo lectura) | "auth" (credenciales) | "edit" (editable)
        let estado = "view";

        document.getElementById("year").textContent = new Date().getFullYear();

        let errores = [];

        // ── Validaciones unificadas con el ESP32 ──────────────────────────
        // Réplica en JS de isValidReadableString() y hasTooManyRepeatedChars()
        // del firmware, para que el formulario rechace EXACTAMENTE lo mismo que
        // el dispositivo (mismo set de caracteres y misma regla de repetidos) y
        // no queden validaciones que pasen aquí pero el ESP32 rechace después.

        // Caracteres permitidos: letras (incluidas vocales acentuadas y ñ/Ñ del
        // español), dígitos y (_-.@!#$%&*?+=). El espacio solo se admite cuando
        // allowSpaces es true (p. ej. el nombre de la planta). Replica el mismo
        // set que isValidReadableString() del ESP32, acentos y ñ incluidos.
        const READABLE_SYMBOLS = "_-.@!#$%&*?+=";
        const READABLE_LETTER = /[a-zA-Z0-9áéíóúüÁÉÍÓÚÜñÑ]/;

        function isValidReadableString(str, allowSpaces) {
            for (const c of str) {
                if (READABLE_LETTER.test(c)) continue;
                if (READABLE_SYMBOLS.includes(c)) continue;
                if (allowSpaces && c === ' ') continue;
                return false;
            }
            return true;
        }

        // Rechaza 4 o más caracteres idénticos consecutivos (aaaa, 1111…),
        // igual que hasTooManyRepeatedChars() del ESP32 (count > 3).
        function hasTooManyRepeatedChars(str) {
            let count = 1;
            for (let i = 1; i < str.length; i++) {
                if (str[i] === str[i - 1]) {
                    if (++count > 3) return true;
                } else {
                    count = 1;
                }
            }
            return false;
        }

        function isStrictInteger(value) {
            return /^[0-9]+$/.test(value);
        }

        function togglePassword() {
            const type = password.type === 'password' ? 'text' : 'password';
            password.type = type;
            const btn = document.getElementById('toggleBtn');
            btn.textContent = type === 'password' ? '👁' : '🙈';
            btn.setAttribute('aria-label', type === 'password' ? 'Mostrar contraseña' : 'Ocultar contraseña');
        }

        function actualizarStepper(n) {
            document.querySelectorAll(".step-dot").forEach((dot) => {
                dot.classList.toggle("active", Number(dot.dataset.step) <= n);
            });
        }

        // Transición manual entre pasos: anima el alto de la tarjeta, desliza el
        // paso entrante según la dirección y mueve el foco al primer campo.
        function cambiarPaso(salida, entrada, direccion) {
            const sinMovimiento = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

            // 1. Alto actual (antes de cambiar de paso).
            const altoInicial = form.offsetHeight;

            // 2. Swap de visibilidad.
            salida.style.display = "none";
            entrada.style.display = "block";

            // 3. Animación de entrada direccional (reinicia si ya estaba aplicada).
            entrada.classList.remove("step-enter-forward", "step-enter-back");
            void entrada.offsetWidth; // fuerza reflow para reiniciar la animación
            entrada.classList.add(direccion === "forward" ? "step-enter-forward" : "step-enter-back");

            // 4. Stepper.
            actualizarStepper(direccion === "forward" ? 2 : 1);

            // 5. Foco al primer campo del nuevo paso (sin saltar el scroll).
            const primerCampo = entrada.querySelector("input, select, textarea, button");

            // 6. Anima el alto de old → new.
            if (sinMovimiento) {
                if (primerCampo) primerCampo.focus();
                return;
            }

            const altoFinal = entrada.scrollHeight;
            form.classList.add("is-animating");
            form.style.height = altoInicial + "px";
            void form.offsetWidth; // reflow antes de animar
            form.style.height = altoFinal + "px";

            form.addEventListener("transitionend", function fin(e) {
                if (e.propertyName !== "height") return;
                form.removeEventListener("transitionend", fin);
                form.style.height = "auto"; // vuelve a ser responsivo
                form.classList.remove("is-animating");
            });

            if (primerCampo) primerCampo.focus({ preventScroll: true });
        }

        // Configura el título y el stepper según el estado. El modo vista usa el
        // dashboard (#dashboard); el form (#stepParams) solo aparece en edición.
        function setEstado(nuevo) {
            estado = nuevo;
            if (nuevo === "view") {
                stepper.style.display = "none";
                cardTitle.textContent = "Parámetros del dispositivo";
            } else if (nuevo === "auth") {
                stepper.style.display = "flex";
                actualizarStepper(1);
                cardTitle.textContent = "Inicia sesión para editar";
            } else if (nuevo === "edit") {
                stepper.style.display = "flex";
                actualizarStepper(2);
                cardTitle.textContent = "Actualiza los parámetros.";
            }
        }

        // Dashboard → credenciales
        btnEdit.addEventListener("click", () => {
            setEstado("auth");
            cambiarPaso(dashboard, stepAuth, "forward");
        });

        // Salir de la pantalla.
        btnExit.addEventListener("click", () => {
            window.location.href = "/exit";
        });

        // Credenciales → dashboard (cancelar el ingreso)
        btnBack.addEventListener("click", () => {
            setEstado("view");
            cambiarPaso(stepAuth, dashboard, "back");
        });

        // Credenciales → edición: valida el formato y luego verifica las
        // credenciales contra el dispositivo (/authusercredentials) antes de desbloquear.
        btnAuthContinue.addEventListener("click", async () => {
            errores = validarAuth();
            if (errores.length > 0) {
                alert("Errores en el formulario:\n" + errores.join("\n"));
                return;
            }

            const credenciales = {
                user: document.getElementById("user").value.trim(),
                pass: document.getElementById("password").value.trim()
            };

            const textoOriginal = btnAuthContinue.textContent;
            btnAuthContinue.disabled = true;
            btnAuthContinue.textContent = "Verificando…";

            try {
                const res = await fetch("/authusercredentials", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify(credenciales)
                });
                const json = await res.json();

                if (!res.ok || !json.status) {
                    alert(json.message || "Usuario o contraseña incorrectos.");
                    return;
                }
            } catch (e) {
                alert("Error de comunicación con el dispositivo");
                return;
            } finally {
                btnAuthContinue.disabled = false;
                btnAuthContinue.textContent = textoOriginal;
            }

            // Credenciales válidas → vuelca en el formulario los valores ya
            // cargados del dispositivo (los mismos del dashboard, sin re-fetch),
            // por si una edición previa dejó campos modificados sin guardar.
            actualizarFormulario();

            setEstado("edit");
            cambiarPaso(stepAuth, stepParams, "forward");
        });

        // Edición → dashboard (descarta cambios recargando los valores del dispositivo)
        btnCancel.addEventListener("click", () => {
            actualizarFormulario();
            actualizarDashboard();
            setEstado("view");
            cambiarPaso(stepParams, dashboard, "back");
        });

        form.addEventListener("submit", function (e) {
            e.preventDefault();
            // En credenciales, Enter avanza al modo edición (no guarda).
            if (estado === "auth") {
                btnAuthContinue.click();
                return;
            }
            // Solo se guarda desde el modo edición.
            if (estado !== "edit") return;

            errores = validarParametros();
            if (errores.length > 0) {
                alert("Errores en el formulario:\n" + errores.join("\n"));
                return;
            }

            const formData = new FormData(form);
            const data = {};

            formData.forEach((value, key) => {
                if (key === 'fpOn' || key === 'fpOff' || key === 'irrH' || key === 'irrM' ||
                    key === 'ventH' || key === 'ventM' ||
                    key === 'ledA' || key === 'ledR' || key === 'ledB')
                    data[key] = Number(value.trim())
                else
                    data[key] = value.trim();
            });

            data.enable = document.getElementById("enable").checked;
            const fechaHoraActual = new Date();
            data['dia'] = Number(fechaHoraActual.getDate());
            data['mes'] = Number(fechaHoraActual.getMonth() + 1);
            data['anio'] = Number(fechaHoraActual.getFullYear().toString().slice(-2));
            data['diaSem'] = Number(fechaHoraActual.getDay());
            data['hr'] = Number(fechaHoraActual.getHours());
            data['min'] = Number(fechaHoraActual.getMinutes());
            data['seg'] = Number(fechaHoraActual.getSeconds());

            const submitBtn = form.querySelector('button[type="submit"]');
            submitBtn.disabled = true;
            submitBtn.textContent = "Enviando…";

            fetch("/newparams", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(data)
            })
                .then(async (res) => {
                    const json = await res.json();
                    const container = document.getElementById("form-card");

                    const div = document.createElement("div");
                    if (res.ok && json.status) {
                        div.className = "mensaje-ok";
                        div.textContent = json.message;
                    } else {
                        div.className = "mensaje-error";
                        div.textContent = json.message || "Error desconocido";
                    }
                    container.innerHTML = "";
                    container.appendChild(div);
                }).catch(() => {
                    document.getElementById("form-card").innerHTML = `<div class="mensaje-error">Error de comunicación con el dispositivo</div>`;
                });
        });

        function validarParametros() {
            errores = [];
            const planta = document.getElementById("planta").value.trim();
            const fpOnInput = document.getElementById("fpOn").value.trim();
            const fpOffInput = document.getElementById("fpOff").value.trim();
            const irrMinInput = document.getElementById("irrM").value.trim();
            const ventMinInput = document.getElementById("ventM").value.trim();
            const irrHInput = document.getElementById("irrH").value.trim();
            const ventHInput = document.getElementById("ventH").value.trim();
            const ledAInput = document.getElementById("ledA").value.trim();
            const ledRInput = document.getElementById("ledR").value.trim();
            const ledBInput = document.getElementById("ledB").value.trim();

            // --- Planta (validación unificada con el ESP32) ---
            if (planta === "") {
                errores.push("El nombre de la planta es obligatorio");
            } else {
                if (planta.length < 3 || planta.length > 20)
                    errores.push("La planta debe tener entre 3 y 20 caracteres");
                // Mismo set de caracteres que el dispositivo (con espacios).
                // Admite acentos y ñ, igual que el ESP32.
                if (!isValidReadableString(planta, true))
                    errores.push("La planta solo admite letras, números, espacios y (_-.@!#$%&*?+=)");
                // Extras del formulario (el ESP32 no los valida, pero suman robustez):
                if (/\s{2,}/.test(planta))
                    errores.push("No uses espacios repetidos");
                if (/^\d+$/.test(planta))
                    errores.push("La planta no puede ser solo números");
                if (hasTooManyRepeatedChars(planta))
                    errores.push("La planta no puede contener más de 3 caracteres repetidos seguidos");
            }

            // --- Fotoperiodo: hora de prendido ---
            if (fpOnInput === "" || !isStrictInteger(fpOnInput)) {
                errores.push("La hora de prendido es obligatoria, numérica y sin decimales");
            } else if (Number(fpOnInput) < 0 || Number(fpOnInput) > 23) {
                errores.push("La hora de prendido debe ser un valor entre 0 y 23 horas");
            }

            // --- Fotoperiodo: hora de apagado ---
            if (fpOffInput === "" || !isStrictInteger(fpOffInput)) {
                errores.push("La hora de apagado es obligatoria, numérica y sin decimales");
            } else if (Number(fpOffInput) < 0 || Number(fpOffInput) > 23) {
                errores.push("La hora de apagado debe ser un valor entre 0 y 23 horas");
            }

            // --- Fotoperiodo: prendido y apagado no pueden ser la misma hora
            //     (sería ambiguo: 0 h o 24 h de luz). El ciclo sí puede cruzar
            //     medianoche, así que no se exige prendido < apagado. ---
            if (isStrictInteger(fpOnInput) && isStrictInteger(fpOffInput) &&
                Number(fpOnInput) === Number(fpOffInput)) {
                errores.push("La hora de prendido y apagado no pueden ser iguales");
            }

            // --- Riego: minutos ---
            if (irrMinInput === "" || !isStrictInteger(irrMinInput)) {
                errores.push("El campo riego-minutos es obligatorio, numérico y sin decimales");
            } else if (Number(irrMinInput) < 0 || Number(irrMinInput) > 59) {
                errores.push("El valor de riego debe ser un valor entre 0 y 59 minutos");
            }

            // --- Ventilación: minutos ---
            if (ventMinInput === "" || !isStrictInteger(ventMinInput)) {
                errores.push("El campo ventilación-minutos es obligatorio, numérico y sin decimales");
            } else if (Number(ventMinInput) < 0 || Number(ventMinInput) > 59) {
                errores.push("El valor de ventilación debe ser un valor entre 0 y 59 minutos");
            }

            // --- Selects de frecuencia (Riego / Ventilación) ---
            if (!isStrictInteger(irrHInput) || Number(irrHInput) < 0 || Number(irrHInput) > 6)
                errores.push("Selecciona una frecuencia de riego válida");
            if (!isStrictInteger(ventHInput) || Number(ventHInput) < 0 || Number(ventHInput) > 6)
                errores.push("Selecciona una frecuencia de ventilación válida");

            // --- Sliders de LEDs (0–100%) ---
            if (!isStrictInteger(ledAInput) || Number(ledAInput) < 0 || Number(ledAInput) > 100)
                errores.push("El LED azul debe estar entre 0 y 100%");
            if (!isStrictInteger(ledRInput) || Number(ledRInput) < 0 || Number(ledRInput) > 100)
                errores.push("El LED rojo debe estar entre 0 y 100%");
            if (!isStrictInteger(ledBInput) || Number(ledBInput) < 0 || Number(ledBInput) > 100)
                errores.push("El LED blanco debe estar entre 0 y 100%");

            return errores;
        }

        function validarAuth() {
            errores = [];
            const user = document.getElementById("user").value.trim();
            const pass = document.getElementById("password").value.trim();

            // Longitud (igual que el ESP32: usuario 4–32, contraseña 8–64).
            if (user.length < 4 || user.length > 32)
                errores.push("El usuario debe tener entre 4 y 32 caracteres.");
            if (pass.length < 8 || pass.length > 64)
                errores.push("La contraseña debe tener entre 8 y 64 caracteres.");

            // Caracteres permitidos: alfanuméricos y (_-.@!#$%&*?+=), sin espacios.
            if (!isValidReadableString(user, false))
                errores.push("El usuario solo admite letras, números y (_-.@!#$%&*?+=).");
            if (!isValidReadableString(pass, false))
                errores.push("La contraseña solo admite letras, números y (_-.@!#$%&*?+=).");

            // No más de 3 caracteres idénticos seguidos.
            if (hasTooManyRepeatedChars(user))
                errores.push("El usuario no puede contener más de 3 caracteres repetidos seguidos.");
            if (hasTooManyRepeatedChars(pass))
                errores.push("La contraseña no puede contener más de 3 caracteres repetidos seguidos.");

            return errores;
        }

        // Función universal para actualizar progreso y valor
        function updateSliderProgress(sliderId, progressId) {
            const slider = document.getElementById(sliderId);
            const progress = document.getElementById(progressId);

            if (!slider || !progress) return;

            const percent = ((slider.value - slider.min) / (slider.max - slider.min)) * 100;

            // Crear el gradiente según el slider
            let color;
            if (sliderId === 'ledA') color = '#2196F3';
            else if (sliderId === 'ledR') color = '#f44336';
            else if (sliderId === 'ledB') color = '#ffffff';

            if (sliderId === 'ledB') {
                // Solo la parte llena lleva borde; el resto lo muestra el riel gris del contenedor
                progress.style.background = color;
                progress.style.width = `${percent}%`;
                progress.style.border = percent > 0 ? '1px solid #9e9e9e' : 'none';
            } else {
                progress.style.background = `linear-gradient(to right, ${color} 0%, ${color} ${percent}%, #e0e0e0 ${percent}%, #e0e0e0 100%)`;
            }
        }

        function updateLampValue(sliderId, valueId, progressId) {
            const slider = document.getElementById(sliderId);
            const valueLabel = document.getElementById(valueId);

            if (!slider || !valueLabel) return;

            valueLabel.textContent = `${slider.value}%`;
            updateSliderProgress(sliderId, progressId);

            slider.addEventListener("input", () => {
                valueLabel.textContent = `${slider.value}%`;
                updateSliderProgress(sliderId, progressId);
            });
        }

        updateLampValue('ledA', 'luzAzulValue', 'progressA');
        updateLampValue('ledR', 'luzRojaValue', 'progressR');
        updateLampValue('ledB', 'luzBlancaValue', 'progressB');
    </script>
</body>

</html>
)===";
