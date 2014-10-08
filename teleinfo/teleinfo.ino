#include "VirtualWire.h"

#include <SoftwareSerial.h>

#define sensorId 0x00410001
#define startFrame 0x02
#define endFrame 0x03
#define startLine 0x0A
#define endLine 0x0D
#define maxFrameLen 280

// On crée une instance de SoftwareSerial
SoftwareSerial* cptSerial;


char HHPHC;

int ISOUSC;             // intensité souscrite
int IINST;              // intensité instantanée en A
int IMAX;               // intensité maxi en A
int PAPP;               // puissance apparente en VA
unsigned long HCHC;  // compteur Heures Creuses en W
unsigned long HCHP;  // compteur Heures Pleines en W
String PTEC;            // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
String ADCO;            // adresse compteur
String OPTARIF;         // option tarifaire
String MOTDETAT;        // status word


struct HomeSensorValue {
  unsigned long id;
  float value;
  byte power;
};


// Fonction d'initialisation de la carte Arduino, appelée
// 1 fois à la mise sous-tension ou après un reset
void setup()
{
  // On initialise le port utilisé par le "moniteur série".
  // Attention de régler la meme vitesse dans sa fenêtre
  Serial.begin(9600);

  // On définit les PINs utilisées par SoftwareSerial,
  // 8 en réception, 9 en émission (en fait nous ne
  // ferons pas d'émission)
  cptSerial = new SoftwareSerial(11, 12);
  // On initialise le port avec le compteur EDF à 1200 bauds :
  //  vitesse de la Télé-Information d'après la doc EDF


  vw_set_tx_pin(8);
  vw_set_rx_pin(9);
  vw_set_ptt_pin(10);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	 // Bits per sec

  Serial.println(F("setup complete"));
}

void loop2() {
  HomeSensorValue msg = {
    sensorId, 666.666, 100
  };
  vw_send((uint8_t *)&msg, sizeof(HomeSensorValue));
  vw_wait_tx();
  Serial.println("send");
  delay(4000);
}

// Boucle principale, appelée en permanence une fois le
// setup() terminé
void loop()
{
  cptSerial->begin(1200);

  // Variable de stockage des caractères reçus
  char charIn = 0;
  int comptChar = 0;

  char bufferTeleinfo[21] = "";
  int bufferLen = 0;
  int checkSum;

  int sequenceNumber = 0;

  // Boucle d'attente du caractère de début de trame
  while (charIn != startFrame)
  {
    // on "zappe" le 8ème bit, car d'après la doc EDF
    // la tramission se fait en 7 bits
    charIn = cptSerial->read() & 0x7F;
  }

  // Boucle d'attente d'affichage des caractères reçus,
  // jusqu'à réception du caractère de fin de trame
  while (charIn != endFrame)
  {
    // S'il y a des caractères disponibles on les traite
    if (cptSerial->available())
    {
      // on "zappe" le 8ème bit
      charIn = cptSerial->read() & 0x7F;
      // on affiche chaque caractère reçu dans le
      // "moniteur série"
      //Serial.print(charIn);

      // incrémente le compteur de caractère reçus
      comptChar++;
      if (charIn == startLine) {
        bufferLen = 0;
      }
      bufferTeleinfo[bufferLen] = charIn;
      // on utilise une limite max pour éviter String trop long en cas erreur réception
      // ajoute le caractère reçu au String pour les N premiers caractères
      if (charIn == endLine)
      {
        checkSum = bufferTeleinfo[bufferLen - 1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum)
        { // we clear the 1st character
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen - 3);
          bufferTeleinfo[bufferLen - 3] =  0x00;
          sequenceNumber++;
          if (! handleBuffer(bufferTeleinfo, sequenceNumber))
          {
            Serial.println("Sequence error ...");
            return ;
          }
        }
        else
        {
          Serial.println("Checksum error ...");
          return ;
        }
      }
      else {
        bufferLen++;
      }
    }
    if (comptChar > maxFrameLen)
    {
      Serial.println("Overflow error ...");
      return ;
    }

  }

  cptSerial->end();

  HomeSensorValue msg = {
    sensorId, PAPP, 100
  };
  vw_send((uint8_t *)&msg, sizeof(HomeSensorValue));
  vw_wait_tx();
  Serial.println(msg.value);
}

boolean handleBuffer(char *bufferTeleinfo, int sequenceNumber)
{
  // create a pointer to the first char after the space
  char* resultString = strchr(bufferTeleinfo, ' ') + 1;
  boolean sequenceIsOK;

  switch (sequenceNumber)
  {
    case 1:
      if (sequenceIsOK = bufferTeleinfo[0] == 'A')
        ADCO = String(resultString);
      break;
    case 2:
      if (sequenceIsOK = bufferTeleinfo[0] == 'O')
        OPTARIF = String(resultString);
      break;
    case 3:
      if (sequenceIsOK = bufferTeleinfo[1] == 'S')
        ISOUSC = atol(resultString);
      break;
    case 4:
      if (sequenceIsOK = bufferTeleinfo[3] == 'C')
        HCHC = atol(resultString);
      break;
    case 5:
      if (sequenceIsOK = bufferTeleinfo[3] == 'P')
        HCHP = atol(resultString);
      break;
    case 6:
      if (sequenceIsOK = bufferTeleinfo[1] == 'T')
        PTEC = String(resultString);
      break;
    case 7:
      if (sequenceIsOK = bufferTeleinfo[1] == 'I')
        IINST = atol(resultString);
      break;
    case 8:
      if (sequenceIsOK = bufferTeleinfo[1] == 'M')
        IMAX = atol(resultString);
      break;
    case 9:
      if (sequenceIsOK = bufferTeleinfo[1] == 'A')
        PAPP = atol(resultString);
      break;
    case 10:
      if (sequenceIsOK = bufferTeleinfo[1] == 'H')
        HHPHC = resultString[0];
      break;
    case 11:
      if (sequenceIsOK = bufferTeleinfo[1] == 'O')
        MOTDETAT = String(resultString);
      break;
  }

  if (!sequenceIsOK)
  {
    Serial.print(F("Out of sequence ..."));
    Serial.println(bufferTeleinfo);
  }

  return sequenceIsOK;
}

char chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
  for (i = 1; i < (len - 2); i++)
    sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return (sum);
}

