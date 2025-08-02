import "./App.css";
import "./components/squareOnOff/SquareOnOff.css";
import { Routes, Route } from "react-router-dom";

import RealtimeView from "./view/realtime/realtimeView.js";
import GraphsView from "./view/graphs/graphsView.js";
import DataView from "./view/data/dataView.js";
import SettingsView from "./view/settings/settingsView.js";

import { useEffect, useState } from "react";
import mqtt from "mqtt";
export let tempMotor, tempFuelCell, tempBattery, speed, relay1, relay2;

function App() {
  const urlString = "ws://broker.emqx.io:8083/mqtt";
  const topicSelected = ["testtopic/578", "testtopic/577", "testtopic/579"];
  const [realtimeData, setRealtimeData] = useState(
    '{"tempMotor":"X","tempFuelCell":"X","tempBattery":"X","speed":"X","relay1":"X","relay2":"X"}',
  );
  const [mqttClient, setMqttClient] = useState(null);
  useEffect(() => {
    setMqttClient(mqtt.connect(urlString, { resubscribe: false }));
  }, []);
  useEffect(() => {
    if (mqttClient) {
      mqttClient.on("connect", () => {
        console.log("Connected");
        topicSelected.forEach((topic) => {
          mqttClient.subscribe(topic, (error) => {
            if (error) {
              console.log("Subscribe to topics error", error);
            } else {
              console.log("Subscribed to " + topic);
              //mqttClient.publish(topic, 'hello stranger');
            }
          });
        });
      });
      mqttClient.on("error", (err) => {
        console.log("Connection error: ", err);
        mqttClient.end();
      });
      mqttClient.on("reconnect", () => {
        console.log("Reco");
      });
      mqttClient.on("disconnect", () => {
        console.log("Deco");
      });

      mqttClient.on("message", (topic, message) => {
        if (topic === "testtopic/578") {
          setRealtimeData(message.toString());
        }
        const payload = { topic, message: message.toString() };
        console.log(payload);
        //let payloadContent = JSON.parse(message.toString());
        //console.log(payloadContent.speed);
      });
    }
  }, [mqttClient]);

  return (
    <div className="App">
      <header className="App-header">
        <Routes>
          <Route path="/Graphs" element={<GraphsView />} />
          <Route
            path="/"
            element={<RealtimeView realtimeData={realtimeData} />}
          />
          <Route path="/Settings" element={<SettingsView />} />
          <Route path="/Data" element={<DataView />} />
        </Routes>
      </header>
    </div>
  );
}

export default App;
