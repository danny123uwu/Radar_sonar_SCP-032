# 🛰️ Radar Sonar — Conexión Directo por USB usando FreeRTOS

Este es un proyecto de electrónica, que hice con compañeros para el curso de Sistemas Operativos. El radar escanea el entorno en dos dimensiones y manda los datos en tiempo real a una interfaz web. Lo interesante aquí es que el código no usa el clásico bucle repetitivo, sino que corre sobre **FreeRTOS** para manejar todo con un sistema de procesos por prioridades.

---

- 1 Microcontrolador ESP32 (es el cerebro que corre todo el sistema de prioridades con FreeRTOS).
- 1 Sensor Ultrasonido HC-SR04 (para medir las distancias y detectar los objetos).  
- 1 Servomotor (SG90 o MG90S) (el motor que hace girar el sensor para el barrido del radar).
- 1 Buzzer (para la alarma acústica cuando algo se acerca demasiado).
- 3 LEDs de colores:1 LED Verde (indicador de zona libre).  
- 1 LED Amarillo (indicador de advertencia).  
- 1 LED Rojo (indicador de peligro crítico). 
- 3 Resistencias (adecuadas para proteger tus LEDs, usually de 220 o 330 ohmios).
- 1 Cable de datos USB (para alimentar el ESP32 y conectar el radar directo a la página web).  
- Protoboard y Cables Jumpers (para hacer todas las uniones de los pines limpiamente).


---

## 📺 Videos de Demostración

Aquí se puede ver cómo funciona el radar escaneando el área:



https://github.com/user-attachments/assets/a2d04862-e4c2-40b6-a241-3f74994c4d53


Y en este otro video se ve la prueba de cómo reacciona el sistema de prioridades y cómo bloquea el servo cuando algo entra en la zona de peligro:

https://github.com/user-attachments/assets/e00e73e2-d2ac-4fef-9a21-e4a80317db2a

---

## 🧠 ¿Cómo funciona el código? (FreeRTOS)

Como este sistema usa un orden de prioridades, dividí el código en tareas independientes (`Tasks`). Así nos aseguramos de que las cosas más importantes (como frenar el motor si hay un choque) pasen antes que las menos importantes (como imprimir texto en la pantalla).

### 📋 Mis Tareas y sus Prioridades

* **`TaskRadar` (Prioridad 3):** Es la que mueve el servomotor de lado a lado (de 15° a 165°) y va midiendo la distancia con el sensor ultrasónico.
* **`TaskAlerta` (Prioridad 3):** Revisa la distancia en todo momento. Si detecta algo cerca, prende los LEDs, suena el buzzer y congela el servo si hay peligro.
* **`TaskLogger` (Prioridad 2):** Se encarga de enviar el estado de salud del sistema a la consola para saber que todo va bien, pero con un delay largo para no saturar la comunicación.
* **`TaskCalib` (Prioridad 1):** Es una rutina de mantenimiento que se activa cada 40 segundos para calibrar el servo y comprobar que se mueva bien.

### 🔒 Candados para evitar errores (Mutexes)
Para que las tareas no se estorben entre sí ni intenten usar el mismo componente al mismo tiempo, usé unos candados llamados **Mutexes**:
* **`mutex_servo`:** Evita que el radar y la calibración muevan el motor a la vez.
* **`mutex_uart`:** Cuida que los mensajes JSON no se amontonen ni se rompan al mandarlos a la computadora.
* **`mutex_sensor`:** Protege los pines del sensor ultrasónico para que la medición sea limpia.

---

## 🔌 Conexiones del Hardware

Para volver a armarlo o usarlo de base, estas son las conexiones en el ESP32:

* **Servomotor (SG90/MG90S):** Pin `GPIO 13`
* **Sensor Ultrasonido HC-SR04 (Trig):** Pin `GPIO 5`
* **Sensor Ultrasonido HC-SR04 (Echo):** Pin `GPIO 18`
* **Buzzer:** Pin `GPIO 14`
* **LED Verde (Zona Libre):** Pin `GPIO 25`
* **LED Amarillo (Advertencia):** Pin `GPIO 26`
* **LED Rojo (Peligro Crítico):** Pin `GPIO 27`

---

## 💻 La Página Web (Interfaz Gráfica)

El archivo `PAGINA_SONAR.html` es la interfaz web del proyecto. Está hecha con HTML, CSS y JavaScript nativo.
* **Conexión Directa:** Usa la tecnología *Web Serial API* para conectarse directamente al ESP32 por medio del cable USB, leyendo los datos sin necesidad de instalar programas raros o servidores intermedios en la computadora.
* **Filtros y Audio:** La página limpia los ruidos de lectura (ignora errores de distancia) y reproduce la alarma de fondo (`Music.mp3`) de manera automática cuando el radar detecta que un objeto se metió a la zona roja de peligro (a 20 cm o menos).

---

## 🚀 Cómo ponerlo a correr

1. Abre `Proyecto_final_funcio.ino` en el IDE de Arduino, instala la librería `ESP32Servo` y sube el código al ESP32.
2. Conecta todos los componentes como dice la lista de arriba.
3. Abre el archivo `PAGINA_SONAR.html` en tu navegador de preferencia.
4. **Importante:** Asegúrate de cerrar el Monitor Serie del IDE de Arduino antes ya que aveces el proceso de la consola de este mismo crea cconflicto con la comunicacion de la pagina con el ESP32, dale al botón **🔌 CONECTAR AL ESP32 POR USB** en la página web y selecciona el puerto donde está conectado tu circuito.
