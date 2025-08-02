import "./squareGauge.css";
import "../squares.css";

export default function squareGauge({ title, unit, value, maxValue }) {
  let percentage = (value / maxValue) * 100;
  if (percentage < 0) {
    percentage = 0;
  } else if (percentage > 100) {
    percentage = 100;
  }
  const maxThreshold = 0.8 * maxValue;
  const minThreshold = 0.6 * maxValue;

  let color = "green";
  if (percentage < minThreshold) {
    color = "green";
  } else if (percentage >= minThreshold && percentage <= maxThreshold) {
    color = "orange";
  } else {
    color = "red";
  }
  return (
    <div className="square">
      <div className="squareTitle">
        <span>{title}</span>
        <span>{unit}</span>
      </div>
      <div className="squareGaugeValue">{value}</div>
      <div className="squareGaugeWrapper">
        <div
          className="squareGaugeProgress"
          style={{
            width: `${percentage}%`,
            backgroundColor: `${color}`,
          }}
        ></div>
      </div>
    </div>
  );
}
