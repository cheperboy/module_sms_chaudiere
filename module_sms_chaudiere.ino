/*
EEPROM data
0 = software_reset
1 = software_reset counter
2...n = send_record for each users
*/


#include "SIM900.h"
#include <SoftwareSerial.h>
#include "sms.h"
#include <Metro.h>

SMSGSM sms;


const int length_num = 16;
const int length_msg = 20;

struct struct_SMS {
  char num[length_num]; 
  char msg[length_msg];
};

struct_SMS sms_out; // structure sms tx

//pin interface Module sim900
const byte PINOUT_ONOFF_MODULE_SIM = 9; // on/off module sim900
//pin interface Chaudiere
const byte PIN_DERANGEMENT = 3; // detection du derangement
//pin interface utilisateur
const byte PIN_ACK = 4;     		// aquitement de l'alarme par l'utilisateur recu
const byte PINOUT_ACK = 5;     	// indication de demande aquitement par l'utilisateur
const byte PINOUT_DEFAUT = 6;   // indication de defaut permanent (trop d'echec d'envoi) programme bloqué

//constantes et paramètres du logiciel
const int DELAY_BEFORE_RETRY = 20 * 1000;
const int SOFTWARE_RESET_MAX = 5;
const int TIME_BEFORE_DETECTION = 30 * 60 * 1000;

// Variables programme
enum type_state {DERANGEMENT, WAIT_ACK, TIMER_OVER, NORMAL};
type_state state = NORMAL;
boolean started = false; //module gsm a fini le boot
int i = 0; // dans boucle for de la loop
int retry = 0;
char* imei = "                           ";

const char* list_user[] = {"0663104827", "0688649102"};
const int list_user_size = sizeof(list_user)/sizeof(*list_user);

boolean send_record[] = {false, false};
const int send_record_size = sizeof(send_record)/sizeof(*send_record);

boolean sent_all_ok        = false; 	// memorise que tout les users sont au courant de la detection
boolean ack                = false; 	// false: pas d'ack / true: ack recu
boolean derangement        = false; 	// false: pas de derangement  / true: derangement
boolean record_process     = false; 	// derangement already processed

Metro time_before_detection = Metro(TIME_BEFORE_DETECTION); 

void setup()
{
  //Init Serial connection.
  Serial.begin(9600);
  Serial.println(F("SETUP"));
  pinMode(PIN_DERANGEMENT, INPUT);
  pinMode(PIN_ACK, INPUT);
  pinMode(PINOUT_ACK, OUTPUT);
  pinMode(PINOUT_DEFAUT, OUTPUT);
  pinMode(PINOUT_ONOFF_MODULE_SIM, OUTPUT);
	// resetGSM();
	// startGSM();
	if (gsm.begin(19200)) {
		Serial.println(F("\tInit ok"));
	}
	else {
		Serial.println(F("\tEchec init"));
	}
	started = true;
	if(gsm.SendATCmdWaitResp("AT+CMGD=1,1", 1000, 50, "OK", 2)) {
		Serial.println("\nSMS DELETED ALL");
	}
	delay(500);
	strcpy(sms_out.msg, "chaudiere en defaut\n");
}
/*
DERANGEMENT = en derangement (reglage timer inactif, ack actif)
	envoi sms
	goto WAIT_ACK

WAIT_ACK
	attent ack
	if ack 
		set timer to 15min
		goto NORMAL

NORMAL
	if detect smth run detection schema
	if detection goto DERANGEMENT	
	if + pressed timer +15min
	if - pressed timer -15min
*/
void loop() 
{
  // Serial.println(F("LOOP"));
	byte user = 0;
	boolean retry = false;
  if(!started) {
		resetGSM();
	}
	
	switch (state){
		case DERANGEMENT: 
			break; 
		case WAIT_ACK: 
			break; 
		case TIMER_OVER: 
			break; 
		case NORMAL:
			if (state_derangement() && time_before_detection.check()) { 
				state = DERANGEMENT; 
			}
			break; 
  }
}
//TODO integrer le code commente ci-dessous dans le case ci-dessus
/*
void loop() 
{
  // Serial.println(F("LOOP"));
	byte user = 0;
	boolean retry = false;
  if(!started) {
		resetGSM();
	}
	else {		
		if (state_derangement() && time_before_detection.check()) { derangement = true; }

		if (derangement && state_ack()) { 
			led_off(PINOUT_ACK);
			derangement    = false;
			record_process = false;
			derangement    = false;
			time_before_detection.reset();
		}

		// process derangement and software_reset
		if (derangement && !record_process) {
			record_process = true;
			led_on(PINOUT_ACK); // indication wait for ack
			send_SMS();
			if (sent_all_ok == false) {
				delay(DELAY_BEFORE_RETRY);
				send_SMS();
				if (sent_all_ok == false) {
					led_on(PINOUT_DEFAUT);
				}
			}
			//
		}
  }
}
*/
char send_SMS() {
	byte user = 0;
	for (user = 0; user < list_user_size; user++) {
		if (send_record[user] == false) {
			send_record[user] = send_generic_SMS(list_user[user]);
		}
	}
	check_SMS_sent();
}
char check_SMS_sent() {
	byte user = 0;
	sent_all_ok = true;
	for (user = 0; user < list_user_size; user++) {
		if (send_record[user] == false) {
			sent_all_ok = false;
		}
	}
}

/*
Envoi sms generique
return: 
	 1 - envoi ok
	-1 - echec envoi
*/
boolean send_generic_SMS(const char *number)
{
	boolean ret_val = false;
	Serial.println(F("\tsend_generic_SMS()"));
	Serial.print(F("\t\tto : "));
	Serial.println(number);
	Serial.println(F(">"));
  if (sms.SendSMS((char*) number, (char*) "sms_out.msg")) {
//  if (sms.SendSMS(number, sms_out.msg)) {
		Serial.println(F("\t\tenvoi OK"));
		ret_val = true;
	}
	else {
		Serial.println(F("\t\tenvoi ECHEC"));
		ret_val = false;
	}
	return(ret_val);
}

boolean state_derangement()
{
	if (digitalRead(PIN_DERANGEMENT) == HIGH){
		return true;
	}
	else return false;
}

boolean state_ack()
{
	if (digitalRead(PIN_ACK) == HIGH){
		return true;
	}
	else return false;
}

boolean led_on(byte led)
{
	digitalWrite(led, HIGH);
}

boolean led_off(byte led)
{
	digitalWrite(led, LOW);
}

void startGSM(){
	Serial.println(F("startGSM()"));
	started = false;
	while(!started) {
		//resetGSM();
		if (initserialGSM()) {
//			Serial.println("\tinitserialGSM() return true");
     	started=true;
   	}
   	else {
			//Serial.println("\tinitserialGSM() return false");
			Serial.println(F("\nretry start"));
 		}
	}
}

boolean resetGSM(){
	Serial.println("\tresetGSM()");
	digitalWrite(PIN_RESET_MODULE_SIM, HIGH);
	delay(2000); 
	digitalWrite(PIN_RESET_MODULE_SIM, LOW);
	delay(5000);
}

boolean initserialGSM(){
	Serial.println(F("\tinitserialGSM()"));
	if (gsm.begin(19200)) {
		Serial.println(F("\t\tReady"));
		return true;
	} 
	else {
		Serial.println(F("\t\tNot ready")); 
		delay(1000);
		return false;
	}
}


