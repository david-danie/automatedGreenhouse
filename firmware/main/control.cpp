// ===========================================================================
//  control.cpp — Lógica de control de actuadores. Ver control.h.
//  Port fiel de turnOnDevices()/manageDevice() del firmware Arduino, adaptado al
//  modelo IDF: config en CropConfig, salidas vía actuators, frecuencias uint16_t.
// ===========================================================================
#include "control.h"
#include "actuators.h"
#include "utils.h"        // daysSinceEpoch
#include "Constants.h"

// Bit de systemEnable dentro de CropConfig.flags (ver storage.h).
static const uint8_t FLAG_SYSTEM_ENABLE = 0x04;   // bit2

// Decide y acciona una salida digital por intervalo (bomba o ventilador).
// intervalHours = horas entre activaciones (0 = apagado). El dispositivo queda
// encendido los primeros durationMinutes de la hora en que toca. STATELESS: todo
// se recalcula del contador continuo epochHours (módulo), sin guardar estado.
static bool deviceActive(uint16_t intervalHours, uint8_t durationMinutes,
                         uint32_t epochHours, uint8_t minuteNow) {
    if (intervalHours == 0) return false;
    return (epochHours % (uint32_t)intervalHours == 0) && (minuteNow < durationMinutes);
}

namespace control {

void update(const CropConfig& cfg, const uint8_t* t, bool rtcValid) {
    // 1. Interruptor general: con el sistema DESACTIVADO no se acciona nada.
    if (!(cfg.flags & FLAG_SYSTEM_ENABLE)) {
        actuators::allOff();
        return;
    }

    // 2. Fail-safe: sin hora fiable del RTC no se agenda nada (evita regar de más
    //    o dejar las luces mal). Todo apagado hasta que la hora vuelva a ser válida.
    if (!rtcValid) {
        actuators::allOff();
        return;
    }

    // 3. Luces: ventana de fotoperiodo [on, off). Si on < off es continua; si
    //    on > off, cruza medianoche.
    uint8_t h   = t[hour];
    uint8_t on  = cfg.photoperiodOn;
    uint8_t off = cfg.photoperiodOff;
    bool luzEncendida = (on < off) ? (h >= on && h < off) : (h >= on || h < off);

    if (luzEncendida) {
        actuators::setWhite(cfg.whiteLedOn > 0);
        actuators::setBlueDuty(cfg.blueDutyCycle);   // 0-100 %
        actuators::setRedDuty(cfg.redDutyCycle);
    } else {
        actuators::setWhite(false);
        actuators::setBlueDuty(0);
        actuators::setRedDuty(0);
    }

    // 4. Riego y ventilación: contador continuo de horas derivado del RTC,
    //    calculado UNA vez y compartido por ambos.
    uint32_t epochHours =
        daysSinceEpoch(2000 + t[year], t[month], t[day]) * 24UL + t[hour];

    actuators::setPump(deviceActive(cfg.irrigationFrequency, cfg.irrigationDuration,
                                    epochHours, t[minute]));
    actuators::setFan(deviceActive(cfg.ventilationFrequency, cfg.ventilationDuration,
                                   epochHours, t[minute]));
}

// Edad del cultivo en días (>=1), derivada del RTC y del ancla. STATELESS: se
// calcula del calendario, no de un contador, así es correcta tras cortes de luz.
int cropDayFromRtc(const CropConfig& cfg, const uint8_t* t, bool rtcValid) {
    if (!rtcValid) return 0;                 // sin hora fiable: no inventes edad
    if (cfg.cropStartDay == 0) return 0;     // cultivo sin anclar
    uint32_t today = daysSinceEpoch(2000 + t[year], t[month], t[day]);
    if (today < cfg.cropStartDay) return 0;  // RTC retrocedió: evita negativos
    return (int)(today - cfg.cropStartDay) + 1;  // día del ancla = día 1
}

} // namespace control