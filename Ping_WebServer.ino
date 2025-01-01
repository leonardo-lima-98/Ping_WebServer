#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESPping.h>

// Pin definition
const int controlPin = 18;  // LED_BUILTIN no ESP32
bool serverState = false;
const char* serverStateMessage = "";

// Hostname for mDNS
const char* hostname = "esp32-device";

// Servidor DB será Pingado
const char* ip = "192.168.1.250"; // Substitua pelo IP do dispositivo que você deseja fazer o ping

// Web server on port 80
WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Valida conexão com servidor, define estado
void validStateServer() { // Essa função tem como objetivo tanto validar o estado do servidor para as funções quanto para o HTML do server local
  if (Ping.ping(ip)) { // Aqui Ping.ping(ip) retorna 1 ou 0
    serverState = true;                 // Servidor está ativo
    serverStateMessage = "Servidor Ativo";
    Serial.println("Servidor Ativo");
  } else {
    serverState = false;                // Servidor está desativado
    serverStateMessage = "Servidor Desativado";
    Serial.println("Servidor Desativado");
  }
}

// Função para lidar com o ping e validação
void handlePing() {
  int maxAttempts = 3;             // Número máximo de tentativas
  int attempt = 0;                 // Contador de tentativas
  // Tentativa inicial de validar o servidor
  while (attempt < maxAttempts) {
    delay(10000);                  // Aguarda 15 segundos entre as tentativas
    if (Ping.ping(ip) == !serverState) {
      validStateServer();
      server.send(200, "text/plain", "OK");
      break;
    }
    Serial.println(attempt);
    attempt++;
    serverStateMessage = "Servidor Indisponivel"; // Altera a mensagem global
    Serial.println("Servidor Indisponivel");
    if (attempt == maxAttempts) {
      break;
      Serial.println("break");
      server.send(500, "text/plain", "Servidor Indisponivel");
    }
  }
}

// Função que simula o botão de pulso
void switchServerState() {
  Serial.println("Alternando estado do servidor...");
  int currentState = digitalRead(controlPin);  // Lê o estado atual do pino
  int newState = !currentState;  // Inverte o estado lógico do pino
  digitalWrite(controlPin, newState);  // Retorna ao estado original
  delay(700);  // Mantém o estado alterado por 0.7 segundo
  digitalWrite(controlPin, currentState);  // Retorna ao estado original
  handlePing();
}

void setup_wifi() {
  Serial.begin(115200);

  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-Config")) {  // Inicia o portal de configuração WiFi
    Serial.println("Falha na conexão WiFi");
    delay(3000);
    ESP.restart();  // Reinicia caso não consiga conectar
  }

  Serial.println("Conectado ao WiFi!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Agora, depois de se conectar ao Wi-Fi, configuramos o mDNS
  if (MDNS.begin(hostname)) {  // Define o nome mDNS para o dispositivo
    Serial.println("mDNS iniciado com sucesso!");
    Serial.println("You can access the server at: http://" + String(hostname) + ".local");
  } else {
    Serial.println("Erro ao iniciar mDNS");
  }
}

void handleRoot() {
  Serial.println("Handling root request...");
  if (SPIFFS.exists("/index.html")) {
    Serial.println("index.html exists, serving file...");
    File file = SPIFFS.open("/index.html", "r");
    if(!file){
      Serial.println("Failed to open index.html");
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
    Serial.println("File size: " + String(file.size()) + " bytes");
    server.streamFile(file, "text/html");
    file.close();
    Serial.println("File served successfully");
  } else {
    Serial.println("index.html not found in SPIFFS");
    server.send(404, "text/plain", "File not found");
  }
}

void handleGetState() {
  server.send(200, "text/plain", String(serverState));
  Serial.println(serverState);
}

void handleSetState() {
  server.send(200, "text/plain", "OK");
  switchServerState();
}

void listFiles() {
  Serial.println("Listing files in SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("File: ");
    Serial.print(file.name());
    Serial.print(" - Size: ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
}

void setup() {
  
  Serial.begin(115200);
  Serial.println("\n\nStarting setup...");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS Mounted successfully");
  
  // List all files in SPIFFS
  listFiles();

  // Initialize pin
  pinMode(controlPin, OUTPUT);
  digitalWrite(controlPin, serverState ? HIGH : LOW);
  Serial.println("Pin initialized");

  // Setup WiFi
  setup_wifi();
  // Valida o estado do servidor
  validStateServer();

  // Configure web server routes
  server.on("/", handleRoot);
  server.on("/getState", handleGetState);
  server.on("/setState", handleSetState);
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("You can also access the server at: http://" + WiFi.localIP().toString());

  Serial.println("Setup complete");
}

void loop() {
  // Handle web server clients
  server.handleClient();
  // Give time to background tasks
  delay(10);
}
