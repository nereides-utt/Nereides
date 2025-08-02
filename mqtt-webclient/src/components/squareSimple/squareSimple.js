import "./squareSimple.css";
import "../squares.css";

export default function squareGauge({ title, unit, value }) {
  return (
    <div className="square">
      <div className="squareTitle">
        <span>{title}</span>
        <span>{unit}</span>
      </div>
      <div className="squareValue">{value}</div>
    </div>
  );
}
