# Technical Documentation

## Purpose

Guiar la redacción, organización y mantenimiento de la documentación de SmartPlant. Aplica a cualquier documento del repositorio: README, `docs/`, guías y documentación del firmware.

El objetivo es una documentación técnica clara y natural, similar a la de los proyectos de código abierto bien documentados. Debe explicar con precisión qué es el proyecto, cómo funciona y cómo se utiliza, sin exagerar sus capacidades ni complicar su explicación.

---

## Contexto del proyecto

SmartPlant es un controlador embebido basado en un ESP32-C3 para automatizar tareas en sistemas de cultivo. Acciona dispositivos como iluminación, riego y ventilación según la configuración establecida. Está pensado para invernaderos y otros espacios de cultivo, sin limitarse a instalaciones indoor.

La configuración se hace desde un portal web alojado en el propio microcontrolador, accesible desde un celular, sin aplicaciones externas ni servicios en la nube. El propósito general es explorar cómo la tecnología puede facilitar la producción local y sustentable de hortalizas.

Este contexto fija el alcance general. No debe usarse para suponer funcionalidades que no estén confirmadas.

---

## Tono y estilo

- Lenguaje técnico, pero accesible. Escribir de forma directa, natural y profesional.
- Priorizar la claridad sobre la formalidad.
- Evitar el tono publicitario, grandilocuente o excesivamente académico.
- Usar una explicación sencilla cuando sea suficiente; no recurrir a expresiones sofisticadas sin necesidad.
- Evitar explicaciones redundantes y repeticiones.
- Resolver cada apartado de forma concisa; no alargarlo por costumbre.
- Mantener una redacción fluida. Un párrafo no debe ser una lista de especificaciones disfrazada.

## Criterios técnicos

- Respetar el alcance y las funcionalidades reales del proyecto.
- No inventar características, capacidades, comportamientos ni decisiones de diseño.
- No presuponer implementaciones que no estén confirmadas.
- Distinguir con claridad entre lo **implementado**, lo **previsto** y las **posibles mejoras futuras**. El estado detallado vive en [ROADMAP.md](../../../docs/ROADMAP.md#estado-actual).
- Usar terminología técnica precisa y la misma en todos los archivos.
- Si una descripción técnica admite varias interpretaciones, consultar antes de asumir.
- No presentar el sistema como más complejo de lo que es.

## Criterios de redacción

- Integrar las aclaraciones dentro de la explicación, de forma natural.
- Evitar la postura defensiva, como si el texto respondiera a objeciones o malentendidos.
- Limitar las construcciones negativas («no es», «no pretende», «no está diseñado para», «no depende de») a los casos en que expliquen una característica.
- No repetir información ya explicada en otro apartado; enlazar a la sección correspondiente.
- No agregar introducciones ni conclusiones genéricas para rellenar.
- Al revisar un texto existente, conservar su sentido y su intención.

---

## Organización

Cada archivo tiene un propósito. No se mezclan niveles de documentación sin necesidad.

| Tipo | Contenido |
|---|---|
| README | Presentación general, propósito, características principales y descripción del proyecto |
| Documentación técnica | Arquitectura, componentes, funcionamiento y decisiones de diseño |
| Guías de instalación | Requisitos, conexiones, configuración y puesta en marcha |
| Manual de uso | Instrucciones prácticas para utilizar y configurar el dispositivo |
| Documentación del firmware | Módulos, funciones, lógica de funcionamiento y estructura del código |
| Contribución y mantenimiento | Criterios para modificar, ampliar y mantener el proyecto |

Los documentos actuales y su relación con estos tipos están en el [índice de la documentación](../../../docs/README.md).

## Forma de trabajo

- Entender el contexto y el propósito del apartado antes de redactar.
- Si falta información técnica importante, preguntar; no inventarla.
- Ante un texto existente, identificar primero qué necesita mejorar y conservar lo que ya funciona.
- Si se propone reorganizar, explicar brevemente por qué mejora la documentación.
- Mantener la coherencia terminológica y de estilo entre archivos, incluida la versión en inglés ([README.en.md](../../../README.en.md)).
- No modificar el alcance conceptual del proyecto por iniciativa propia.
- Entregar textos listos para incorporarse al repositorio, sin comentarios editoriales mezclados.
