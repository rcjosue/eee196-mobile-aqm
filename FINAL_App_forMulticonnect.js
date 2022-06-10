//POSSIBLE EDITS: Write Format & Host
import React, {Component} from 'react';
import {AppState, Platform, StyleSheet, Text, View} from 'react-native';
import {BleManager} from 'react-native-ble-plx';
import base64 from 'react-native-base64';
import {AsyncStorage} from 'react-native';
import NetInfo from "@react-native-community/netinfo";
import init from 'react_native_mqtt';

init({
  size: 10000,
  storageBackend: AsyncStorage,
  defaultExpires: 1000 * 3600 * 24,
  enableCache: true,
  reconnect: true,
  sync : {
  }
});

var connected_to_mqtt = 0;

export default class CoE198 extends Component {
  constructor(props) {
    super(props)
    this.manager = new BleManager();
    this.state = {
      info: null,
      device: null,
      latitude: null,
      longitude: null,
      time: null,
      server_data: null,
      server_long: null,
      server_lat: null,
      server_time: null,
      counter: null,
      key: null,
      payload: null,
      error: null,
      con_type: null,
      global: null,
      values: {}
    }
  }

  updateValue(key,value){
    this.setState({values: {... this.state.values, [key]: value}})
  }

  info(message) {
    this.setState({info:message})
  }

  error(message) {
    this.setState({info:"Error" + message})
  }

  server_data(message) {
    this.setState({server_data:message})
  }

  server_long(message) {
    this.setState({server_long:message})
  }

  server_lat(message) {
    this.setState({server_lat:message})
  }

  server_time(message) {
    this.setState({server_time:message})
  }

  counter(message) {
    this.setState({counter:message})
  }

  key(message) {
    this.setState({key:message})
  }

  payload(message) {
    this.setState({payload:message})
  }

  con_type(message) {
    this.setState({con_type:message})
  }

  dump(message) {
    this.setState({dump:message})
  }

  dump_data(message) {
    this.setState({dump_data:message})
  }

  global(message) {
    this.setState({global:message})
  }

  componentDidMount() {
    this.intervalID = setInterval(() => this.tick(),1000);
    this.watchId = navigator.geolocation.watchPosition(
      (position) => {
        this.setState({
          latitude: position.coords.latitude,
          longitude: position.coords.longitude,
          error: null,
        });
      },
      (error) => this.setState({ error: error.message }),
      { enableHighAccuracy: true, timeout: 20000, maximumAge: 1000, distanceFilter: 10 },
    );
    NetInfo.addEventListener('connectionChange', (connectionInfo) => {
      if (connectionInfo.type == 'wifi') {
        this.connect_mqtt()
      }
      else
      {
        this.dump('Change is not "To WiFi"!')
      }
    })
  }
    
  componentWillMount() {
    clearInterval(this.intervalID);
    console.log("mounted")
    NetInfo.getConnectionInfo().then((connectionInfo) => {
      this.con_type(connectionInfo.type)
      var isWifi = this.state.con_type
      if (isWifi == 'wifi') {
        this.connect_mqtt()
      }
      else if (isWifi == 'cellular') {
        this.connect_mqtt()
      }
      else {
        this.con_type(isWifi)
      }
    })
    const subscription = this.manager.onStateChange((state) => {
      if (state === 'PoweredOn') {
        this.scanAndConnect_1()
        subscription.remove()
      }
    }, true);
  }

  componentWillUnmount() {
    navigator.geolocation.clearWatch(this.watchId);
  }

  tick() {
    this.setState({time: new Date().toLocaleString()});
  }

  connect_mqtt() {
    if (connected_to_mqtt == 0) {
      connected_to_mqtt = 1
      client = new Paho.MQTT.Client('3.0.58.83', 8083, '/mqtt', '')
      client.onConnectionLost = onConnectionLost;
      client.onMessageArrived = onMessageArrived;
      client.connect({onSuccess:onConnect});
      async function onConnect(){
        await AsyncStorage.removeItem('counter')
        var message = ""
        var dump_counter = 0
        while (dump_counter < 128) {
          const dump_key = 'mqtt_payload_' + dump_counter
          const mqtt_payload_dump = await AsyncStorage.getItem(dump_key)
          if (mqtt_payload_dump == null) {
            dump_counter = 128
          }
          else {
          message = new Paho.MQTT.Message(mqtt_payload_dump, "v1/devices/me/telemetry", 1, 0);
          message.destinationName = 'v1/devices/me/telemetry';
          message.qos = 1;
          client.publish(message);
          await AsyncStorage.removeItem(dump_key)
          dump_counter = dump_counter + 1;
          }
        }
      }
      function onConnectionLost(responseObject) {
        connected_to_mqtt = 0
        if (responseObject.errorCode !== 0) {
          console.log("onConnectionLost:"+responseObject.errorMessage);
        }
      }
      function onMessageArrived(message) {
        console.log("onMessageArrived:"+message.payloadString);
      }
    }
  }

