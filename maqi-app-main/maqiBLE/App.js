/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 * @flow strict-local
 */

import React, {Component} from 'react';
//import type {Node} from 'react';
import {
  //SafeAreaView,  ScrollView,  StatusBar,   useColorScheme,  NativeModules,  NativeEventEmitter,
  StyleSheet,
  Text,
  View,
} from 'react-native';

import {BleManager} from 'react-native-ble-plx';
import NetInfo from '@react-native-community/netinfo';
import base64 from 'react-native-base64';
import AsyncStorage from '@react-native-async-storage/async-storage';
import init from 'react_native_mqtt';

// import {
//   Colors,
//   DebugInstructions,
//   Header,
//   LearnMoreLinks,
//   ReloadInstructions,
// } from 'react-native/Libraries/NewAppScreen';

// const App: () => Node = () => {
//   const isDarkMode = useColorScheme() === 'dark';

//   const backgroundStyle = {
//     backgroundColor: isDarkMode ? Colors.darker : Colors.lighter,
//   };

//   return (
//     <SafeAreaView style={backgroundStyle}>
//       <StatusBar barStyle={isDarkMode ? 'light-content' : 'dark-content'} />
//       <ScrollView
//         contentInsetAdjustmentBehavior="automatic"
//         style={backgroundStyle}>
//         <Header />
//         <View
//           style={{
//             backgroundColor: isDarkMode ? Colors.black : Colors.white,
//           }}>
//           <LearnMoreLinks />
//         </View>
//       </ScrollView>
//     </SafeAreaView>
//   );
// };

init({
  size: 10000,
  storageBackend: AsyncStorage,
  defaultExpires: 1000 * 3600 * 24,
  enableCache: true,
  reconnect: true,
  sync: {},
});

export default class App extends Component {
  constructor(props) {
    super(props);
    this.manager = new BleManager();
    this.state = {
      info: null,
      dump: null,
      device: null,
      con_type: null,
      counter: null,
      latitude: null,
      longitude: null,
      time: null,
      payload: null,
      mqttPayload: null,
      error: null,
    };
  }

  updateTime() {
    this.setState({time: new Date().toLocaleString()});
  }
  updatePair(key, value) {
    this.setState({values: {...this.state.values, [key]: value}});
  }
  info(message) {
    this.setState({info: message});
  }
  key(message) {
    this.setState({key: message});
  }
  con_type(message) {
    this.setState({con_type: message});
  }

  componentDidMount() {
    this.intervalID = setInterval(() => this.updateTime(), 1000);
    // Subscribe to connection changes
    NetInfo.addEventListener(connectionInfo => {
      this.con_type(connectionInfo.type);
      const isWifi = this.state.con_type;
      if (isWifi == 'wifi' || isWifi == 'cellular') {
        this.dump_mem();
        this.setState({dump: 'Not connected to network!'});
      } else {
        this.con_type(isWifi);
        this.setState({dump: 'Not connected to network!'});
      }
    });
    // NetInfo.refresh().then(connectionInfo => {
    //   this.con_type(connectionInfo.type);
    //   const isWifi = this.state.con_type;
    //   if (isWifi == 'wifi' || isWifi == 'cellular') {
    //     this.dump_mem();
    //   } else {
    //     this.con_type(isWifi);
    //     this.setState({dump: 'Not connected to network!'});
    //   }
    // });

    console.log('Mounted');
    this.scanAndConnect();
    this.setState({info: 'Mounted'});
  }

