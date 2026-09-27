/*
 * ============================================================================
 *  INSTRUMENTACIÓN BIOMÉDICA III — Guía de Laboratorio N.º 1
 *  Acondicionamiento de señales de alta impedancia
 *
 *  Simulador de electrodo de pH con ESP32 (DAC interno, GPIO25)
 *
 *  Cumple los requisitos de la sección 5 de la guía:
 *   - Lee un valor de pH (0 a 14) desde el Monitor Serial
 *   - Calcula el voltaje con la ecuación de Nernst (sección 2.2)
 *   - Aplica un factor de escala para hacer visible la variación
 *   - Limita el resultado al rango válido del DAC (0 a 3.3 V)
 *   - Entrega el voltaje en GPIO25 mediante el DAC interno
 *   - Muestra pH ingresado y voltaje entregado en el Monitor Serial
 *
 *  Escuela Profesional de Ingeniería Biomédica — UNMSM
 * ============================================================================
 */

// ------------------------- CONFIGURACIÓN --------------------------------

const int PIN_DAC = 25;          // GPIO25 = canal 1 del DAC interno del ESP32

const float VREF     = 3.3;      // Voltaje de referencia del DAC [V]
const float E0       = 1.65;     // Offset: punto neutro (pH 7) al centro del
                                 // rango del DAC -> permite pH ácidos y básicos
const float PENDIENTE = 0.05916; // Pendiente de Nernst a 25 °C [V / unidad pH]
const float K        = 3.0;      // Factor de escala (amplifica la variación
                                 // para que se vea bien en el osciloscopio)

// Con K = 3:  59.16 mV/pH  ->  177.5 mV/pH
// pH 0  -> 2.892 V     pH 4  -> 2.182 V
// pH 7  -> 1.650 V     pH 10 -> 1.118 V      pH 14 -> 0.408 V
// Todo el rango 0–14 cae dentro de 0–3.3 V, sin recorte.

// ------------------------- VARIABLES ------------------------------------

float pH_actual = 7.0;           // Valor inicial al encender

// ------------------------- FUNCIONES ------------------------------------

// Ecuación de Nernst escalada (sección 2.2 de la guía)
float pH_a_voltaje(float pH) {
  float V = E0 - (PENDIENTE * K) * (pH - 7.0);

  // Limitar al rango válido del DAC del ESP32
  if (V < 0.0)  V = 0.0;
  if (V > VREF) V = VREF;

  return V;
}

// Entrega el voltaje en GPIO25 y devuelve el voltaje real (cuantizado a 8 bits)
float entregar_voltaje(float V) {
  int valor_dac = (int)round((V / VREF) * 255.0);   // DAC de 8 bits: 0–255
  if (valor_dac < 0)   valor_dac = 0;
  if (valor_dac > 255) valor_dac = 255;

dacWrite(PIN_DAC, valor_dac);

  return (valor_dac * VREF) / 255.0;   // voltaje realmente entregado
}

void mostrar_resultado(float pH, float V_ideal, float V_real) {
  Serial.println(F("--------------------------------------------"));
  Serial.print(F("  pH ingresado    : "));  Serial.println(pH, 2);
  Serial.print(F("  V teorico       : "));  Serial.print(V_ideal, 4);
  Serial.println(F(" V"));
  Serial.print(F("  V entregado DAC : "));  Serial.print(V_real, 4);
  Serial.println(F(" V   (GPIO25)"));
  Serial.print(F("  Codigo DAC      : "));
  Serial.println((int)round((V_real / VREF) * 255.0));
  Serial.println(F("--------------------------------------------"));
  Serial.println(F("Ingrese otro valor de pH (0 a 14):"));
}

// ------------------------- SETUP ----------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F(" SIMULADOR DE ELECTRODO DE pH  -  ESP32 DAC "));
  Serial.println(F(" Lab. N.1 - Instrumentacion Biomedica III   "));
  Serial.println(F("============================================"));
  Serial.print(F(" Ecuacion: V = "));   Serial.print(E0, 3);
  Serial.print(F(" - ("));              Serial.print(PENDIENTE, 5);
  Serial.print(F(" x "));               Serial.print(K, 1);
  Serial.println(F(") x (pH - 7)"));
  Serial.print(F(" Sensibilidad escalada: "));
  Serial.print(PENDIENTE * K * 1000.0, 1);
  Serial.println(F(" mV por unidad de pH"));
  Serial.println(F(" Salida analogica: GPIO25 (DAC1)"));
  Serial.println(F("============================================"));

  // Estado inicial: pH 7
  float V_ideal = pH_a_voltaje(pH_actual);
  float V_real  = entregar_voltaje(V_ideal);
  mostrar_resultado(pH_actual, V_ideal, V_real);
}

// ------------------------- LOOP -----------------------------------------

void loop() {
  if (Serial.available() > 0) {

    String entrada = Serial.readStringUntil('\n');
    entrada.trim();
    entrada.replace(",", ".");        // acepta 7,5 o 7.5

    if (entrada.length() == 0) return;

    float pH = entrada.toFloat();

    // Validacion: rango 0 a 14
    if (pH < 0.0 || pH > 14.0) {
      Serial.println(F("  [ERROR] Valor fuera de rango. Ingrese un pH entre 0 y 14."));
      return;
    }

    pH_actual = pH;

    float V_ideal = pH_a_voltaje(pH_actual);
    float V_real  = entregar_voltaje(V_ideal);

    mostrar_resultado(pH_actual, V_ideal, V_real);
  }
}
