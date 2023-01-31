#include <Arduino.h>
#include <WiFi.h> // Include the Wi-Fi library
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" // Provide the token generation process info.
#include "addons/RTDBHelper.h"  // Provide the RTDB payload printing info and other helper functions.
#include <ESP_Mail_Client.h>
#include <math.h>

String code = "swqolfyakdeipecz";
//------------------------------FUNCIONES MULTITAREA
void Task1(void *pvParameters);
void Task2(void *pvParameters);
void keepWifi(void *pvParameters);

// ------------------------------------------------DATOS WIFI Y FIREBASE---------------------------------------------------
const char *ssid = "REPLACE_WITH_YOUR_SSID";         // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "REPLACE_WITH_YOUR_PASSWORD"; // The password of the Wi-Fi network
#define API_KEY "REPLACE_WITH_YOUR_FIREBASE_PROJECT_API_KEY"
#define FIREBASE_PROJECT_ID ""
#define USER_EMAIL ""
#define USER_PASSWORD ""
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
// -----------------------------------------------------------DATOS EMAIL---------------------------------------------------
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
/* The sign in credentials */
#define AUTHOR_EMAIL "YOUR_EMAIL@XXXX.com"
#define AUTHOR_PASSWORD "YOUR_EMAIL_PASS"
/* Recipient's email*/
#define RECIPIENT_EMAIL "RECIPIENTE_EMAIL@XXXX.com"
/* The SMTP Session object used for Email sending */
SMTPSession smtp;
/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);
//-------------------------------------- DATOS CONTROL---------------------------------------------------------------------
String cambio = "";
String documentPath = "gatos/user1";
String mask = "Cambio";
#define analogPin 34 /* ESP8266 Analog Pin ADC0 = A0 */
int adcValue = 0;    /* Variable to store Output of ADC */
float ntu;           // Nephelometric Turbidity Units
boolean bandera = false;
const int relay = 12; // PIN GPI O D?
boolean valorCambio = false;
boolean valorNoCambio = false;
//-----------------------------------------------------------------------------FUNCIONES EMAIL--------------------------------------------------------
void sendEmail(int op) // Envia un email al usuario
{
  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
   */
  smtp.debug(1);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "ESP";
  message.sender.email = AUTHOR_EMAIL;
  if (op == 1)
  {
    message.subject = "El agua del bebedero esta llena!!";
  }
  else
  {
    message.subject = "El bebedero se encuentra vacio!!";
  }
  message.addRecipient("Hazel", RECIPIENT_EMAIL);

  // Send raw text message
  if (op == 1)
  {
    String textMsg = "Le notificamos que el bebedero de sus mascotas se encuentra lleno.\nGracias por usar nuesto servicio";
    message.text.content = textMsg.c_str();
  }
  else
  {
    String textMsg = "Le notificamos que es nececesario de reponer el agua del bebedero ya que se encuentra vacio.\nGracias por usar nuesto servicio";
    message.text.content = textMsg.c_str();
  }

  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}
//-----------------------------------------------------------------------------FUNCIONES FIREBASE--------------------------------------------------------

void firebaseInit() // Inicia la conexión con Firebase
{
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  if (WiFi.status() == WL_CONNECTED && Firebase.ready())
  {
    Serial.println("ok.......");
    Serial.println("Firebase connected.......");
  }
}

void firestoreRead() // Lee los datos de un nodo específico
{
  if (WiFi.status() == WL_CONNECTED && Firebase.ready())
  {
    Serial.println("Get a document... ");

    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), mask.c_str()))
    {
      // String cadena = fbdo.payload();
      // Serial.println(cadena);
      //
      // Create a FirebaseJson object and set content with received payload
      FirebaseJson payload;
      payload.setJsonData(fbdo.payload().c_str());

      // Get the data from FirebaseJson object
      FirebaseJsonData jsonData;
      payload.get(jsonData, "fields/Cambio/booleanValue", true);
      cambio = jsonData.to<String>();
      Serial.println(cambio);
      //
    }
    else
    {
      Serial.println(fbdo.errorReason());
    }
  }
}

void firestoreUpdate(String valor) // Actualiza el valor de los datos de un nodo
{
  if (WiFi.status() == WL_CONNECTED && Firebase.ready())
  {

    FirebaseJson content;

    content.set("fields/Cambio/booleanValue", String(valor).c_str());
    Serial.print("Create a document... ");

    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "Cambio"))
    {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      return;
    }
    else
    {
      Serial.println(fbdo.errorReason());
    }

    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw()))
    {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      return;
    }
    else
    {
      Serial.println(fbdo.errorReason());
    }
  }
}
//-------------------------------------------------------------------------MULTITAREA-------------------------------
void Task1(void *pvParameters) // ADC sensor
{
  for (;;)
  {
    adcValue = analogRead(analogPin);       /* Read the Analog Input value */
    float Vadc = adcValue * (5.0 / 4095.0); // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
    if (Vadc >= 1.50)
    { // EL BEBEDRO ESTA LLENO
      if (bandera == false)
      {
        firebaseInit();
        firestoreUpdate("true");
        sendEmail(1);
        bandera = true;
      }
    }
    else if(Vadc <= 0.6)
    { // ES NECESARIO REPONER EL AGUA
      if (bandera == true)
      {
        firebaseInit();
        firestoreUpdate("false");
        sendEmail(2);
        bandera = false;
      }
    }
    Serial.println(Vadc);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
void Task2(void *pvParameters) // BOMBA DE AGUA
{
  // relay NC
  pinMode(relay, OUTPUT);
  for (;;)
  {
    if (bandera == true)
    {
      digitalWrite(relay, HIGH);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      digitalWrite(relay, LOW);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    else
    {
      digitalWrite(relay, LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}
void setup()
{
  //-----------------------------------------------------------------------------CONEXION WFI--------------------------------------------------------
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  WiFi.begin(ssid, password); // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
  //-----------------------------------------------------------------------------CONEXION FIREBASE--------------------------------------------------------
  // firebaseInit();
  // -----------------------------------------MULTITAREA---------------------------------------------
  xTaskCreatePinnedToCore(
      Task1,                      // Function to call
      "Task1",                    // Name for this task, mainly for debug
      8000,                       // Stack size
      NULL,                       // pvParameters to pass to the function
      1,                          // Priority
      NULL,                       // Task handler to use
      CONFIG_ARDUINO_RUNNING_CORE // Core where to run
  );
  xTaskCreatePinnedToCore(
      Task2,   // Function to call
      "Task2", // Name for this task, mainly for debug
      5000,    // Stack size
      NULL,    // pvParameters to pass to the function
      1,       // Priority
      NULL,    // Task handler to use
      1        // Core where to run
  );
}

void loop()
{
  delay(10);
}