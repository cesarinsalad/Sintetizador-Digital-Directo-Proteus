# Generador de Ondas Digital con PIC18F4550 y GLCD

Este repositorio contiene el código fuente y el esquemático de simulación para un generador de funciones digital basado en el microcontrolador PIC18F4550. El sistema es capaz de generar cuatro tipos de formas de onda con frecuencia y amplitud ajustables, controladas a través de una interfaz de usuario en una pantalla GLCD de 128x64.

(Sugerencia: Reemplaza esta URL con una captura de pantalla de tu propio circuito en Proteus)
---
# 📋 Características

**Microcontrolador**: PIC18F4550 funcionando a 48 MHz (a partir de un cristal de 20 MHz).

## Generación de Ondas:

_Senoidal:_ 10 Hz - 100 Hz | Vpp 1.0V - 2.3V.

_Cuadrada:_ 15 Hz - 150 Hz | Vp 3.3V - 4.5V.

_Triangular:_ 20 Hz - 200 Hz | Vp 2.0V - 4.4V.

_Diente de Sierra (Pendiente Negativa):_ 30 Hz - 300 Hz | Vp 1.5V - 4.6V.

**Salida Analógica:** DAC R-2R de 8 bits implementado en hardware.

**Interfaz de Usuario:** Pantalla gráfica GLCD de 128x64 píxeles y 4 pulsadores para la navegación por menús.

**Técnica de Generación:** Síntesis Digital Directa (DDS) utilizando interrupciones del Timer1 para una generación precisa y en segundo plano.
---
# ⚙️ Diseño del Hardware

El circuito fue diseñado y simulado en Proteus 8 Professional. Los componentes clave incluyen:

- PIC18F4550 (U1): El cerebro del sistema.

- GLCD 128x64 (LCD1): Para la visualización de la interfaz. PORTD se utiliza para el bus de datos y PORTB para las señales de control.

- DAC R-2R: Una red de resistencias de 10kΩ y 20kΩ conectada a PORTC y PORTA para convertir la señal digital en una salida analógica.

- Oscilador: Un cristal de 20 MHz con dos capacitores de 22pF.

- Pulsadores: Cuatro botones conectados a PORTA con resistencias de pull-up para la entrada del usuario.

- Circuito de Reset: Una resistencia de pull-up de 10kΩ en el pin MCLR para garantizar un funcionamiento estable.
---
# 💡 Arquitectura del Software

El firmware fue desarrollado en C con el compilador CCS C. La arquitectura se basa en la separación de tareas críticas en tiempo y tareas de interfaz.

## **Puntos Clave del Código:**

**Síntesis Digital Directa (DDS):**

- Una interrupción del Timer1 se dispara a una alta frecuencia (40 kHz).

- Un acumulador de fase de 16 bits se incrementa en cada interrupción. El tamaño del incremento determina la frecuencia de salida.

- Para la onda senoidal, se utiliza una tabla de consulta (LUT) de 256 puntos para una alta fidelidad. La tabla se escala en tiempo de ejecución (fuera de la ISR) para optimizar el rendimiento.

- Las demás ondas se generan algorítmicamente dentro de la ISR.

## **Configuración Robusta (main):**

El programa sigue una secuencia de inicialización estricta:

- Deshabilitar periféricos conflictivos como el Parallel Slave Port (PSP_DISABLED) y el ADC.

- Configurar la dirección de los puertos (TRIS).

- Inicializar la GLCD.

- Configurar y habilitar las interrupciones.

## Interfaz de Usuario:

- Un sistema de menús basado en una máquina de estados (enum).

- Se utiliza una bandera ui_needs_update para que la pantalla solo se redibuje cuando hay un cambio, evitando el parpadeo y liberando ciclos de CPU.

- Se implementó una lógica de validación de límites que ajusta automáticamente los parámetros al cambiar de tipo de onda, previniendo estados inválidos.
---
# 🚀 Cómo Usar

## Simulación:

1. Abre el archivo de proyecto .pdsprj en Proteus 8 o superior.

2. Haz doble clic en el microcontrolador PIC18F4550.

3. Carga el archivo .hex compilado en la propiedad "Program File".

4. Asegúrate de que la frecuencia del procesador esté establecida en 48MHz.

5. Ejecuta la simulación.

**Compilación:**

- El código fuente se encuentra en el archivo DDS_Proyecto.c.

- El proyecto requiere las librerías HDM64GS12.c y graphics.c del compilador CCS C.

- Compila el proyecto para generar el archivo .hex.
---
# 📸 Capturas de Pantalla

(Aquí algunas capturas de pantalla de tu simulación, mostrando el menú en la LCD y las diferentes ondas en el osciloscopio.)

## Menú Principal en la GLCD

## Onda Senoidal en el Osciloscopio
---
Desarrollado como parte del curso de Sistemas Digitales.
