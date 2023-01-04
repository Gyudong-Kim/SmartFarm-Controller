void read_soil_1() {
  rs485.begin(38400);

  byte request[] = {0x05, 0x04, 0x00, 0x07, 0x00, 0x03, 0x00, 0x4E};
  byte response[20];
  int byteReceived;
  int i = 0;

  rs485.write(request, sizeof(request));
  delay(100);
  
  //Copy input data to output  
  while (rs485.available()) {
    byteReceived = rs485.read();   // Read the byte 
    response[i++] = byteReceived;
  }
  sensor_soil_humi = (response[3] * 256 + response[4]) / 100.00;
  sensor_soil_temp = (response[5] * 256 + response[6]) / 100.00;
  sensor_soil_ec = (response[7] * 256 + response[8]) / 100.00;
  
  int size = i;
  for(i = 0; i < size; i++) {
    Serial.print("0x");
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  Serial.print("Soil Temp 1: ");
  Serial.println(sensor_soil_temp);
  Serial.print("Soil Humi 1: ");
  Serial.println(sensor_soil_humi);
  Serial.print("Soil EC 1: ");
  Serial.println(sensor_soil_ec);
  Serial.println();

  rs485.end();
}

void read_soil_2() {
  rs485.begin(38400);

  byte request[] = {0x06, 0x04, 0x00, 0x07, 0x00, 0x03, 0x00, 0x7D};
  byte response[20];
  int byteReceived;
  int i = 0;
  
  rs485.write(request, sizeof(request));
  delay(100);
  
  //Copy input data to output  
  while (rs485.available()) {
    byteReceived = rs485.read();   // Read the byte 
    response[i++] = byteReceived;
  }
  sensor_soil_humi_2 = (response[3] * 256 + response[4]) / 100.0;
  sensor_soil_temp_2 = (response[5] * 256 + response[6]) / 100.0;
  sensor_soil_ec_2 = (response[7] * 256 + response[8]) / 100.0;
  
  int size = i;
  for(i = 0; i < size; i++) {
    Serial.print("0x");
    Serial.print(response[i],HEX);
    Serial.print(" ");
  }
  Serial.println();

  Serial.print("Soil Temp 2: ");
  Serial.println(sensor_soil_temp_2);
  Serial.print("Soil Humi 2: ");
  Serial.println(sensor_soil_humi_2);
  Serial.print("Soil EC 2: ");
  Serial.println(sensor_soil_ec_2);
  Serial.println();

  rs485.end();
}