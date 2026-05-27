#include <ESP32Servo.h>

// ── Pines
#define PIN_SERVO     13
#define PIN_TRIG       5
#define PIN_ECHO      18
#define LED_VERDE     25
#define LED_AMARILLO  26
#define LED_ROJO      27
#define PIN_BUZZER    14

Servo miServo;

// ── Semáforos y Variables Globales ─────────────────────────
SemaphoreHandle_t mutex_servo;
SemaphoreHandle_t mutex_uart;
SemaphoreHandle_t mutex_sensor; 
SemaphoreHandle_t sem_binario;

volatile bool modoInversion = false;

// ── Prototipos de Funciones ────────────────────────────────
void TaskRadar(void* pv);
void TaskCalib(void* pv);
void TaskAlerta(void* pv);
void TaskLogger(void* pv);
long medirDistancia();
void enviarEstado(const char* tarea, const char* mutex_estado, const char* nota);

// ═════════════════════════════════════════════════════════════
// SETUP
// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(LED_VERDE,    OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO,     OUTPUT);
  pinMode(PIN_BUZZER,   OUTPUT);

  miServo.attach(PIN_SERVO);
  miServo.write(90);

  mutex_servo  = xSemaphoreCreateMutex();
  mutex_uart   = xSemaphoreCreateMutex();
  mutex_sensor = xSemaphoreCreateMutex(); 
  
  sem_binario  = xSemaphoreCreateBinary();
  xSemaphoreGive(sem_binario);

  xTaskCreatePinnedToCore(TaskRadar,  "Radar",  4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(TaskCalib,  "Calib",  2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskAlerta, "Alerta", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(TaskLogger, "Logger", 4096, NULL, 2, NULL, 1);
}

void loop() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg == "INVERSION_ON")  modoInversion = true;
    if (msg == "INVERSION_OFF") modoInversion = false;
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}

// ═════════════════════════════════════════════════════════════
// TAREA RADAR: REACCIÓN SUAVE Y VELOCIDAD AJUSTADA
// ═════════════════════════════════════════════════════════════
void TaskRadar(void* pv) {
  int angulo = 15;
  int delta  = 2; 

  for (;;) {
    SemaphoreHandle_t recurso = modoInversion ? sem_binario : mutex_servo;

    if (xSemaphoreTake(recurso, pdMS_TO_TICKS(50)) == pdTRUE) {
      miServo.write(angulo);
      vTaskDelay(pdMS_TO_TICKS(25)); 
      
      long dist = medirDistancia();

      if (xSemaphoreTake(mutex_uart, pdMS_TO_TICKS(20)) == pdTRUE) {
        char json[128];
        snprintf(json, sizeof(json), "{\"tipo\":\"radar\",\"angulo\":%d,\"dist\":%ld,\"modo\":\"%s\"}", 
                 angulo, dist, modoInversion ? "inversion" : "normal");
        Serial.println(json);
        xSemaphoreGive(mutex_uart);
      }
      xSemaphoreGive(recurso);
      
      angulo += delta;
      if (angulo >= 165 || angulo <= 15) delta = -delta;
    }
    
    // Control de velocidad lenta solicitado para proteger el puerto
    vTaskDelay(pdMS_TO_TICKS(150));
  }
}

// ═════════════════════════════════════════════════════════════
// TAREA CALIB: RUTINA PERIÓDICA DE MANTENIMIENTO DEL SERVO
// ═════════════════════════════════════════════════════════════
void TaskCalib(void* pv) {
  vTaskDelay(pdMS_TO_TICKS(3000));
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(40000)); 
    
    SemaphoreHandle_t recurso = modoInversion ? sem_binario : mutex_servo;
    
    if (xSemaphoreTake(recurso, pdMS_TO_TICKS(5000)) == pdTRUE) {
      enviarEstado("TaskCalib", "TOMADO", "calibrando...");
      digitalWrite(LED_AMARILLO, HIGH);
      
      for (int i = 15; i <= 90; i += 2) {
        miServo.write(i);
        vTaskDelay(pdMS_TO_TICKS(25));
      }
      
      miServo.write(90);
      vTaskDelay(pdMS_TO_TICKS(200));
      digitalWrite(LED_AMARILLO, LOW);
      
      xSemaphoreGive(recurso);
      enviarEstado("TaskCalib", "LIBRE", "ok");
    }
  }
}

