/*
 * ============================================================================
 *  UNMSM - Escuela Profesional de Ingeniería Biomédica
 *  Instrumentación Biomédica III - Laboratorio N.º 2
 *  Etapa de ganancia y calibración
 * ----------------------------------------------------------------------------
 *  Simulador de electrodo de pH con el DAC interno del ESP32 DevKit V1
 *
 *    Vin = Vbias - S * (pH - 7)
 *      Vbias = 1.65 V   (mitad del rango 0-3.3 V del DAC)
 *      S     = 0.05916 V/pH (pendiente de Nernst a 25 °C)
 *
 *  Salida analógica : GPIO25 (DAC1)  --> pin 3 (IN+) del TL084 (buffer, Etapa A)
 *  Referencia       : GND del ESP32  --> GND común de la fuente dual
 *
 *  Uso (Monitor Serie a 115200 baudios, fin de línea "Nueva línea"):
 *    7        -> genera el voltaje para pH = 7 (acepta decimales: 4.5)
 *    tabla    -> imprime la tabla teórica de la Etapa C (Vin, Vout, código DAC)
 *    estado   -> muestra el pH y el voltaje que se está generando ahora
 *    vfs 3.28 -> ajusta el fondo de escala real del DAC (medido con multímetro)
 *    ayuda    -> lista de comandos
 * ============================================================================
 */

#include <Arduino.h>

// ----------------------------- Configuración --------------------------------
namespace Config {
  constexpr uint8_t  DAC_PIN       = 25;       // DAC1 (GPIO25). DAC2 = GPIO26
  constexpr uint32_t BAUDRATE      = 115200;

  constexpr float    V_BIAS        = 1.65f;    // V   (pH neutro)
  constexpr float    NERNST_S      = 0.05916f; // V/pH a 25 °C
  constexpr float    PH_NEUTRO     = 7.0f;
  constexpr float    PH_MIN        = 0.0f;
  constexpr float    PH_MAX        = 14.0f;

  constexpr float    GANANCIA_AV   = 3.0f;     // Av = 1 + Rf/R1 = 1 + 20k/10k
  constexpr uint8_t  DAC_BITS      = 8;        // DAC del ESP32: 8 bits
  constexpr uint16_t DAC_MAX_CODE  = (1 << DAC_BITS) - 1;  // 255
  constexpr float    DAC_VFS_INIT  = 3.30f;    // fondo de escala nominal (V)

  constexpr float    PH_INICIAL    = 7.0f;     // al encender: salida = Vbias
}

// Puntos de pH de la tabla de toma de datos (Etapa C)
const float PH_TABLA[] = {0, 2, 4, 6, 7, 8, 10, 12, 14};
const size_t N_TABLA   = sizeof(PH_TABLA) / sizeof(PH_TABLA[0]);

// ------------------------------- Estado -------------------------------------
struct EstadoSimulador {
  float   ph        = Config::PH_INICIAL;
  float   vinIdeal  = Config::V_BIAS;   // V según Nernst (sin cuantizar)
  float   vinReal   = Config::V_BIAS;   // V que realmente entrega el DAC
  uint8_t codigoDAC = 128;
  float   dacVfs    = Config::DAC_VFS_INIT;
} estado;

// ------------------------- Modelo del electrodo -----------------------------
// Voltaje teórico del electrodo simulado para un pH dado
float nernstVoltaje(float ph) {
  return Config::V_BIAS - Config::NERNST_S * (ph - Config::PH_NEUTRO);
}

// Convierte un voltaje deseado al código entero del DAC (0-255)
uint8_t voltajeACodigo(float v, float vfs) {
  long codigo = lroundf(v / vfs * Config::DAC_MAX_CODE);
  return (uint8_t)constrain(codigo, 0L, (long)Config::DAC_MAX_CODE);
}

// Voltaje equivalente (ideal) de un código DAC
float codigoAVoltaje(uint8_t codigo, float vfs) {
  return codigo * vfs / Config::DAC_MAX_CODE;
}

bool phValido(float ph) {
  return !isnan(ph) && ph >= Config::PH_MIN && ph <= Config::PH_MAX;
}

// ------------------------------ Salida DAC ----------------------------------
void aplicarPH(float ph) {
  estado.ph        = ph;
  estado.vinIdeal  = nernstVoltaje(ph);
  estado.codigoDAC = voltajeACodigo(estado.vinIdeal, estado.dacVfs);
  estado.vinReal   = codigoAVoltaje(estado.codigoDAC, estado.dacVfs);
  dacWrite(Config::DAC_PIN, estado.codigoDAC);
}

// ------------------------------ Interfaz serie ------------------------------
void imprimirLinea() {
  Serial.println(F("----------------------------------------------------------------"));
}

