import "./realtimeView.css";
import "../views.css";
import Navbar from "../fragment/navbarFragment/navbarFragment";
import SquareSimple from "../../components/squareSimple/squareSimple";
import SquareGauge from "../../components/squareGauge/squareGauge";
import SquareOnOff from "../../components/squareOnOff/squareOnOff";
import RectangleGauge from "../../components/rectangleGauge/rectangleGauge";
import RectangleTime from "../../components/rectangleTime/rectangleTime";
import { useEffect, useState } from "react";

export default function realtimeView({ realtimeData }) {
  //console.log(realtimeData);
  console.log(typeof realtimeData);
  let data;
  let speed, tempMotor, tempBattery, tempFuelCell, relay1, relay2;
  if (realtimeData !== "") {
    data = JSON.parse(realtimeData);
    console.log(data);
    speed = data.speed;
    tempMotor = data.tempMotor;
    tempBattery = data.tempBattery;
    tempFuelCell = data.tempFuelCell;
    relay1 = data.relay1;
    relay2 = data.relay2;
  }

  return (
    <div className={"mainContainer"}>
      {/*<Navbar linkIsClicked={linkIsClicked} />*/}
      <div className="mainGrid">
        <div className={"gridLine"}>
          <h1>Navigation</h1>
        </div>
        <div className={"gridLine"}>
          <SquareSimple title="Speed" unit="km/h" value={speed} />
          <RectangleTime title={"Runtime"} time={6243} />
          <RectangleTime title={"Remaining Time"} time={5738} />
        </div>
        <div className={"gridLine"}>
          <h1>Temperatures</h1>
        </div>
        <div className={"gridLine"}>
          <RectangleGauge
            title={"H2 Pressure"}
            value={187}
            maxValue={640}
            unit={"bar"}
          />
          <SquareGauge
            title="Motor"
            unit="°C"
            value={tempMotor}
            maxValue={100}
          />
          <SquareGauge
            title="Fuel Cell"
            unit="°C"
            value={tempFuelCell}
            maxValue={100}
          />
        </div>
        <div className={"gridLine"}>
          <SquareOnOff title="relay1" isOn={relay1} />
          <SquareOnOff title="relay2" isOn={relay2} />
          <RectangleGauge
            title={"Battery Level"}
            value={67}
            maxValue={100}
            invertMax={true}
            unit={"%"}
          />
        </div>
      </div>
      <Navbar />
    </div>
  );
}