// ═════════════════════════════════════════════════════════════
// TAREA ALERTA: SISTEMA MULTI-ZONA TIPO SENSOR DE REVERSA
// ═════════════════════════════════════════════════════════════
void TaskAlerta(void* pv) {
  int contadorFalsos = 0;
  
  for (;;) {
    long dist = medirDistancia();
    
    if (dist > 2 && dist <= 100) {
      contadorFalsos++;
    } else {
      contadorFalsos = 0;
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AMARILLO, LOW);
      digitalWrite(LED_ROJO, LOW);
      digitalWrite(PIN_BUZZER, LOW);
    }
    
    if (contadorFalsos >= 3) {
      if (dist <= 20) {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_AMARILLO, LOW);
        digitalWrite(LED_ROJO, HIGH);
        digitalWrite(PIN_BUZZER, HIGH); 
        
        enviarEstado("TaskAlerta", "ESPERANDO", "PELIGRO < 20cm");
        
        SemaphoreHandle_t recurso = modoInversion ? sem_binario : mutex_servo;
        if (xSemaphoreTake(recurso, pdMS_TO_TICKS(2000)) == pdTRUE) {
          enviarEstado("TaskAlerta", "TOMADO", "Servo Bloqueado");
          miServo.write(90);           
          vTaskDelay(pdMS_TO_TICKS(500)); 
          xSemaphoreGive(recurso);
          enviarEstado("TaskAlerta", "LIBRE", "Liberando servo");
        }
      } 
      else if (dist <= 50) {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_AMARILLO, HIGH);
        digitalWrite(LED_ROJO, LOW);
        
        enviarEstado("TaskAlerta", "LIBRE", "Advertencia < 50cm");
        
        digitalWrite(PIN_BUZZER, HIGH);
        vTaskDelay(pdMS_TO_TICKS(80)); 
        digitalWrite(PIN_BUZZER, LOW);
      } 
      else if (dist <= 100) {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_AMARILLO, LOW);
        digitalWrite(LED_ROJO, LOW);
        digitalWrite(PIN_BUZZER, LOW); 
        
        enviarEstado("TaskAlerta", "LIBRE", "Objeto detectado");
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}

// ═════════════════════════════════════════════════════════════
// TAREA LOGGER: TELEMETRÍA MÁS LENTA Y LIMPIA
// ═════════════════════════════════════════════════════════════
void TaskLogger(void* pv) {
  for (;;) {
    if (xSemaphoreTake(mutex_uart, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Solo enviamos un mensaje de actividad en lugar de saturar la consola
      enviarEstado("TaskLogger", "ACTIVO", "Monitor OK");
      
      if (modoInversion) {
        volatile uint32_t x = 0;
        for (uint32_t i = 0; i < 500000; i++) x += i; 
      }
      xSemaphoreGive(mutex_uart);
    }
    // Aumentamos el delay a 3 segundos (3000ms) para que no haga spam
    vTaskDelay(pdMS_TO_TICKS(modoInversion ? 150 : 3000));
  }
}

// ═════════════════════════════════════════════════════════════
// FUNCIONES AUXILIARES: DISPARO ULTRASONIDO Y ENVÍO UART
// ═════════════════════════════════════════════════════════════
long medirDistancia() {
  long distanciaFinal = 0;
  if (xSemaphoreTake(mutex_sensor, pdMS_TO_TICKS(30)) == pdTRUE) {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    
    distanciaFinal = (pulseIn(PIN_ECHO, HIGH, 15000) * 0.034) / 2;
    
    xSemaphoreGive(mutex_sensor);
  }
  return distanciaFinal;
}

void enviarEstado(const char* tarea, const char* mutex_estado, const char* nota) {
  if (xSemaphoreTake(mutex_uart, pdMS_TO_TICKS(10)) == pdTRUE) {
    char json[200];
    snprintf(json, sizeof(json), "{\"tipo\":\"estado\",\"tarea\":\"%s\",\"mutex\":\"%s\",\"nota\":\"%s\"}", 
             tarea, mutex_estado, nota);
    Serial.println(json);
    xSemaphoreGive(mutex_uart);
  }
}