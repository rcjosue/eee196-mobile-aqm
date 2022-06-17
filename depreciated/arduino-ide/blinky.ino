#define led 2

void setup() {
  // put your setup code here, to run once:
  pinMode(led, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("Hello World!");
  digitalWrite(led, HIGH);
  delay(500);

  digitalWrite(led, LOW);
  delay(500);
}