  scanAndConnect() {
    this.setState({info: 'Scanning'});
    console.log('Scanning');

    this.manager.startDeviceScan(null, null, (error, device) => {
      //console.log('Device: ' + device.name);
      if (error) {
        this.setState({info: 'Error: ' + error.message});
        console.log(error);
        this.scanAndConnect();
        // Handle error (scanning will be stopped automatically)
        return;
      }
      // Check if it is a device you are looking for based on advertisement data
      // or other criteria.
      if (
        device.name === 'maqiBLE_1' ||
        device.name === 'maqiBLE_2' ||
        device.name === 'maqiBLE_3'
      ) {
        this.setupDevice(device);
      }
    });
  }
  setupDevice(device) {
    // Stop scanning as it's not necessary if you are scanning for one device.
    this.setState({device: device.name});
    this.info('Found AQM Device: ' + device.name);
    this.manager.stopDeviceScan();
    console.log('Connecting ' + device.name);
    const device_id = 'Device_4'; //this.state.device.name; move to top
    const topic = `tb/mqtt-integration-guide/sensors/${device_id}/telemetry`;
    client = new Paho.MQTT.Client('18.140.52.158', 8083, '/mqtt', '');
    client.onConnectionLost = onConnectionLost;
    client.onMessageArrived = onMessageArrived;
    client.connect({onSuccess: onConnect});
    function onConnect() {
      console.log('connected');
    }
    function onConnectionLost(responseObject) {
      if (responseObject.errorCode !== 0) {
        console.log('onConnectionLost:' + responseObject.errorMessage);
      }
    }
    function onMessageArrived(message) {
      console.log('onMessageArrived:'); // + message.payloadString);
    }

    device
      .connect({requestMTU: 280})
      .then(device => {
        this.info('Discovering services and characteristics');
        console.log('Discovering services and characteristics');
        return device.discoverAllServicesAndCharacteristics();
      })
      .then(device => {
        this.info('Setting notifications');
        console.log('Setting notifications');
        return this.setupNotifications(device);
      })

      // .then(device => {
      //   this.info('Connected to BLE');
      //   console.log('Connected to BLE');
      // })
      .catch(error => {
        this.setState({info: 'Error: ' + error.message});

        this.scanAndConnect();
      });
  }
  async save_payload(item) {
    //Save from mqtt_payload_0 to mqtt_payload_64
    var value = await AsyncStorage.getItem('counter');
    if (value == null || value == '64') {
      await AsyncStorage.setItem('counter', '0');
      value = await AsyncStorage.getItem('counter');
    } else {
      value = await AsyncStorage.getItem('counter');
    }
    this.setState({counter: value});
    const key = 'mqtt_payload_' + value;
    this.setState({key: key});
    await AsyncStorage.setItem(key, item);
    const payload = await AsyncStorage.getItem(key);
    this.setState({payload: payload});
    const val_int = parseInt(value);
    const new_val_int = val_int + 1;
    const new_val = '' + new_val_int;
    await AsyncStorage.setItem('counter', new_val);
    return;
  }

  async dump_mem() {
    //console.log(item);
    this.setState({dump: 'Dumping All Contents!'});
    var message = '';
    //  '{"ts":1654951056000,"pm10":"","pm25":"1","temp":"1","hum":"","long":"121.04664227522113","lat":"14.553687875023439","device_id":"Device_4"}';
    if (this.state.device == null) this.setState({device: 'maqiBLE_1'});
    const device_id = this.state.device;
    const topic = `tb/mqtt-integration-guide/sensors/${device_id}/telemetry`;
    client = new Paho.MQTT.Client('18.140.52.158', 8083, '/mqtt', '');
    client.onConnectionLost = onConnectionLost;
    client.onMessageArrived = onMessageArrived;
    client.connect({onSuccess: onConnect, onFailure: onConnectionLost});
    var dump_counter = 0;

    async function onConnect() {
      while (dump_counter < 64) {
        const dump_key = 'mqtt_payload_' + dump_counter;
        const mqtt_payload_dump = await AsyncStorage.getItem(dump_key);
        if (mqtt_payload_dump == null) {
          dump_counter = 64;
        } else {
          message = new Paho.MQTT.Message(mqtt_payload_dump, topic, 1, 0);
          message.destinationName = topic;
          client.publish(message);
          await AsyncStorage.removeItem(dump_key);
          dump_counter = dump_counter + 1;
        }
      }
      return;
    }

    function onConnectionLost(responseObject) {
      console.log('disconnected');
      if (responseObject.errorCode !== 0) {
        console.log('onConnectionLost:' + responseObject.errorMessage);
      }
    }
    function onMessageArrived(message) {
      console.log('onMessageArrived:'); // + message.payloadString);
    }
  }

  async setupNotifications(device) {
    var message = '';
    device.writeCharacteristicWithoutResponseForService(
      '000000ff-0000-1000-8000-00805f9b34fb',
      '0000ff01-0000-1000-8000-00805f9b34fb',
      'AQA=',
    );
    console.log('first ok');
    const timestamp = this.state.time;
    const time_epoch = Date.parse(timestamp);
    const time_int = parseInt(time_epoch) / 1000;
    const time_base64 = base64.encode('' + time_int);
    device.writeCharacteristicWithoutResponseForService(
      '000000ff-0000-1000-8000-00805f9b34fb',
      '0000ff04-0000-1000-8000-00805f9b34fb',
      time_base64,
    );
    console.log('second ok');
    this.setState({time: time_base64});
    console.log('time: ' + time_int);
    console.log('time base: ' + time_base64);
    console.log('third becu');
    device.monitorCharacteristicForService(
      '000000ff-0000-1000-8000-00805f9b34fb',
      '0000ff01-0000-1000-8000-00805f9b34fb',
      (error, characteristic) => {
        if (error) {
          console.log('third ok');
          this.info('Error: ' + error.message);
          this.scanAndConnect();
          return;
        }
        this.updatePair(characteristic.uuid, characteristic.value);
        const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb'];
        const decode_data = base64.decode(data);
        const split_data = decode_data.split('_', 1);
        const device_id = device.name;
        const mqtt_payload = split_data + ',"device_id":"' + device_id + '"}';

        this.send_payload(mqtt_payload);
      },
    );
  }

