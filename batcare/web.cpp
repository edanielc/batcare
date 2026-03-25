#include "web.h"
#include "globals.h"
#include "utils.h"
#include "hardware.h"

// Prototipo de función definida en batcare.ino
void guardarConfiguracion();

void configurarWebServer() {
  server.on("/", []() { server.send(200, "text/html", generarPaginaWeb()); });
  server.on("/auto", []() { estado.modoAutomatico = true; agregarLog("Modo auto"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/manual", []() { estado.modoAutomatico = false; agregarLog("Modo manual"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/on", []() { estado.modoAutomatico = false; encenderBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/off", []() { estado.modoAutomatico = false; apagarBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/config", HTTP_GET, []() { server.send(200, "text/html", generarPaginaConfig()); });
  
  server.on("/saveconfig", HTTP_POST, []() {
    if (server.hasArg("alto") && server.hasArg("bajo") && server.hasArg("histeresis") &&
        server.hasArg("pausa") && server.hasArg("horaInicio") && server.hasArg("horaFin") &&
        server.hasArg("factor")) {
      
      config.umbralAlto = server.arg("alto").toInt();
      config.umbralBajo = server.arg("bajo").toInt();
      config.histeresis = server.arg("histeresis").toInt();
      config.pausaMs = server.arg("pausa").toInt() * 60000;
      config.horaInicio = server.arg("horaInicio").toInt();
      config.horaFin = server.arg("horaFin").toInt();
      config.factorVoltaje = server.arg("factor").toFloat();

      if (config.umbralAlto < config.umbralBajo) {
        uint16_t tmp = config.umbralAlto;
        config.umbralAlto = config.umbralBajo;
        config.umbralBajo = tmp;
      }
      if (config.horaInicio > 23) config.horaInicio = 0;
      if (config.horaFin > 23) config.horaFin = 23;
      if (config.histeresis > 50) config.histeresis = 50;
      if (config.pausaMs < 0) config.pausaMs = 0;
      if (config.pausaMs > 3600000) config.pausaMs = 3600000;
      if (config.factorVoltaje <= 0.0) config.factorVoltaje = 14.1 / 805.0;

      guardarConfiguracion();
      agregarLog("Config guardada");
      server.sendHeader("Location", "/config");
      server.send(302, "text/plain", "");
    } else {
      server.send(400, "text/plain", "Faltan parámetros");
    }
  });
}

String generarPaginaWeb() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Control de Carga Solar</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #000000; color: #333; padding: 20px; }";
  html += ".container { max-width: 1200px; margin: 0 auto; }";
  html += "h1 { text-align: center; margin-bottom: 20px; color: #2c3e50; font-weight: 300; }";
  html += ".panel { background: white; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); padding: 20px; margin-bottom: 20px; }";
  html += "h2 { font-size: 1.3rem; color: #2c3e50; margin-bottom: 15px; border-left: 4px solid #3498db; padding-left: 12px; }";
  html += ".status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }";
  html += ".status-info { display: flex; flex-direction: column; gap: 10px; }";
  html += ".status-item { display: flex; justify-content: space-between; border-bottom: 1px solid #eee; padding: 8px 0; }";
  html += ".status-label { font-weight: 600; color: #7f8c8d; }";
  html += ".status-value { font-weight: 500; }";
  html += ".status-big { text-align: right; }";
  html += ".voltage-big { font-size: 3rem; font-weight: 600; color: #2c3e50; line-height: 1; }";
  html += ".adc-big { font-size: 2rem; font-weight: 500; color: #3498db; }";
  html += ".unit { font-size: 1rem; font-weight: normal; color: #7f8c8d; }";
  html += ".buttons { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 10px; }";
  html += "button, .btn { padding: 10px 20px; border: none; border-radius: 30px; font-size: 1rem; cursor: pointer; transition: all 0.3s ease; font-weight: 500; text-decoration: none; display: inline-block; text-align: center; }";
  html += ".btn-primary { background: #3498db; color: white; }";
  html += ".btn-primary:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += ".btn-secondary { background: #95a5a6; color: white; }";
  html += ".btn-secondary:hover { background: #7f8c8d; }";
  html += ".btn-danger { background: #e74c3c; color: white; }";
  html += ".btn-danger:hover { background: #c0392b; }";
  html += ".btn-success { background: #2ecc71; color: white; }";
  html += ".btn-success:hover { background: #27ae60; }";
  html += ".btn-config { background: #f39c12; color: white; }";
  html += ".btn-config:hover { background: #e67e22; transform: scale(1.05); }";
  html += ".active-mode { background: #2ecc71; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; }";
  html += "th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "th { background-color: #f8f9fa; font-weight: 600; }";
  html += "tr:hover { background-color: #f5f5f5; }";
  html += ".live { font-style: italic; color: #7f8c8d; margin-top: 10px; font-size: 0.9rem; }";
  html += ".formula { font-size: 0.8em; color: #7f8c8d; margin-top: 10px; }";
  html += "@media (max-width: 768px) { .status-grid { grid-template-columns: 1fr; } .status-big { text-align: left; margin-top: 15px; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>☀️ Control Solar</h1>";
  
  // Panel estado
  html += "<div class='panel'>";
  html += "<h2>Estado Actual</h2>";
  html += "<div class='status-grid'>";
  html += "<div class='status-info'>";
  html += "<div class='status-item'><span class='status-label'>Modo:</span><span class='status-value'>" + String(estado.modoAutomatico ? "Automático" : "Manual") + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Bomba:</span><span class='status-value'>" + String(estado.bombaEncendida ? "ENCENDIDA" : "APAGADA") + "</span></div>";
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<div class='status-item'><span class='status-label'>Hora:</span><span class='status-value'>" + String(timeStr) + "</span></div>";
  }
  html += "<div class='status-item'><span class='status-label'>Tiempo total activado (histórico):</span><span class='status-value'>" + formatDuration(tiempoTotalEncendido) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Total operaciones (histórico):</span><span class='status-value'>" + String(totalOperaciones) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Hoy activado:</span><span class='status-value'>" + formatDuration(tiempoTotalEncendidoDia) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Hoy operaciones:</span><span class='status-value'>" + String(totalOperacionesDia) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Factor calibración:</span><span class='status-value'>" + String(config.factorVoltaje, 6) + " V/ADC</span></div>";
  html += "</div>";
  html += "<div class='status-big'>";
  html += "<div>Voltaje</div>";
  html += "<div class='voltage-big'>" + String(voltageActual, 2) + "<span class='unit'> V</span></div>";
  html += "<div style='margin-top: 15px;'>ADC RAW</div>";
  html += "<div class='adc-big'>" + String(rawValue) + "</div>";
  html += "<div class='formula'>Fórmula: Voltaje = ADC × " + String(config.factorVoltaje, 6) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Panel controles
  html += "<div class='panel'>";
  html += "<h2>Controles</h2>";
  html += "<div class='buttons'>";
  if(estado.modoAutomatico) {
    html += "<a href='/auto'><button class='btn btn-success active-mode'>Modo Automático</button></a>";
    html += "<a href='/manual'><button class='btn btn-secondary'>Modo Manual</button></a>";
  } else {
    html += "<a href='/auto'><button class='btn btn-secondary'>Modo Automático</button></a>";
    html += "<a href='/manual'><button class='btn btn-success active-mode'>Modo Manual</button></a>";
  }
  if(!estado.modoAutomatico) {
    html += "<a href='/on'><button class='btn btn-primary'>Encender Bomba</button></a>";
    html += "<a href='/off'><button class='btn btn-danger'>Apagar Bomba</button></a>";
  } else {
    html += "<button class='btn btn-secondary' disabled style='opacity:0.6;'>Encender Bomba (solo manual)</button>";
    html += "<button class='btn btn-secondary' disabled style='opacity:0.6;'>Apagar Bomba (solo manual)</button>";
  }
  html += "<a href='/config'><button class='btn btn-config'>⚙️ Configuración</button></a>";
  html += "</div>";
  html += "</div>";
  
  // Tabla de operaciones
  html += "<div class='panel'>";
  html += "<h2>Registro de Operaciones</h2>";
  html += "<table>";
  html += "   <tr><th>Hora</th><th>Evento</th><th>Duración (ON)</th><th>Tiempo Apagado</th></tr>";
  
  // Calcular tiempos apagado
  int indices[MAX_REGISTROS];
  int count = 0;
  for (int i = 0; i < totalOperacionesHist; i++) {
    int idx = (registroIndex - totalOperacionesHist + i + MAX_REGISTROS) % MAX_REGISTROS;
    indices[count++] = idx;
  }
  
  for (int i = 0; i < count; i++) {
    int idx = indices[i];
    RegistroOperacion& reg = registroOperaciones[idx];
    String duracionOn = "";
    String tiempoApagado = "";
    
    if (reg.encendido) {
      int prevIdx = -1;
      for (int j = i-1; j >= 0; j--) {
        int pIdx = indices[j];
        if (!registroOperaciones[pIdx].encendido) {
          prevIdx = pIdx;
          break;
        }
      }
      if (prevIdx != -1) {
        uint32_t diff = reg.timestamp - registroOperaciones[prevIdx].timestamp;
        tiempoApagado = formatDuration(diff * 1000);
      } else {
        tiempoApagado = "-";
      }
      duracionOn = "-";
    } else {
      duracionOn = formatDuration(reg.duracion);
      tiempoApagado = "-";
    }
    
    html += "   <tr>";
    html += "     <td>" + formatTime(reg.timestamp) + "</td>";
    html += "     <td>" + String(reg.encendido ? "ENCENDIDO" : "APAGADO") + "</td>";
    html += "     <td>" + duracionOn + "</td>";
    html += "     <td>" + tiempoApagado + "</td>";
    html += "   </tr>";
  }
  
  html += " </table>";
  html += "</div>";
  
  // Últimos eventos
  html += "<div class='panel'>";
  html += "<h2>Últimos Eventos</h2>";
  html += "<table>";
  for(uint8_t i = 0; i < MAX_LOG_ENTRIES; i++) {
    uint8_t idx = (logIndex + MAX_LOG_ENTRIES - i - 1) % MAX_LOG_ENTRIES;
    html += "   <tr><td>" + String(logEntries[idx].mensaje) + "</td></tr>";
  }
  html += " </table>";
  html += "</div>";
  
  html += "</div></body></html>";
  return html;
}

String generarPaginaConfig() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuración - Control Solar</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f5f7fa; color: #333; padding: 20px; }";
  html += ".container { max-width: 800px; margin: 0 auto; }";
  html += "h1 { text-align: center; margin-bottom: 20px; color: #2c3e50; font-weight: 300; }";
  html += ".panel { background: white; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); padding: 25px; margin-bottom: 20px; }";
  html += "h2 { font-size: 1.3rem; color: #2c3e50; margin-bottom: 20px; border-left: 4px solid #f39c12; padding-left: 12px; }";
  html += ".form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += ".form-group { display: flex; flex-direction: column; }";
  html += "label { font-weight: 600; margin-bottom: 5px; color: #7f8c8d; }";
  html += "input { padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px; font-size: 1rem; transition: border 0.3s; }";
  html += "input:focus { outline: none; border-color: #f39c12; }";
  html += ".note { font-size: 0.8em; color: #7f8c8d; margin-top: 4px; }";
  html += ".buttons { display: flex; gap: 12px; margin-top: 25px; flex-wrap: wrap; }";
  html += "button, .btn { padding: 10px 20px; border: none; border-radius: 30px; font-size: 1rem; cursor: pointer; transition: all 0.3s ease; font-weight: 500; text-decoration: none; display: inline-block; text-align: center; }";
  html += ".btn-primary { background: #f39c12; color: white; }";
  html += ".btn-primary:hover { background: #e67e22; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += ".btn-secondary { background: #95a5a6; color: white; }";
  html += ".btn-secondary:hover { background: #7f8c8d; }";
  html += ".formula { background: #f8f9fa; padding: 12px; border-radius: 8px; margin-top: 20px; font-size: 0.9rem; color: #2c3e50; text-align: center; }";
  html += ".current-time { margin-top: 20px; text-align: center; font-size: 0.9rem; color: #7f8c8d; }";
  html += ".calib-info { background: #e8f4fd; border-left: 4px solid #3498db; padding: 12px; margin-top: 15px; border-radius: 6px; font-size: 0.85rem; }";
  html += "@media (max-width: 600px) { .form-grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>⚙️ Configuración Avanzada</h1>";
  html += "<div class='panel'>";
  html += "<h2>Parámetros de Control</h2>";
  html += "<form action='/saveconfig' method='POST'>";
  html += "<div class='form-grid'>";
  
  // Umbral Alto
  html += "<div class='form-group'>";
  html += "<label>Umbral Alto (ADC)</label>";
  html += "<input type='number' name='alto' value='" + String(config.umbralAlto) + "' required>";
  float voltajeAlto = config.umbralAlto * config.factorVoltaje;
  html += "<div class='note'>≈ " + String(voltajeAlto, 2) + " V</div>";
  html += "</div>";
  
  // Umbral Bajo
  html += "<div class='form-group'>";
  html += "<label>Umbral Bajo (ADC)</label>";
  html += "<input type='number' name='bajo' value='" + String(config.umbralBajo) + "' required>";
  float voltajeBajo = config.umbralBajo * config.factorVoltaje;
  html += "<div class='note'>≈ " + String(voltajeBajo, 2) + " V</div>";
  html += "</div>";
  
  // Histéresis
  html += "<div class='form-group'>";
  html += "<label>Histéresis (ADC)</label>";
  html += "<input type='number' name='histeresis' value='" + String(config.histeresis) + "' required>";
  html += "<div class='note'>Margen para evitar oscilaciones</div>";
  html += "</div>";
  
  // Pausa (minutos)
  html += "<div class='form-group'>";
  html += "<label>Pausa (minutos)</label>";
  html += "<input type='number' name='pausa' min='0' value='" + String(config.pausaMs / 60000) + "' required>";
  html += "<div class='note'>Tiempo de espera antes de nuevo encendido (0 = sin pausa)</div>";
  html += "</div>";
  
  // Hora inicio
  html += "<div class='form-group'>";
  html += "<label>Hora inicio (local)</label>";
  html += "<input type='number' name='horaInicio' min='0' max='23' value='" + String(config.horaInicio) + "' required>";
  html += "<div class='note'>0-23, hora Guatemala</div>";
  html += "</div>";
  
  // Hora fin
  html += "<div class='form-group'>";
  html += "<label>Hora fin (local)</label>";
  html += "<input type='number' name='horaFin' min='0' max='23' value='" + String(config.horaFin) + "' required>";
  html += "<div class='note'>0-23, hora Guatemala</div>";
  html += "</div>";
  
  // Factor de calibración
  html += "<div class='form-group' style='grid-column: span 2;'>";
  html += "<label>Factor de calibración (V/ADC)</label>";
  html += "<input type='number' step='0.000001' name='factor' value='" + String(config.factorVoltaje, 6) + "' required>";
  html += "<div class='note'>Fórmula: Voltaje = ADC × este factor</div>";
  html += "</div>";
  
  html += "</div>";
  
  html += "<div class='buttons'>";
  html += "<button type='submit' class='btn btn-primary'>💾 Guardar Cambios</button>";
  html += "<a href='/' class='btn btn-secondary'>← Volver al inicio</a>";
  html += "</div>";
  html += "</form>";
  
  html += "<div class='calib-info'>";
  html += "📐 <strong>¿Cómo calibrar?</strong><br>";
  html += "1. Mide el voltaje real de la batería con un multímetro (ej. 12.6 V).<br>";
  html += "2. Anota el valor ADC RAW que muestra la página principal (ej. 720).<br>";
  html += "3. Calcula el factor: voltaje_real / ADC_raw (ej. 12.6 / 720 = 0.0175).<br>";
  html += "4. Ingresa ese factor aquí y guarda. ¡El voltaje mostrado será correcto!";
  html += "</div>";
  
  html += "<div class='formula'>";
  html += "<strong>Fórmula de conversión actual:</strong> Voltaje = ADC × " + String(config.factorVoltaje, 6);
  html += "</div>";
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<div class='current-time'>🕒 Hora actual (Guatemala): " + String(timeStr) + "</div>";
  }
  
  html += "</div>";
  html += "</div></body></html>";
  return html;
}