void imprimirEstado() {
  Serial.printf("pH = %5.2f | Vin teorico = %.4f V | DAC = %3u -> Vin DAC = %.4f V | "
                "Vout esperado (Av=%.0f) = %.4f V\n",
                estado.ph, estado.vinIdeal, estado.codigoDAC, estado.vinReal,
                Config::GANANCIA_AV, Config::GANANCIA_AV * estado.vinReal);
}

void imprimirTabla() {
  imprimirLinea();
  Serial.printf("Tabla teorica (Vfs DAC = %.3f V, Av = %.1f)\n", estado.dacVfs, Config::GANANCIA_AV);
  Serial.println(F(" pH | Vin teor (V) | Vout teor (V) | cod DAC | Vin DAC (V) | Vout DAC (V)"));
  for (size_t i = 0; i < N_TABLA; i++) {
    float ph   = PH_TABLA[i];
    float vin  = nernstVoltaje(ph);
    uint8_t c  = voltajeACodigo(vin, estado.dacVfs);
    float vq   = codigoAVoltaje(c, estado.dacVfs);
    Serial.printf("%3.0f |    %.4f    |    %.4f     |   %3u   |   %.4f    |   %.4f\n",
                  ph, vin, Config::GANANCIA_AV * vin, c, vq, Config::GANANCIA_AV * vq);
  }
  Serial.printf("Sensibilidad teorica del sistema = Av*S = %.4f V/pH\n",
                Config::GANANCIA_AV * Config::NERNST_S);
  imprimirLinea();
}

void imprimirAyuda() {
  imprimirLinea();
  Serial.println(F("Simulador de pH - Lab 2 (IB III, UNMSM)"));
  Serial.println(F("Comandos:"));
  Serial.println(F("  <numero>   pH a simular, 0 a 14 (ej: 7  o  4.5)"));
  Serial.println(F("  tabla      tabla teorica para la Etapa C"));
  Serial.println(F("  estado     pH y voltaje actuales"));
  Serial.println(F("  vfs <V>    fondo de escala real del DAC (ej: vfs 3.28)"));
  Serial.println(F("  ayuda      esta lista"));
  Serial.printf ("Salida: GPIO%u (DAC1). Resolucion: %.1f mV/paso (~%.2f pH)\n",
                 Config::DAC_PIN, 1000.0f * estado.dacVfs / Config::DAC_MAX_CODE,
                 (estado.dacVfs / Config::DAC_MAX_CODE) / Config::NERNST_S);
  imprimirLinea();
}

// Devuelve true si el texto es un número decimal válido (ej: "7", "4.5", "-1")
bool esNumero(const String &s, float &valor) {
  if (s.length() == 0) return false;
  char *fin = nullptr;
  valor = strtof(s.c_str(), &fin);
  return fin != s.c_str() && *fin == '\0';
}

void procesarComando(String entrada) {
  entrada.trim();
  entrada.replace(',', '.');           // acepta "4,5" como 4.5
  if (entrada.length() == 0) return;

  String cmd = entrada;
  cmd.toLowerCase();

  float valor;
  if (esNumero(cmd, valor)) {
    if (!phValido(valor)) {
      Serial.printf("[ERROR] pH = %.2f fuera de rango. Ingrese un valor entre %.0f y %.0f.\n",
                    valor, Config::PH_MIN, Config::PH_MAX);
      return;
    }
    aplicarPH(valor);
    Serial.print(F("[OK] "));
    imprimirEstado();
  }
  else if (cmd == "tabla")  imprimirTabla();
  else if (cmd == "estado") imprimirEstado();
  else if (cmd == "ayuda" || cmd == "help" || cmd == "?") imprimirAyuda();
  else if (cmd.startsWith("vfs")) {
    float v;
    String arg = cmd.substring(3); arg.trim();
    if (esNumero(arg, v) && v > 2.5f && v < 3.6f) {
      estado.dacVfs = v;
      aplicarPH(estado.ph);            // regenera con la nueva escala
      Serial.printf("[OK] Fondo de escala del DAC = %.3f V\n", v);
      imprimirEstado();
    } else {
      Serial.println(F("[ERROR] Uso: vfs <voltios>, entre 2.5 y 3.6 (ej: vfs 3.28)"));
    }
  }
  else {
    Serial.printf("[ERROR] Comando no reconocido: \"%s\". Escriba 'ayuda'.\n", entrada.c_str());
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(Config::BAUDRATE);
  delay(500);
  aplicarPH(Config::PH_INICIAL);       // arranca en pH 7 -> 1.65 V
  imprimirAyuda();
  imprimirEstado();
  Serial.println(F("Ingrese un valor de pH:"));
}

void loop() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    procesarComando(linea);
  }
}