  async send_payload(item) {
    const start = new Date().getTime();
    // Format Payload (unneccessary now)
    // let test_split = item.split(/[:|,]/);
    // if (test_split[4].slice(1, -1) == '') test_split[4] = '"0"';
    // if (test_split[6].slice(1, -1) == '') test_split[6] = '"0"';
    // if (test_split[8].slice(1, -1) == '') test_split[8] = '"0"';
    // if (test_split[10].slice(1, -1) == '') test_split[10] = '"0"';

    // let mqttPayload = JSON.stringify({
    //   timestamp: test_split[1],
    //   pm10: test_split[4].slice(1, -1),
    //   pm25: test_split[6].slice(1, -1),
    //   temperature: test_split[8].slice(1, -1),
    //   humidity: test_split[10].slice(1, -1),
    //   latitude: test_split[12].slice(1, -1),
    //   longitude: test_split[14].slice(1, -4),
    //   device_id: 'Device_4', //this.state.device,
    // });
    // console.log('payload', mqttPayload);

    NetInfo.refresh().then(connectionInfo => {
      this.con_type(connectionInfo.type);
      const isWifi = this.state.con_type;

      const device_id = this.state.device;
      const topic = `tb/mqtt-integration-guide/sensors/${device_id}/telemetry`;

      if (isWifi == 'wifi' || isWifi == 'cellular') {
        const message = new Paho.MQTT.Message(item, topic, 1, 0);
        message.destinationName = topic;
        client.publish(message);
        const end = new Date().getTime();
        console.log('sending time = ' + (end - start));
        //this.save_payload(item);
      } else {
        this.save_payload(item);
      }
    });
    this.setState({payload: item});
    //console.log(item);
  }

  render() {
    return (
      <>
        <View style={styles.header}>
          <Text style={styles.title}>MAQI</Text>
        </View>

        <View style={styles.filler} />
        <View style={styles.container}>
          <Text style={styles.welcome}>Welcome to BLE Client!</Text>
          <Text style={styles.info}>Info: {this.state.info}</Text>
          <Text style={styles.info}>Dump: {this.state.dump}</Text>

          <Text style={styles.infoheader}>Notify</Text>
          <Text style={styles.info}>Payload: {this.state.payload}</Text>
          <Text style={styles.info}>Time: {this.state.time}</Text>

          <Text style={styles.infoheader}>MQTT and Save</Text>
          <Text style={styles.info}>
            The connection is {this.state.con_type}
          </Text>
          <Text style={styles.info}>
            The MQTT Counter is {this.state.counter}
          </Text>
          <Text style={styles.info}>The Key is {this.state.key}</Text>
          <Text style={styles.info}>
            The MQTT Payload string is {this.state.payload}
          </Text>
        </View>
        <View style={styles.filler} />
      </>
    );
  }
}
const styles = StyleSheet.create({
  container: {
    flex: 3,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
  filler: {
    height: 40,
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#FDFDFD',
  },
  header: {
    height: 80,
    paddingTop: 38,
    backgroundColor: '#187bcd',
  },
  title: {
    textAlign: 'center',
    color: '#fff',
    fontSize: 20,
    fontWeight: 'bold',
  },
  welcome: {
    fontSize: 20,
    textAlign: 'center',
    color: '#333333',
  },
  instructions: {
    textAlign: 'center',
    color: '#333333',
    marginBottom: 5,
  },
  infoheader: {
    fontSize: 20,
    textAlign: 'right',
    color: '#333333',
    marginBottom: 5,
    marginTop: 20,
  },
  info: {
    fontSize: 15,
    textAlign: 'center',
    color: '#333333',
  },
  // sectionContainer: {
  //   marginTop: 32,
  //   paddingHorizontal: 24,
  // },
  // sectionTitle: {
  //   fontSize: 24,
  //   fontWeight: '600',
  // },
  // sectionDescription: {
  //   marginTop: 8,
  //   fontSize: 18,
  //   fontWeight: '400',
  // },
  // highlight: {
  //   fontWeight: '700',
  // },
});

//export default App;
