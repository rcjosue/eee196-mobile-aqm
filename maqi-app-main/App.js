import React, { useState, useEffect } from "react";
import {
  Platform,
  Text,
  View,
  StyleSheet,
  TouchableOpacity,
  AsyncStorageStatic,
  Button,
} from "react-native";
import MQTTConnection from "./src/MQTTConnection";
import Constants from "expo-constants";
import * as Location from "expo-location";
import Header from "./components/header";
import { Buffer } from "buffer";
global.Buffer = Buffer;

export default function App() {
  const [location, setLocation] = useState(null);
  const [errorMsg, setErrorMsg] = useState(null);
  const [pressed, setPressed] = useState(false);

  useEffect(() => {
    (async () => {
      if (Platform.OS === "android" && !Constants.isDevice) {
        setErrorMsg(
          "Oops, this will not work on Snack in an Android emulator. Try it on your device!"
        );
        return;
      }
      let { status } = await Location.requestForegroundPermissionsAsync();
      if (status !== "granted") {
        setErrorMsg("Permission to access location was denied");
        return;
      }

      let location = await Location.getCurrentPositionAsync({});
      setLocation(location);
    })();

    /*
    //MQTT Setup
    this.mqttConnect = new MQTTConnection();
    this.mqttConnect.onMQTTConnect = this.onMQTTConnect;
    this.mqttConnect.onMQTTLost = this.onMQTTLost;
    this.mqttConnect.onMQTTMessageArrived = this.onMQTTMessageArrived;
    this.mqttConnect.onMQTTMessageDelivered = this.onMQTTMessageDelivered;

    // this.mqttConnect.connect(ip, port);
    this.mqttConnect.connect("broker.hivemq.com", 1883);
    // this.mqttConnect.connect("broker.mqttdashboard.com", 8000);

    onMQTTConnect = () => {
      console.log("App onMQTTConnect");
      this.mqttConnect.subscribeChannel(
        "tb/mqtt-integration-tutorial/sensors/Device_1/temperature"
      );
      // this.mqttConnect.subscribeChannel("hanth2");
    };

    onMQTTLost = () => {
      console.log("App onMQTTLost");
    };

    onMQTTMessageArrived = (message) => {
      console.log("App onMQTTMessageArrived: ", message);
      console.log(
        "APP onMQTTMessageArrived payloadString: ",
        message.payloadString
      );
    };

    onMQTTMessageDelivered = (message) => {
      console.log("App onMQTTMessageDelivered: ", message);
    };

    return () => {
      this.mqttConnect.close();
    };
    */
  }, [pressed]);

  let text = "Waiting..";
  if (errorMsg) {
    text = errorMsg;
  } else if (location) {
    text = JSON.stringify(location);
  }

  return (
    <>
      <Header />
      <View style={styles.container}>
        <Text style={styles.paragraph}>{text}</Text>
        <Button
          title="Send data"
          onPress={() => {
            this.mqttConnect.send(
              "tb/mqtt-integration-tutorial/sensors/Device_2/temperature",
              "{'temperature':42}"
            );
            setPressed(!pressed);
          }}
        />
        {/* <Button
          title="Send data"
          onPress={() =>
            this.mqttConnect.send("hanth2", "{'temperature':4231}")
          }
        /> */}
      </View>
    </>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    padding: 20,
  },
  paragraph: {
    fontSize: 18,
    textAlign: "center",
  },
});
