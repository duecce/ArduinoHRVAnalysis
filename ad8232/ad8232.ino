/*
 *
 * Author(s):
 *    Andrea Arcangeli
 *    Martina De Marinis
 *
 */

#include "RealTime_IIR.hpp" // libreria filtro IIR
#include <arduinoFFT.h> // libreria FFT

#define OLDEST    0
#define OLD       1
#define CURRENT   2

#define Fc  150
#define Tc  (1.0/Fc)*1000000
#define M_AVG_ORDER 35

#define Fc_resample 4
#define Tc_resample (1.0/Fc_resample)*1000000

#define FFT_N_SAMPLES   128

// Il periodo di valutazione dell'FFT è pari a 10 secondi
#define Tc_FFT  10 * 1000000
#define Fc_FFT  (1.0 / Tc_FFT) / 1000000

#define INIT_INTERVAL 10 * 1000000 // durata [usec] della fase di inizializzazione
#define DELAY         3  * 1000000 // inizializiamo i parametri dopo DELAY [usec] dall'avvio

#define MIN_PH_RR	250
#define AVG_PH_RR	1000
#define MAX_PH_RR	2000

#define TH_RATIO	0.60

// Variabili per la gestione dei 3 tempi di campionamento:
// t0: rilevazione campione ecg raw
// t1: ricampionamento a Fc_resample
// t2: esecuzione della FFT
unsigned long t_init = micros(), t0 = micros(), t1 = micros(), t2 = micros();

// Filtro IIR ellittico:
// Freq. campionamento 150 Hz
// Passa-Banda [6.5 - 22.5] Hz 

// coefficienti denominatore
double b[] = {0.00416541322971639,	-0.0184082690726671,	0.0362234397797539,	-0.0434557934362996, 	0.0310548978670479,	
			  0,	-0.0310548978670479,	0.0434557934362996,	-0.0362234397797539,	0.0184082690726671,	-0.00416541322971639};

// coefficienti numeratore
double a[] = {1,	-7.68579709312419,	27.8406255940322,	-62.5108131474546,	96.2651881592322,	
         -106.180004886593,	84.9314628945120,	-48.6562228763279,	19.1205579190093,	-4.66084804789229,	0.536612409721376};

#define M	  11 // numero di coefficienti del numeratore del filtro
#define N	  11 // numero di coefficienti del denominatore del filtro

// creazione istanza filtro IIR 
RT_IIR<double> iir = RT_IIR<double>(N, M, b, a);
// creazione istanza classe arduinoFFT
arduinoFFT FFT = arduinoFFT(); 

// variabili d'appoggio algoritmo Pan-Tompkins
double sample, filtered_sample, quad_sample, m_avg_out;
double old_sample = 0, current_sample = 0;

// finestra filtro a media mobile
double m_avg_data[M_AVG_ORDER] = {0.0};
// indice finestra filtro a media mobile
unsigned short counter = 0;
// calcolo del coefficiente del filtro a media mobile
double coeff_m_avg = 1.0 / M_AVG_ORDER;

// Parametri per la soglia adattiva
double RESET_MAX_VALUE = 1200, RAISING_VALUE = 0, FALLING_VALUE = 0, max_derivative = 0, min_derivative = 0, derivative;
double max_value = 0;
double th_value = 0;

unsigned long tmp = 0, sample_time = 0;
long dt = 0;

unsigned long time_R[] = {0, 0, 0};
long int_RR[] = {0, 0, 0};

double data[FFT_N_SAMPLES] = {0.0};
// vettore campioni ordinati e ricampionati a Fc_resample
double vReal[FFT_N_SAMPLES];
double vImag[FFT_N_SAMPLES];
int FFT_counter = 0;

// abilita rilevamento picco R
bool enable_capture_R = false;
// abilita l'esecuzione dell'FFT
bool enable_FFT = false;
// indica l'avvenuto riempimento del vettore 'data' dei dati ricampionati
bool data_full = false;
// indica la fase di inizializzazione per la valutazione dei parametri
bool initializing = true;

