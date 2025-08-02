import React from "react";
import "./SquareOnOff.css";
import "../squares.css";

export default function SquareOnOff({ title, isOn }) {
  let value = isOn === true ? "on" : "off";
  const dotColor = isOn === true ? "green" : "red";

  return (
    <div className="square">
      <div className="squareTitle">
        <span>{title}</span>
        <span
          className="coloredDot"
          style={{ backgroundColor: `${dotColor}` }}
        ></span>
      </div>
      <div className="squareValue">{value}</div>
    </div>
  );
}
