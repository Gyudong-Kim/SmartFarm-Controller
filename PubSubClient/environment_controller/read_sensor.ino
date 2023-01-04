void read_co2() {
  rs485.begin(38400);
  
  byte request[] = {0x1E, 0x04, 0x00, 0x04, 0x00, 0x01, 0x72, 0x64};
  byte response[16];
  int byteReceived;
  int i = 0;
  
  rs485.write(request, sizeof(request));
  delay(100);
  
  //Copy input data to output
  while (rs485.available()) {
    byteReceived = rs485.read(); // Read the byte 
    response[i++] = byteReceived;
  }
  
  sensor_co2 = response[3] * 256 + response[4];
  
  int size = i;
  for(i = 0; i < size; i++) {
    Serial.print("0x");
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  Serial.print("CO2: ");
  Serial.println(sensor_co2);
  Serial.println();

  rs485.end();
}

void read_temp_humi() {
  rs485.begin(38400);
  
  byte request[] = {0x31, 0x04, 0x00, 0x40, 0x00, 0x02, 0x75, 0xEF};
  byte response[16];
  int byteReceived;
  int i = 0;

  rs485.write(request, sizeof(request));
  delay(100);
  
  //Copy input data to output  
  while (rs485.available()) {
    byteReceived = rs485.read(); // Read the byte 
    response[i++] = byteReceived;
  }

  sensor_humi = (response[3] * 256 + response[4]) / 10.0;
  sensor_temp = (response[5] * 256 + response[6]) / 10.0;
  
  int size = i;
  for(i = 0; i < size; i++) {
    Serial.print("0x");
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  Serial.print("Temperature: ");
  Serial.print(sensor_temp);
  Serial.print(", Humidity: ");
  Serial.println(sensor_humi);
  Serial.println();

  rs485.end();
}