  scanAndConnect_1() {
    this.manager.startDeviceScan(null, null, (error, device) => {    
    console.log(device)
    if (error) {
      this.error(error.message)
      this.scanAndConnect_1()
      return
    }
    if ((device.name == 'CoE_198_S')||(device.name == 'CoE_198')) {
      this.info("Connecting to CoE 198")
      console.log("Connecting to CoE 198")
      this.manager.stopDeviceScan()
      device.connect({requestMTU: 183})
        .then((device) => {
          this.info("Discovering services and characteristics");
          console.log("Discovering services and characteristics");
          return device.discoverAllServicesAndCharacteristics()
        })
        .then((device) => {
          this.info("Setting notifications")
          console.log("Setting notifications")
          this.scanAndConnect_2()
          return this.setupNotifications_1(device)
        })
        .then((device) => {
          this.info("Connected to BLE");
          console.log("Connected to BLE");
        })
        .catch((error) => {
          this.error(error.message)
          this.scanAndConnect_1()
        })
    }
    });
  }
  scanAndConnect_2() {
    this.manager.startDeviceScan(null, null, (error, device2) => {    
    console.log(device2)
    if (error) {
      this.error(error.message)
      this.scanAndConnect_2()
      return
    }
    if ((device2.name == 'CoE_198_S')||(device2.name == 'CoE_198')) {
      this.info("Connecting to CoE 198 2")
      console.log("Connecting to CoE 198 2")
      this.manager.stopDeviceScan()
      device2.connect({requestMTU: 183})
        .then((device2) => {
          this.info("Discovering services and characteristics 2");
          console.log("Discovering services and characteristics 2");
          return device2.discoverAllServicesAndCharacteristics()
        })
        .then((device2) => {
          this.info("Setting notifications 2")
          console.log("Setting notifications 2")
          this.scanAndConnect_3()
          return this.setupNotifications_2(device2)
        })
        .then((device2) => {
          this.info("Connected to BLE 2");
          console.log("Connected to BLE 2");
        })
        .catch((error) => {
          this.error(error.message)
          this.scanAndConnect_2()
        })
    }
    });
  }
  scanAndConnect_3() {
    this.manager.startDeviceScan(null, null, (error, device3) => {    
    console.log(device3)
    if (error) {
      this.error(error.message)
      this.scanAndConnect_3()
      return
    }
    if ((device3.name == 'CoE_198_S')||(device3.name == 'CoE_198')) {
      this.info("Connecting to CoE 198 3")
      console.log("Connecting to CoE 198 3")
      this.manager.stopDeviceScan()
      device3.connect({requestMTU: 183})
        .then((device3) => {
          this.info("Discovering services and characteristics 3");
          console.log("Discovering services and characteristics 3");
          return device3.discoverAllServicesAndCharacteristics()
        })
        .then((device3) => {
          this.info("Setting notifications 3")
          console.log("Setting notifications 3")
          return this.setupNotifications_3(device3)
        })
        .then((device3) => {
          this.info("Connected to BLE 3");
          console.log("Connected to BLE 3");
        })
        .catch((error) => {
          this.error(error.message)
          this.scanAndConnect_3()
        })
    }
    });
  }
  async setupNotifications_1(device) {  
    var split_data = ""
    var mqtt_payload = ""
    var isWifi = ""
    device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb', "AQA=")
    if (device.name == 'CoE_198_S'){
      const timestamp = this.state.time   
      const time_epoch = Date.parse(timestamp)
      const time_int=parseInt(time_epoch)/1000
      const time_base64 = base64.encode("" + time_int)
      device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64) 
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
      if (error) {
        this.error(error.message)
        this.scanAndConnect_1()
        return
      }
      this.updateValue(characteristic.uuid, characteristic.value)  
      const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
      const decode_data = base64.decode(data)
      const split_data = decode_data.split("-", 1)
      const mqtt_payload = "" + split_data

      NetInfo.getConnectionInfo().then((connectionInfo) => {
        this.con_type(connectionInfo.type)
        isWifi = this.state.con_type
        if ((isWifi == 'wifi') || (isWifi == 'cellular')) {
          message = new Paho.MQTT.Message(mqtt_payload, "v1/devices/me/telemetry", 1, 0);
          message.destinationName = 'v1/devices/me/telemetry';
          message.qos = 1
          this.save_payload_1(mqtt_payload)
          client.publish(message);
        }
        else {
          this.save_payload_1(mqtt_payload)
        }
      })
      this.payload(mqtt_payload)
      })
    }

    if (device.name == 'CoE_198') {
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
        if (error) {
          this.error(error.message)
          this.scanAndConnect_1()
          return
        }
        this.updateValue(characteristic.uuid, characteristic.value)  
        const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
        
        const long = this.state.longitude
        const lat = this.state.latitude
        const timestamp = this.state.time 
        const time_epoch = Date.parse(timestamp)
        const decode_data = base64.decode(data)
        const split_data = decode_data.split("-", 1)
        const long_readable = "\"longitude\":\"" + long + "\","
        const lat_readable = "\"latitude\":\"" + lat + "\"" + "}}"
        const time_readable = "{\"ts\":\"" + time_epoch + "\","
        const long_base64 = base64.encode(long_readable)
        const lat_base64 = base64.encode(lat_readable)
        const time_base64 = base64.encode(time_readable)
        this.server_data(split_data)
        this.server_long(long_readable)
        this.server_lat(lat_readable)
        this.server_time(time_readable)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff02-0000-1000-8000-00805f9b34fb', long_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff03-0000-1000-8000-00805f9b34fb', lat_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64)
        mqtt_payload = "" + time_readable + split_data + long_readable + lat_readable
        this.payload(mqtt_payload)
        NetInfo.getConnectionInfo().then((connectionInfo) => {
          this.con_type(connectionInfo.type)
          isWifi = this.state.con_type
          if (isWifi == 'wifi') {
            this.save_payload_1(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_1(mqtt_payload)
            }
          }
          else if (isWifi == 'cellular'){
            this.save_payload_1(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_1(mqtt_payload)
            }
          }
          else {
            this.save_payload_1(mqtt_payload)
          }
        })
      })
    }  
  }
  async setupNotifications_2(device) {  
    var split_data = ""
    var mqtt_payload = ""
    var isWifi = ""
    device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb', "AQA=")
    if (device.name == 'CoE_198_S'){
      const timestamp = this.state.time   
      const time_epoch = Date.parse(timestamp)
      const time_int=parseInt(time_epoch)/1000
      const time_base64 = base64.encode("" + time_int)
      device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64) 
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
      if (error) {
        this.error(error.message)
        this.scanAndConnect_2()
        return
      }
      this.updateValue(characteristic.uuid, characteristic.value)  
      const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
      const decode_data = base64.decode(data)
      const split_data = decode_data.split("-", 1)
      const mqtt_payload = "" + split_data

      NetInfo.getConnectionInfo().then((connectionInfo) => {
        this.con_type(connectionInfo.type)
        isWifi = this.state.con_type
        if ((isWifi == 'wifi') || (isWifi == 'cellular')) {
          message = new Paho.MQTT.Message(mqtt_payload, "v1/devices/me/telemetry", 1, 0);
          message.destinationName = 'v1/devices/me/telemetry';
          message.qos = 1
          this.save_payload_2(mqtt_payload)
          client.publish(message);
        }
        else {
          this.save_payload_2(mqtt_payload)
        }
      })
      this.payload(mqtt_payload)
      })
    }

    if (device.name == 'CoE_198') {
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
        if (error) {
          this.error(error.message)
          this.scanAndConnect_2()
          return
        }
        this.updateValue(characteristic.uuid, characteristic.value)  
        const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
        
        const long = this.state.longitude
        const lat = this.state.latitude
        const timestamp = this.state.time 
        const time_epoch = Date.parse(timestamp)
        const decode_data = base64.decode(data)
        const split_data = decode_data.split("-", 1)
        const long_readable = "\"longitude\":\"" + long + "\","
        const lat_readable = "\"latitude\":\"" + lat + "\"" + "}}"
        const time_readable = "{\"ts\":\"" + time_epoch + "\","
        const long_base64 = base64.encode(long_readable)
        const lat_base64 = base64.encode(lat_readable)
        const time_base64 = base64.encode(time_readable)
        this.server_data(split_data)
        this.server_long(long_readable)
        this.server_lat(lat_readable)
        this.server_time(time_readable)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff02-0000-1000-8000-00805f9b34fb', long_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff03-0000-1000-8000-00805f9b34fb', lat_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64)
        mqtt_payload = "" + time_readable + split_data + long_readable + lat_readable
        this.payload(mqtt_payload)
        NetInfo.getConnectionInfo().then((connectionInfo) => {
          this.con_type(connectionInfo.type)
          isWifi = this.state.con_type
          if (isWifi == 'wifi') {
            this.save_payload_2(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_2(mqtt_payload)
            }
          }
          else if (isWifi == 'cellular'){
            this.save_payload_2(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_2(mqtt_payload)
            }
          }
          else {
            this.save_payload_2(mqtt_payload)
          }
        })
      })
    }  
  }
  async setupNotifications_3(device) {  
    var split_data = ""
    var mqtt_payload = ""
    var isWifi = ""
    device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb', "AQA=")
    if (device.name == 'CoE_198_S'){
      const timestamp = this.state.time   
      const time_epoch = Date.parse(timestamp)
      const time_int=parseInt(time_epoch)/1000
      const time_base64 = base64.encode("" + time_int)
      device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64) 
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
      if (error) {
        this.error(error.message)
        this.scanAndConnect_3()
        return
      }
      this.updateValue(characteristic.uuid, characteristic.value)  
      const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
      const decode_data = base64.decode(data)
      const split_data = decode_data.split("-", 1)
      const mqtt_payload = "" + split_data

      NetInfo.getConnectionInfo().then((connectionInfo) => {
        this.con_type(connectionInfo.type)
        isWifi = this.state.con_type
        if ((isWifi == 'wifi') || (isWifi == 'cellular')) {
          message = new Paho.MQTT.Message(mqtt_payload, "v1/devices/me/telemetry", 1, 0);
          message.destinationName = 'v1/devices/me/telemetry';
          message.qos = 1
          this.save_payload_3(mqtt_payload)
          client.publish(message);
        }
        else {
          this.save_payload_3(mqtt_payload)
        }
      })
      this.payload(mqtt_payload)
      })
    }

    if (device.name == 'CoE_198') {
      device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
        if (error) {
          this.error(error.message)
          this.scanAndConnect_3()
          return
        }
        this.updateValue(characteristic.uuid, characteristic.value)  
        const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
        
        const long = this.state.longitude
        const lat = this.state.latitude
        const timestamp = this.state.time 
        const time_epoch = Date.parse(timestamp)
        const decode_data = base64.decode(data)
        const split_data = decode_data.split("-", 1)
        const long_readable = "\"longitude\":\"" + long + "\","
        const lat_readable = "\"latitude\":\"" + lat + "\"" + "}}"
        const time_readable = "{\"ts\":\"" + time_epoch + "\","
        const long_base64 = base64.encode(long_readable)
        const lat_base64 = base64.encode(lat_readable)
        const time_base64 = base64.encode(time_readable)
        this.server_data(split_data)
        this.server_long(long_readable)
        this.server_lat(lat_readable)
        this.server_time(time_readable)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff02-0000-1000-8000-00805f9b34fb', long_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff03-0000-1000-8000-00805f9b34fb', lat_base64)
        device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64)
        mqtt_payload = "" + time_readable + split_data + long_readable + lat_readable
        this.payload(mqtt_payload)
        NetInfo.getConnectionInfo().then((connectionInfo) => {
          this.con_type(connectionInfo.type)
          isWifi = this.state.con_type
          if (isWifi == 'wifi') {
            this.save_payload_3(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_3(mqtt_payload)
            }
          }
          else if (isWifi == 'cellular'){
            this.save_payload_3(mqtt_payload)
            if (connected_to_mqtt == 1) {
              this.send_payload_3(mqtt_payload)
            }
          }
          else {
            this.save_payload_3(mqtt_payload)
          }
        })
      })
    }  
  }
  async dump_mem() { //Send and Delete Entries in memory
    if (connected_to_mqtt == 1){
    this.global(connected_to_mqtt)
    this.dump('Dumping All Contents!')
    this.dump_data('Dumping current Content')
    await AsyncStorage.removeItem('counter')
    var message = ""

    var dump_counter = 0
    while (dump_counter < 128) {
      this.dump(dump_counter)
          const dump_key = 'mqtt_payload_' + dump_counter
          const mqtt_payload_dump = await AsyncStorage.getItem(dump_key)
          if (mqtt_payload_dump == null){
            dump_counter = 128
          }
          else {
          this.dump(dump_key)
          this.dump_data(mqtt_payload_dump)
          message = new Paho.MQTT.Message(mqtt_payload_dump, "v1/devices/me/telemetry", 1, 0);
          message.destinationName = 'v1/devices/me/telemetry';
          message.qos = 1;
          client.publish(message);
          await AsyncStorage.removeItem(dump_key)
          dump_counter = dump_counter + 1;
          }
    }
      return
    }
  }

  async save_payload_1(item) { //Save from mqtt_payload_0 to mqtt_payload_128
    var value = await AsyncStorage.getItem('counter')
    if ((value == null) || (value == '128')) {
      await AsyncStorage.setItem('counter', '0')
      value = await AsyncStorage.getItem('counter')
    }
    else {
      value = await AsyncStorage.getItem('counter')
    }
        this.counter(value)
        const key = 'mqtt_payload_' + value
        this.key(key)
        await AsyncStorage.setItem(key, item)
        const payload = await AsyncStorage.getItem(key)
        this.payload(payload)
        const val_int = parseInt(value)
        const new_val_int = val_int + 1
        const new_val = ("" + new_val_int)
        await AsyncStorage.setItem('counter', new_val)
    return
  }
  async save_payload_2(item) { //Save from mqtt_payload_0 to mqtt_payload_128
    var value = await AsyncStorage.getItem('counter')
    if ((value == null) || (value == '128')) {
      await AsyncStorage.setItem('counter', '0')
      value = await AsyncStorage.getItem('counter')
    }
    else {
      value = await AsyncStorage.getItem('counter')
    }
        this.counter(value)
        const key = 'mqtt_payload_' + value
        this.key(key)
        await AsyncStorage.setItem(key, item)
        const payload = await AsyncStorage.getItem(key)
        this.payload(payload)
        const val_int = parseInt(value)
        const new_val_int = val_int + 1
        const new_val = ("" + new_val_int)
        await AsyncStorage.setItem('counter', new_val)
    return
  }
  async save_payload_3(item) { //Save from mqtt_payload_0 to mqtt_payload_128
    var value = await AsyncStorage.getItem('counter')
    if ((value == null) || (value == '128')) {
      await AsyncStorage.setItem('counter', '0')
      value = await AsyncStorage.getItem('counter')
    }
    else {
      value = await AsyncStorage.getItem('counter')
    }
        this.counter(value)
        const key = 'mqtt_payload_' + value
        this.key(key)
        await AsyncStorage.setItem(key, item)
        const payload = await AsyncStorage.getItem(key)
        this.payload(payload)
        const val_int = parseInt(value)
        const new_val_int = val_int + 1
        const new_val = ("" + new_val_int)
        await AsyncStorage.setItem('counter', new_val)
    return
  }
  async send_payload_1(item) { //send over mqtt
        const message = new Paho.MQTT.Message(item, "v1/devices/me/telemetry", 1, 0);
        message.destinationName = 'v1/devices/me/telemetry';
        message.qos = 1;
        client.publish(message);
    return
  }
  
  async send_payload_2(item) { //send over mqtt
        const message = new Paho.MQTT.Message(item, "v1/devices/me/telemetry", 1, 0);
        message.destinationName = 'v1/devices/me/telemetry';
        message.qos = 1;
        client.publish(message);
    return
  }   
  async send_payload_3(item) { //send over mqtt
        const message = new Paho.MQTT.Message(item, "v1/devices/me/telemetry", 1, 0);
        message.destinationName = 'v1/devices/me/telemetry';
        message.qos = 1;
        client.publish(message)
    return
  } 
  render() {
    return (
      <View style={styles.container}>
        <Text style={styles.info}>{this.state.info}</Text>
        <Text style={styles.welcome}>Welcome to BLE Daemon!</Text> 
        <Text style={styles.info}>{this.state.global}</Text> 
        <Text style={styles.info}>{this.state.dump}</Text>
        <Text style={styles.info}>{this.state.dump_data}</Text>
        <Text style={styles.infoheader}>Notify</Text>
        <Text style={styles.info}>{this.state.server_data}</Text>
        <Text style={styles.info}>{this.state.server_long}</Text>
        <Text style={styles.info}>{this.state.server_lat}</Text>
        <Text style={styles.info}>{this.state.server_time}</Text>
        <Text style={styles.infoheader}>MQTT and Save</Text>
        <Text style={styles.info}>The con_type is {this.state.con_type}</Text>
        <Text style={styles.info}>The MQTT Counter is {this.state.counter}</Text>
        <Text style={styles.info}>The Key is {this.state.key}</Text>
        <Text style={styles.info}>The MQTT Payload string is {this.state.payload}</Text>
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
});