/*************************************
   This example is from https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG
 *************************************/
 
long startTime;
bool rx_done = false;
double myFreq = 868000000;
uint16_t sf = 7, bw = 4, cr = 1, preamble = 8, txPower = 22;

//uint16_t sf = 12, bw = 0, cr = 0, preamble = 8, txPower = 22;


char s[6] ;
String serial_number = "508011";//SERIAL NUMBER
bool   This_is_me = false;


//----------------------------------------------------------
void hexDump(uint8_t * buf, uint16_t len) 
{
  char alphabet[17] = "0123456789abcdef";

  Serial.println( "==  begin ==");

  for (uint16_t i = 0; i < 6; i++)
  {
    s[i] = buf[i];
    Serial.print(buf[i], HEX);
 
  }
  Serial.println( "==  begin 1 ==");

  Serial.println(s);//$$$$$$$$$$$$$$$$$

  Serial.println( "==  begin 2 ==");


  if (serial_number ==s) 
  { 
    Serial.println( "== OK ==");
    This_is_me = true;
    uint8_t payload[] = "It's me";
  } 
  else
 { 
    Serial.println( "=NO==");
    This_is_me = false;
    uint8_t payload[] = "It's not me";
 }

}
//--------------------------------------------------
void recv_cb(rui_lora_p2p_recv_t data) 
{
  rx_done = true;


    Serial.println("      -----  recv_cb   -----    ");
  if (data.BufferSize == 0) 
  {
    Serial.println("Empty buffer.");
    return;
  }

  char buff[92];
  sprintf(buff, "Incoming message, length: %d, RSSI: %d, SNR: %d", data.BufferSize, data.Rssi, data.Snr);
  
  Serial.print("buff =");//### 
  Serial.println(buff);

  Serial.print("serial nunber =");//###
  Serial.println(serial_number);//###

  hexDump(data.Buffer, data.BufferSize);
}
//-------------------------------
void send_cb(void) 
{
  Serial.printf("P2P set Rx mode %s\r\n", api.lora.precv(65534) ? "Success" : "Fail");
}
//===========================================================================
//===========================================================================
//===========================================================================
void setup() 
{
  Serial.begin(115200);
  Serial.println("RAKwireless LoRaWan P2P Example");
  Serial.println("------------------------------------------------------");
  delay(2000);
  startTime = millis();
  Serial.println("P2P Start");
  Serial.printf("Hardware ID: %s\r\n", api.system.chipId.get().c_str());
  Serial.printf("Model ID: %s\r\n", api.system.modelId.get().c_str());
  Serial.printf("RUI API Version: %s\r\n", api.system.apiVersion.get().c_str());
  Serial.printf("Firmware Version: %s\r\n", api.system.firmwareVersion.get().c_str());
  Serial.printf("AT Command Version: %s\r\n", api.system.cliVersion.get().c_str());
  Serial.printf("Set Node device work mode %s\r\n", api.lora.nwm.set() ? "Success" : "Fail");
  Serial.printf("Set P2P mode frequency %3.3f: %s\r\n", (myFreq / 1e6), api.lora.pfreq.set(myFreq) ? "Success" : "Fail");
  Serial.printf("Set P2P mode spreading factor %d: %s\r\n", sf, api.lora.psf.set(sf) ? "Success" : "Fail");
  Serial.printf("Set P2P mode bandwidth %d: %s\r\n", bw, api.lora.pbw.set(bw) ? "Success" : "Fail");
  Serial.printf("Set P2P mode code rate 4/%d: %s\r\n", (cr + 5), api.lora.pcr.set(0) ? "Success" : "Fail");
  Serial.printf("Set P2P mode preamble length %d: %s\r\n", preamble, api.lora.ppl.set(8) ? "Success" : "Fail");
  Serial.printf("Set P2P mode tx power %d: %s\r\n", txPower, api.lora.ptp.set(22) ? "Success" : "Fail");

Serial.println("  --------------   SLAVE --------------");

  api.lora.registerPRecvCallback(recv_cb);
  api.lora.registerPSendCallback(send_cb);
  Serial.printf("P2P set Rx mode %s\r\n", api.lora.precv(3000) ? "Success" : "Fail");
  // let's kick-start things by waiting 3 seconds.
}

void loop() 
{
  uint8_t payload[] = "It's me";
  bool send_result = false;
  if (rx_done)
  {
    rx_done = false;

        while (!send_result) 
        {
          
              delay(1000);
              Serial.printf("Set P2P to Tx mode %s\r\n", api.lora.precv(0) ? "Success" : "Fail");
           
           
              send_result = api.lora.psend(sizeof(payload), payload);
              if(This_is_me == true) {   Serial.println("This is me");
            } 
            else
            {
              Serial.println("This not me");
            }
       }
         
      //        Serial.printf("P2P send %s\r\n", send_result ? "Success" : "Fail");

  }
  delay(500);
}