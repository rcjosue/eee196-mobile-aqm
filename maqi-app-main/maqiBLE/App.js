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
import {AsyncStorage} from '@react-native-community/async-storage';
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
      latitude: null,
      longitude: null,
      time: null,
      payload: null,
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

  componentDidMount() {
    this.intervalID = setInterval(() => this.updateTime(), 1000);
    NetInfo.addEventListener('connectionChange', connectionInfo => {
      if (connectionInfo.type == 'wifi') {
        this.setState({info: 'Wifi'});
      } else {
        this.setState({dump: 'Not WiFi"!'});
      }
    });
    console.log('Mounted');
    this.scanAndConnect();
    this.setState({info: 'Mounted'});
  }

  scanAndConnect() {
    this.setState({info: 'Scanning'});
    console.log('Scanning');

    this.manager.startDeviceScan(null, null, (error, device) => {
      console.log('Device:');
      console.log(device.name);
      console.log('Scanning devices');
      if (error) {
        this.setState({info: 'Error: ' + error.message});
        console.log(error);
        //console.log(JSON.stringify(error));
        this.scanAndConnect();
        // Handle error (scanning will be stopped automatically)
        return;
      }

      // Check if it is a device you are looking for based on advertisement data
      // or other criteria.
      if (device.name === 'CoE199Dv') {
        // client = new Paho.MQTT.Client(
        //   host:'thingsboard.cloud',
        //   port:9090,
        //   //'/mqtt',
        //   clientId'231cfe90-df3b-11ec-b5bf-ff45c37940c6',
        // );
        // client.onConnectionLost = onConnectionLost;
        // client.onMessageArrived = onMessageArrived;
        // client.connect();
        // function onConnectionLost(responseObject) {
        //   if (responseObject.errorCode !== 0) {
        //     console.log('onConnectionLost:' + responseObject.errorMessage);
        //   }
        // }
        // function onMessageArrived(message) {
        //   console.log('onMessageArrived:' + message.payloadString);
        // }

        // Stop scanning as it's not necessary if you are scanning for one device.
        this.info('Found AQM Device: ' + device.name);
        this.manager.stopDeviceScan();
        console.log('Connecting ' + device.name);

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
    });
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
        const mqtt_payload = '' + split_data;

        //   message = new Paho.MQTT.Message(
        //     mqtt_payload,
        //     'v1/devices/me/telemetry',
        //     1,
        //     0,
        //   );
        //   message.destinationName = 'v1/devices/me/telemetry';
        //   client.publish(message);
        //   // NetInfo.getConnectionInfo().then(connectionInfo => {
        //   //   this.con_type(connectionInfo.type);
        //   //   const isWifi = this.state.con_type;
        //   //   if (isWifi == 'wifi' || isWifi == 'cellular') {
        //   //     message = new Paho.MQTT.Message(
        //   //       mqtt_payload,
        //   //       'v1/devices/me/telemetry',
        //   //       1,
        //   //       0,
        //   //     );
        //   //     message.destinationName = 'v1/devices/me/telemetry';
        //   //     this.save_payload(mqtt_payload);
        //   //     client.publish(message);
        //   //   } else {
        //   //     this.save_payload(mqtt_payload);
        //   //   }
        //   // });
        this.setState({payload: mqtt_payload});
        console.log(mqtt_payload);
      },
    );
  }

  render() {
    return (
      <View style={styles.container}>
        <Text style={styles.welcome}>Welcome to BLE Client!</Text>
        <Text style={styles.info}>Info: {this.state.info}</Text>
        <Text style={styles.info}>Dump: {this.state.dump}</Text>
        <Text style={styles.info}>Payload: {this.state.payload}</Text>
        <Text style={styles.info}>Time: {this.state.time}</Text>
      </View>
    );
  }
}
const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
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
