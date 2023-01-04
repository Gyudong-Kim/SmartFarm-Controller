int read_pe300() {
  rs485.begin(38400);
  
  byte request[] = {0x1F, 0x04, 0x00, 0x01, 0x00, 0x03, 0xE2, 0x75};  // for PE300(통합)
  byte response[24];
  int byteReceived;
  int i = 0;
  
  rs485.write(request, sizeof(request));
  delay(100);
  
  //Copy input data to output  
  while (rs485.available()) {
    byteReceived = rs485.read();   // Read the byte 
    response[i++] = byteReceived;
  }

  sensor_ec = (response[3] * 256 + response[4]) / 1000.0;
  sensor_ph = (response[5] * 256 + response[6]) / 100.0;
  
  int size = i;
  for(i = 0; i < size; i++) {
    Serial.print("0x");
    Serial.print(response[i],HEX);
    Serial.print(" ");
  }
  Serial.println();

  Serial.print("pH: ");
  Serial.println(sensor_ph);
  Serial.print("EC: ");
  Serial.println(sensor_ec);
  Serial.println();

  rs485.end();
}