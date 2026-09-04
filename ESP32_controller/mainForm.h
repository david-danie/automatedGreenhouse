static const char mainForm[] = R"===(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="theme-color" content="#4caf50">
    <title>Actualiza parámetros</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
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
        #mainForm {
            transition: height 0.35s ease;
        }
        #mainForm.is-animating {
            overflow: hidden;
        }
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
        .form-header {
            text-align: center;
        }
        .form-header h1 {
            color: #2e7d32;
            font-size: 24px;
            font-weight: 500;
            margin-bottom: 8px;
        }
        .welcome {
            text-align: center;
            padding: 8px 4px 4px;
        }
        .welcome-emoji {
            font-size: 44px;
            line-height: 1;
            margin-bottom: 12px;
        }
        .welcome-text {
            color: #4b6b4d;
            font-size: 14px;
            line-height: 1.5;
        }
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
        .section-toggle {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 13px;
            font-weight: 400;
            color: #2e7d32;
            cursor: pointer;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 40px;
            height: 22px;
            flex-shrink: 0;
        }
        .switch input {
            position: absolute;
            inset: 0;
            width: 100%;
            height: 100%;
            margin: 0;
            opacity: 0;
            cursor: pointer;
        }
        .switch-slider {
            position: absolute;
            inset: 0;
            background: #ccc;
            border-radius: 22px;
            transition: background .2s ease;
            pointer-events: none;
        }
        .switch-slider::before {
            content: "";
            position: absolute;
            top: 2px;
            left: 2px;
            width: 18px;
            height: 18px;
            background: #fff;
            border-radius: 50%;
            box-shadow: 0 1px 2px rgba(0, 0, 0, .3);
            transition: transform .2s ease;
        }
        .switch input:checked + .switch-slider {
            background: #4caf50;
        }
        .switch input:checked + .switch-slider::before {
            transform: translateX(18px);
        }
        .switch input:focus-visible + .switch-slider {
            box-shadow: 0 0 0 2px rgba(76, 175, 80, .4);
        }
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
        #ledA::-webkit-slider-thumb {
            background: #2196F3;
            box-shadow: 0 2px 8px rgba(33, 150, 243, 0.5);
        }
        #ledA::-moz-range-thumb {
            background: #2196F3;
            box-shadow: 0 2px 8px rgba(33, 150, 243, 0.5);
        }
        #ledR::-webkit-slider-thumb {
            background: #f44336;
            box-shadow: 0 2px 8px rgba(244, 67, 54, 0.5);
        }
        #ledR::-moz-range-thumb {
            background: #f44336;
            box-shadow: 0 2px 8px rgba(244, 67, 54, 0.5);
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
        .toggle-password svg {
            display: block;
            width: 20px;
            height: 20px;
        }
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
        .toast {
            position: fixed;
            left: 50%;
            bottom: 24px;
            transform: translateX(-50%) translateY(8px);
            width: calc(100% - 32px);
            max-width: 360px;
            padding: 14px 18px;
            border-radius: 8px;
            font-size: 14px;
            text-align: center;
            box-shadow: 0 6px 20px rgba(0, 0, 0, 0.15);
            opacity: 0;
            transition: opacity 0.25s ease, transform 0.25s ease;
            z-index: 10;
            pointer-events: none;
        }
        .toast.show {
            opacity: 1;
            transform: translateX(-50%) translateY(0);
        }
        .toast-ok {
            background: #e8f5e9;
            border: 1px solid #4caf50;
            color: #2e7d32;
        }
        .toast-error {
            background: #ffebee;
            border: 1px solid #f44336;
            color: #c62828;
        }
        @media (prefers-reduced-motion: reduce) {
            .toast {
                transition: opacity 0.25s ease;
            }
            .toast.show {
                transform: translateX(-50%);
            }
        }
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
        .dash-stack {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
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
        .wifi-entry {
            text-align: center;
        }
        .wifi-chip {
            flex: none;
            width: auto;
            display: inline-flex;
            align-items: center;
            gap: 8px;
            margin-top: 16px;
            padding: 8px 14px;
            background: #f1f8e9;
            border: 1px solid #c8e6c9;
            border-radius: 999px;
            color: #2e7d32;
            font-size: 13px;
            font-weight: 500;
            box-shadow: none;
        }
        .wifi-chip:hover {
            background: #e8f5e9;
        }
        .wifi-dot {
            width: 9px;
            height: 9px;
            border-radius: 50%;
            background: #bdbdbd;
            flex-shrink: 0;
        }
        .wifi-dot.is-on {
            background: #4caf50;
        }
        .wifi-scanning {
            text-align: center;
            padding: 28px 4px;
        }
        .spinner {
            width: 36px;
            height: 36px;
            margin: 0 auto 14px;
            border: 3px solid #c8e6c9;
            border-top-color: #4caf50;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
        }
        @keyframes spin {
            to {
                transform: rotate(360deg);
            }
        }
        @media (prefers-reduced-motion: reduce) {
            .spinner {
                animation-duration: 1.6s;
            }
        }
        .wifi-rescan {
            flex: none;
            width: auto;
            display: inline-block;
            margin-top: 4px;
            padding: 4px 0;
            background: none;
            box-shadow: none;
            color: #2e7d32;
            font-size: 13px;
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="form-card" id="form-card">
        <div class="form-header">
            <h1 id="cardTitle">Mi cultivo 🌱</h1>
            <div class="stepper" id="stepper" aria-hidden="true" style="display:none;">
                <span class="step-dot active" data-step="1"></span>
                <span class="step-dot" data-step="2"></span>
            </div>
        </div>
        <form id="mainForm" novalidate>
            <div id="dashboard" style="display:none;">
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
                <div class="wifi-entry">
                    <button type="button" class="wifi-chip" id="btnWifi">
                        <span class="wifi-dot" id="wifiDot"></span>
                        <span id="wifiChipText">Conexión a Internet</span>
                    </button>
                </div>
            </div>
            <div id="stepParams" style="display:none;">
                <div class="section-header">
                    <span>General</span>
                    <label class="section-toggle" for="enable">
                        Sistema activo
                        <span class="switch">
                            <input type="checkbox" id="enable" name="enable">
                            <span class="switch-slider"></span>
                        </span>
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
                    <label class="section-toggle" for="ledB">
                        Luz Blanca
                        <span class="switch">
                            <input type="checkbox" id="ledB" name="ledB">
                            <span class="switch-slider"></span>
                        </span>
                    </label>
                </div>
                <div class="section-header">
                    <span>Riego</span>
                </div>
                <div class="form-group2">
                    <div class="form-group">
                        <label for="irrH">Frecuencia</label>
                        <div class="select-container">
                            <select id="irrH" name="irrH" required>
                                <option value="1">24 veces al día</option>
                                <option value="2">12 veces al día</option>
                                <option value="3">8 veces al día</option>
                                <option value="4">6 veces al día</option>
                                <option value="6">4 veces al día</option>
                                <option value="8">3 veces al día</option>
                                <option value="12">2 veces al día</option>
                                <option value="24">1 vez al día</option>
                                <option value="48">Cada 2 días</option>
                                <option value="72">Cada 3 días</option>
                                <option value="168">Cada semana</option>
                                <option value="0">No regar</option>
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
                                <option value="1">24 veces al día</option>
                                <option value="2">12 veces al día</option>
                                <option value="3">8 veces al día</option>
                                <option value="4">6 veces al día</option>
                                <option value="6">4 veces al día</option>
                                <option value="8">3 veces al día</option>
                                <option value="12">2 veces al día</option>
                                <option value="24">1 vez al día</option>
                                <option value="48">Cada 2 días</option>
                                <option value="72">Cada 3 días</option>
                                <option value="168">Cada semana</option>
                                <option value="0">No ventilar</option>
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
            <div id="stepWelcome" style="display:none;">
                <div class="welcome">
                    <div class="welcome-emoji">🌱</div>
                    <p class="welcome-text">
                        Dispositivo nuevo. Crea tu usuario para empezar.
                    </p>
                </div>
                <div class="buttons">
                    <button type="button" class="btn-primary" id="btnWelcomeRegister">Crear usuario</button>
                    <button type="button" class="btn-secondary" id="btnWelcomeExit">Salir</button>
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
                            aria-label="Mostrar contraseña"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg></button>
                    </div>
                </div>
                <div class="buttons">
                    <button type="button" class="btn-primary" id="btnAuthContinue">Continuar</button>
                    <button type="button" class="btn-secondary" id="btnBack">Volver</button>
                </div>
            </div>
            <div id="stepExit" style="display:none;">
                <div class="welcome">
                    <div class="welcome-emoji" id="exitEmoji">👋</div>
                    <p class="welcome-text" id="exitText"></p>
                </div>
                <div class="buttons" id="exitButtons">
                    <button type="button" class="btn-primary" id="btnExitBack">Volver</button>
                </div>
            </div>
            <div id="stepWifi" style="display:none;">
                <div id="wifiScanning" class="wifi-scanning">
                    <div class="spinner"></div>
                    <p class="welcome-text">Buscando redes Wi-Fi…</p>
                </div>
                <div id="wifiScanError" class="welcome" style="display:none;">
                    <div class="welcome-emoji">📡</div>
                    <p class="welcome-text" id="wifiScanErrorText">No se encontraron redes.</p>
                </div>
                <div id="wifiForm" style="display:none;">
                    <div class="form-group">
                        <label for="wifiSsid">Red Wi-Fi</label>
                        <div class="select-container">
                            <select id="wifiSsid" name="wifiSsid"></select>
                        </div>
                    </div>
                    <div class="form-group" id="wifiPassGroup">
                        <label for="wifiPass">Contraseña</label>
                        <div class="password-wrapper">
                            <input type="password" id="wifiPass" name="wifiPass" autocomplete="new-password" maxlength="63">
                            <button type="button" class="toggle-password" onclick="toggleWifiPassword()" id="wifiToggleBtn"
                                aria-label="Mostrar contraseña"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg></button>
                        </div>
                    </div>
                </div>
                <div class="wifi-entry">
                    <button type="button" class="wifi-rescan" id="btnWifiRescan" style="display:none;">Volver a buscar</button>
                </div>
                <div class="buttons">
                    <button type="button" class="btn-primary" id="btnWifiConnect" style="display:none;">Conectar</button>
                    <button type="button" class="btn-secondary" id="btnWifiBack">Volver</button>
                </div>
            </div>
        </form>
        <div class="footer">
            <p>© <span id="year">2026</span> Smartplant • Versión 1.0</p>
        </div>
    </div>
    <script>
        const PARAMS_ENDPOINT = "/getparams";
        const TOKEN_KEY = "spToken";
        function getToken() { return localStorage.getItem(TOKEN_KEY) || ""; }
        function setToken(t) { if (t) localStorage.setItem(TOKEN_KEY, t); }
        function clearToken() { localStorage.removeItem(TOKEN_KEY); }
        let params = {};
        const VALID_FREQUENCIES = [0, 1, 2, 3, 4, 6, 8, 12, 24, 48, 72, 168];
        const FREQ_LABELS = {
            0: "Apagado",
            1: "24 × día", 2: "12 × día", 3: "8 × día", 4: "6 × día",
            6: "4 × día", 8: "3 × día", 12: "2 × día", 24: "1 × día",
            48: "Cada 2 días", 72: "Cada 3 días", 168: "Semanal"
        };
        async function obtenerValoresDispositivo() {
            try {
                const token = getToken();
                const url = PARAMS_ENDPOINT + (token ? ("?token=" + encodeURIComponent(token)) : "");
                const res = await fetch(url, {
                    headers: { "Accept": "application/json" }
                });
                if (!res.ok) throw new Error("HTTP " + res.status);
                params = await res.json();
                return true;
            } catch (e) {
                console.warn("No se pudieron cargar los parámetros del dispositivo.", e);
                return false;
            }
        }
        function pintarMensaje(ok, texto) {
            const container = document.getElementById("form-card");
            const div = document.createElement("div");
            div.className = ok ? "mensaje-ok" : "mensaje-error";
            div.textContent = texto;
            container.innerHTML = "";
            container.appendChild(div);
        }
        let toastTimer = null;
        function mostrarToast(ok, texto) {
            let toast = document.getElementById("toast");
            if (!toast) {
                toast = document.createElement("div");
                toast.id = "toast";
                document.body.appendChild(toast);
            }
            toast.className = "toast " + (ok ? "toast-ok" : "toast-error");
            toast.textContent = texto;
            void toast.offsetWidth;
            toast.classList.add("show");
            if (toastTimer) clearTimeout(toastTimer);
            toastTimer = setTimeout(() => toast.classList.remove("show"), 2800);
        }
        async function cargarParametros() {
            if (!await obtenerValoresDispositivo()) {
                pintarMensaje(false, "No se pudo leer la configuración del dispositivo. Recarga la página para reintentar.");
                return;
            }
            if (params.hasRegisteredUser === false) {
                setEstado("welcome");
                stepWelcome.style.display = "block";
            } else {
                actualizarDashboard();
                setEstado("view");
                dashboard.style.display = "block";
            }
        }
        function actualizarFormulario() {
            const p = params;
            document.getElementById('planta').value = p.planta || "";
            document.getElementById('enable').checked = p.enable === true || p.enable === "true";
            document.getElementById('fpOn').value = p.fpOn;
            document.getElementById('fpOff').value = p.fpOff;
            document.getElementById('ledA').value = p.ledA;
            document.getElementById('luzAzulValue').innerText = `${p.ledA}%`;
            document.getElementById('ledR').value = p.ledR;
            document.getElementById('luzRojaValue').innerText = `${p.ledR}%`;
            document.getElementById('ledB').checked = Number(p.ledB) > 0;
            document.getElementById('irrH').value = p.irrH;
            document.getElementById('irrM').value = p.irrM;
            document.getElementById('ventH').value = p.ventH;
            document.getElementById('ventM').value = p.ventM;
            updateSliderProgress('ledA', 'progressA');
            updateSliderProgress('ledR', 'progressR');
        }
        function actualizarDashboard() {
            const p = params;
            const activo = p.enable === true || p.enable === "true";
            document.getElementById('dashPlanta').textContent = p.planta || "Planta";
            const status = document.getElementById('dashStatus');
            status.textContent = activo ? "Activo" : "Inactivo";
            status.classList.toggle('is-off', !activo);
            document.getElementById('dashSemana').textContent = p.semana ?? "—";
            document.getElementById('dashDia').textContent = p.dia ?? "—";
            document.getElementById('dashFpOn').textContent = p.fpOn ?? "—";
            document.getElementById('dashFpOff').textContent = p.fpOff ?? "—";
            setDashLed('dashLedA', p.ledA);
            setDashLed('dashLedR', p.ledR);
            setDashLedB(p.ledB);
            document.getElementById('dashIrrH').textContent = FREQ_LABELS[p.irrH] ?? "—";
            document.getElementById('dashIrrM').textContent = p.irrM ?? "—";
            document.getElementById('dashVentH').textContent = FREQ_LABELS[p.ventH] ?? "—";
            document.getElementById('dashVentM').textContent = p.ventM ?? "—";
            actualizarWifiChip();
        }
        function actualizarWifiChip() {
            const conectado = params.wifiConnected === true;
            wifiDot.classList.toggle("is-on", conectado);
            wifiChipText.textContent = conectado
                ? ("Conectado a " + (params.wifiSsid || "Wi-Fi"))
                : "Conexión a Internet";
        }
        function setDashLed(valueId, val) {
            const n = Number(val) || 0;
            document.getElementById(valueId).textContent = `${n}%`;
        }
        function setDashLedB(val) {
            document.getElementById('dashLedB').textContent = Number(val) > 0 ? "ON" : "OFF";
        }
        document.addEventListener('DOMContentLoaded', () => {
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
        const stepWelcome = document.getElementById("stepWelcome");
        const btnWelcomeRegister = document.getElementById("btnWelcomeRegister");
        const btnWelcomeExit = document.getElementById("btnWelcomeExit");
        const stepper = document.getElementById("stepper");
        const cardTitle = document.getElementById("cardTitle");
        const stepExit = document.getElementById("stepExit");
        const btnExitBack = document.getElementById("btnExitBack");
        const exitEmoji = document.getElementById("exitEmoji");
        const exitText = document.getElementById("exitText");
        const exitButtons = document.getElementById("exitButtons");
        const stepWifi = document.getElementById("stepWifi");
        const btnWifi = document.getElementById("btnWifi");
        const wifiDot = document.getElementById("wifiDot");
        const wifiChipText = document.getElementById("wifiChipText");
        const wifiScanning = document.getElementById("wifiScanning");
        const wifiScanError = document.getElementById("wifiScanError");
        const wifiScanErrorText = document.getElementById("wifiScanErrorText");
        const wifiForm = document.getElementById("wifiForm");
        const wifiSsidSelect = document.getElementById("wifiSsid");
        const wifiPassGroup = document.getElementById("wifiPassGroup");
        const wifiPass = document.getElementById("wifiPass");
        const btnWifiRescan = document.getElementById("btnWifiRescan");
        const btnWifiConnect = document.getElementById("btnWifiConnect");
        const btnWifiBack = document.getElementById("btnWifiBack");
        let estado = "view";
        let exitVolverA = "view";
        let authIntent = "edit";
        document.getElementById("year").textContent = new Date().getFullYear();
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
        const EYE_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>';
        const EYE_OFF_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line></svg>';
        function togglePassword() {
            const type = password.type === 'password' ? 'text' : 'password';
            password.type = type;
            const btn = document.getElementById('toggleBtn');
            btn.innerHTML = type === 'password' ? EYE_ICON : EYE_OFF_ICON;
            btn.setAttribute('aria-label', type === 'password' ? 'Mostrar contraseña' : 'Ocultar contraseña');
        }
        function toggleWifiPassword() {
            const type = wifiPass.type === 'password' ? 'text' : 'password';
            wifiPass.type = type;
            const btn = document.getElementById('wifiToggleBtn');
            btn.innerHTML = type === 'password' ? EYE_ICON : EYE_OFF_ICON;
            btn.setAttribute('aria-label', type === 'password' ? 'Mostrar contraseña' : 'Ocultar contraseña');
        }
        function actualizarStepper(n) {
            document.querySelectorAll(".step-dot").forEach((dot) => {
                dot.classList.toggle("active", Number(dot.dataset.step) <= n);
            });
        }
        function cambiarPaso(salida, entrada, direccion) {
            const sinMovimiento = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
            const altoInicial = form.offsetHeight;
            salida.style.display = "none";
            entrada.style.display = "block";
            entrada.classList.remove("step-enter-forward", "step-enter-back");
            void entrada.offsetWidth;
            entrada.classList.add(direccion === "forward" ? "step-enter-forward" : "step-enter-back");
            actualizarStepper(direccion === "forward" ? 2 : 1);
            const primerCampo = entrada.querySelector("input, select, textarea, button");
            if (sinMovimiento) {
                if (primerCampo) primerCampo.focus();
                return;
            }
            const altoFinal = entrada.scrollHeight;
            form.classList.add("is-animating");
            form.style.height = altoInicial + "px";
            void form.offsetWidth;
            form.style.height = altoFinal + "px";
            form.addEventListener("transitionend", function fin(e) {
                if (e.propertyName !== "height") return;
                form.removeEventListener("transitionend", fin);
                form.style.height = "auto";
                form.classList.remove("is-animating");
            });
            if (primerCampo) primerCampo.focus({ preventScroll: true });
        }
        function setEstado(nuevo) {
            estado = nuevo;
            if (nuevo === "view") {
                stepper.style.display = "none";
                cardTitle.textContent = "Mi cultivo 🌱";
            } else if (nuevo === "welcome") {
                stepper.style.display = "none";
                cardTitle.textContent = "Bienvenido 👋";
            } else if (nuevo === "register") {
                stepper.style.display = "none";
                cardTitle.textContent = "Crea tu cuenta";
                btnBack.style.display = "";
                btnAuthContinue.textContent = "Registrar";
            } else if (nuevo === "auth") {
                stepper.style.display = "flex";
                actualizarStepper(1);
                cardTitle.textContent = "Inicia sesión";
                btnBack.style.display = "";
                btnAuthContinue.textContent = "Continuar";
            } else if (nuevo === "edit") {
                stepper.style.display = "flex";
                actualizarStepper(2);
                cardTitle.textContent = "Editar parámetros";
            } else if (nuevo === "wifi") {
                stepper.style.display = "none";
                cardTitle.textContent = "Conexión a Internet";
            } else if (nuevo === "exit") {
                stepper.style.display = "none";
            }
        }
        function pasoVisible() {
            if (estado === "welcome") return stepWelcome;
            if (estado === "register" || estado === "auth") return stepAuth;
            if (estado === "edit") return stepParams;
            if (estado === "wifi") return stepWifi;
            if (estado === "exit") return stepExit;
            return dashboard;
        }
        function irAExit(opts) {
            const salida = pasoVisible();
            exitVolverA = opts.volverA || "view";
            exitEmoji.textContent = opts.emoji;
            exitText.textContent = opts.texto;
            exitButtons.style.display = opts.volver ? "" : "none";
            setEstado("exit");
            cardTitle.textContent = opts.titulo;
            cambiarPaso(salida, stepExit, "forward");
        }
        btnEdit.addEventListener("click", async () => {
            authIntent = "edit";
            await obtenerValoresDispositivo();
            if (params.sessionValid) {
                actualizarFormulario();
                setEstado("edit");
                cambiarPaso(dashboard, stepParams, "forward");
            } else {
                clearToken();
                setEstado("auth");
                cambiarPaso(dashboard, stepAuth, "forward");
            }
        });
        async function salir() {
            const volverA = estado === "welcome" ? "welcome" : "view";
            clearToken();
            try {
                await fetch("/exit");
            } catch (e) {
            }
            irAExit({
                titulo: "Hasta pronto",
                emoji: "👋",
                texto: "Sesión cerrada. Ya puedes cerrar la ventana.",
                volver: true,
                volverA
            });
        }
        btnExit.addEventListener("click", salir);
        btnExitBack.addEventListener("click", () => {
            const destino = exitVolverA === "welcome" ? stepWelcome : dashboard;
            setEstado(exitVolverA);
            cambiarPaso(stepExit, destino, "back");
        });
        btnWelcomeRegister.addEventListener("click", () => {
            setEstado("register");
            cambiarPaso(stepWelcome, stepAuth, "forward");
        });
        btnWelcomeExit.addEventListener("click", salir);
        btnBack.addEventListener("click", () => {
            if (estado === "register") {
                setEstado("welcome");
                cambiarPaso(stepAuth, stepWelcome, "back");
            } else {
                setEstado("view");
                cambiarPaso(stepAuth, dashboard, "back");
            }
        });
        btnAuthContinue.addEventListener("click", async () => {
            const errores = validarAuth();
            if (errores.length > 0) {
                alert("Errores en el formulario:\n" + errores.join("\n"));
                return;
            }
            const credenciales = {
                user: document.getElementById("user").value.trim(),
                pass: document.getElementById("password").value.trim()
            };
            const esRegistro = estado === "register";
            const esReset = !esRegistro && credenciales.pass === "**reset**";
            const endpoint = esRegistro ? "/usercredentials" : "/authusercredentials";
            const textoOriginal = btnAuthContinue.textContent;
            btnAuthContinue.disabled = true;
            btnAuthContinue.textContent = esRegistro ? "Registrando…" : "Verificando…";
            let json;
            try {
                const res = await fetch(endpoint, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify(credenciales)
                });
                json = await res.json();
                if (!res.ok || !json.status) {
                    alert(json.message || (esRegistro
                        ? "No se pudo registrar el usuario."
                        : "Usuario o contraseña incorrectos."));
                    return;
                }
            } catch (e) {
                alert("Error de comunicación con el dispositivo");
                return;
            } finally {
                btnAuthContinue.disabled = false;
                btnAuthContinue.textContent = textoOriginal;
            }
            if (esReset) {
                clearToken();
                document.getElementById("user").value = "";
                document.getElementById("password").value = "";
                irAExit({
                    titulo: "Reiniciando…",
                    emoji: "🔄",
                    texto: (json.message || "Factory reset ejecutado.") +
                        " El equipo se reinicia y el Wi-Fi se caerá. Reconéctate para configurarlo de nuevo.",
                    volver: false
                });
                return;
            }
            if (esRegistro) {
                setToken(json.token);
                document.getElementById("user").value = "";
                document.getElementById("password").value = "";
                await obtenerValoresDispositivo();
                actualizarDashboard();
                setEstado("view");
                cambiarPaso(stepAuth, dashboard, "back");
                return;
            }
            setToken(json.token);
            if (authIntent === "wifi") {
                setEstado("wifi");
                cambiarPaso(stepAuth, stepWifi, "forward");
                escanearRedes();
            } else {
                actualizarFormulario();
                setEstado("edit");
                cambiarPaso(stepAuth, stepParams, "forward");
            }
        });
        btnCancel.addEventListener("click", () => {
            setEstado("view");
            cambiarPaso(stepParams, dashboard, "back");
        });
        form.addEventListener("submit", function (e) {
            e.preventDefault();
            if (estado === "auth" || estado === "register") {
                btnAuthContinue.click();
                return;
            }
            if (estado !== "edit") return;
            const errores = validarParametros();
            if (errores.length > 0) {
                alert("Errores en el formulario:\n" + errores.join("\n"));
                return;
            }
            const formData = new FormData(form);
            const data = {};
            formData.forEach((value, key) => {
                if (key === 'fpOn' || key === 'fpOff' || key === 'irrH' || key === 'irrM' ||
                    key === 'ventH' || key === 'ventM' ||
                    key === 'ledA' || key === 'ledR')
                    data[key] = Number(value.trim())
                else if (key === 'ledB')
                    {}
                else
                    data[key] = value.trim();
            });
            data.enable = document.getElementById("enable").checked;
            data.ledB = document.getElementById("ledB").checked ? 1 : 0;
            const fechaHoraActual = new Date();
            data['dia'] = Number(fechaHoraActual.getDate());
            data['mes'] = Number(fechaHoraActual.getMonth() + 1);
            data['anio'] = Number(fechaHoraActual.getFullYear().toString().slice(-2));
            data['diaSem'] = Number(fechaHoraActual.getDay()) + 1;
            data['hr'] = Number(fechaHoraActual.getHours());
            data['min'] = Number(fechaHoraActual.getMinutes());
            data['seg'] = Number(fechaHoraActual.getSeconds());
            delete data.user;
            delete data.pass;
            data.token = getToken();
            const submitBtn = form.querySelector('button[type="submit"]');
            const submitOriginal = submitBtn.textContent;
            submitBtn.disabled = true;
            submitBtn.textContent = "Enviando…";
            fetch("/newparams", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(data)
            })
                .then(async (res) => {
                    const json = await res.json();
                    submitBtn.disabled = false;
                    submitBtn.textContent = submitOriginal;
                    if (res.ok && json.status) {
                        mostrarToast(true, json.message || "Parámetros actualizados");
                        await obtenerValoresDispositivo();
                        actualizarDashboard();
                        setEstado("view");
                        cambiarPaso(stepParams, dashboard, "back");
                    } else if (res.status === 401) {
                        clearToken();
                        mostrarToast(false, json.message || "Tu sesión expiró. Vuelve a iniciar sesión.");
                        setEstado("auth");
                        cambiarPaso(stepParams, stepAuth, "forward");
                    } else {
                        mostrarToast(false, json.message || "Error desconocido");
                    }
                }).catch(() => {
                    submitBtn.disabled = false;
                    submitBtn.textContent = submitOriginal;
                    mostrarToast(false, "Error de comunicación con el dispositivo");
                });
        });
        const WIFI_SCAN_ENDPOINT = "/wifiscan";
        const WIFI_SAVE_ENDPOINT = "/wificredentials";
        function signalBars(rssi) {
            if (rssi >= -55) return "▂▄▆█";
            if (rssi >= -65) return "▂▄▆";
            if (rssi >= -75) return "▂▄";
            return "▂";
        }
        btnWifi.addEventListener("click", async () => {
            await obtenerValoresDispositivo();
            if (params.sessionValid) {
                setEstado("wifi");
                cambiarPaso(dashboard, stepWifi, "forward");
                escanearRedes();
            } else {
                clearToken();
                authIntent = "wifi";
                setEstado("auth");
                cambiarPaso(dashboard, stepAuth, "forward");
            }
        });
        async function escanearRedes() {
            wifiScanning.style.display = "block";
            wifiForm.style.display = "none";
            wifiScanError.style.display = "none";
            btnWifiConnect.style.display = "none";
            btnWifiRescan.style.display = "none";
            let data;
            try {
                const res = await fetch(WIFI_SCAN_ENDPOINT, { headers: { "Accept": "application/json" } });
                if (!res.ok) throw new Error("HTTP " + res.status);
                data = await res.json();
            } catch (e) {
                mostrarErrorEscaneo("No se pudo escanear. Revisa el dispositivo e intenta de nuevo.");
                return;
            }
            const redes = Array.isArray(data.networks) ? data.networks : [];
            if (redes.length === 0) {
                mostrarErrorEscaneo("No se encontraron redes Wi-Fi cercanas.");
                return;
            }
            redes.sort((a, b) => (b.rssi ?? -999) - (a.rssi ?? -999));
            wifiSsidSelect.innerHTML = "";
            redes.forEach((r) => {
                const opt = document.createElement("option");
                opt.value = r.ssid;
                opt.textContent = `${r.ssid}  ${r.secure ? "🔒" : "🔓"} ${signalBars(r.rssi)}`;
                opt.dataset.secure = r.secure ? "1" : "0";
                wifiSsidSelect.appendChild(opt);
            });
            actualizarCampoPass();
            wifiScanning.style.display = "none";
            wifiForm.style.display = "block";
            btnWifiConnect.style.display = "";
            btnWifiRescan.style.display = "";
        }
        function mostrarErrorEscaneo(texto) {
            wifiScanning.style.display = "none";
            wifiForm.style.display = "none";
            wifiScanErrorText.textContent = texto;
            wifiScanError.style.display = "block";
            btnWifiConnect.style.display = "none";
            btnWifiRescan.style.display = "";
        }
        function actualizarCampoPass() {
            const opt = wifiSsidSelect.selectedOptions[0];
            const segura = !opt || opt.dataset.secure === "1";
            wifiPassGroup.style.display = segura ? "" : "none";
            if (!segura) wifiPass.value = "";
        }
        wifiSsidSelect.addEventListener("change", actualizarCampoPass);
        btnWifiRescan.addEventListener("click", escanearRedes);
        btnWifiBack.addEventListener("click", () => {
            setEstado("view");
            cambiarPaso(stepWifi, dashboard, "back");
        });
        btnWifiConnect.addEventListener("click", async () => {
            const opt = wifiSsidSelect.selectedOptions[0];
            if (!opt) { mostrarToast(false, "Selecciona una red."); return; }
            const segura = opt.dataset.secure === "1";
            const ssid = opt.value;
            const pass = wifiPass.value;
            if (segura && (pass.length < 8 || pass.length > 63)) {
                mostrarToast(false, "La contraseña Wi-Fi debe tener entre 8 y 63 caracteres.");
                return;
            }
            const original = btnWifiConnect.textContent;
            btnWifiConnect.disabled = true;
            btnWifiRescan.style.display = "none";
            btnWifiConnect.textContent = "Conectando…";
            let json;
            try {
                const res = await fetch(WIFI_SAVE_ENDPOINT, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ ssid, pass, token: getToken() })
                });
                json = await res.json();
                if (res.status === 401) {
                    clearToken();
                    mostrarToast(false, json.message || "Tu sesión expiró. Vuelve a iniciar sesión.");
                    authIntent = "wifi";
                    btnWifiConnect.disabled = false;
                    btnWifiConnect.textContent = original;
                    setEstado("auth");
                    cambiarPaso(stepWifi, stepAuth, "forward");
                    return;
                }
                if (!res.ok || !json.status) {
                    mostrarToast(false, json.message || "No se pudo iniciar la conexión.");
                    btnWifiConnect.disabled = false;
                    btnWifiConnect.textContent = original;
                    btnWifiRescan.style.display = "";
                    return;
                }
            } catch (e) {
                mostrarToast(false, "Error de comunicación con el dispositivo.");
                btnWifiConnect.disabled = false;
                btnWifiConnect.textContent = original;
                btnWifiRescan.style.display = "";
                return;
            }
            esperarConexion(ssid);
        });
        async function esperarConexion(ssid) {
            const intentos = 10;
            for (let i = 0; i < intentos; i++) {
                await new Promise((r) => setTimeout(r, 1500));
                const ok = await obtenerValoresDispositivo();
                if (ok && params.wifiConnected === true) {
                    btnWifiConnect.disabled = false;
                    btnWifiConnect.textContent = "Conectar";
                    btnWifiRescan.style.display = "";
                    mostrarToast(true, "Conectado a " + (params.wifiSsid || ssid) + ".");
                    actualizarDashboard();
                    setEstado("view");
                    cambiarPaso(stepWifi, dashboard, "back");
                    return;
                }
            }
            btnWifiConnect.disabled = false;
            btnWifiConnect.textContent = "Conectar";
            btnWifiRescan.style.display = "";
            mostrarToast(false, "No se pudo conectar a " + ssid + ". Verifica la contraseña e intenta de nuevo.");
        }
        function validarParametros() {
            const errores = [];
            const planta = document.getElementById("planta").value.trim();
            const fpOnInput = document.getElementById("fpOn").value.trim();
            const fpOffInput = document.getElementById("fpOff").value.trim();
            const irrMinInput = document.getElementById("irrM").value.trim();
            const ventMinInput = document.getElementById("ventM").value.trim();
            const irrHInput = document.getElementById("irrH").value.trim();
            const ventHInput = document.getElementById("ventH").value.trim();
            const ledAInput = document.getElementById("ledA").value.trim();
            const ledRInput = document.getElementById("ledR").value.trim();
            if (planta === "") {
                errores.push("El nombre de la planta es obligatorio");
            } else {
                if (planta.length < 3 || planta.length > 20)
                    errores.push("La planta debe tener entre 3 y 20 caracteres");
                if (!isValidReadableString(planta, true))
                    errores.push("La planta solo admite letras, números, espacios y (_-.@!#$%&*?+=)");
                if (/\s{2,}/.test(planta))
                    errores.push("No uses espacios repetidos");
                if (/^\d+$/.test(planta))
                    errores.push("La planta no puede ser solo números");
                if (hasTooManyRepeatedChars(planta))
                    errores.push("La planta no puede contener más de 3 caracteres repetidos seguidos");
            }
            if (fpOnInput === "" || !isStrictInteger(fpOnInput)) {
                errores.push("La hora de prendido es obligatoria, numérica y sin decimales");
            } else if (Number(fpOnInput) < 0 || Number(fpOnInput) > 23) {
                errores.push("La hora de prendido debe ser un valor entre 0 y 23 horas");
            }
            if (fpOffInput === "" || !isStrictInteger(fpOffInput)) {
                errores.push("La hora de apagado es obligatoria, numérica y sin decimales");
            } else if (Number(fpOffInput) < 0 || Number(fpOffInput) > 23) {
                errores.push("La hora de apagado debe ser un valor entre 0 y 23 horas");
            }
            if (isStrictInteger(fpOnInput) && isStrictInteger(fpOffInput) &&
                Number(fpOnInput) === Number(fpOffInput)) {
                errores.push("La hora de prendido y apagado no pueden ser iguales");
            }
            if (irrMinInput === "" || !isStrictInteger(irrMinInput)) {
                errores.push("El campo riego-minutos es obligatorio, numérico y sin decimales");
            } else if (Number(irrMinInput) < 0 || Number(irrMinInput) > 59) {
                errores.push("El valor de riego debe ser un valor entre 0 y 59 minutos");
            }
            if (ventMinInput === "" || !isStrictInteger(ventMinInput)) {
                errores.push("El campo ventilación-minutos es obligatorio, numérico y sin decimales");
            } else if (Number(ventMinInput) < 0 || Number(ventMinInput) > 59) {
                errores.push("El valor de ventilación debe ser un valor entre 0 y 59 minutos");
            }
            if (!isStrictInteger(irrHInput) || !VALID_FREQUENCIES.includes(Number(irrHInput)))
                errores.push("Selecciona una frecuencia de riego válida");
            if (!isStrictInteger(ventHInput) || !VALID_FREQUENCIES.includes(Number(ventHInput)))
                errores.push("Selecciona una frecuencia de ventilación válida");
            if (!isStrictInteger(ledAInput) || Number(ledAInput) < 0 || Number(ledAInput) > 100)
                errores.push("El LED azul debe estar entre 0 y 100%");
            if (!isStrictInteger(ledRInput) || Number(ledRInput) < 0 || Number(ledRInput) > 100)
                errores.push("El LED rojo debe estar entre 0 y 100%");
            return errores;
        }
        function validarAuth() {
            const errores = [];
            const user = document.getElementById("user").value.trim();
            const pass = document.getElementById("password").value.trim();
            if (user.length < 4 || user.length > 32)
                errores.push("El usuario debe tener entre 4 y 32 caracteres.");
            if (pass.length < 8 || pass.length > 64)
                errores.push("La contraseña debe tener entre 8 y 64 caracteres.");
            if (!isValidReadableString(user, false))
                errores.push("El usuario solo admite letras, números y (_-.@!#$%&*?+=).");
            if (!isValidReadableString(pass, false))
                errores.push("La contraseña solo admite letras, números y (_-.@!#$%&*?+=).");
            if (hasTooManyRepeatedChars(user))
                errores.push("El usuario no puede contener más de 3 caracteres repetidos seguidos.");
            if (hasTooManyRepeatedChars(pass))
                errores.push("La contraseña no puede contener más de 3 caracteres repetidos seguidos.");
            return errores;
        }
        function updateSliderProgress(sliderId, progressId) {
            const slider = document.getElementById(sliderId);
            const progress = document.getElementById(progressId);
            if (!slider || !progress) return;
            const percent = ((slider.value - slider.min) / (slider.max - slider.min)) * 100;
            let color;
            if (sliderId === 'ledA') color = '#2196F3';
            else if (sliderId === 'ledR') color = '#f44336';
            progress.style.background = `linear-gradient(to right, ${color} 0%, ${color} ${percent}%, #e0e0e0 ${percent}%, #e0e0e0 100%)`;
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
    </script>
</body>
</html>
)===";