bool leadsOff = false;

void setup() {
  pinMode(A0, INPUT);
  pinMode(0, INPUT);
  pinMode(1, INPUT);
  Serial.begin(115200);
}

void loop() {
  // FFT
  if(micros() - t2 >= Tc_FFT && enable_FFT == true && data_full == true && initializing == false && leadsOff == false) {
	double freq;
    char str[32] = {0};
    t2 = micros();
    double meanValue = 0;
    // calcolo del valore medio
    for (unsigned char i = 0; i < FFT_N_SAMPLES; i++)
      meanValue += data[i] * 1.0/FFT_N_SAMPLES;
	  // riordino campioni e rimuovo il valor medio 
    for(unsigned char i = 0; i < FFT_N_SAMPLES; i++){
      vImag[i] = 0.0;
      if (FFT_counter+i < FFT_N_SAMPLES) {
        vReal[i] = data[FFT_counter+i] - meanValue;
      } else {
        vReal[i] = data[i-FFT_N_SAMPLES+FFT_counter] - meanValue;
      }
    }
	  // finestratura dei dati
    FFT.Windowing(vReal, FFT_N_SAMPLES, FFT_WIN_TYP_RECTANGLE, FFT_FORWARD);
	  // calcolo FFT
    FFT.Compute(vReal, vImag, FFT_N_SAMPLES, FFT_FORWARD); 
	  // calcolo modulo
    FFT.ComplexToMagnitude(vReal, vImag, FFT_N_SAMPLES); 
	  // trasmissione valori FFT alla seriale, formato: f.fff v.vvv (frequenze e modulo fft) 
    for(unsigned char i = 0; i < (FFT_N_SAMPLES >> 1); i++) {
      freq = (1.0*Fc_resample / FFT_N_SAMPLES) * i;
      Serial.print(freq, 3);
      Serial.print(String(' '));
      Serial.println(vReal[i], 3);
    }
    double freq_peak_max = FFT.MajorPeak(vReal, FFT_N_SAMPLES, Fc_resample); // Frequenza associata al picco massimo
  }
  
  // Ricampionamento a Fc_sample
  if (micros() - t1 >= Tc_resample && initializing == false && leadsOff == false) {
    t1 = micros();
    if (int_RR[OLDEST] > 0) {
      if (sample_time == 0) {
        sample_time = time_R[OLDEST];
      } else {
        sample_time += 250;
      }
      double Dx, Dy, m, dx, resampled_value;
      if (sample_time > time_R[OLD]) {
        Dx = (double)(time_R[CURRENT] - time_R[OLD]);
        Dy = (double)(int_RR[CURRENT] - int_RR[OLD]);
		// calcolo coefficiente angolare retta interpolante
        m = Dy / Dx;
        dx = (double)(sample_time - time_R[OLD]);
		// calcolo del valore ricampionato
        resampled_value = (double)(int_RR[OLD]) + m*dx;
      } else {
        Dx = (double)(time_R[OLD] - time_R[OLDEST]);
        Dy = (double)(int_RR[OLD] - int_RR[OLDEST]);
        m = Dy / Dx;
        dx = (double)(sample_time - time_R[OLDEST]);
        resampled_value = (double)(int_RR[OLDEST]) + m*dx;
      }
      // verifico validità campione 
      if (resampled_value > MAX_PH_RR || isnan(resampled_value) || resampled_value < MIN_PH_RR) {
	    // assegno al campione l'ultimo valore valido
        resampled_value = (FFT_counter > 0) ? data[FFT_counter-1] : data[FFT_N_SAMPLES-1];
      }
      if(enable_FFT == false) {
        char str[32] = {0};
        sprintf(str, "%d %d 400 1200", (int)(resampled_value), (int)(int_RR[OLDEST]));
        Serial.println(str);
      }
      data[FFT_counter] = resampled_value;
      FFT_counter ++;
      if (FFT_counter >= FFT_N_SAMPLES) {
        data_full = true;
        FFT_counter = 0;
      }
    }
  }

  // Campionamento ecg raw
  if (micros() - t0 >= Tc && leadsOff == false) {
    t0 = micros();
	  // campione raw
    sample = analogRead(A0);
	  // campione filtrato
    filtered_sample = iir.add(sample);
	  // campione quadrato
    quad_sample = filtered_sample*filtered_sample;
	  // applicazione filtro media mobile
    m_avg_data[counter] = quad_sample;
    m_avg_out = 0;
    for (unsigned short i = 0; i < M_AVG_ORDER; i++) {
      m_avg_out += m_avg_data[i];
    }
	  // campione in uscita dal filtro a media mobile
    m_avg_out *= coeff_m_avg;

    counter ++;
    if (counter == M_AVG_ORDER) 
      counter = 0;
    current_sample = m_avg_out;
    if (initializing == true && micros() - t_init >= DELAY ) {
      if (micros() - t_init >= INIT_INTERVAL + DELAY) {
        // setto i parametri della rilevazione dopo l'acquisizione di 10 secondi di segnale
        initializing = false;
        enable_capture_R = true;
        RAISING_VALUE = max_derivative * 0.10;
        FALLING_VALUE = min_derivative * 0.00;
        RESET_MAX_VALUE = max_value;
        th_value = TH_RATIO * max_value;
        if(enable_FFT == false) {
          char str2[64];
          sprintf(str2, "RV: %d\tFV: %d\tRMV: %d\tTH: %d\tMAX VALUE: %d\n", int(RAISING_VALUE), int(FALLING_VALUE), int(RESET_MAX_VALUE), int(th_value), int(max_value));
          Serial.println(str2);
        }
        time_R[OLDEST] = millis() - 2000;
        time_R[OLD] = millis() - 1000;
        time_R[CURRENT] = millis();
      } else {
        // aggiorno i parametri, se siamo nella fase di inizializzazione
        derivative = current_sample - old_sample;
        if (derivative > max_derivative)
          max_derivative = derivative;
        if (derivative < min_derivative)
          min_derivative = derivative;
        if (current_sample > max_value && current_sample < RESET_MAX_VALUE)
          max_value = current_sample;
      }
    } else if(initializing == false) {
      // rilevazione picco R
      if (current_sample - old_sample > RAISING_VALUE && current_sample > th_value && enable_capture_R == true) {
        tmp = millis();
        dt = tmp - time_R[CURRENT];
        // verifico intervallo R-R fisiologico
        if (dt > MIN_PH_RR) {
          time_R[OLDEST] = time_R[OLD];
          time_R[OLD] = time_R[CURRENT];
          time_R[CURRENT] = tmp;
          enable_capture_R = false;
          int_RR[OLDEST] = int_RR[OLD];
          int_RR[OLD] = int_RR[CURRENT];
          int_RR[CURRENT] = dt;
        }
      // verifico l'avvenuto rilevamento entro MAX_PH_RR
      } else if (millis() - time_R[OLD] > MAX_PH_RR && enable_capture_R == true) {
        max_value = RESET_MAX_VALUE;
        th_value = TH_RATIO * max_value;
        time_R[OLD] = time_R[CURRENT];
        time_R[OLDEST] = time_R[OLD] - AVG_PH_RR; 
        int_RR[OLD] = AVG_PH_RR;
        int_RR[OLDEST] = AVG_PH_RR;
      }
      
      // aggiornamento della soglia adattiva, nella fase discendente
      if (current_sample - old_sample < FALLING_VALUE && current_sample < th_value && enable_capture_R == false ) {
        // la soglia adattiva è calcolata come la media tra il valore precedente e quello relativo
        // all'ultimo picco R rilevato, per limitare l'influenza di eventuali outlier
        th_value = TH_RATIO * RESET_MAX_VALUE;
        max_value = 0;
        enable_capture_R = true;
      }
      // aggiornamento valore massimo
      if (m_avg_out > max_value && m_avg_out < RESET_MAX_VALUE)
        max_value = m_avg_out;
    }
    old_sample = current_sample;
  }
}
